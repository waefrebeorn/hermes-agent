/*
 * logger.c — Structured logger for C Hermes agent.
 *
 * Writes agent.log and errors.log in Python-compatible format:
 *   2026-05-26 12:34:56,789 LEVEL module: message
 *
 * Thread-safe via mutex; lazy-init on first log call.
 */

#include "hermes_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <errno.h>

/* ──────────────────────────────────────────────
 *  Static state
 * ────────────────────────────────────────────── */

static FILE *g_log_fp       = NULL;   /* agent.log (INFO+)       */
static FILE *g_err_fp       = NULL;   /* errors.log (WARNING+)   */
static bool  g_log_inited   = false;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

/* Per-turn session context for log tagging (L14) */
static char  g_ctx_session[64]  = "";
static char  g_ctx_model[64]    = "";
static char  g_ctx_provider[64] = "";
static int   g_ctx_iteration    = -1;

/* Max file size in bytes — rotate when exceeded */
#define LOG_MAX_BYTES  (10 * 1024 * 1024)   /* 10 MB */

/* ──────────────────────────────────────────────
 *  Helpers
 * ────────────────────────────────────────────── */

const char *hermes_log_level_name(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:    return "DEBUG";
        case LOG_INFO:     return "INFO";
        case LOG_WARNING:  return "WARNING";
        case LOG_ERROR:    return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default:           return "LVL?";
    }
}

/* Current datetime in Python log format "YYYY-MM-DD HH:MM:SS,mmm" */
static void format_timestamp(char *buf, size_t sz) {
    struct timeval tv;
    struct tm      tm;
    time_t         now;

    gettimeofday(&tv, NULL);
    now = tv.tv_sec;
    localtime_r(&now, &tm);

    snprintf(buf, sz,
             "%04d-%02d-%02d %02d:%02d:%02d,%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             (int)(tv.tv_usec / 1000));
}

/* Rotate a log file if it exceeds LOG_MAX_BYTES */
static void rotate_if_needed(FILE **fpp, const char *path) {
    if (!path) return;
    if (ftell(*fpp) < LOG_MAX_BYTES) return;

    fclose(*fpp);
    *fpp = NULL;

    /* Rename agent.log → agent.log.1, errors.log → errors.log.1 */
    char old_path[1024];
    snprintf(old_path, sizeof(old_path), "%s.1", path);
    rename(path, old_path);

    *fpp = fopen(path, "a");
}

static void ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0755);
    }
}

/* Open a log file; returns NULL on failure */
static FILE *open_log(const char *path) {
    FILE *fp = fopen(path, "a");
    if (fp) setbuf(fp, NULL);  /* unbuffered — logs appear immediately */
    return fp;
}

/* ──────────────────────────────────────────────
 *  Public API
 * ────────────────────────────────────────────── */

void hermes_log_init(void) {
    if (g_log_inited) return;

    pthread_mutex_lock(&g_log_lock);

    if (g_log_inited) { pthread_mutex_unlock(&g_log_lock); return; }

    /* Resolve log directory */
    const char *home_env = getenv("SLERMES_HOME");
    if (!home_env) home_env = getenv("HERMES_HOME");
    if (!home_env) home_env = getenv("HOME");
    if (!home_env) {
        pthread_mutex_unlock(&g_log_lock);
        return;   /* No home directory — can't log to file */
    }

    char log_dir[1024];
    snprintf(log_dir, sizeof(log_dir), "%s/logs", home_env);
    ensure_dir(log_dir);

    char agent_path[1024];
    char errors_path[1024];
    snprintf(agent_path, sizeof(agent_path), "%s/agent.log", log_dir);
    snprintf(errors_path, sizeof(errors_path), "%s/errors.log", log_dir);

    g_log_fp = open_log(agent_path);
    g_err_fp = open_log(errors_path);

    g_log_inited = true;
    pthread_mutex_unlock(&g_log_lock);
}

void hermes_log(log_level_t level, const char *module, const char *fmt, ...) {
    /* Lazy init on first use */
    if (!g_log_inited) hermes_log_init();

    char   ts[64];
    char   msg[4096];
    va_list args;

    format_timestamp(ts, sizeof(ts));

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    const char *lvl_name = hermes_log_level_name(level);

    pthread_mutex_lock(&g_log_lock);

    /* Write to agent.log for INFO+ */
    if (g_log_fp && level >= LOG_INFO) {
        fprintf(g_log_fp, "%s %-7s %s: %s", ts, lvl_name, module, msg);
        /* Append per-turn context if set */
        if (g_ctx_session[0] || g_ctx_model[0])
            fprintf(g_log_fp, " [session:%s model:%s iter:%d]",
                    g_ctx_session, g_ctx_model, g_ctx_iteration);
        fprintf(g_log_fp, "\n");
        rotate_if_needed(&g_log_fp, NULL);
    }

    /* Write to errors.log for WARNING+ */
    if (g_err_fp && level >= LOG_WARNING) {
        fprintf(g_err_fp, "%s %-7s %s: %s", ts, lvl_name, module, msg);
        if (g_ctx_session[0] || g_ctx_model[0])
            fprintf(g_err_fp, " [session:%s model:%s iter:%d]",
                    g_ctx_session, g_ctx_model, g_ctx_iteration);
        fprintf(g_err_fp, "\n");
        rotate_if_needed(&g_err_fp, NULL);
    }

    pthread_mutex_unlock(&g_log_lock);
}

void hermes_log_set_context(const char *session_id, const char *model,
                             const char *provider, int iteration) {
    pthread_mutex_lock(&g_log_lock);
    if (session_id) {
        strncpy(g_ctx_session, session_id, sizeof(g_ctx_session) - 1);
        g_ctx_session[sizeof(g_ctx_session) - 1] = '\0';
    }
    if (model) {
        strncpy(g_ctx_model, model, sizeof(g_ctx_model) - 1);
        g_ctx_model[sizeof(g_ctx_model) - 1] = '\0';
    }
    if (provider) {
        strncpy(g_ctx_provider, provider, sizeof(g_ctx_provider) - 1);
        g_ctx_provider[sizeof(g_ctx_provider) - 1] = '\0';
    }
    if (iteration >= 0)
        g_ctx_iteration = iteration;
    pthread_mutex_unlock(&g_log_lock);
}

void hermes_log_shutdown(void) {
    pthread_mutex_lock(&g_log_lock);
    if (g_log_fp) { fclose(g_log_fp); g_log_fp = NULL; }
    if (g_err_fp) { fclose(g_err_fp); g_err_fp = NULL; }
    g_log_inited = false;
    pthread_mutex_unlock(&g_log_lock);
}
