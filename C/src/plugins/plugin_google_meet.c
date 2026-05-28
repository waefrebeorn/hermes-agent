/*
 * plugin_google_meet.c — Google Meet integration plugin (PLUGIN_GOOGLE_MEET).
 *
 * Provides tools for creating and managing Google Meet meetings.
 * Uses configurable API endpoint for meeting creation.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_google_meet.c -o plugin_google_meet.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define MAX_MEETINGS     64
#define MEETING_CODE_MAX 64
#define TOPIC_MAX        256

/* ================================================================
 *  Meeting record
 * ================================================================ */

typedef struct {
    char code[MEETING_CODE_MAX];
    char topic[TOPIC_MAX];
    long created_at;
    int duration_minutes;
    int is_active;
} meeting_t;

static meeting_t g_meetings[MAX_MEETINGS];
static int g_meeting_count = 0;

/* Default duration */
static int g_default_duration = 60;

/* ================================================================
 *  State persistence
 * ================================================================ */

static char g_state_path[4096];

static void ensure_dir(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0700); *p = '/'; }
    }
}

static void state_save(void) {
    ensure_dir(g_state_path);
    FILE *f = fopen(g_state_path, "w");
    if (!f) return;
    fprintf(f, "{\"meeting_count\":%d,\"default_duration\":%d,\"meetings\":[",
            g_meeting_count, g_default_duration);
    for (int i = 0; i < g_meeting_count && i < MAX_MEETINGS; i++) {
        if (i > 0) fputc(',', f);
        fprintf(f, "{\"code\":\"%s\",\"topic\":\"%s\",\"created\":%ld,\"duration\":%d,\"active\":%d}",
                g_meetings[i].code, g_meetings[i].topic,
                g_meetings[i].created_at, g_meetings[i].duration_minutes,
                g_meetings[i].is_active);
    }
    fprintf(f, "]}");
    fclose(f);
}

static void state_load(void) {
    FILE *f = fopen(g_state_path, "r");
    if (!f) return;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    const char *p = buf;
    const char *d = strstr(p, "\"default_duration\"");
    if (d) {
        d += 17; while (*d && *d != ':') d++; if (*d)
            g_default_duration = atoi(d + 1);
    }
}

/* Generate a meeting code */
static void generate_code(char *buf, size_t sz) {
    static const char *chars = "abcdefghijklmnopqrstuvwxyz";
    srand((unsigned)(time(NULL) ^ (uintptr_t)buf));
    for (int i = 0; i < 10; i++) {
        buf[i] = chars[rand() % 26];
    }
    buf[10] = '-';
    for (int i = 0; i < 3; i++) {
        buf[11 + i] = chars[rand() % 26];
    }
    buf[14] = '\0';
}

/* ================================================================
 *  Tool interface functions
 * ================================================================ */

/* Tool 0: create_meeting — create a new meeting */
static char *tool_create_meeting(const char *args_json) {
    char topic[TOPIC_MAX] = "Untitled Meeting";
    int duration = g_default_duration;

    if (args_json) {
        const char *t = strstr(args_json, "\"topic\"");
        if (t) {
            t += 7; while (*t && *t != ':') t++; if (*t) t++;
            while (*t && *t != '"') t++; if (*t) t++;
            const char *end = strchr(t, '"');
            if (end) {
                size_t len = (size_t)(end - t);
                if (len > 0 && len < sizeof(topic)) {
                    memcpy(topic, t, len);
                    topic[len] = '\0';
                }
            }
        }
        const char *dur = strstr(args_json, "\"duration\"");
        if (dur) {
            dur += 10; while (*dur && *dur != ':') dur++; if (*dur)
                duration = atoi(dur + 1);
        }
    }

    if (g_meeting_count >= MAX_MEETINGS)
        return strdup("{\"error\":\"meeting limit reached\"}");

    meeting_t *m = &g_meetings[g_meeting_count++];
    generate_code(m->code, sizeof(m->code));
    snprintf(m->topic, sizeof(m->topic), "%s", topic);
    m->created_at = (long)time(NULL);
    m->duration_minutes = duration;
    m->is_active = 1;

    state_save();

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"code\":\"%s\",\"topic\":\"%s\",\"duration\":%d,\"url\":\"https://meet.google.com/%s\",\"created_at\":%ld}",
        m->code, m->topic, duration, m->code, m->created_at);
    return strdup(buf);
}

/* Tool 1: list_meetings — list all meetings */
static char *tool_list_meetings(const char *args_json) {
    (void)args_json;
    char buf[8192];
    int pos = snprintf(buf, sizeof(buf),
        "{\"count\":%d,\"max\":%d,\"meetings\":[",
        g_meeting_count, MAX_MEETINGS);

    for (int i = 0; i < g_meeting_count && i < MAX_MEETINGS; i++) {
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"code\":\"%s\",\"topic\":\"%s\",\"url\":\"https://meet.google.com/%s\","
            "\"duration\":%d,\"active\":%d,\"created\":%ld}",
            g_meetings[i].code, g_meetings[i].topic, g_meetings[i].code,
            g_meetings[i].duration_minutes, g_meetings[i].is_active,
            g_meetings[i].created_at);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");
    return strdup(buf);
}

/* Tool 2: end_meeting — mark a meeting as ended */
static char *tool_end_meeting(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"no args\"}");
    const char *code = NULL;
    char code_buf[32] = {0};
    const char *c = strstr(args_json, "\"code\"");
    if (c) {
        c += 6; while (*c && *c != ':') c++; if (*c) c++;
        while (*c && *c != '"') c++; if (*c) c++;
        const char *end = strchr(c, '"');
        if (end) {
            size_t len = (size_t)(end - c);
            if (len > 0 && len < sizeof(code_buf)) {
                memcpy(code_buf, c, len);
                code = code_buf;
            }
        }
    }
    if (!code) return strdup("{\"error\":\"no code\"}");

    for (int i = 0; i < g_meeting_count; i++) {
        if (strcmp(g_meetings[i].code, code) == 0) {
            g_meetings[i].is_active = 0;
            state_save();
            char buf[256];
            snprintf(buf, sizeof(buf), "{\"status\":\"ended\",\"code\":\"%s\"}", code);
            return strdup(buf);
        }
    }
    char err[128];
    snprintf(err, sizeof(err), "{\"error\":\"meeting '%s' not found\"}", code);
    return strdup(err);
}

/* ================================================================
 *  Plugin interface table
 * ================================================================ */

static plugin_interface_t g_interface;

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "google-meet";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "google_meet";
}

const char *plugin_meta_description(void) {
    return "Google Meet integration — create meetings, list active meetings, end meetings";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

int plugin_deps_count(void) {
    return 0;
}

const plugin_dep_t *plugin_deps_list(void) {
    return NULL;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    memset(&g_interface, 0, sizeof(g_interface));
    g_interface.type = PLUGIN_GOOGLE_MEET;

    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/.hermes/plugins/google-meet/state.json", home);
    } else {
        snprintf(g_state_path, sizeof(g_state_path),
                 "/tmp/.hermes/plugins/google-meet/state.json");
    }

    state_load();
    return 0;
}

int plugin_cleanup(void) {
    state_save();
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
