/*
 * context_references.c — @file:@folder:@git:@url reference expander.
 * Port of Python agent/context_references.py (518 lines).
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_context_refs.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define SENSITIVE_DIRS_MAX 16
#define SENSITIVE_FILES_MAX 16
#define SOFT_LIMIT_RATIO 0.25
#define HARD_LIMIT_RATIO 0.50
#define MAX_FILE_SIZE (4 * 1024 * 1024)  /* 4MB max per file */
#define GIT_TIMEOUT_SEC 30
#define FOLDER_LIST_LIMIT 200

/* ================================================================
 *  Sensitive path lists
 * ================================================================ */

static const char *SENSITIVE_HOME_DIRS[] = {
    ".ssh", ".aws", ".gnupg", ".kube", ".docker", ".azure", ".config/gh", NULL
};

static const char *SENSITIVE_HOME_FILES[] = {
    ".ssh/authorized_keys", ".ssh/id_rsa", ".ssh/id_ed25519",
    ".ssh/config", ".bashrc", ".zshrc", ".profile",
    ".bash_profile", ".zprofile", ".netrc", ".pgpass", ".npmrc", ".pypirc",
    ".gitconfig", NULL
};

static const char *CODE_LANG_MAP[][2] = {
    {".py", "python"}, {".js", "javascript"}, {".ts", "typescript"},
    {".tsx", "tsx"}, {".jsx", "jsx"}, {".json", "json"},
    {".md", "markdown"}, {".sh", "bash"}, {".yml", "yaml"},
    {".yaml", "yaml"}, {".toml", "toml"}, {".c", "c"},
    {".h", "c"}, {".rs", "rust"}, {".go", "go"},
    {".rb", "ruby"}, {".php", "php"}, {".java", "java"},
    {".cpp", "cpp"}, {".hpp", "cpp"}, {".css", "css"},
    {".html", "html"}, {".sql", "sql"}, {".lua", "lua"},
    {NULL, NULL}
};

/* URL fetcher callback (default: NULL = URL references produce warning) */
static context_ref_url_fetcher_t g_url_fetcher = NULL;

void context_ref_set_url_fetcher(context_ref_url_fetcher_t fetcher) {
    g_url_fetcher = fetcher;
}

/* ================================================================
 *  Utilities
 * ================================================================ */

int context_ref_estimate_tokens(const char *text) {
    if (!text) return 0;
    size_t len = strlen(text);
    return (int)(len / 4) + 1;
}

static bool is_binary_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == 0) return true;
    }
    return false;
}

static const char *code_fence_language(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "";
    for (int i = 0; CODE_LANG_MAP[i][0] != NULL; i++) {
        if (strcmp(dot, CODE_LANG_MAP[i][0]) == 0)
            return CODE_LANG_MAP[i][1];
    }
    return "";
}

/* Read entire file into allocated string, or NULL on failure */
static char *read_file_content(const char *path, int line_start, int line_end) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Get size */
    struct stat st;
    if (fstat(fileno(f), &st) < 0 || st.st_size > MAX_FILE_SIZE) {
        fclose(f);
        return NULL;
    }

    char *content = (char *)malloc((size_t)st.st_size + 1);
    if (!content) { fclose(f); return NULL; }
    size_t n = fread(content, 1, (size_t)st.st_size, f);
    fclose(f);
    content[n] = '\0';

        /* Line-range extraction */
    if (line_start > 0) {
        int line = 1;
        char *start = content;
        char *p = content;
        while (*p && line < line_start) {
            if (*p == '\n') line++;
            p++;
        }
        start = p;
        if (line_end > line_start) {
            /* Walk until we've seen (line_end - line_start + 1) newlines
             * from the start position, then terminate at that newline. */
            int lines_to_count = line_end - line_start + 1;
            while (*p && lines_to_count > 0) {
                if (*p == '\n') lines_to_count--;
                if (lines_to_count == 0) {
                    *p = '\0';
                    break;
                }
                p++;
            }
        } else {
            /* Single line: find end of line */
            while (*p && *p != '\n') p++;
            *p = '\0';
        }

        char *result = strdup(start);
        free(content);
        return result;
    }

    return content;
}

/* Resolve path relative to cwd, with security checks */
static char *resolve_path(const char *cwd, const char *target, const char *allowed_root, char **err) {
    char full[4096];
    *err = NULL;

    if (target[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) { *err = strdup("HOME not set"); return NULL; }
        if (target[1] == '/' || target[1] == '\0')
            snprintf(full, sizeof(full), "%s%s", home, target + 1);
        else
            snprintf(full, sizeof(full), "%s/%s", home, target + 1);
    } else if (target[0] == '/') {
        snprintf(full, sizeof(full), "%s", target);
    } else {
        snprintf(full, sizeof(full), "%s/%s", cwd ? cwd : ".", target);
    }

    /* Resolve to absolute path */
    char resolved[4096];
    if (!realpath(full, resolved)) {
        /* Path may not exist yet for some checks; use non-resolved */
        snprintf(resolved, sizeof(resolved), "%s", full);
    }

    /* Security: check allowed_root */
    if (allowed_root) {
        char allowed_resolved[4096];
        if (!realpath(allowed_root, allowed_resolved))
            snprintf(allowed_resolved, sizeof(allowed_resolved), "%s", allowed_root);
        if (strncmp(resolved, allowed_resolved, strlen(allowed_resolved)) != 0) {
            *err = strdup("path is outside the allowed workspace");
            return NULL;
        }
    }

    return strdup(resolved);
}

static bool is_sensitive_path(const char *path) {
    const char *home = getenv("HOME");
    if (!home) return false;

    /* Check sensitive files */
    for (int i = 0; SENSITIVE_HOME_FILES[i]; i++) {
        char sensitive[4096];
        snprintf(sensitive, sizeof(sensitive), "%s/%s", home, SENSITIVE_HOME_FILES[i]);
        if (strcmp(path, sensitive) == 0) return true;
    }

    /* Check sensitive dirs (path starts with sensitive dir) */
    for (int i = 0; SENSITIVE_HOME_DIRS[i]; i++) {
        char sensitive[4096];
        snprintf(sensitive, sizeof(sensitive), "%s/%s/", home, SENSITIVE_HOME_DIRS[i]);
        if (strncmp(path, sensitive, strlen(sensitive)) == 0) return true;
    }

    /* Check hermes_home /.env */
    const char *hermes_home = getenv("HERMES_HOME");
    if (hermes_home) {
        char env_path[4096];
        snprintf(env_path, sizeof(env_path), "%s/.env", hermes_home);
        if (strcmp(path, env_path) == 0) return true;

        char hub_path[4096];
        snprintf(hub_path, sizeof(hub_path), "%s/skills/.hub", hermes_home);
        if (strncmp(path, hub_path, strlen(hub_path)) == 0) return true;
    }

    return false;
}

/* ================================================================
 *  Regex-like parsing (manual, no PCRE dependency)
 * ================================================================ */

/*
 * Pattern: @diff or @staged or @kind:value
 * kind = file|folder|git|url
 * value = quoted or bare
 */
static bool match_at_ref(const char *p, const char **end, context_ref_t *ref) {
    const char *start = p;
    if (*p != '@') return false;
    p++;

    /* Check simple: @diff or @staged */
    if (strncmp(p, "diff", 4) == 0 && !isalnum((unsigned char)p[4])) {
        ref->kind = CONTEXT_REF_DIFF;
        snprintf(ref->raw, sizeof(ref->raw), "@diff");
        *end = p + 4;
        return true;
    }
    if (strncmp(p, "staged", 6) == 0 && !isalnum((unsigned char)p[6])) {
        ref->kind = CONTEXT_REF_STAGED;
        snprintf(ref->raw, sizeof(ref->raw), "@staged");
        *end = p + 6;
        return true;
    }

    /* @kind:value */
    const char *kind_start = p;
    while (*p && isalpha((unsigned char)*p)) p++;
    if (p == kind_start) return false;
    int kind_len = (int)(p - kind_start);

    if (*p != ':') return false;
    p++; /* skip colon */

    /* Determine kind */
    if (kind_len == 3 && strncmp(kind_start, "url", 3) == 0) {
        ref->kind = CONTEXT_REF_URL;
    } else if (kind_len == 4 && strncmp(kind_start, "file", 4) == 0) {
        ref->kind = CONTEXT_REF_FILE;
    } else if (kind_len == 6 && strncmp(kind_start, "folder", 6) == 0) {
        ref->kind = CONTEXT_REF_FOLDER;
    } else if (kind_len == 3 && strncmp(kind_start, "git", 3) == 0) {
        ref->kind = CONTEXT_REF_GIT;
    } else {
        ref->kind = CONTEXT_REF_UNKNOWN;
    }

    /* Parse value: quoted or bare */
    const char *val_start = p;
    char quote = 0;
    if (*p == '`' || *p == '"' || *p == '\'') {
        quote = *p;
        p++;
        val_start = p;
        while (*p && *p != quote) p++;
        if (*p == quote) {
            /* Copy value without quotes */
            int vlen = (int)(p - val_start);
            if (vlen >= (int)sizeof(ref->target)) vlen = sizeof(ref->target) - 1;
            memcpy(ref->target, val_start, vlen);
            ref->target[vlen] = '\0';
            p++; /* skip closing quote */
        } else {
            /* Unclosed quote, take rest as value */
            int vlen = (int)(p - val_start);
            if (vlen >= (int)sizeof(ref->target)) vlen = sizeof(ref->target) - 1;
            memcpy(ref->target, val_start, vlen);
            ref->target[vlen] = '\0';
        }
    } else {
        /* Bare value: stop at whitespace, closing paren, end of message */
        while (*p && !isspace((unsigned char)*p) &&
               *p != ')' && *p != ']' && *p != '}')
            p++;
        int vlen = (int)(p - val_start);
        if (vlen >= (int)sizeof(ref->target)) vlen = sizeof(ref->target) - 1;
        memcpy(ref->target, val_start, vlen);
        ref->target[vlen] = '\0';

        /* Strip trailing punctuation */
        int len = (int)strlen(ref->target);
        while (len > 0 && strchr(",.;!?", ref->target[len-1]))
            ref->target[--len] = '\0';
    }

    /* Strip trailing balanced brackets */
    int tlen = (int)strlen(ref->target);
    while (tlen >= 2) {
        char last = ref->target[tlen - 1];
        char expected = 0;
        if (last == ')') expected = '(';
        else if (last == ']') expected = '[';
        else if (last == '}') expected = '{';
        else break;
        int open_count = 0, close_count = 0;
        for (int i = 0; i < tlen; i++) {
            if (ref->target[i] == expected) open_count++;
            if (ref->target[i] == last) close_count++;
        }
        if (close_count > open_count) {
            ref->target[--tlen] = '\0';
        } else break;
    }

    /* For @git:N, parse count into target */
    if (ref->kind == CONTEXT_REF_GIT) {
        /* target is already the number or path */
    }

    /* For @file:path:line or path:start-end, parse line range */
    ref->line_start = 0;
    ref->line_end = 0;
    if (ref->kind == CONTEXT_REF_FILE) {
        /* Check for :line or :start-end suffix */
        const char *colon = strrchr(ref->target, ':');
        if (colon && colon > ref->target) {
            const char *num_str = colon + 1;
            if (isdigit((unsigned char)*num_str)) {
                ref->line_start = atoi(num_str);
                const char *dash = strchr(num_str, '-');
                if (dash) {
                    ref->line_end = atoi(dash + 1);
                } else {
                    ref->line_end = ref->line_start;
                }
                /* Remove the :line suffix from target */
                int base_len = (int)(colon - ref->target);
                ref->target[base_len] = '\0';
            }
        }
    }

    /* Record positions */
    int raw_len = (int)(p - start);
    if (raw_len >= (int)sizeof(ref->raw)) raw_len = sizeof(ref->raw) - 1;
    memcpy(ref->raw, start, raw_len);
    ref->raw[raw_len] = '\0';

    *end = p;
    return true;
}

int context_ref_parse(const char *message, context_ref_t *refs, int max_refs) {
    if (!message) return 0;
    int count = 0;

    for (const char *p = message; *p; ) {
        context_ref_t ref;
        memset(&ref, 0, sizeof(ref));
        const char *next = NULL;

        if (match_at_ref(p, &next, &ref)) {
            ref.start_pos = (int)(p - message);
            ref.end_pos = (int)(next - message);
            if (count >= max_refs) return -1;
            refs[count++] = ref;
            p = next;
        } else {
            p++;
        }
    }

    return count;
}

/* ================================================================
 *  Reference expanders
 * ================================================================ */

/* Each expander returns (warning, block) where one may be NULL.
 * Block is the expanded content to inject. Warning is a message.
 * Caller must free both. */

static void expand_file_ref(const context_ref_t *ref, const char *cwd,
                            const char *allowed_root,
                            char **warning, char **block) {
    *warning = NULL;
    *block = NULL;

    if (ref->kind != CONTEXT_REF_FILE) return;

    char *err = NULL;
    char *resolved = resolve_path(cwd, ref->target, allowed_root, &err);
    if (err) { *warning = err; free(resolved); return; }
    if (!resolved) { *warning = strdup("failed to resolve path"); return; }

    if (is_sensitive_path(resolved)) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: path is a sensitive credential file and cannot be attached", ref->raw);
        free(resolved);
        return;
    }

    struct stat st;
    if (stat(resolved, &st) < 0) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: file not found", ref->raw);
        free(resolved);
        return;
    }
    if (!S_ISREG(st.st_mode)) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: path is not a file", ref->raw);
        free(resolved);
        return;
    }
    if (is_binary_file(resolved)) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: binary files are not supported", ref->raw);
        free(resolved);
        return;
    }

    char *content = read_file_content(resolved, ref->line_start, ref->line_end);
    free(resolved);
    if (!content) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: failed to read file", ref->raw);
        return;
    }

    int tokens = context_ref_estimate_tokens(content);
    const char *lang = code_fence_language(ref->target);

    *block = malloc(strlen(ref->raw) + strlen(content) + 256);
    snprintf(*block, strlen(ref->raw) + strlen(content) + 256,
             "📄 %s (%d tokens)\n```%s\n%s\n```",
             ref->raw, tokens, lang, content);
    free(content);
}

static void expand_folder_ref(const context_ref_t *ref, const char *cwd,
                              const char *allowed_root,
                              char **warning, char **block) {
    *warning = NULL;
    *block = NULL;

    if (ref->kind != CONTEXT_REF_FOLDER) return;

    char *err = NULL;
    char *resolved = resolve_path(cwd, ref->target, allowed_root, &err);
    if (err) { *warning = err; free(resolved); return; }
    if (!resolved) { *warning = strdup("failed to resolve path"); return; }

    struct stat st;
    if (stat(resolved, &st) < 0) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: folder not found", ref->raw);
        free(resolved);
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: path is not a folder", ref->raw);
        free(resolved);
        return;
    }

    /* Build folder listing */
    /* Use allocated string buffer */
    size_t buf_size = 65536;
    char *listing = (char *)calloc(1, buf_size);
    if (!listing) { free(resolved); return; }

    /* Relative path prefix */
    char rel_path[1024] = {0};
    if (cwd) {
        /* Try to make relative */
        size_t cwd_len = strlen(cwd);
        if (strncmp(resolved, cwd, cwd_len) == 0 && resolved[cwd_len] == '/')
            snprintf(rel_path, sizeof(rel_path), "%s/", resolved + cwd_len + 1);
        else
            snprintf(rel_path, sizeof(rel_path), "%s/", resolved);
    } else {
        snprintf(rel_path, sizeof(rel_path), "%s/", resolved);
    }

    /* Remove trailing / from resolved for display */
    size_t rlen = strlen(resolved);
    if (rlen > 0 && resolved[rlen - 1] == '/') resolved[rlen - 1] = '\0';

    int written = snprintf(listing, buf_size, "%s/\n", rel_path);

    /* Walk directory (non-recursive, just one level) */
    DIR *dir = opendir(resolved);
    if (dir) {
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(dir)) != NULL && count < FOLDER_LIST_LIMIT) {
            if (entry->d_name[0] == '.') continue; /* skip hidden */
            if (strcmp(entry->d_name, "__pycache__") == 0) continue;

            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", resolved, entry->d_name);
            struct stat est;
            stat(full_path, &est);

            size_t remaining = buf_size - (size_t)written;
            if (remaining < 128) break;

            if (S_ISDIR(est.st_mode)) {
                written += snprintf(listing + written, remaining,
                                    "  - %s/\n", entry->d_name);
            } else {
                written += snprintf(listing + written, remaining,
                                    "  - %s (%lld bytes)\n",
                                    entry->d_name, (long long)est.st_size);
            }
            count++;
        }
        closedir(dir);
        if (count >= FOLDER_LIST_LIMIT) {
            size_t remaining = buf_size - (size_t)written;
            if (remaining > 8)
                written += snprintf(listing + written, remaining, "- ...\n");
        }
    }

    int tokens = context_ref_estimate_tokens(listing);
    *block = malloc(strlen(ref->raw) + strlen(listing) + 256);
    snprintf(*block, strlen(ref->raw) + strlen(listing) + 256,
             "📁 %s (%d tokens)\n%s",
             ref->raw, tokens, listing);
    free(listing);
    free(resolved);
}

static void run_git_command(const char *cwd, char *const *args,
                            char **output, int *exit_code) {
    *output = NULL;
    *exit_code = -1;

    int pipefd[2];
    if (pipe(pipefd) < 0) return;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return; }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (cwd) (void)(void)chdir(cwd);
        /* Build argv */
        int argc;
        for (argc = 0; args[argc]; argc++);
        char **argv = (char **)malloc((argc + 2) * sizeof(char *));
        argv[0] = "git";
        for (int i = 0; args[i]; i++) argv[i + 1] = args[i];
        argv[argc + 1] = NULL;
        execvp("git", argv);
        _exit(127);
    }

    /* Parent */
    close(pipefd[1]);

    /* Read with timeout (simplified: just read) */
    size_t buf_size = 65536;
    size_t total = 0;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { close(pipefd[0]); waitpid(pid, NULL, 0); return; }

    ssize_t n;
    while ((n = read(pipefd[0], buf + total, buf_size - total - 1)) > 0) {
        total += (size_t)n;
        if (total >= buf_size - 1) {
            buf_size *= 2;
            char *nb = realloc(buf, buf_size);
            if (!nb) break;
            buf = nb;
        }
    }
    close(pipefd[0]);
    buf[total] = '\0';

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else {
        *exit_code = -1;
    }

    *output = buf;
}

static void expand_git_ref(const context_ref_t *ref, const char *cwd,
                           char **warning, char **block) {
    *warning = NULL;
    *block = NULL;

    char *output = NULL;
    int exit_code = 0;
    const char *label = NULL;
    char *args_buffer[16] = {0};
    int arg_count = 0;

    if (ref->kind == CONTEXT_REF_DIFF) {
        args_buffer[arg_count++] = "diff";
        label = "git diff";
    } else if (ref->kind == CONTEXT_REF_STAGED) {
        args_buffer[arg_count++] = "diff";
        args_buffer[arg_count++] = "--staged";
        label = "git diff --staged";
    } else if (ref->kind == CONTEXT_REF_GIT) {
        int count = 1;
        if (ref->target[0]) {
            count = atoi(ref->target);
            if (count < 1) count = 1;
            if (count > 10) count = 10;
        }
        char count_str[16];
        snprintf(count_str, sizeof(count_str), "%d", count);
        args_buffer[arg_count++] = "log";
        args_buffer[arg_count++] = "-p";
        args_buffer[arg_count++] = count_str;
        if (arg_count < 14) args_buffer[arg_count] = NULL;
        label = "git log -p";
    } else {
        return;
    }

    char *git_args[16];
    for (int i = 0; i <= arg_count; i++) git_args[i] = args_buffer[i];

    run_git_command(cwd, git_args, &output, &exit_code);

    if (!output) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: git command timed out or failed", ref->raw);
        return;
    }

    /* Strip trailing whitespace */
    size_t olen = strlen(output);
    while (olen > 0 && (output[olen-1] == '\n' || output[olen-1] == ' '))
        output[--olen] = '\0';

    if (!output[0]) {
        snprintf(output, 64, "(no output)");
    }

    int tokens = context_ref_estimate_tokens(output);
    *block = malloc(strlen(ref->raw) + strlen(output) + 256);
    snprintf(*block, strlen(ref->raw) + strlen(output) + 256,
             "🧾 %s (%d tokens)\n```diff\n%s\n```",
             label, tokens, output);
    free(output);
}

static void expand_url_ref(const context_ref_t *ref,
                           char **warning, char **block) {
    *warning = NULL;
    *block = NULL;

    if (ref->kind != CONTEXT_REF_URL) return;

    if (!g_url_fetcher) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: URL fetcher not configured", ref->raw);
        return;
    }

    char *content = g_url_fetcher(ref->target);
    if (!content || !content[0]) {
        *warning = malloc(512);
        snprintf(*warning, 512, "%s: no content extracted", ref->raw);
        free(content);
        return;
    }

    int tokens = context_ref_estimate_tokens(content);
    *block = malloc(strlen(ref->raw) + strlen(content) + 256);
    snprintf(*block, strlen(ref->raw) + strlen(content) + 256,
             "🌐 %s (%d tokens)\n%s",
             ref->raw, tokens, content);
    free(content);
}

/* ================================================================
 *  Remove reference tokens from message
 * ================================================================ */

static char *remove_ref_tokens(const char *message, const context_ref_t *refs, int ref_count) {
    size_t len = strlen(message) + 1;
    char *result = (char *)malloc(len);
    if (!result) return NULL;

    size_t pos = 0;
    int cursor = 0;

    for (int i = 0; i < ref_count; i++) {
        /* Copy text before this ref */
        if (refs[i].start_pos > cursor) {
            size_t chunk = (size_t)(refs[i].start_pos - cursor);
            memcpy(result + pos, message + cursor, chunk);
            pos += chunk;
        }
        cursor = refs[i].end_pos;
    }
    /* Copy remaining */
    if (message[cursor]) {
        size_t remaining = strlen(message + cursor);
        memcpy(result + pos, message + cursor, remaining);
        pos += remaining;
    }
    result[pos] = '\0';

    return result;
}

/* ================================================================
 *  Main expansion entry point
 * ================================================================ */

context_ref_result_t *context_ref_expand(const char *message,
                                          const char *cwd,
                                          int context_length,
                                          const char *allowed_root) {
    context_ref_result_t *result = (context_ref_result_t *)calloc(1, sizeof(context_ref_result_t));
    if (!result) return NULL;

    result->original_message = strdup(message ? message : "");
    result->result = NULL;
    result->warnings[0] = '\0';
    result->injected_tokens = 0;
    result->expanded = false;
    result->blocked = false;

    /* Parse references */
    result->ref_count = context_ref_parse(message, result->refs, MAX_CONTEXT_REFS);
    if (result->ref_count <= 0) {
        /* No references found */
        result->result = strdup(message ? message : "");
        return result;
    }

    /* Expand each reference */
    char *blocks[MAX_CONTEXT_REFS];
    int total_tokens = 0;
    int block_count = 0;
    memset(blocks, 0, sizeof(blocks));

    for (int i = 0; i < result->ref_count; i++) {
        char *warning = NULL;
        char *block = NULL;

        switch (result->refs[i].kind) {
            case CONTEXT_REF_FILE:
                expand_file_ref(&result->refs[i], cwd, allowed_root, &warning, &block);
                break;
            case CONTEXT_REF_FOLDER:
                expand_folder_ref(&result->refs[i], cwd, allowed_root, &warning, &block);
                break;
            case CONTEXT_REF_DIFF:
            case CONTEXT_REF_STAGED:
            case CONTEXT_REF_GIT:
                expand_git_ref(&result->refs[i], cwd, &warning, &block);
                break;
            case CONTEXT_REF_URL:
                expand_url_ref(&result->refs[i], &warning, &block);
                break;
            default:
                warning = malloc(512);
                snprintf(warning, 512, "%s: unsupported reference type", result->refs[i].raw);
                break;
        }

        if (warning) {
            size_t wlen = strlen(result->warnings);
            size_t wspace = sizeof(result->warnings) - wlen;
            if (wspace > 2) {
                if (result->warnings[0]) strncat(result->warnings, "\n", wspace - 1);
                strncat(result->warnings, warning, wspace - 2);
            }
            free(warning);
        }

        if (block) {
            blocks[block_count++] = block;
            total_tokens += context_ref_estimate_tokens(block);
        }
    }

    result->injected_tokens = total_tokens;

    /* Check limits */
    int hard_limit = context_length > 0 ? (int)(context_length * HARD_LIMIT_RATIO) : 0;
    int soft_limit = context_length > 0 ? (int)(context_length * SOFT_LIMIT_RATIO) : 0;

    if (hard_limit > 0 && total_tokens > hard_limit) {
        char wbuf[256];
        snprintf(wbuf, sizeof(wbuf),
                 "@ context injection refused: %d tokens exceeds the 50%% hard limit (%d).",
                 total_tokens, hard_limit);
        size_t wlen = strlen(result->warnings);
        size_t wspace = sizeof(result->warnings) - wlen;
        if (wspace > 2) {
            if (result->warnings[0]) strncat(result->warnings, "\n", wspace - 1);
            strncat(result->warnings, wbuf, wspace - 2);
        }
        result->blocked = true;
        result->result = strdup(message ? message : "");
        for (int i = 0; i < block_count; i++) free(blocks[i]);
        return result;
    }

    if (soft_limit > 0 && total_tokens > soft_limit) {
        char wbuf[256];
        snprintf(wbuf, sizeof(wbuf),
                 "@ context injection warning: %d tokens exceeds the 25%% soft limit (%d).",
                 total_tokens, soft_limit);
        size_t wlen = strlen(result->warnings);
        size_t wspace = sizeof(result->warnings) - wlen;
        if (wspace > 2) {
            if (result->warnings[0]) strncat(result->warnings, "\n", wspace - 1);
            strncat(result->warnings, wbuf, wspace - 2);
        }
    }

    result->expanded = (block_count > 0 || result->warnings[0]);

    /* Build final message: stripped + warnings + blocks */
    char *stripped = remove_ref_tokens(message, result->refs, result->ref_count);
    if (!stripped) {
        result->result = strdup(message ? message : "");
        for (int i = 0; i < block_count; i++) free(blocks[i]);
        return result;
    }

    /* Calculate total size for final message */
    size_t final_size = strlen(stripped) + 128;
    if (result->warnings[0]) final_size += strlen(result->warnings) + 64;
    for (int i = 0; i < block_count; i++) final_size += strlen(blocks[i]) + 16;

    char *final = (char *)malloc(final_size);
    if (!final) {
        free(stripped);
        result->result = strdup(message ? message : "");
        for (int i = 0; i < block_count; i++) free(blocks[i]);
        return result;
    }
    final[0] = '\0';

    /* Build: stripped */
    strcat(final, stripped);

    /* Warnings */
    if (result->warnings[0]) {
        strcat(final, "\n\n--- Context Warnings ---\n");
        /* Split warnings by newline, add "- " prefix */
        char *wcopy = strdup(result->warnings);
        if (wcopy) {
            char *line = strtok(wcopy, "\n");
            while (line) {
                strcat(final, "- ");
                strcat(final, line);
                strcat(final, "\n");
                line = strtok(NULL, "\n");
            }
            free(wcopy);
        }
    }

    /* Blocks */
    if (block_count > 0) {
        strcat(final, "\n\n--- Attached Context ---\n\n");
        for (int i = 0; i < block_count; i++) {
            if (i > 0) strcat(final, "\n\n");
            strcat(final, blocks[i]);
            free(blocks[i]);
        }
    }

    result->result = final;
    free(stripped);
    return result;
}

void context_ref_free(context_ref_result_t *result) {
    if (!result) return;
    free(result->result);
    free(result->original_message);
    free(result);
}
