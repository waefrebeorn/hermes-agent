/*
 * debug_helpers.c — Per-tool debug session logging.
 * Port of Python tools/debug_helpers.py.
 *
 * Each tool can create a DebugSession activated by a tool-specific
 * environment variable. When enabled, tool calls are logged to a
 * JSON file in the logs directory.
 */

#include "debug_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ─── UUID generation (simple) ──────────────────────────── */

static void generate_uuid(char *buf, size_t buf_size)
{
    /* Simple UUID v4-like: 8-4-4-4-12 hex digits */
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    srand(seed);

    const char *hex = "0123456789abcdef";
    int pos = 0;
    for (int i = 0; i < 36 && pos < (int)buf_size - 1; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buf[pos++] = '-';
        } else if (i == 14) {
            buf[pos++] = '4';  /* Version 4 */
        } else if (i == 19) {
            buf[pos++] = hex[8 + (rand() % 4)];  /* Variant */
        } else {
            buf[pos++] = hex[rand() % 16];
        }
    }
    buf[pos] = '\0';
}

/* ─── ISO-8601 timestamp ────────────────────────────────── */

static void get_timestamp(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm) {
        strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", tm);
    } else {
        snprintf(buf, buf_size, "unknown");
    }
}

/* ─── Debug Session ─────────────────────────────────────── */

void debug_session_init(debug_session_t *session,
                         const char *tool_name,
                         const char *env_var,
                         const char *hermes_home)
{
    if (!session) return;

    memset(session, 0, sizeof(*session));

    if (tool_name)
        strncpy(session->tool_name, tool_name, sizeof(session->tool_name) - 1);

    /* Check env var */
    if (env_var) {
        const char *val = getenv(env_var);
        session->enabled = (val && (strcasecmp(val, "true") == 0
                                   || strcmp(val, "1") == 0));
    }

    if (session->enabled) {
        generate_uuid(session->session_id, sizeof(session->session_id));
        get_timestamp(session->start_time, sizeof(session->start_time));

        if (hermes_home) {
            snprintf(session->log_dir, sizeof(session->log_dir),
                     "%s/logs", hermes_home);
            mkdir(session->log_dir, 0755);
        }
    }
}

bool debug_session_active(const debug_session_t *session)
{
    return session && session->enabled;
}

void debug_session_log_call(debug_session_t *session,
                             const char *call_name,
                             const char *call_data)
{
    if (!session || !session->enabled) return;
    if (session->call_count >= DEBUG_MAX_CALLS) return;

    debug_call_t *call = &session->calls[session->call_count];
    get_timestamp(call->timestamp, sizeof(call->timestamp));

    if (call_name)
        strncpy(call->tool_name, call_name, sizeof(call->tool_name) - 1);
    if (call_data)
        strncpy(call->call_data, call_data, sizeof(call->call_data) - 1);

    session->call_count++;
}

void debug_session_save(debug_session_t *session,
                         const char *hermes_home)
{
    if (!session || !session->enabled) return;

    char log_path[1024];
    if (hermes_home) {
        snprintf(log_path, sizeof(log_path),
                 "%s/logs/%s_debug_%s.json",
                 hermes_home, session->tool_name, session->session_id);
    } else if (session->log_dir[0]) {
        snprintf(log_path, sizeof(log_path),
                 "%s/%s_debug_%s.json",
                 session->log_dir, session->tool_name, session->session_id);
    } else {
        return;
    }

    FILE *f = fopen(log_path, "w");
    if (!f) return;

    char end_time[64];
    get_timestamp(end_time, sizeof(end_time));

    fprintf(f, "{\n");
    fprintf(f, "  \"session_id\": \"%s\",\n", session->session_id);
    fprintf(f, "  \"tool_name\": \"%s\",\n", session->tool_name);
    fprintf(f, "  \"start_time\": \"%s\",\n", session->start_time);
    fprintf(f, "  \"end_time\": \"%s\",\n", end_time);
    fprintf(f, "  \"debug_enabled\": true,\n");
    fprintf(f, "  \"total_calls\": %d,\n", session->call_count);
    fprintf(f, "  \"tool_calls\": [\n");

    for (int i = 0; i < session->call_count; i++) {
        debug_call_t *call = &session->calls[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"timestamp\": \"%s\",\n", call->timestamp);
        fprintf(f, "      \"tool_name\": \"%s\",\n", call->tool_name);

        /* Escape JSON string values */
        fprintf(f, "      \"call_data\": ");
        if (call->call_data[0]) {
            /* If call_data looks like JSON (starts with { or [), embed directly */
            if (call->call_data[0] == '{' || call->call_data[0] == '[') {
                fprintf(f, "%s", call->call_data);
            } else {
                fprintf(f, "\"%s\"", call->call_data);
            }
        } else {
            fprintf(f, "null");
        }
        fprintf(f, "\n    }%s\n", (i < session->call_count - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
}

char *debug_session_get_info(const debug_session_t *session)
{
    if (!session || !session->enabled) return NULL;

    char log_path[1024];
    if (session->log_dir[0]) {
        snprintf(log_path, sizeof(log_path),
                 "%s/%s_debug_%s.json",
                 session->log_dir, session->tool_name, session->session_id);
    } else {
        snprintf(log_path, sizeof(log_path),
                 "%s_debug_%s.json",
                 session->tool_name, session->session_id);
    }

    size_t needed = snprintf(NULL, 0,
        "{"
        "\"enabled\":true,"
        "\"session_id\":\"%s\","
        "\"log_path\":\"%s\","
        "\"total_calls\":%d"
        "}",
        session->session_id, log_path, session->call_count);

    char *json = (char *)malloc(needed + 1);
    if (!json) return NULL;

    sprintf(json,
        "{"
        "\"enabled\":true,"
        "\"session_id\":\"%s\","
        "\"log_path\":\"%s\","
        "\"total_calls\":%d"
        "}",
        session->session_id, log_path, session->call_count);

    return json;
}
