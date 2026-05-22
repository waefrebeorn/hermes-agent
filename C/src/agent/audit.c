/*
 * audit.c — P164: Security audit log for Hermes C.
 *
 * Logs all security events (approvals, denials, redactions, violations)
 * to ~/.slermes/logs/security.log. Thread-safe append.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ================================================================
 *  Audit Log State
 * ================================================================ */

static FILE *g_audit_file = NULL;
static pthread_mutex_t g_audit_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_audit_initialized = false;

/* ================================================================
 *  Initialization
 * ================================================================ */

bool audit_init(const char *log_dir) {
    pthread_mutex_lock(&g_audit_mutex);

    if (g_audit_initialized) {
        pthread_mutex_unlock(&g_audit_mutex);
        return true;
    }

    /* Determine log path */
    char log_path[4096];
    if (log_dir && log_dir[0]) {
        snprintf(log_path, sizeof(log_path), "%s/security.log", log_dir);
    } else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(log_path, sizeof(log_path), "%s/.slermes/logs/security.log", home);

        /* Ensure logs directory exists */
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s/.slermes/logs", home);
        mkdir(dir, 0755);
    }

    g_audit_file = fopen(log_path, "a");
    if (!g_audit_file) {
        /* Try /tmp as fallback */
        g_audit_file = fopen("/tmp/hermes_security.log", "a");
        if (!g_audit_file) {
            pthread_mutex_unlock(&g_audit_mutex);
            return false;
        }
    }

    setbuf(g_audit_file, NULL); /* Unbuffered */
    g_audit_initialized = true;
    pthread_mutex_unlock(&g_audit_mutex);
    return true;
}

void audit_shutdown(void) {
    pthread_mutex_lock(&g_audit_mutex);
    if (g_audit_file) {
        fclose(g_audit_file);
        g_audit_file = NULL;
    }
    g_audit_initialized = false;
    pthread_mutex_unlock(&g_audit_mutex);
}

/* ================================================================
 *  Security Event Logging
 * ================================================================ */

void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail)
{
    /* Auto-init if needed */
    if (!g_audit_initialized) {
        audit_init(NULL);
    }

    pthread_mutex_lock(&g_audit_mutex);
    if (!g_audit_file) {
        pthread_mutex_unlock(&g_audit_mutex);
        return;
    }

    /* Get timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);

    /* Sanitize detail (remove newlines) */
    char detail_clean[4096];
    if (detail) {
        size_t j = 0;
        for (size_t i = 0; detail[i] && j < sizeof(detail_clean) - 1; i++) {
            if (detail[i] == '\n' || detail[i] == '\r')
                detail_clean[j++] = ' ';
            else
                detail_clean[j++] = detail[i];
        }
        detail_clean[j] = '\0';
    } else {
        detail_clean[0] = '\0';
    }

    fprintf(g_audit_file, "[%s] %s|%s|%s|%s|%s\n",
            ts,
            category ? category : "",
            action ? action : "",
            result ? result : "",
            reason ? reason : "",
            detail_clean);

    pthread_mutex_unlock(&g_audit_mutex);
}

/* ================================================================
 *  Convenience wrappers
 * ================================================================ */

void audit_log_approval(const char *tool, const char *command, bool approved) {
    audit_log_security("approval", tool,
                       approved ? "approved" : "denied",
                       "user_response", command);
}

void audit_log_redaction(const char *context, const char *pattern_matched) {
    audit_log_security("redaction", context, "redacted", pattern_matched, NULL);
}

void audit_log_violation(const char *rule, const char *detail) {
    audit_log_security("violation", rule, "blocked", detail, NULL);
}
