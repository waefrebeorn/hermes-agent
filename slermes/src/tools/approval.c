/*
 * approval.c — Dangerous command approval security for Hermes C.
 * Phase 91: Intercepts dangerous tool calls, asks user for approval.
 * Mirrors Python's tools/approval.py (~1393 LOC).
 */

#include "slermes.h"
#include "slermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* ================================================================
 *  Dangerous Patterns
 * ================================================================ */

/* Terminal commands that require approval */
static const char *DANGEROUS_TERMINAL_PATTERNS[] = {
    "rm -rf", "rm -r /", "rm -rf /", "rm -fr /",
    "mkfs", "format", "dd if=", "dd of=",
    "> /dev/", "> /dev/sd", "> /dev/nvme",
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
    bool permanent;  /* remember for this session */
} g_approval_cache[MAX_APPROVED];
static int g_approval_count = 0;

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

/* Check if a terminal command is dangerous */
bool approval_is_terminal_dangerous(const char *command) {
    return matches_any(command, DANGEROUS_TERMINAL_PATTERNS);
}

/* Check if a file path is dangerous */
bool approval_is_path_dangerous(const char *path) {
    return matches_any(path, DANGEROUS_FILE_PATTERNS);
}

/* Check if a URL is dangerous (internal/SSRF) */
bool approval_is_url_dangerous(const char *url) {
    return matches_any(url, DANGEROUS_WEB_PATTERNS);
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
    fprintf(stderr, "  Approve? [y/N/a(always)/n] ");

    /* If not interactive (piped input), deny by default */
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "\n  → Denied (non-interactive mode)\n");
        return false;
    }

    char response[16];
    if (!fgets(response, sizeof(response), stdin))
        return false;

    /* Normalize response */
    for (char *p = response; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (response[0] == 'y') {
        approval_cache_add(tool, detail, true, false);
        return true;
    }
    if (response[0] == 'a') {
        approval_cache_add(tool, detail, true, true);
        fprintf(stderr, "  → Approved for this session.\n");
        return true;
    }
    approval_cache_add(tool, detail, false, true);
    fprintf(stderr, "  → Denied.\n");
    return false;
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
        if (cmd && approval_is_terminal_dangerous(cmd)) {
            danger_reason = "Dangerous shell command";
            danger_detail = cmd;
        }
    }

    /* Check file tools */
    if (strcmp(tool_name, "write_file") == 0 || strcmp(tool_name, "patch") == 0) {
        const char *path = json_get_str(args, "path", "");
        if (path && approval_is_path_dangerous(path)) {
            snprintf(detail_buf, sizeof(detail_buf), "%s → %s", tool_name, path);
            danger_reason = "Modifying system file";
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

/* Register approval management tool */
void registry_init_approval(void) {
    registry_register("approval_status",
        "Check the current security approval status. Shows cached approvals for this session.",
        "{\"type\":\"object\",\"properties\":{}}",
        approval_status_handler);
}
