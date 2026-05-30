/*
 * sandbox_escape.c — O14: Sandbox escape detection for Hermes C.
 *
 * Checks command strings and file paths for escape patterns before
 * they reach subprocess execution. Integrates with exec_code and
 * terminal tool handlers to prevent sandbox breakout attempts.
 *
 * Detection categories:
 * - Path traversal (../, ~/ expansion abuse)
 * - Sensitive paths (/etc, /proc, /sys, /dev, SSH keys, shadow)
 * - Shell injection (;, &&, ||, backticks, $())
 * - Known escape commands (bwrap bypass, nsenter, docker run)
 * - Environment poisoning (LD_PRELOAD, PATH hijacking)
 * - Fork bomb patterns (repeated fork/exec, :(){:|:&})
 * - Network escape (reverse shells, curl/wget to suspicious hosts)
 *
 * Logs all blocked attempts to stderr (audit integration point).
 */

#include "hermes.h"
#include "hermes_sandbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ──────────────────────────────────────────────
 *  Escape Pattern Table
 * ────────────────────────────────────────────── */

#define MAX_PATTERNS 64

typedef struct {
    const char *pattern;      /* Substring to match */
    int flags;                /* Category flags */
    const char *reason;       /* Description for log */
} escape_pattern_t;

static escape_pattern_t g_patterns[MAX_PATTERNS];
static int g_pattern_count = 0;
static bool g_escape_enabled = true;
static int g_blocked_count = 0;
static int g_attempt_count = 0;
static bool g_initialized = false;

/* Add a built-in pattern */
static void add_pattern(const char *pattern, int flags, const char *reason) {
    if (g_pattern_count >= MAX_PATTERNS) return;
    g_patterns[g_pattern_count].pattern = pattern;
    g_patterns[g_pattern_count].flags = flags;
    g_patterns[g_pattern_count].reason = reason;
    g_pattern_count++;
}

/* Initialize default escape patterns */
void sandbox_escape_init(void) {
    if (g_initialized) return;
    g_initialized = true;
    g_escape_enabled = true;
    g_blocked_count = 0;
    g_attempt_count = 0;
    g_pattern_count = 0;

    /* ── Path traversal ── */
    add_pattern("../",          SANDBOX_FLAG_PATH,     "path traversal: ../");
    add_pattern("..%2f",        SANDBOX_FLAG_PATH,     "path traversal: URL-encoded ../");
    add_pattern("..\\",         SANDBOX_FLAG_PATH,     "path traversal: Windows ..\\");
    add_pattern("../../../",    SANDBOX_FLAG_PATH,     "path traversal: deep ../");

    /* ── Sensitive system paths ── */
    add_pattern("/etc/passwd",  SANDBOX_FLAG_PATH,     "sensitive: /etc/passwd");
    add_pattern("/etc/shadow",  SANDBOX_FLAG_PATH,     "sensitive: /etc/shadow");
    add_pattern("/etc/sudoers", SANDBOX_FLAG_PATH,     "sensitive: /etc/sudoers");
    add_pattern("/etc/ssh/",    SANDBOX_FLAG_PATH,     "sensitive: SSH config");
    add_pattern("/root/.ssh/",  SANDBOX_FLAG_PATH,     "sensitive: root SSH keys");
    add_pattern("/home/*/.ssh/",SANDBOX_FLAG_PATH,     "sensitive: user SSH keys");

    /* ── /proc/ and /sys/ access ── */
    add_pattern("/proc/self/",  SANDBOX_FLAG_PROC_SYS, "proc: self introspection");
    add_pattern("/proc/1/",     SANDBOX_FLAG_PROC_SYS, "proc: init process access");
    add_pattern("/proc/",       SANDBOX_FLAG_PROC_SYS, "proc: filesystem access");
    add_pattern("/sys/",        SANDBOX_FLAG_PROC_SYS, "sysfs: kernel parameter access");

    /* ── Shell injection ── */
    add_pattern("; ",           SANDBOX_FLAG_INJECTION, "shell injection: semicolon");
    add_pattern(";|",           SANDBOX_FLAG_INJECTION, "shell injection: pipe-semicolon");
    add_pattern("|;",           SANDBOX_FLAG_INJECTION, "shell injection: semi-pipe");
    add_pattern("`",            SANDBOX_FLAG_INJECTION, "shell injection: backtick exec");
    add_pattern("$(",           SANDBOX_FLAG_INJECTION, "shell injection: subprocess");
    add_pattern("$(<",          SANDBOX_FLAG_INJECTION, "shell injection: file read");

    /* ── Fork bomb patterns ── */
    add_pattern(":(){",         SANDBOX_FLAG_FORK_BOMB, "fork bomb: bash fork");
    add_pattern("fork()",       SANDBOX_FLAG_FORK_BOMB, "fork bomb: direct fork");
    add_pattern("forkbomb",     SANDBOX_FLAG_FORK_BOMB, "fork bomb: explicit");

    /* ── Known sandbox escape commands ── */
    add_pattern("bwrap --",     SANDBOX_FLAG_ESCAPE_CMD, "escape: bwrap bypass");
    add_pattern("nsenter",      SANDBOX_FLAG_ESCAPE_CMD, "escape: nsenter");
    add_pattern("unshare",      SANDBOX_FLAG_ESCAPE_CMD, "escape: unshare namespace");
    add_pattern("chroot ",      SANDBOX_FLAG_ESCAPE_CMD, "escape: chroot");
    add_pattern("docker run",   SANDBOX_FLAG_ESCAPE_CMD, "escape: docker container");
    add_pattern("docker exec",  SANDBOX_FLAG_ESCAPE_CMD, "escape: docker exec");
    add_pattern("kubectl exec", SANDBOX_FLAG_ESCAPE_CMD, "escape: k8s exec");
    add_pattern("flatpak run",  SANDBOX_FLAG_ESCAPE_CMD, "escape: flatpak run");
    add_pattern("snap run",     SANDBOX_FLAG_ESCAPE_CMD, "escape: snap run");

    /* ── Environment poisoning ── */
    add_pattern("LD_PRELOAD",   SANDBOX_FLAG_ENV_POISON, "env: LD_PRELOAD injection");
    add_pattern("LD_LIBRARY_PATH", SANDBOX_FLAG_ENV_POISON, "env: LD_LIBRARY_PATH hijack");
    add_pattern("LD_AUDIT",     SANDBOX_FLAG_ENV_POISON, "env: LD_AUDIT injection");
    add_pattern("LD_DEBUG",     SANDBOX_FLAG_ENV_POISON, "env: LD_DEBUG leak");
    add_pattern("PYTHONPATH=",  SANDBOX_FLAG_ENV_POISON, "env: PYTHONPATH hijack");
    add_pattern("PERL5LIB=",    SANDBOX_FLAG_ENV_POISON, "env: PERL5LIB hijack");

    /* ── Network escape (reverse shells, data exfil) ── */
    add_pattern(">/dev/tcp/",   SANDBOX_FLAG_NET_ACCESS, "net: bash TCP device");
    add_pattern(">/dev/udp/",   SANDBOX_FLAG_NET_ACCESS, "net: bash UDP device");
    add_pattern("nc -e ",       SANDBOX_FLAG_NET_ACCESS, "net: reverse shell via nc");
    add_pattern("nc -l",        SANDBOX_FLAG_NET_ACCESS, "net: netcat listener");
    add_pattern("bash -i >&",   SANDBOX_FLAG_NET_ACCESS, "net: reverse bash shell");
    add_pattern("ncat",         SANDBOX_FLAG_NET_ACCESS, "net: ncat");
    add_pattern("socat",        SANDBOX_FLAG_NET_ACCESS, "net: socat relay");
    add_pattern("curl -o-",     SANDBOX_FLAG_NET_ACCESS, "net: pipe-to-sh download");
    add_pattern("wget -O-",     SANDBOX_FLAG_NET_ACCESS, "net: pipe-to-sh download");
}

void sandbox_escape_enable(bool enabled) {
    g_escape_enabled = enabled;
}

bool sandbox_escape_is_enabled(void) {
    return g_escape_enabled;
}

bool sandbox_escape_add_pattern(const char *pattern, const char *reason) {
    if (!pattern || !reason) return false;
    if (g_pattern_count >= MAX_PATTERNS) return false;
    g_patterns[g_pattern_count].pattern = pattern;
    g_patterns[g_pattern_count].flags = SANDBOX_FLAG_CUSTOM;
    g_patterns[g_pattern_count].reason = reason;
    g_pattern_count++;
    return true;
}

int sandbox_escape_blocked_count(void) {
    return g_blocked_count;
}

int sandbox_escape_attempt_count(void) {
    return g_attempt_count;
}

/* ──────────────────────────────────────────────
 *  Escape Check Logic
 * ────────────────────────────────────────────── */

/* Normalize a string for matching:
 * - Lowercase ASCII (optional, we keep case-sensitive by default)
 * - We DON'T strip whitespace — that would lose context for shell injection checks
 */
static bool str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return false;
    return strstr(haystack, needle) != NULL;
}

/* Scan command for escape patterns.
 * Returns the first matched pattern info via result. */
sandbox_escape_result_t sandbox_escape_check(const char *cmd, int len, const char *context) {
    sandbox_escape_result_t result = {false, ""};

    if (!cmd || !*cmd) return result;
    if (!g_escape_enabled) return result;

    g_attempt_count++;

    /* Truncate to len if provided */
    char stack_buf[4096];
    const char *check_str = cmd;
    char *buf = NULL;

    if (len >= 0 && (size_t)len < strlen(cmd)) {
        if ((size_t)len < sizeof(stack_buf) - 1) {
            memcpy(stack_buf, cmd, len);
            stack_buf[len] = '\0';
            check_str = stack_buf;
        } else {
            buf = (char *)malloc((size_t)(len + 1));
            if (buf) {
                memcpy(buf, cmd, len);
                buf[len] = '\0';
                check_str = buf;
            }
        }
    }

    /* Check each pattern */
    const char *first_reason = NULL;

    for (int i = 0; i < g_pattern_count; i++) {
        if (str_contains(check_str, g_patterns[i].pattern)) {
            if (!first_reason) {
                first_reason = g_patterns[i].reason;
            }
        }
    }

    free(buf);

    if (first_reason) {
        result.blocked = true;
        snprintf(result.reason, sizeof(result.reason),
                 "[sandbox] escape blocked [%s]: %s",
                 context ? context : "unknown", first_reason);
        g_blocked_count++;
        fprintf(stderr, "SANDBOX_ESCAPE: %s\n", result.reason);
    }

    return result;
}

/* Check a path specifically for escape patterns */
sandbox_escape_result_t sandbox_escape_check_path(const char *path, const char *context) {
    sandbox_escape_result_t result = {false, ""};
    if (!path || !*path) return result;

    /* Deep path traversal — more than 2 levels of ../ */
    const char *p = path;
    int depth_count = 0;
    while ((p = strstr(p, "..")) != NULL) {
        /* Must be preceded by / or at start, followed by / or end */
        if ((p == path || *(p-1) == '/') &&
            (*(p+2) == '/' || *(p+2) == '\0')) {
            depth_count++;
            if (depth_count >= 2) {
                result.blocked = true;
                snprintf(result.reason, sizeof(result.reason),
                         "deep path traversal (%d levels) in [%s]",
                         depth_count, context ? context : "unknown");
                g_blocked_count++;
                fprintf(stderr, "SANDBOX_ESCAPE: %s\n", result.reason);
                return result;
            }
        }
        p += 2;
    }

    /* Sensitive home paths */
    if (strstr(path, "/etc/") || strstr(path, "/proc/") ||
        strstr(path, "/sys/") || strstr(path, "/dev/") ||
        strstr(path, "/boot/") || strstr(path, "/root/") ||
        strstr(path, "/var/log/auth") || strstr(path, "/var/log/secure")) {
        result.blocked = true;
        snprintf(result.reason, sizeof(result.reason),
                 "sensitive path access detected in [%s]",
                 context ? context : "unknown");
        g_blocked_count++;
        fprintf(stderr, "SANDBOX_ESCAPE: %s\n", result.reason);
    }

    return result;
}

/* Exempt certain known-safe patterns from injection detection.
 * Called by the tool handlers before calling sandbox_escape_check.
 * For example, the terminal tool's internal timeout wrapper should
 * not trigger injection detection. */
bool sandbox_escape_is_internal_cmd(const char *cmd) {
    if (!cmd) return false;
    /* Internal tool wrappers start with known prefixes */
    if (strncmp(cmd, "timeout ", 8) == 0) return true;
    if (strncmp(cmd, "python3 -c ", 11) == 0) return true;
    if (strncmp(cmd, "python -c ", 10) == 0) return true;
    return false;
}
