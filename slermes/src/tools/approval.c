/*
 * approval.c — Dangerous command approval security for Hermes C.
 * Phase 91: Intercepts dangerous tool calls, asks user for approval.
 * Mirrors Python's tools/approval.py (~1393 LOC).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_url_safety.h"
#include "hermes_tirith.h"
#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>

/* Forward declarations for P164 audit logging */
void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail);

/* ================================================================
 *  Dangerous Patterns
 * ================================================================ */

/* Terminal commands that require approval */
static const char *DANGEROUS_TERMINAL_PATTERNS[] = {
    "rm -rf", "rm -r /", "rm -rf /", "rm -fr /",
    "mkfs", "format", "dd if=", "dd of=",
    "> /dev/sd", "> /dev/nvme", "> /dev/hd",
    "> /dev/mmcblk", "> /dev/vd", "> /dev/xvd",
    "chmod 777 /", "chown -R",
    "wget ", "curl ", /* External downloads */
    "sudo ", "su ",
    "passwd", "usermod", "useradd",
    "iptables", "ufw",
    "reboot", "shutdown", "poweroff", "halt",
    "kill -9", "pkill",
    "docker rm ", "docker rmi ", "docker system prune",
    "systemctl stop ", "systemctl disable ",
    "dd if=/dev/zero", "dd if=/dev/urandom",
    ":(){ :|:& };:", /* Fork bomb */
    NULL
};

/* File paths that require approval */
static const char *DANGEROUS_FILE_PATTERNS[] = {
    "/etc/", "/boot/", "/sys/", "/proc/",
    "/dev/sd", "/dev/nvme", "/dev/mmcblk",
    "/lib/", "/lib64/", "/usr/lib/",
    "/bin/", "/sbin/", "/usr/bin/", "/usr/sbin/",
    NULL
};

/* Path traversal patterns (in args or paths) */
static const char *PATH_TRAVERSAL_PATTERNS[] = {
    "../", "..\\", "%2e%2e", "%2E%2E",
    "..%2f", "..%5c",
    NULL
};

/* Web URLs / patterns that require approval */
static const char *DANGEROUS_WEB_PATTERNS[] = {
    "127.0.0.1", "localhost", "169.254.",
    "10.", "172.1", "192.168.",
    "internal", "corp", "private",
    NULL
};

/* ================================================================
 *  Approval State
 * ================================================================ */

/* Per-session approved/denied cache */
#define MAX_APPROVED 256
static struct {
    char command[512];
    char tool[64];
    bool approved;   /* true=approved, false=denied */
    bool permanent;  /* saved to allowlist file */
} g_approval_cache[MAX_APPROVED];
static int g_approval_count = 0;

/* YOLO mode: when true, skip all approval prompts */
static bool g_yolo_approval = false;

/* Allowlist file path (set at init) */
static char g_allowlist_path[HERMES_PATH_MAX] = {0};

void approval_set_yolo(bool enabled) {
    g_yolo_approval = enabled;
}

/* Set allowlist file path (called during init) */
void approval_set_allowlist_path(const char *path) {
    if (path) snprintf(g_allowlist_path, sizeof(g_allowlist_path), "%s", path);
}

static bool (*g_approval_send_fn)(const char *platform, const char *chat_id,
                                   const char *text) = NULL;
static char *(*g_approval_wait_fn)(int timeout_sec) = NULL;
static const char *g_approval_platform = NULL;
static const char *g_approval_chat_id = NULL;

/* Set gateway send function and current chat context */
void approval_set_gateway_send(bool (*fn)(const char *, const char *, const char *),
                                const char *platform, const char *chat_id) {
    g_approval_send_fn = fn;
    g_approval_platform = platform;
    g_approval_chat_id = chat_id;
}

/* Set gateway approval wait function — returns "y", "n", "a", or "" (timeout) */
void approval_set_gateway_wait(char *(*fn)(int timeout_sec)) {
    g_approval_wait_fn = fn;
}

/* P162: Forward declaration for approval timeout */
static int g_approval_timeout;

/* Load persistent allowlist from file */
void approval_load_allowlist(void) {
    if (!g_allowlist_path[0]) return;
    char *err = NULL;
    json_t *root = json_parse_file(g_allowlist_path, &err);
    if (!root) { free(err); return; }
    if (json_len(root) == 0) { json_free(root); return; }

    for (size_t i = 0; i < json_len(root) && g_approval_count < MAX_APPROVED; i++) {
        json_t *entry = json_get(root, i);
        if (!entry) continue;
        const char *tool = json_get_str(entry, "tool", "");
        const char *cmd = json_get_str(entry, "command", "");
        bool approved = json_get_bool(entry, "approved", true);
        if (tool[0] && cmd[0]) {
            snprintf(g_approval_cache[g_approval_count].tool, sizeof(g_approval_cache[0].tool), "%s", tool);
            snprintf(g_approval_cache[g_approval_count].command, sizeof(g_approval_cache[0].command), "%s", cmd);
            g_approval_cache[g_approval_count].approved = approved;
            g_approval_cache[g_approval_count].permanent = true;
            g_approval_count++;
        }
    }
    json_free(root);
}

/* Save persistent allowlist to file */
void approval_save_allowlist(void) {
    if (!g_allowlist_path[0]) return;
    json_t *arr = json_array();
    if (!arr) return;

    for (int i = 0; i < g_approval_count; i++) {
        if (!g_approval_cache[i].permanent) continue;
        json_t *entry = json_object();
        json_set(entry, "tool", json_string(g_approval_cache[i].tool));
        json_set(entry, "command", json_string(g_approval_cache[i].command));
        json_set(entry, "approved", json_bool(g_approval_cache[i].approved));
        json_append(arr, entry);
    }

    char *ser = json_serialize_pretty(arr, 2);
    json_free(arr);
    if (!ser) return;

    FILE *f = fopen(g_allowlist_path, "w");
    if (f) {
        fputs(ser, f);
        fclose(f);
    }
    free(ser);
}

/* Reset approval cache for new session */
void approval_reset_session(void) {
    g_approval_count = 0;
}

/* Check if a command is in the approval cache */
static int approval_cache_lookup(const char *tool, const char *cmd) {
    for (int i = 0; i < g_approval_count; i++) {
        if (strcmp(g_approval_cache[i].tool, tool) == 0 &&
            strcmp(g_approval_cache[i].command, cmd) == 0) {
            return g_approval_cache[i].approved ? 1 : -1;
        }
    }
    return 0; /* Not in cache */
}

/* Add to approval cache */
static void approval_cache_add(const char *tool, const char *cmd,
                                bool approved, bool permanent) {
    if (g_approval_count >= MAX_APPROVED) return;
    snprintf(g_approval_cache[g_approval_count].tool, sizeof(g_approval_cache[0].tool), "%s", tool);
    snprintf(g_approval_cache[g_approval_count].command, sizeof(g_approval_cache[0].command), "%s", cmd);
    g_approval_cache[g_approval_count].approved = approved;
    g_approval_cache[g_approval_count].permanent = permanent;
    g_approval_count++;
}

/* ================================================================
 *  Pattern Matching
 * ================================================================ */

static bool matches_any(const char *str, const char **patterns) {
    if (!str) return false;
    for (int i = 0; patterns[i]; i++) {
        if (strstr(str, patterns[i]))
            return true;
    }
    return false;
}

/* Normalize a command string before dangerous-pattern matching.
 * Strips ANSI escape sequences and null bytes.
 * Port of Python tools/approval.py _normalize_command_for_detection().
 * Returns a malloc'd normalized copy, or NULL on failure. */
char *approval_normalize_command(const char *command) {
    if (!command) return NULL;

    /* Step 1: Strip ANSI escape sequences via stack-allocated buffer.
     * ansi_strip_buf writes in-place if buf == text, so we use a separate buf. */
    size_t len = strlen(command);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;

    if (ansi_has_escape(command)) {
        ansi_strip_buf(command, buf, len + 1);
    } else {
        memcpy(buf, command, len + 1);
    }

    /* Step 2: Strip null bytes in-place. */
    char *dst = buf;
    for (const char *src = buf; *src; src++) {
        if (*src != '\0') {
            *dst++ = *src;
        }
    }
    *dst = '\0';

    /* Note: Unicode NFKC normalization (Python step 3) requires ICU — skipped in C. */

    return buf;
}

/* Check if a terminal command is dangerous. Normalizes first. */
bool approval_is_terminal_dangerous(const char *command) {
    if (!command) return false;
    char *normalized = approval_normalize_command(command);
    if (!normalized) return matches_any(command, DANGEROUS_TERMINAL_PATTERNS);
    bool result = matches_any(normalized, DANGEROUS_TERMINAL_PATTERNS);
    free(normalized);
    return result;
}

/* Check if a file path is dangerous */
bool approval_is_path_dangerous(const char *path) {
    return matches_any(path, DANGEROUS_FILE_PATTERNS);
}

/* Check if a path contains traversal (../) patterns */
bool approval_is_path_traversal(const char *path) {
    if (!path) return false;
    return matches_any(path, PATH_TRAVERSAL_PATTERNS);
}

/* Check if a URL is dangerous (SSRF/internal) — uses DNS resolution */
bool approval_is_url_dangerous(const char *url) {
    if (!url) return false;
    /* First check quick string patterns (no DNS) */
    if (matches_any(url, DANGEROUS_WEB_PATTERNS)) return true;
    /* Then do full DNS-based safety check */
    return !url_is_safe(url);
}

/* ================================================================
 *  User Prompt
 * ================================================================ */

/* Ask user for approval. Returns true if approved. */
static bool approval_prompt_user(const char *tool, const char *reason,
                                  const char *detail) {
    fprintf(stderr, "\n⚠️  DANGEROUS OPERATION DETECTED\n");
    fprintf(stderr, "  Tool: %s\n", tool);
    fprintf(stderr, "  Reason: %s\n", reason);
    if (detail) fprintf(stderr, "  Detail: %s\n", detail);
    fprintf(stderr, "  Timeout: %ds\n", g_approval_timeout);

    /* If gateway mode: send approval prompt via messaging platform */
    if (g_approval_send_fn && g_approval_wait_fn &&
        g_approval_platform && g_approval_chat_id) {
        char prompt[2048];
        snprintf(prompt, sizeof(prompt),
            "⚠️ *Approve dangerous command?*\n\n`%s`\n\nReason: %s\nReply y/n/a(always)",
            detail ? detail : tool, reason ? reason : "dangerous operation");
        if (!g_approval_send_fn(g_approval_platform, g_approval_chat_id, prompt)) {
            fprintf(stderr, "\n  → Denied (gateway send failed)\n");
            audit_log_security("approval", tool, "denied", "gateway send failed", detail);
            return false;
        }
        fprintf(stderr, "  → Sent approval prompt to %s/%s\n",
                g_approval_platform, g_approval_chat_id);
        /* Register pending approval context for gateway response collection */
        gw_approval_set_context(g_approval_platform, g_approval_chat_id);
        char *resp = g_approval_wait_fn(g_approval_timeout);
        if (!resp || !resp[0]) {
            free(resp);
            fprintf(stderr, "\n  → Auto-denied (timeout after %ds)\n", g_approval_timeout);
            audit_log_security("approval", tool, "timeout", reason, detail);
            return false;
        }
        /* Normalize */
        for (char *p = resp; *p; p++) *p = (char)tolower((unsigned char)*p);

        bool approved = false;
        if (resp[0] == 'y') {
            approval_cache_add(tool, detail, true, false);
            audit_log_security("approval", tool, "approved_once", reason, detail);
            approved = true;
        } else if (resp[0] == 'a') {
            approval_cache_add(tool, detail, true, true);
            approval_save_allowlist();
            audit_log_security("approval", tool, "approved_always", reason, detail);
            approved = true;
        } else {
            approval_cache_add(tool, detail, false, true);
            audit_log_security("approval", tool, "denied", reason, detail);
        }
        free(resp);
        return approved;
    }

    /* CLI mode: print prompt and wait for stdin */
    fprintf(stderr, "  Approve? [y/N/a(always)/n] ");

    /* If not interactive (piped input), deny by default */
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "\n  → Denied (non-interactive mode)\n");
        audit_log_security("approval", tool, "denied", "non-interactive", detail);
        return false;
    }

    /* P162: Approval timeout — use alarm or select-based timeout */
    char response[16];
    response[0] = '\0';

    /* Set up timeout via select on stdin */
    struct timeval tv;
    tv.tv_sec = g_approval_timeout;
    tv.tv_usec = 0;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int sel_ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
    if (sel_ret <= 0) {
        fprintf(stderr, "\n  → Auto-denied (timeout after %ds)\n", g_approval_timeout);
        audit_log_security("approval", tool, "timeout", reason, detail);
        return false;
    }

    if (!fgets(response, sizeof(response), stdin))
        return false;

    /* Normalize response */
    for (char *p = response; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (response[0] == 'y') {
        approval_cache_add(tool, detail, true, false);
        audit_log_security("approval", tool, "approved_once", reason, detail);
        return true;
    }
    if (response[0] == 'a') {
        approval_cache_add(tool, detail, true, true);
        approval_save_allowlist();
        fprintf(stderr, "  → Approved permanently (saved to allowlist).\n");
        audit_log_security("approval", tool, "approved_always", reason, detail);
        return true;
    }
    approval_cache_add(tool, detail, false, true);
    fprintf(stderr, "  → Denied.\n");
    audit_log_security("approval", tool, "denied", reason, detail);
    return false;
}

/* ================================================================
 *  Command Allowlist (P161)
 * ================================================================ */

/* Per-tool accepted command patterns. When configured, only commands matching
 * these patterns are allowed. Format: "tool_name:pattern" */
#define MAX_ALLOWLIST_ENTRIES 128

static struct {
    char tool[64];
    char pattern[256];  /* Glob pattern: *, ? wildcards supported */
} g_allowlist[MAX_ALLOWLIST_ENTRIES];
static int g_allowlist_count = 0;

/* P161: Add command allowlist entry */
bool allowlist_add(const char *tool, const char *pattern) {
    if (!tool || !pattern || g_allowlist_count >= MAX_ALLOWLIST_ENTRIES)
        return false;

    /* Check for duplicates */
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i].tool, tool) == 0 &&
            strcmp(g_allowlist[i].pattern, pattern) == 0)
            return true;
    }

    snprintf(g_allowlist[g_allowlist_count].tool, sizeof(g_allowlist[0].tool), "%s", tool);
    snprintf(g_allowlist[g_allowlist_count].pattern, sizeof(g_allowlist[0].pattern), "%s", pattern);
    g_allowlist_count++;
    return true;
}

/* P161: Remove allowlist entry */
bool allowlist_remove(const char *tool, const char *pattern) {
    if (!tool) return false;
    for (int i = 0; i < g_allowlist_count; i++) {
        bool match_tool = strcmp(g_allowlist[i].tool, tool) == 0;
        bool match_pat = !pattern || strcmp(g_allowlist[i].pattern, pattern) == 0;
        if (match_tool && match_pat) {
            g_allowlist[i] = g_allowlist[--g_allowlist_count];
            g_allowlist[g_allowlist_count].tool[0] = '\0';
            g_allowlist[g_allowlist_count].pattern[0] = '\0';
            return true;
        }
    }
    return false;
}

/* P161: Clear all allowlist entries */
void allowlist_clear(void) {
    memset(g_allowlist, 0, sizeof(g_allowlist));
    g_allowlist_count = 0;
}

/* Simple glob-style pattern matching.
 * Supports: * (any chars), ? (single char). Literal match for everything else.
 * Returns true if `str` matches `pattern`. */
static bool glob_match(const char *str, const char *pattern) {
    if (!str || !pattern) return false;
    while (*pattern) {
        if (*pattern == '*') {
            /* Skip consecutive stars */
            while (pattern[1] == '*') pattern++;
            pattern++;
            if (*pattern == '\0') return true;  /* trailing * matches everything */
            /* Try matching the rest at every position */
            while (*str) {
                if (glob_match(str, pattern)) return true;
                str++;
            }
            return false;
        } else if (*pattern == '?') {
            if (*str == '\0') return false;
            str++; pattern++;
        } else {
            if (*str != *pattern) return false;
            str++; pattern++;
        }
    }
    return *str == '\0';
}

/* P161: Check if a command is allowed by the allowlist for a tool.
 * Returns: true if allowed, false if denied.
 * If the tool has no allowlist entries, all commands pass.
 * If it has entries, the command must match at least one. */
bool allowlist_check(const char *tool, const char *command) {
    if (!tool || !command) return false;

    /* Count entries for this tool */
    bool has_entries = false;
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i].tool, tool) == 0) {
            has_entries = true;
            bool has_glob = (strchr(g_allowlist[i].pattern, '*') != NULL ||
                               strchr(g_allowlist[i].pattern, '?') != NULL);
            if (has_glob) {
                if (glob_match(command, g_allowlist[i].pattern))
                    return true; /* Matched glob */
            } else {
                /* Bare pattern: substring match (backward compat) */
                if (strstr(command, g_allowlist[i].pattern))
                    return true; /* Matched substring */
            }
        }
    }

    /* If no entries exist for this tool, allow all (opt-in system) */
    if (!has_entries) return true;

    /* Had entries but none matched */
    return false;
}

/* ================================================================
 *  Approval Timeout (P162)
 * ================================================================ */

/* Gateway approval callbacks — set by server.c when running in gateway mode */
/* Default approval timeout in seconds */
static int g_approval_timeout = 30;

void approval_set_timeout(int seconds) {
    g_approval_timeout = seconds > 0 ? seconds : 30;
}

int approval_get_timeout(void) {
    return g_approval_timeout;
}

/* ================================================================
 *  Main Approval Check
 * ================================================================ */

typedef struct {
    const char *tool_name;
    const char *args_json;
} tool_invocation_t;

/* Check if a tool invocation needs approval.
   Returns: 1 = approved, 0 = denied, -1 = not dangerous */
int approval_check(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return -1;

    /* Parse args if JSON */
    json_t *args = json_parse(args_json, NULL);
    if (!args) return -1;

    const char *danger_reason = NULL;
    const char *danger_detail = NULL;
    char detail_buf[2048];
    detail_buf[0] = '\0';

    /* Check terminal tool */
    if (strcmp(tool_name, "terminal") == 0) {
        const char *cmd = json_get_str(args, "command", "");

        /* P161: Check command allowlist */
        if (!allowlist_check(tool_name, cmd)) {
            danger_reason = "Command not in allowlist";
            danger_detail = cmd;
            json_free(args);
            /* Denied — command not in allowlist */
            audit_log_security("allowlist", tool_name, "denied", "command not in allowlist", cmd);
            return 0;
        }

        if (cmd && approval_is_terminal_dangerous(cmd)) {
            danger_reason = "Dangerous shell command";
            danger_detail = cmd;
        }
        /* Also run Tirith scan on terminal commands */
        if (!danger_reason && cmd) {
            tirith_verdict_t t = tirith_inline_scan(cmd);
            if (t == TIRITH_BLOCK) {
                danger_reason = "Tirith security: blocked pattern";
                danger_detail = cmd;
            } else if (t == TIRITH_WARN) {
                /* Warnings only flag if there's a matching dangerous pattern */
                danger_reason = "Tirith warning: suspicious command pattern";
                danger_detail = cmd;
            }
        }
    }

    /* Check file tools */
    if (strcmp(tool_name, "write_file") == 0 || strcmp(tool_name, "patch") == 0) {
        const char *path = json_get_str(args, "path", "");
        if (path && (approval_is_path_dangerous(path) || approval_is_path_traversal(path))) {
            snprintf(detail_buf, sizeof(detail_buf), "%s \u2192 %s", tool_name, path);
            danger_reason = approval_is_path_traversal(path) ?
                "Path traversal detected" : "Modifying system file";
            danger_detail = detail_buf;
        }
    }

    /* Check web tools for SSRF */
    if (strcmp(tool_name, "web_get") == 0 || strcmp(tool_name, "web_search") == 0) {
        const char *url = json_get_str(args, "url", "");
        if (!url || !*url) url = json_get_str(args, "query", "");
        if (url && approval_is_url_dangerous(url)) {
            snprintf(detail_buf, sizeof(detail_buf), "%s → %s", tool_name, url);
            danger_reason = "Potential SSRF to internal network";
            danger_detail = detail_buf;
        }
    }

    json_free(args);

    if (!danger_reason) return -1; /* Not dangerous */

    /* YOLO mode: skip all approval prompts */
    if (g_yolo_approval) return 1;

    /* Check cache */
    int cached = approval_cache_lookup(tool_name, danger_detail);
    if (cached == 1) return 1;  /* Previously approved */
    if (cached == -1) return 0; /* Previously denied */

    /* Ask user */
    return approval_prompt_user(tool_name, danger_reason, danger_detail) ? 1 : 0;
}

/* Approval tool — lets agent manage approval settings */
static char *approval_status_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    char *result = (char *)malloc(4096);
    if (!result) return strdup("{\"error\": \"OOM\"}");

    /* Report cached approvals */
    snprintf(result, 4096,
        "{"
        "  \"cached_approvals\": %d,"
        "  \"session_reset\": \"use approval_reset to clear cache\""
        "}",
        g_approval_count);
    return result;
}

/* P39: Query approval cache — return count */
int approval_cache_count(void) {
    return g_approval_count;
}

/* P39: Get approval cache entry text for display */
const char *approval_cache_entry(int index) {
    static char buf[256];
    if (index < 0 || index >= g_approval_count) return NULL;
    snprintf(buf, sizeof(buf), "[%d] %s/%s: %s",
             index,
             g_approval_cache[index].tool,
             g_approval_cache[index].command,
             g_approval_cache[index].approved ? "APPROVED" : "DENIED");
    return buf;
}

/* P39: Clear last N entries, or all if N=0 */
void approval_cache_clear_last(int n) {
    if (n <= 0 || n >= g_approval_count) {
        g_approval_count = 0;
        return;
    }
    g_approval_count -= n;
}

/* Register approval management tool */
void registry_init_approval(void) {
    registry_register("approval_status",
        "Check the current security approval status. Shows cached approvals for this session.",
        "{\"type\":\"object\",\"properties\":{}}",
        approval_status_handler);
}
