/**
 * @file shell_hooks.c
 * @brief B07: Shell-script hooks bridge for C.
 *
 * Reads the hooks: block from config, registers callbacks on the
 * hook registry, and runs configured shell commands when events fire.
 *
 * Config format (from cli-config.yaml):
 *   hooks:
 *     pre_tool_call:
 *       - command: "/path/to/script.sh"
 *         matcher: "terminal"      # optional regex
 *         timeout: 30              # optional, default 60
 *     post_tool_call:
 *       - command: "python3 /path/hook.py"
 *
 * Wire protocol (stdin JSON -> stdout JSON):
 *   Stdin:  {"hook_event_name":"...", "tool_name":"...", ...}
 *   Stdout: {"decision":"block","reason":"..."} or {"context":"..."}
 */
#include "hermes_hooks.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <regex.h>
#include <time.h>

/* ── Constants ──────────────────────────────────────────────────── */

#define SHELL_HOOK_MAX_ENTRIES    64
#define SHELL_HOOK_DEFAULT_TIMEOUT 60
#define SHELL_HOOK_MAX_TIMEOUT     300
#define SHELL_HOOK_ALLOWLIST_PATH  ".hermes/shell-hooks-allowlist.json"

/* ── Internal types ─────────────────────────────────────────────── */

typedef struct {
    char    event[64];
    char    command[512];
    char    matcher[256];
    int     timeout;
    regex_t matcher_re;         /* compiled regex, valid if matcher_valid */
    bool    matcher_valid;
} shell_hook_spec_t;

/* ── Global state ───────────────────────────────────────────────── */

static shell_hook_spec_t g_hooks[SHELL_HOOK_MAX_ENTRIES];
static int g_hook_count = 0;

/* ── Config parsing ─────────────────────────────────────────────── */

/**
 * Parse a single shell hook spec from a JSON node.
 * node is an object with "command", "matcher", "timeout" keys.
 * Returns 1 on success, 0 on skip.
 */
static int parse_single_hook(const char *event, const json_t *node) {
    if (!event || !node || g_hook_count >= SHELL_HOOK_MAX_ENTRIES)
        return 0;

    shell_hook_spec_t *h = &g_hooks[g_hook_count];

    snprintf(h->event, sizeof(h->event), "%s", event);

    const char *cmd = json_get_str(node, "command", NULL);
    if (!cmd || !cmd[0]) return 0;
    snprintf(h->command, sizeof(h->command), "%s", cmd);

    const char *matcher = json_get_str(node, "matcher", NULL);
    if (matcher && matcher[0]) {
        snprintf(h->matcher, sizeof(h->matcher), "%s", matcher);
        if (regcomp(&h->matcher_re, matcher, REG_EXTENDED | REG_NOSUB) == 0)
            h->matcher_valid = true;
    }

    double timeout = json_get_num(node, "timeout", SHELL_HOOK_DEFAULT_TIMEOUT);
    if (timeout < 1) timeout = SHELL_HOOK_DEFAULT_TIMEOUT;
    if (timeout > SHELL_HOOK_MAX_TIMEOUT) timeout = SHELL_HOOK_MAX_TIMEOUT;
    h->timeout = (int)timeout;

    g_hook_count++;
    return 1;
}

/**
 * Parse the hooks: config block from a JSON object.
 * The object keys are event names, values are arrays of hook specs.
 * Returns number of parsed specs.
 */
int shell_hooks_parse_json(const json_t *hooks_json) {
    if (!hooks_json || hooks_json->type != JSON_OBJECT) return 0;

    int count = 0;
    for (size_t i = 0; i < hooks_json->c.count; i++) {
        const char *event = hooks_json->c.keys[i];
        const json_t *entries = hooks_json->c.items[i];

        if (!event || !entries || entries->type != JSON_ARRAY)
            continue;

        for (size_t j = 0; j < entries->c.count; j++) {
            if (parse_single_hook(event, entries->c.items[j]))
                count++;
        }
    }
    return count;
}

/* ── Allowlist ──────────────────────────────────────────────────── */

/**
 * Build the allowlist path from hermes_home.
 */
static void allowlist_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s", home, SHELL_HOOK_ALLOWLIST_PATH + 1);
}

/**
 * Check if (event, command) pair is in the allowlist.
 */
static bool allowlist_check(const char *event, const char *command) {
    char path[1024];
    allowlist_path(path, sizeof(path));

    json_t *root = json_parse_file(path, NULL);
    if (!root) return false;

    const json_t *approvals = json_obj_get(root, "approvals");
    bool found = false;
    if (approvals && approvals->type == JSON_ARRAY) {
        for (size_t i = 0; i < approvals->c.count && !found; i++) {
            const json_t *entry = approvals->c.items[i];
            if (!entry) continue;
            const char *e = json_get_str(entry, "event", "");
            const char *c = json_get_str(entry, "command", "");
            if (strcmp(e, event) == 0 && strcmp(c, command) == 0)
                found = true;
        }
    }

    json_free(root);
    return found;
}

/**
 * Record an approval in the allowlist.
 */
static void allowlist_record(const char *event, const char *command) {
    char path[1024];
    allowlist_path(path, sizeof(path));

    json_t *root = json_parse_file(path, NULL);
    if (!root) root = json_object();

    json_t *approvals = json_obj_get(root, "approvals");
    if (!approvals) {
        approvals = json_array();
        json_set(root, "approvals", approvals);
    }

    json_t *entry = json_object();
    json_set(entry, "event", json_string(event));
    json_set(entry, "command", json_string(command));

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    json_set(entry, "approved_at", json_string(ts));

    json_append(approvals, entry);

    char *out = json_serialize_pretty(root, 2);
    if (out) {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
    json_free(root);
}

/* ── Subprocess callback ────────────────────────────────────────── */

/**
 * Build stdin JSON payload for a shell hook invocation.
 */
static char *build_payload(const char *event, const char *tool_name,
                            const char *tool_input, const char *session_id) {
    json_t *payload = json_object();
    json_set(payload, "hook_event_name", json_string(event ? event : ""));
    json_set(payload, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(payload, "tool_input", json_string(tool_input ? tool_input : ""));
    json_set(payload, "session_id", json_string(session_id ? session_id : ""));

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)))
        json_set(payload, "cwd", json_string(cwd));

    char *result = json_serialize(payload);
    json_free(payload);
    return result;
}

/**
 * Callback function registered on the hook registry.
 * Spawns the shell command, feeds JSON via heredoc, captures stdout.
 */
static char *shell_hook_callback(const char *event, const char *payload,
                                  void *userdata) {
    (void)event;
    const shell_hook_spec_t *spec = (const shell_hook_spec_t *)userdata;
    if (!spec) return NULL;

    /* Build command with heredoc for stdin JSON */
    size_t cmdlen = strlen(spec->command) + (payload ? strlen(payload) : 0) + 128;
    char *full_cmd = (char *)malloc(cmdlen);
    if (!full_cmd) return NULL;

    if (payload)
        snprintf(full_cmd, cmdlen, "%s << 'HERMES_HOOK_EOF'\n%s\nHERMES_HOOK_EOF",
                 spec->command, payload);
    else
        snprintf(full_cmd, cmdlen, "%s", spec->command);

    /* Run via popen("r") to capture stdout */
    FILE *fp = popen(full_cmd, "r");
    free(full_cmd);
    if (!fp) return NULL;

    /* Read all stdout */
    char buf[16384];
    size_t pos = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && pos < sizeof(buf) - 1)
        buf[pos++] = (char)c;
    buf[pos] = '\0';

    int status = pclose(fp);

    /* On non-zero exit, log but still parse stdout */
    if (status == -1) return NULL;

    /* Trim trailing whitespace */
    while (pos > 0 && (buf[pos - 1] == '\n' || buf[pos - 1] == ' ' || buf[pos - 1] == '\r'))
        pos--;
    buf[pos] = '\0';

    if (pos == 0) return NULL;
    return strdup(buf);
}

/* ── Check if a tool name matches a spec's matcher ──────────────── */

static bool spec_matches_tool(const shell_hook_spec_t *spec, const char *tool_name) {
    if (!spec->matcher[0]) return true;  /* no matcher = matches all */
    if (!tool_name) return false;

    if (spec->matcher_valid) {
        return regexec(&spec->matcher_re, tool_name, 0, NULL, 0) == 0;
    }

    /* Fallback to literal comparison */
    return strcmp(spec->matcher, tool_name) == 0;
}

/* ── Registration ───────────────────────────────────────────────── */

/**
 * Register all configured shell hooks on the hook registry.
 * Called once at startup after config is loaded.
 * Returns number of registered hooks.
 */
int shell_hooks_register_all(void) {
    int registered = 0;
    for (int i = 0; i < g_hook_count; i++) {
        if (hook_register(g_hooks[i].event, shell_hook_callback, &g_hooks[i]))
            registered++;
    }
    return registered;
}

/**
 * Clean up all shell hook registrations.
 */
void shell_hooks_shutdown(void) {
    for (int i = 0; i < g_hook_count; i++) {
        hook_unregister(g_hooks[i].event, shell_hook_callback, &g_hooks[i]);
        if (g_hooks[i].matcher_valid)
            regfree(&g_hooks[i].matcher_re);
    }
    g_hook_count = 0;
}

/**
 * Return the number of currently configured shell hook specs.
 */
int shell_hooks_count(void) {
    return g_hook_count;
}
