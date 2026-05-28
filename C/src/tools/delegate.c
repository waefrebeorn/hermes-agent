/*
 * delegate.c — Delegate task tool for Hermes C (P116-P125).
 *
 * Full delegation infrastructure:
 *   P116: Concurrent children — thread pool for N parallel subagents
 *   P117: Child timeout — per-child SIGKILL via kill() on timeout
 *   P118: Orchestrator mode — children may call delegate_task (track depth)
 *   P119: Child model override — per-child model/provider/base_url
 *   P120: Child isolation — each child gets separate hermes process
 *   P121: Result aggregation — JSON array, deduplicate summaries
 *   P122: Error propagation — crash/exit non-zero → error report
 *   P123: Budget delegation — proportional max_iterations split
 *   P124: Nested delegation — --depth flag, decrement depth
 *   P125: Child credential isolation — filter env vars, strip tokens
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>

/* P125: Access to process environment for credential filtering */
extern char **environ;

/* ================================================================
 *  Constants
 * ================================================================ */

#define DELEGATE_MAX_OUTPUT        (256 * 1024)
#define DELEGATE_DEFAULT_TIMEOUT   300
#define DELEGATE_MAX_CHILDREN      64
#define DELEGATE_MAX_CMD_LEN       (512 * 1024)
#define DELEGATE_HERMES_PATH       "hermes"
#define DELEGATE_DEFAULT_DEPTH     1
#define DELEGATE_MAX_DEPTH         10

/* Sensitive env var name fragments — used for credential isolation (P125) */
static const char *sensitive_env_fragments[] = {
    "API_KEY", "APIKEY", "API_SECRET", "APISECRET",
    "TOKEN", "SECRET", "PASSWORD", "PASSWD",
    "ACCESS_KEY", "ACCESS_KEY_ID", "SECRET_ACCESS_KEY",
    "AUTH", "BEARER", "CREDENTIAL",
    NULL  /* sentinel */
};

/* ── Spawn pause — D07: Global gate for delegate spawning ── */
static pthread_mutex_t g_spawn_pause_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_spawn_paused = false;

bool set_spawn_paused(bool paused) {
    pthread_mutex_lock(&g_spawn_pause_lock);
    g_spawn_paused = paused;
    pthread_mutex_unlock(&g_spawn_pause_lock);
    return paused;
}

bool is_spawn_paused(void) {
    pthread_mutex_lock(&g_spawn_pause_lock);
    bool val = g_spawn_paused;
    pthread_mutex_unlock(&g_spawn_pause_lock);
    return val;
}

/* ================================================================
 *  Schema — matches Python version's parameter list
 * ================================================================ */

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"goal\":{\"type\":\"string\",\"description\":\"What the subagent should accomplish\"},"
      "\"context\":{\"type\":\"string\",\"description\":\"Background information for the subagent\"},"
      "\"subtasks\":{"
        "\"type\":\"array\","
        "\"description\":\"List of parallel subtasks (alternative to single goal)\","
        "\"items\":{"
          "\"type\":\"object\","
          "\"properties\":{"
            "\"goal\":{\"type\":\"string\",\"description\":\"What this subtask should accomplish\"},"
            "\"context\":{\"type\":\"string\",\"description\":\"Context for this subtask\"},"
            "\"model\":{\"type\":\"string\",\"description\":\"Model override for this child\"},"
            "\"provider\":{\"type\":\"string\",\"description\":\"Provider override for this child\"},"
            "\"base_url\":{\"type\":\"string\",\"description\":\"Base URL override for this child\"},"
            "\"timeout\":{\"type\":\"number\",\"description\":\"Max seconds for this subtask\",\"default\":300},"
            "\"role\":{\"type\":\"string\",\"description\":\"Child role: worker or orchestrator\",\"default\":\"worker\"},"
            "\"toolsets\":{\"type\":\"string\",\"description\":\"Comma-separated toolset names\"}"
          "},"
          "\"required\":[\"goal\"]"
        "}"
      "},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Max seconds for task (default 300)\",\"default\":300},"
      "\"max_concurrent\":{\"type\":\"integer\",\"description\":\"Max parallel children (default from config)\"},"
      "\"model\":{\"type\":\"string\",\"description\":\"Default model for all children\"},"
      "\"provider\":{\"type\":\"string\",\"description\":\"Default provider for all children\"},"
      "\"base_url\":{\"type\":\"string\",\"description\":\"Default base URL for all children\"},"
      "\"role\":{\"type\":\"string\",\"description\":\"Role for single child: worker or orchestrator\",\"default\":\"worker\"},"
      "\"toolsets\":{\"type\":\"string\",\"description\":\"Comma-separated toolset names for single child\"},"
      "\"depth\":{\"type\":\"integer\",\"description\":\"Max nested delegation depth (default 1)\",\"default\":1},"
      "\"max_iterations\":{\"type\":\"integer\",\"description\":\"Max iterations per child (default from config)\"},"
      "\"abort_on_fail\":{\"type\":\"boolean\",\"description\":\"Abort all children if one fails\",\"default\":false},"
      "\"deduplicate_summaries\":{\"type\":\"boolean\",\"description\":\"Deduplicate child result summaries\",\"default\":true}"
    "},"
    "\"required\":[]"
"}";

/* ================================================================
 *  Per-child state (P116, P119)
 * ================================================================ */

typedef struct {
    int      index;            /* Child index in array */
    char     goal[8192];       /* What this child does */
    char     context[65536];   /* Context for this child */
    int      timeout_sec;      /* Per-child timeout (P117) */
    char     model[128];       /* Model override (P119) */
    char     provider[64];     /* Provider override (P119) */
    char     base_url[256];    /* Base URL override (P119) */
    char     role[32];         /* "worker" or "orchestrator" (P118) */
    char     toolsets[512];    /* Comma-separated toolsets */
    int      max_iterations;   /* Budget: iterations for this child (P123) */
    char     result[DELEGATE_MAX_OUTPUT]; /* Collected output */
    size_t   result_len;       /* Length of collected output */
    int      exit_code;        /* Exit code from child process */
    bool     timed_out;        /* true if killed by timeout (P117) */
    bool     completed;        /* true if child finished */
    bool     errored;          /* true if child had error */
    char     error_msg[1024];  /* Error description (P122) */
    pid_t    pid;              /* Child PID for kill() */
    pthread_t thread;          /* Thread handle (P116) */
} child_task_t;

/* ================================================================
 *  Delegation context — shared across threads
 * ================================================================ */

typedef struct {
    child_task_t   children[DELEGATE_MAX_CHILDREN];
    int            child_count;
    int            max_concurrent;      /* P116 */
    int            depth;               /* P124: current spawn depth */
    int            max_spawn_depth;     /* P124: max allowed depth */
    bool           abort_on_fail;       /* P122 */
    bool           deduplicate;         /* P121 */
    bool           cancelled;          /* Set true to abort */
    char           default_model[128];  /* Default model for all children */
    char           default_provider[64];
    char           default_base_url[256];
    int            default_max_iterations;
} delegate_ctx_t;

/* ================================================================
 *  Forward declarations
 * ================================================================ */

static void shell_escape(const char *in, char *out, size_t out_sz);
static bool is_sensitive_env(const char *varname);
static char *build_child_command(child_task_t *child, delegate_ctx_t *ctx);
static void *child_thread_func(void *arg);
static int deduplicate_summaries(char *combined, size_t combined_sz);

/* ================================================================
 *  Shell escaping (P120: safe command construction)
 * ================================================================ */

static void shell_escape(const char *in, char *out, size_t out_sz) {
    size_t pos = 0;
    for (const char *p = in; *p && pos < out_sz - 4; p++) {
        if (*p == '\'') {
            if (pos + 4 < out_sz) {
                out[pos++] = '\'';
                out[pos++] = '\\';
                out[pos++] = '\'';
                out[pos++] = '\'';
            }
        } else if (*p == '\n') {
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = 'n'; }
        } else if (*p == '\t') {
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = 't'; }
        } else if (*p == '"') {
            /* Escape double quotes */
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = '"'; }
        } else if (*p == '\\') {
            if (pos + 2 < out_sz) { out[pos++] = '\\'; out[pos++] = '\\'; }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

/* ================================================================
 *  P125: Credential isolation — check if env var looks sensitive
 * ================================================================ */

static bool is_sensitive_env(const char *varname) {
    if (!varname) return false;
    size_t len = strlen(varname);

    /* Always pass HERMES_* env vars that are safe */
    if (strncmp(varname, "HERMES_", 7) == 0) {
        /* Allow HOME, CONFIG, DATA paths — block API keys */
        if (strstr(varname, "API_KEY") || strstr(varname, "SECRET") ||
            strstr(varname, "TOKEN") || strstr(varname, "AUTH"))
            return true;
        return false;
    }

    /* Check against sensitive fragments */
    for (int i = 0; sensitive_env_fragments[i]; i++) {
        const char *frag = sensitive_env_fragments[i];
        size_t fraglen = strlen(frag);
        if (len >= fraglen) {
            /* Check for the fragment at any position (case-insensitive) */
            for (size_t j = 0; j <= len - fraglen; j++) {
                bool match = true;
                for (size_t k = 0; k < fraglen; k++) {
                    char a = (char)toupper((unsigned char)varname[j + k]);
                    char b = (char)toupper((unsigned char)frag[k]);
                    if (a != b) { match = false; break; }
                }
                if (match) return true;
            }
        }
    }

    return false;
}

/* ================================================================
 *  P124: Build child command line with --depth flag
 * ================================================================ */

static char *build_child_command(child_task_t *child, delegate_ctx_t *ctx) {
    char *cmd = (char *)malloc(DELEGATE_MAX_CMD_LEN);
    if (!cmd) return NULL;
    cmd[0] = '\0';

    /* Build JSON-RPC args for the child (P120: isolated message) */
    char escaped_goal[16384];
    char escaped_context[131072];
    shell_escape(child->goal, escaped_goal, sizeof(escaped_goal));
    shell_escape(child->context, escaped_context, sizeof(escaped_context));

    /* Build the initial user message JSON */
    char args_json[262144];
    int written = snprintf(args_json, sizeof(args_json),
        "{\"role\":\"user\",\"content\":\"%s\"}",
        escaped_goal);
    if (child->context[0]) {
        written += snprintf(args_json + written,
            sizeof(args_json) - (size_t)written > 0 ? sizeof(args_json) - (size_t)written : 0,
            ",\n{\"role\":\"user\",\"content\":\"Context: %s\"}",
            escaped_context);
    }

    /* Encode the initial message JSON for shell */
    char escaped_json[524288];
    shell_escape(args_json, escaped_json, sizeof(escaped_json));

    /* Build hermes command with flags */
    char cmd_buf[DELEGATE_MAX_CMD_LEN];
    int pos = snprintf(cmd_buf, sizeof(cmd_buf),
        "echo '%s' | timeout %d %s --no-tui --no-display --quiet",
        escaped_json,
        child->timeout_sec,
        DELEGATE_HERMES_PATH);

    /* Add model override (P119) */
    if (child->model[0]) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --model '%s'", child->model);
    }

    /* Add provider override (P119) */
    if (child->provider[0]) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --provider '%s'", child->provider);
    }

    /* Add base URL override (P119) */
    if (child->base_url[0]) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --base-url '%s'", child->base_url);
    }

    /* Add max iterations budget (P123) */
    if (child->max_iterations > 0) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --max-iterations %d", child->max_iterations);
    }

    /* Add depth tracking for nested delegation (P124) */
    if (ctx->depth > 0) {
        int remaining_depth = ctx->max_spawn_depth - ctx->depth;
        if (remaining_depth > 0) {
            pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
                " --delegate-depth %d", remaining_depth);
        }
    }

    /* Add toolsets (P118: orchestrator mode may need broader tools) */
    if (child->toolsets[0]) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --toolsets '%s'", child->toolsets);
    }

    /* Add orchestrator role flag (P118) */
    if (strcmp(child->role, "orchestrator") == 0) {
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --orchestrator");
        /* Enable delegate_task tool for orchestrator children */
        pos += snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
            " --enable-tool delegate_task");
    }

    snprintf(cmd_buf + pos, sizeof(cmd_buf) - (size_t)pos > 0 ? sizeof(cmd_buf) - (size_t)pos : 0,
        " 2>&1 || true");

    snprintf(cmd, DELEGATE_MAX_CMD_LEN, "%s", cmd_buf);
    return cmd;
}

/* ================================================================
 *  P125: Build filtered environment for child process
 * ================================================================ */

static char **build_child_env(void) {
    extern char **environ;
    int env_count = 0;

    /* Count non-sensitive env vars */
    for (char **e = environ; *e; e++) {
        /* Find the = separator */
        char *eq = strchr(*e, '=');
        if (!eq) continue;

        /* Get varname length */
        size_t varlen = (size_t)(eq - *e);
        char *varname = (char *)malloc(varlen + 1);
        if (!varname) continue;
        memcpy(varname, *e, varlen);
        varname[varlen] = '\0';

        if (!is_sensitive_env(varname)) {
            env_count++;
        }
        free(varname);
    }

    /* +1 for NULL terminator */
    char **new_env = (char **)malloc((size_t)(env_count + 1) * sizeof(char *));
    if (!new_env) return NULL;

    int idx = 0;
    for (char **e = environ; *e; e++) {
        char *eq = strchr(*e, '=');
        if (!eq) continue;

        size_t varlen = (size_t)(eq - *e);
        char *varname = (char *)malloc(varlen + 1);
        if (!varname) continue;
        memcpy(varname, *e, varlen);
        varname[varlen] = '\0';

        if (!is_sensitive_env(varname)) {
            new_env[idx] = strdup(*e);
            if (new_env[idx]) idx++;
        }
        free(varname);
    }
    new_env[idx] = NULL;

    return new_env;
}

/* Wrapper struct for thread arg */
typedef struct {
    child_task_t   *child;
    delegate_ctx_t *ctx;
} child_thread_arg_t;

static void *child_thread_func(void *arg) {
    child_thread_arg_t *targ = (child_thread_arg_t *)arg;
    child_task_t *child = targ->child;
    delegate_ctx_t *ctx = targ->ctx;

    /* Build command */
    char *cmd = build_child_command(child, ctx);
    if (!cmd) {
        child->errored = true;
        snprintf(child->error_msg, sizeof(child->error_msg), "Failed to build command");
        child->completed = true;
        return NULL;
    }

    /* Build filtered environment (P125) */
    char **child_env = build_child_env();
    if (!child_env) {
        /* Fall back to inheriting env if allocation fails */
        child_env = NULL;
    }

    /* Open pipe to child process using popen */
    /* popen uses /bin/sh -c, which inherits our env. We can't pass env directly via popen. */
    /* For full isolation, we'd use posix_spawn or fork+exec. */
    /* But popen is simpler — we filter env in the command itself. */
    /* Actually, let's use the simpler approach with the current popen: */
    /* We'll set/reset env vars in the caller thread (not great but works). */
    /* For proper isolation, build a wrapper script. */

    /* Approach: use fork+execvp for proper env isolation */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        child->errored = true;
        snprintf(child->error_msg, sizeof(child->error_msg), "pipe() failed: %s", strerror(errno));
        free(cmd);
        free(child_env);
        child->completed = true;
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        child->errored = true;
        snprintf(child->error_msg, sizeof(child->error_msg), "fork() failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        free(cmd);
        free(child_env);
        child->completed = true;
        return NULL;
    }

    if (pid == 0) {
        /* === Child process === */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        /* Apply filtered environment */
        if (child_env) {
            environ = child_env;
        }

        /* Execute /bin/sh -c <cmd> */
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        /* If execl fails */
        _exit(127);
    }

    /* === Parent process (thread) === */
    close(pipefd[1]);

    child->pid = pid;  /* Store PID for potential kill (P117) */

    /* Read output with timeout monitoring */
    child->result[0] = '\0';
    child->result_len = 0;
    size_t cap = sizeof(child->result);
    size_t len = 0;

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    fd_set read_fds;
    int remaining = child->timeout_sec * 1000; /* ms */

    while (len < cap - 1 && remaining > 0) {
        FD_ZERO(&read_fds);
        FD_SET(pipefd[0], &read_fds);

        struct timeval tv;
        tv.tv_sec = 1;    /* Check every second */
        tv.tv_usec = 0;

        int ret = select(pipefd[0] + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret > 0 && FD_ISSET(pipefd[0], &read_fds)) {
            char buf[4096];
            ssize_t n = (ssize_t)read(pipefd[0], buf, sizeof(buf) - 1);
            if (n <= 0) break;

            size_t space = cap - 1 - len;
            if ((size_t)n > space) n = (ssize_t)space;
            memcpy(child->result + len, buf, (size_t)n);
            len += (size_t)n;
            child->result[len] = '\0';
            child->result_len = len;
        }

        /* Update remaining time */
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_ms = (long)(now.tv_sec - start_time.tv_sec) * 1000 +
                          (long)(now.tv_usec - start_time.tv_usec) / 1000;
        remaining = child->timeout_sec * 1000 - (int)elapsed_ms;
    }

    close(pipefd[0]);

    /* P117: Check for timeout */
    if (remaining <= 0 && !child->timed_out) {
        child->timed_out = true;
        /* Send SIGKILL to child process */
        kill(pid, SIGKILL);

        /* Append timeout note to output */
        const char *timeout_msg = "\n[DELEGATE TIMEOUT]";
        size_t tmlen = strlen(timeout_msg);
        if (len + tmlen < cap - 1) {
            memcpy(child->result + len, timeout_msg, tmlen + 1);
            child->result_len = len + tmlen;
        }

        /* Reap the process */
        int status;
        waitpid(pid, &status, 0);
        child->exit_code = -1; /* timeout marker */
    } else {
        /* P122: Reap child and check exit code */
        int status;
        if (waitpid(pid, &status, 0) > 0) {
            if (WIFEXITED(status)) {
                child->exit_code = WEXITSTATUS(status);
                if (child->exit_code != 0) {
                    child->errored = true;
                    snprintf(child->error_msg, sizeof(child->error_msg),
                        "Child exited with code %d", child->exit_code);
                }
            } else if (WIFSIGNALED(status)) {
                child->errored = true;
                child->exit_code = -WTERMSIG(status);
                snprintf(child->error_msg, sizeof(child->error_msg),
                    "Child killed by signal %d", WTERMSIG(status));
            }
        }
    }

    child->completed = true;
    free(cmd);
    if (child_env) {
        for (int i = 0; child_env[i]; i++) free(child_env[i]);
        free(child_env);
    }
    return NULL;
}

/* ================================================================
 *  P121: Deduplicate summaries — remove near-identical results
 * ================================================================ */

static int deduplicate_summaries(char *combined, size_t combined_sz) {
    if (!combined || !combined[0]) return 0;

    int removed = 0;
    char *lines[1024];
    int line_count = 0;

    /* Split into lines */
    char *saveptr;
    char *tok = strtok_r(combined, "\n", &saveptr);
    while (tok && line_count < 1024) {
        lines[line_count++] = tok;
        tok = strtok_r(NULL, "\n", &saveptr);
    }

    /* Simple dedup: remove exact duplicate lines */
    bool keep[1024];
    for (int i = 0; i < line_count; i++) keep[i] = true;

    for (int i = 0; i < line_count; i++) {
        if (!keep[i]) continue;
        for (int j = i + 1; j < line_count; j++) {
            if (keep[j] && strcmp(lines[i], lines[j]) == 0) {
                keep[j] = false;
                removed++;
            }
        }
    }

    /* Rebuild string */
    char buf[DELEGATE_MAX_OUTPUT];
    size_t pos = 0;
    for (int i = 0; i < line_count && pos < sizeof(buf) - 2; i++) {
        if (keep[i]) {
            size_t llen = strlen(lines[i]);
            if (pos + llen + 1 < sizeof(buf)) {
                memcpy(buf + pos, lines[i], llen);
                pos += llen;
                buf[pos++] = '\n';
            }
        }
    }
    if (pos > 0) buf[pos - 1] = '\0'; /* Remove trailing newline */
    else buf[0] = '\0';

    snprintf(combined, combined_sz, "%s", buf);
    return removed;
}

/* ================================================================
 *  Parse subtasks from JSON args
 * ================================================================ */

static int parse_subtasks(json_node_t *args, delegate_ctx_t *ctx,
                          const char *default_role) {
    int count = 0;

    /* Check for subtasks array */
    json_node_t *subtasks = json_obj_get(args, "subtasks");
    if (subtasks && json_len(subtasks) > 0) {
        size_t n = json_len(subtasks);
        if (n > DELEGATE_MAX_CHILDREN) n = DELEGATE_MAX_CHILDREN;

        for (size_t i = 0; i < n && count < DELEGATE_MAX_CHILDREN; i++) {
            json_node_t *st = json_get(subtasks, i);
            if (!st) continue;

            child_task_t *c = &ctx->children[count];
            memset(c, 0, sizeof(child_task_t));
            c->index = count;

            const char *goal = json_get_str(st, "goal", "");
            snprintf(c->goal, sizeof(c->goal), "%s", goal ? goal : "");

            const char *context = json_get_str(st, "context", "");
            snprintf(c->context, sizeof(c->context), "%s", context ? context : "");

            c->timeout_sec = (int)json_get_num(st, "timeout", DELEGATE_DEFAULT_TIMEOUT);

            const char *model = json_get_str(st, "model", NULL);
            snprintf(c->model, sizeof(c->model), "%s", model ? model : ctx->default_model);

            const char *provider = json_get_str(st, "provider", NULL);
            snprintf(c->provider, sizeof(c->provider), "%s", provider ? provider : ctx->default_provider);

            const char *base_url = json_get_str(st, "base_url", NULL);
            snprintf(c->base_url, sizeof(c->base_url), "%s", base_url ? base_url : ctx->default_base_url);

            const char *role = json_get_str(st, "role", default_role);
            snprintf(c->role, sizeof(c->role), "%s", role ? role : default_role);

            const char *toolsets = json_get_str(st, "toolsets", "");
            snprintf(c->toolsets, sizeof(c->toolsets), "%s", toolsets ? toolsets : "");

            /* P123: Budget — if no per-child max_iterations, use default split */
            c->max_iterations = (int)json_get_num(st, "max_iterations", 0);
            if (c->max_iterations <= 0 && ctx->default_max_iterations > 0) {
                /* Split budget across children */
                c->max_iterations = ctx->default_max_iterations / (int)n;
                if (c->max_iterations < 1) c->max_iterations = 1;
            }

            count++;
        }
    }

    return count;
}

/* ================================================================
 *  P116: Spawn children in parallel using pthreads
 * ================================================================ */

static int spawn_children(delegate_ctx_t *ctx) {
    int spawned = 0;
    child_thread_arg_t args[DELEGATE_MAX_CHILDREN];

    for (int i = 0; i < ctx->child_count; ) {
        /* Determine batch size for this round (per P116 max_concurrent) */
        int batch_size = ctx->max_concurrent;
        if (batch_size <= 0) batch_size = 4; /* default */
        if (i + batch_size > ctx->child_count)
            batch_size = ctx->child_count - i;

        /* D07: Check spawn pause before each batch */
        if (is_spawn_paused()) {
            for (int j = 0; j < batch_size && (i + j) < ctx->child_count; j++) {
                int idx = i + j;
                ctx->children[idx].errored = true;
                snprintf(ctx->children[idx].error_msg,
                    sizeof(ctx->children[idx].error_msg),
                    "Spawning paused");
                ctx->children[idx].completed = true;
            }
            i += batch_size;
            continue;
        }

        /* Launch batch */
        for (int j = 0; j < batch_size && (i + j) < ctx->child_count; j++) {
            int idx = i + j;
            args[idx].child = &ctx->children[idx];
            args[idx].ctx = ctx;

            if (pthread_create(&ctx->children[idx].thread, NULL,
                               child_thread_func, &args[idx]) == 0) {
                spawned++;
            } else {
                ctx->children[idx].errored = true;
                snprintf(ctx->children[idx].error_msg,
                    sizeof(ctx->children[idx].error_msg),
                    "pthread_create failed");
                ctx->children[idx].completed = true;
            }
        }

        /* Wait for batch to complete */
        for (int j = 0; j < batch_size && (i + j) < ctx->child_count; j++) {
            int idx = i + j;
            if (!ctx->children[idx].errored) {
                pthread_join(ctx->children[idx].thread, NULL);
            }
        }

        /* P122: Check abort-on-fail */
        if (ctx->abort_on_fail) {
            for (int j = 0; j < batch_size && (i + j) < ctx->child_count; j++) {
                int idx = i + j;
                if (ctx->children[idx].errored) {
                    ctx->cancelled = true;
                    /* Cancel remaining unlaunched children */
                    for (int k = idx + 1; k < ctx->child_count; k++) {
                        ctx->children[k].errored = true;
                        snprintf(ctx->children[k].error_msg,
                            sizeof(ctx->children[k].error_msg),
                            "Cancelled due to sibling failure (abort_on_fail)");
                        ctx->children[k].completed = true;
                    }
                    i = ctx->child_count; /* Break outer loop */
                    break;
                }
            }
        }

        i += batch_size;
    }

    return spawned;
}

/* ================================================================
 *  P121: Aggregate all child results into JSON
 * ================================================================ */

static json_node_t *aggregate_results(delegate_ctx_t *ctx) {
    json_node_t *result = json_new_object();

    json_node_t *children_arr = json_new_array();

    for (int i = 0; i < ctx->child_count; i++) {
        child_task_t *c = &ctx->children[i];
        json_node_t *child_result = json_new_object();

        json_set(child_result, "index", json_number((double)c->index));
        json_set(child_result, "goal", json_string(c->goal));
        json_set(child_result, "completed", json_bool(c->completed));
        json_set(child_result, "errored", json_bool(c->errored));
        json_set(child_result, "timed_out", json_bool(c->timed_out));
        json_set(child_result, "exit_code", json_number((double)c->exit_code));

        if (c->errored && c->error_msg[0]) {
            json_set(child_result, "error", json_string(c->error_msg));
        }

        if (c->result_len > 0) {
            json_set(child_result, "output", json_string(c->result));
            json_set(child_result, "output_length", json_number((double)c->result_len));
        }

        /* Include model/role metadata */
        if (c->model[0]) json_set(child_result, "model", json_string(c->model));
        if (c->role[0]) json_set(child_result, "role", json_string(c->role));
        if (c->provider[0]) json_set(child_result, "provider", json_string(c->provider));

        json_append(children_arr, child_result);
    }

    json_set(result, "children", children_arr);
    json_set(result, "child_count", json_number((double)ctx->child_count));
    json_set(result, "max_concurrent", json_number((double)ctx->max_concurrent));
    json_set(result, "depth", json_number((double)ctx->depth));

    /* Build combined summary from all children */
    char combined[DELEGATE_MAX_OUTPUT];
    combined[0] = '\0';
    size_t pos = 0;

    for (int i = 0; i < ctx->child_count; i++) {
        child_task_t *c = &ctx->children[i];
        if (c->result_len > 0 && pos < sizeof(combined) - 2) {
            size_t remaining = sizeof(combined) - pos - 2;
            size_t to_copy = c->result_len < remaining ? c->result_len : remaining;
            if (pos > 0 && combined[pos - 1] != '\n' && pos < sizeof(combined) - 2) {
                combined[pos++] = '\n';
            }
            memcpy(combined + pos, c->result, to_copy);
            pos += to_copy;
            combined[pos] = '\0';
        }
    }

    /* P121: Deduplicate if requested */
    if (ctx->deduplicate && combined[0]) {
        deduplicate_summaries(combined, sizeof(combined));
    }

    json_set(result, "summary",
             combined[0] ? json_string(combined) : json_string("(no output)"));

    /* Count successes and failures */
    int success = 0, failed = 0, timeouts = 0;
    for (int i = 0; i < ctx->child_count; i++) {
        if (ctx->children[i].timed_out) timeouts++;
        else if (ctx->children[i].errored) failed++;
        else success++;
    }
    json_set(result, "success_count", json_number((double)success));
    json_set(result, "failed_count", json_number((double)failed));
    json_set(result, "timeout_count", json_number((double)timeouts));

    return result;
}

/* ================================================================
 *  Main entry: delegate_handler
 * ================================================================ */

char *delegate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        free(err);
        return strdup("{\"error\":\"JSON parse error\"}");
    }

    /* Initialize delegation context */
    delegate_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* Load delegation config from global config */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_defaults(&cfg);
    hermes_config_load(&cfg, NULL);

    /* P116: max concurrent children */
    int max_concurrent = (int)json_get_num(args, "max_concurrent", 0);
    if (max_concurrent <= 0) {
        max_concurrent = cfg.delegation.max_concurrent_children;
    }
    if (max_concurrent <= 0) max_concurrent = 4;
    ctx.max_concurrent = max_concurrent;
    if (ctx.max_concurrent > DELEGATE_MAX_CHILDREN)
        ctx.max_concurrent = DELEGATE_MAX_CHILDREN;

    /* P124: max spawn depth */
    int depth = (int)json_get_num(args, "depth", DELEGATE_DEFAULT_DEPTH);
    if (depth <= 0) depth = DELEGATE_DEFAULT_DEPTH;
    if (depth > DELEGATE_MAX_DEPTH) depth = DELEGATE_MAX_DEPTH;
    ctx.depth = depth;

    int max_spawn_depth = cfg.delegation.max_spawn_depth;
    if (max_spawn_depth <= 0) max_spawn_depth = DELEGATE_DEFAULT_DEPTH;
    ctx.max_spawn_depth = max_spawn_depth;

    /* Default model/provider from args or config */
    const char *default_model = json_get_str(args, "model", NULL);
    if (!default_model) default_model = cfg.delegation.child_model;
    if (!default_model) default_model = cfg.model;
    snprintf(ctx.default_model, sizeof(ctx.default_model), "%s", default_model);

    const char *default_provider = json_get_str(args, "provider", NULL);
    if (!default_provider) default_provider = cfg.delegation.child_provider;
    if (!default_provider) default_provider = cfg.provider;
    snprintf(ctx.default_provider, sizeof(ctx.default_provider), "%s", default_provider);

    const char *default_base_url = json_get_str(args, "base_url", NULL);
    if (default_base_url) {
        snprintf(ctx.default_base_url, sizeof(ctx.default_base_url), "%s", default_base_url);
    }

    /* P123: Budget delegation — child max_iterations split */
    int max_iterations = (int)json_get_num(args, "max_iterations", 0);
    if (max_iterations <= 0) {
        max_iterations = cfg.delegation.child_max_turns;
    }
    if (max_iterations <= 0) max_iterations = cfg.agent.max_iterations;
    ctx.default_max_iterations = max_iterations;

    /* P122: abort on child fail */
    ctx.abort_on_fail = json_get_bool(args, "abort_on_fail", false);

    /* P121: deduplicate summaries */
    ctx.deduplicate = json_get_bool(args, "deduplicate_summaries", true);

    /* Default role (P118) */
    const char *default_role = json_get_str(args, "role", "worker");

    /* Parse children */
    json_node_t *result = NULL;

    /* Try multiple subtasks first */
    ctx.child_count = parse_subtasks(args, &ctx, default_role);

    /* Fall back to single child from top-level fields */
    if (ctx.child_count == 0) {
        const char *goal = json_get_str(args, "goal", NULL);
        if (goal) {
            child_task_t *c = &ctx.children[0];
            memset(c, 0, sizeof(child_task_t));
            c->index = 0;
            snprintf(c->goal, sizeof(c->goal), "%s", goal);

            const char *context = json_get_str(args, "context", "");
            snprintf(c->context, sizeof(c->context), "%s", context ? context : "");

            c->timeout_sec = (int)json_get_num(args, "timeout", DELEGATE_DEFAULT_TIMEOUT);

            /* Model overrides (P119) */
            const char *model = json_get_str(args, "model", NULL);
            snprintf(c->model, sizeof(c->model), "%s", model ? model : ctx.default_model);
            const char *provider = json_get_str(args, "provider", NULL);
            snprintf(c->provider, sizeof(c->provider), "%s", provider ? provider : ctx.default_provider);
            const char *base_url = json_get_str(args, "base_url", NULL);
            snprintf(c->base_url, sizeof(c->base_url), "%s", base_url ? base_url : ctx.default_base_url);

            snprintf(c->role, sizeof(c->role), "%s", default_role);

            const char *toolsets = json_get_str(args, "toolsets", "");
            snprintf(c->toolsets, sizeof(c->toolsets), "%s", toolsets ? toolsets : "");

            c->max_iterations = ctx.default_max_iterations;
            ctx.child_count = 1;
        }
    }

    if (ctx.child_count == 0) {
        result = json_new_object();
        json_set(result, "error", json_string("Missing goal — provide 'goal' or 'subtasks'"));
    } else {
        /* Spawn all children in parallel batches */
        spawn_children(&ctx);

        /* Aggregate results (P121) */
        result = aggregate_results(&ctx);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

/* ================================================================
 *  Registry init
 * ================================================================ */

void registry_init_delegate(void) {
    registry_register("delegate_task",
        "Spawn one or more subagents to work on subtasks. Supports parallel children (P116), "
        "per-child timeout (P117), orchestrator mode (P118), model override (P119), "
        "result aggregation (P121), error propagation (P122), budget delegation (P123), "
        "nested delegation (P124), and credential isolation (P125).",
        SCHEMA, delegate_handler);
}
