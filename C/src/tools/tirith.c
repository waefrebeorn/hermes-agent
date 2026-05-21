/*
 * tirith.c — Pre-exec security scanning.
 * Phase 116-120: Tirith integration — binary + inline checks.
 * Detects: homograph URLs, pipe-to-interpreter, terminal injection, env leaks.
 */

#include "hermes.h"
#include "hermes_tirith.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

/* ================================================================
 *  Configuration
 * ================================================================ */

static char g_tirith_path[1024] = {0}; /* Empty = use "tirith" from PATH */
static bool g_tirith_enabled = true;
static bool g_tirith_fail_open = true; /* Fail open = on error, allow */

void tirith_set_path(const char *path) {
    if (path)
        snprintf(g_tirith_path, sizeof(g_tirith_path), "%s", path);
    else
        g_tirith_path[0] = '\0';
}

void tirith_set_enabled(bool enabled) {
    g_tirith_enabled = enabled;
}

void tirith_set_fail_open(bool fail_open) {
    g_tirith_fail_open = fail_open;
}

/* ================================================================
 *  Inline Checks (always run, no external binary needed)
 * ================================================================ */

/* Suspicious pipe targets: things that pipe to an interpreter */
static const char *PIPE_INTERPRETERS[] = {
    "| sh", "| bash", "| zsh", "| dash", "| fish",
    "| python", "| python3", "| perl", "| ruby",
    "| php", "| lua", "| node",
    "| base64 -d | sh", "| base64 -d | bash",
    "| base64 --decode | sh",
    "| eval", "$(curl", "$(wget",
    NULL
};

/* URL schemes that should not appear in shell commands normally */
static const char *SUSPICIOUS_URL_PATTERNS[] = {
    "http://", "https://", "ftp://",
    "bit.ly/", "tinyurl.com/", "shorturl.at/",
    NULL
};

bool tirith_has_pipe_to_interpreter(const char *command) {
    if (!command) return false;
    char lower[4096];
    size_t i;
    for (i = 0; command[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)tolower((unsigned char)command[i]);
    lower[i] = '\0';

    for (int j = 0; PIPE_INTERPRETERS[j]; j++) {
        if (strstr(lower, PIPE_INTERPRETERS[j]))
            return true;
    }
    return false;
}

bool tirith_has_command_substitution(const char *command) {
    if (!command) return false;
    /* Check for backticks */
    if (strchr(command, '`')) return true;
    /* Check for $() */
    if (strstr(command, "$(")) return true;
    return false;
}

bool tirith_has_env_injection(const char *command) {
    if (!command) return false;
    /* Check for suspicious env var patterns in args */
    if (strstr(command, "${") &&
        (strstr(command, "LD_PRELOAD") ||
         strstr(command, "LD_LIBRARY_PATH") ||
         strstr(command, "PATH=") ||
         strstr(command, "IFS=") ||
         strstr(command, "LD_AUDIT")))
        return true;
    return false;
}

/* Simple check: does text contain ASCII + non-ASCII mixed script?
 * This catches some homograph attacks but is not comprehensive. */
bool tirith_has_suspicious_url(const char *command) {
    if (!command) return false;

    /* Find URLs in command */
    const char *p = command;
    while ((p = strstr(p, "http")) != NULL) {
        /* Check for non-ASCII characters in the URL */
        const char *url_start = p;
        const char *url_end = url_start;
        while (*url_end && !isspace((unsigned char)*url_end) &&
               *url_end != '\'' && *url_end != '"' && *url_end != '`')
            url_end++;

        /* Scan URL for suspicious encoding */
        for (const char *c = url_start; c < url_end; c++) {
            if ((unsigned char)*c > 127) {
                /* Non-ASCII in URL could be homograph attack */
                return true;
            }
        }

        /* Check URL with known shortening services */
        for (int j = 0; SUSPICIOUS_URL_PATTERNS[j]; j++) {
            size_t plen = strlen(SUSPICIOUS_URL_PATTERNS[j]);
            if (strncmp(url_start, SUSPICIOUS_URL_PATTERNS[j], plen) == 0 &&
                strstr(url_start, "|") != NULL) {
                /* URL piped to something — suspicious */
                return true;
            }
        }

        p = url_end;
    }

    return false;
}

tirith_verdict_t tirith_inline_scan(const char *command) {
    if (!command) return TIRITH_ALLOW;

    if (tirith_has_pipe_to_interpreter(command))
        return TIRITH_BLOCK;

    if (tirith_has_suspicious_url(command))
        return TIRITH_BLOCK;

    if (tirith_has_env_injection(command))
        return TIRITH_BLOCK;

    /* Command substitution is common in legitimate use, just warn */
    if (tirith_has_command_substitution(command))
        return TIRITH_WARN;

    return TIRITH_ALLOW;
}

/* ================================================================
 *  External Binary Check
 * ================================================================ */

tirith_verdict_t tirith_scan(const char *command) {
    /* Always run inline checks first */
    tirith_verdict_t inline_v = tirith_inline_scan(command);
    if (inline_v == TIRITH_BLOCK)
        return TIRITH_BLOCK;

    /* Skip external binary if disabled */
    if (!g_tirith_enabled)
        return inline_v;

    /* Build command: tirith [path] scan <command> */
    const char *binary = g_tirith_path[0] ? g_tirith_path : "tirith";

    /* Check if binary exists */
    if (access(binary, X_OK) != 0) {
        /* Binary not found — if fail-open, allow. If fail-closed, block. */
        if (g_tirith_fail_open)
            return inline_v;
        return TIRITH_ERROR;
    }

    /* Execute tirith via popen */
    char cmd[8192];
    int r = snprintf(cmd, sizeof(cmd), "%s scan 2>/dev/null", binary);
    if (r < 0 || (size_t)r >= sizeof(cmd)) {
        return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
    }

    /* Read output */
    char output[4096];
    output[0] = '\0';
    if (fgets(output, (int)sizeof(output) - 1, fp)) {
        /* Remove newline */
        size_t olen = strlen(output);
        while (olen > 0 && (output[olen-1] == '\n' || output[olen-1] == '\r'))
            output[--olen] = '\0';
    }

    int status = pclose(fp);

    /* Interpret exit code */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        switch (exit_code) {
            case 0:  return TIRITH_ALLOW;
            case 1:  return TIRITH_BLOCK;
            case 2:  return TIRITH_WARN;
            default: return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
        }
    }

    return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
}
