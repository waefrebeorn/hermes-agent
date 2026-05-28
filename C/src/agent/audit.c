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
#include <sys/stat.h>
#include <unistd.h>

/* ================================================================
 *  Audit Log State
 * ================================================================ */

static FILE *g_audit_file = NULL;
static pthread_mutex_t g_audit_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_audit_initialized = false;

/* O12: Rotation state */
static char g_audit_path[4096] = {0};       /* current log path */
static size_t g_audit_max_size = 0;         /* 0 = no rotation */
static int    g_audit_max_files = 0;        /* 0 = no rotation */
static int    g_audit_max_age_days = 0;     /* 0 = no expiry */

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

    /* Store path for rotation */
    snprintf(g_audit_path, sizeof(g_audit_path), "%s", log_path);

    g_audit_file = fopen(log_path, "a");
    if (!g_audit_file) {
        /* Try /tmp as fallback */
        g_audit_file = fopen("/tmp/hermes_security.log", "a");
        if (!g_audit_file) {
            g_audit_path[0] = '\0';
            pthread_mutex_unlock(&g_audit_mutex);
            return false;
        }
        snprintf(g_audit_path, sizeof(g_audit_path), "/tmp/hermes_security.log");
    }

    setbuf(g_audit_file, NULL); /* Unbuffered */
    g_audit_initialized = true;
    pthread_mutex_unlock(&g_audit_mutex);
    return true;
}

/* O12: Set rotation parameters */
void audit_set_rotation(size_t max_size_kb, int max_files, int max_age_days) {
    pthread_mutex_lock(&g_audit_mutex);
    g_audit_max_size = max_size_kb * 1024;
    g_audit_max_files = max_files;
    g_audit_max_age_days = max_age_days;
    pthread_mutex_unlock(&g_audit_mutex);
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
 *  Rotation
 * ================================================================ */

/* Check if current log exceeds max_size, rotate if so.
 * Also expire old rotated logs past max_age_days.
 * Must be called with g_audit_mutex held. */
static void audit_check_rotate(void) {
    if (!g_audit_file || !g_audit_path[0]) return;
    if (g_audit_max_size == 0 || g_audit_max_files == 0) return;

    /* Check current file size */
    struct stat st;
    if (stat(g_audit_path, &st) != 0) return;
    if ((size_t)st.st_size < g_audit_max_size) return;

    /* Rotate: shift .N.log → .N+1.log, then .log → .1.log */
    /* Close current file first */
    fclose(g_audit_file);
    g_audit_file = NULL;

    /* Shift existing rotated files */
    for (int i = g_audit_max_files - 1; i >= 1; i--) {
        char old_name[4160], new_name[4160];
        snprintf(old_name, sizeof(old_name), "%s.%d.log", g_audit_path, i);
        snprintf(new_name, sizeof(new_name), "%s.%d.log", g_audit_path, i + 1);
        rename(old_name, new_name);
    }

    /* Rename current → .1.log */
    {
        char rotated[4160];
        snprintf(rotated, sizeof(rotated), "%s.1.log", g_audit_path);
        rename(g_audit_path, rotated);
    }

    /* Open new log */
    g_audit_file = fopen(g_audit_path, "a");
    if (g_audit_file) setbuf(g_audit_file, NULL);

    /* Expire logs past max_age_days */
    if (g_audit_max_age_days > 0) {
        time_t now = time(NULL);
        for (int i = 1; i <= g_audit_max_files + 2; i++) {
            char path[4160];
            snprintf(path, sizeof(path), "%s.%d.log", g_audit_path, i);
            if (stat(path, &st) == 0) {
                if (now - st.st_mtime > (time_t)g_audit_max_age_days * 86400)
                    unlink(path);
            }
            /* Also check .gz variants */
            snprintf(path, sizeof(path), "%s.%d.log.gz", g_audit_path, i);
            if (stat(path, &st) == 0) {
                if (now - st.st_mtime > (time_t)g_audit_max_age_days * 86400)
                    unlink(path);
            }
        }
    }
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

    /* O12: Check if rotation needed before writing */
    audit_check_rotate();
    if (!g_audit_file) {  /* rotation may have failed to reopen */
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
