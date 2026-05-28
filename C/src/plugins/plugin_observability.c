/*
 * plugin_observability.c — Metrics & observability plugin (PLUGIN_OBSERVABILITY).
 *
 * Records events and maintains aggregated counters. Exposes metrics
 * via the obs_record_event / obs_get_metrics interface.
 *
 * Uses a simple in-memory ring buffer for recent events and
 * persistent JSON counters for aggregated metrics.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_observability.c -o plugin_observability.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ================================================================
 *  Constants
 * ================================================================ */

#define MAX_METRICS        128
#define METRIC_NAME_MAX    64
#define EVENT_RING_SIZE    64
#define EVENT_DATA_MAX     256

/* ================================================================
 *  Metric counter
 * ================================================================ */

typedef struct {
    char name[METRIC_NAME_MAX];
    long count;
    double sum;        /* for metrics that accumulate values */
    double last_value;
    long last_timestamp;
} metric_t;

static metric_t g_metrics[MAX_METRICS];
static int g_metric_count = 0;

/* ================================================================
 *  Event ring buffer
 * ================================================================ */

typedef struct {
    char name[METRIC_NAME_MAX];
    char data[EVENT_DATA_MAX];
    long timestamp;
} event_entry_t;

static event_entry_t g_event_ring[EVENT_RING_SIZE];
static int g_event_head = 0;
static int g_event_count = 0;
static int g_event_wraps = 0;

/* ================================================================
 *  State persistence path
 * ================================================================ */

static char g_state_path[4096];

static void ensure_dir(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
}

/* ================================================================
 *  Metric helpers
 * ================================================================ */

static metric_t *find_metric(const char *name) {
    for (int i = 0; i < g_metric_count; i++) {
        if (strcmp(g_metrics[i].name, name) == 0)
            return &g_metrics[i];
    }
    return NULL;
}

static metric_t *get_or_create_metric(const char *name) {
    metric_t *m = find_metric(name);
    if (m) return m;
    if (g_metric_count >= MAX_METRICS) return NULL;
    m = &g_metrics[g_metric_count++];
    snprintf(m->name, sizeof(m->name), "%s", name);
    m->count = 0;
    m->sum = 0;
    m->last_value = 0;
    m->last_timestamp = 0;
    return m;
}

static void record_event_to_ring(const char *name, const char *data) {
    event_entry_t *e = &g_event_ring[g_event_head];
    snprintf(e->name, sizeof(e->name), "%s", name ? name : "unknown");
    snprintf(e->data, sizeof(e->data), "%s", data ? data : "");
    e->timestamp = (long)time(NULL);
    g_event_head = (g_event_head + 1) % EVENT_RING_SIZE;
    if (g_event_count < EVENT_RING_SIZE) g_event_count++;
    else g_event_wraps++;
}

/* ================================================================
 *  State persistence
 * ================================================================ */

static void state_save(void) {
    ensure_dir(g_state_path);
    FILE *f = fopen(g_state_path, "w");
    if (!f) return;

    fprintf(f, "{\"metrics\":[");
    for (int i = 0; i < g_metric_count; i++) {
        if (i > 0) fputc(',', f);
        fprintf(f, "{\"name\":\"%s\",\"count\":%ld,\"sum\":%.1f,\"last_value\":%.1f,\"last_ts\":%ld}",
                g_metrics[i].name, g_metrics[i].count,
                g_metrics[i].sum, g_metrics[i].last_value,
                g_metrics[i].last_timestamp);
    }
    fprintf(f, "],\"event_count\":%d,\"event_wraps\":%d}", g_event_count, g_event_wraps);
    fclose(f);
}

static void state_load(void) {
    /* Simple in-memory init. Persistence reload is best-effort. */
    FILE *f = fopen(g_state_path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);

    /* Parse metrics array entries: {\"name\":\"...\",\"count\":...} */
    const char *p = buf;
    int parsed = 0;
    while ((p = strstr(p, "\"name\"")) && parsed < MAX_METRICS) {
        p += 7; /* skip "name" */
        while (*p && *p != ':') p++;
        if (*p != ':') break;
        p++;
        while (*p && *p != '"') p++;
        if (*p != '"') break;
        p++;
        const char *name_start = p;
        while (*p && *p != '"') p++;
        if (*p != '"') break;
        size_t name_len = (size_t)(p - name_start);
        if (name_len >= METRIC_NAME_MAX) name_len = METRIC_NAME_MAX - 1;

        metric_t *m = &g_metrics[parsed];
        memcpy(m->name, name_start, name_len);
        m->name[name_len] = '\0';

        /* Parse count */
        const char *c = strstr(p, "\"count\"");
        if (c) { c += 7; while (*c && *c != ':') c++; if (*c) m->count = atol(c + 1); }
        /* Parse sum */
        c = strstr(p, "\"sum\"");
        if (c) { c += 5; while (*c && *c != ':') c++; if (*c) m->sum = atof(c + 1); }
        /* Parse last_value */
        c = strstr(p, "\"last_value\"");
        if (c) { c += 12; while (*c && *c != ':') c++; if (*c) m->last_value = atof(c + 1); }
        /* Parse last_ts */
        c = strstr(p, "\"last_ts\"");
        if (c) { c += 9; while (*c && *c != ':') c++; if (*c) m->last_timestamp = atol(c + 1); }

        parsed++;
        p++; /* move past the closing brace */
    }
    g_metric_count = parsed;
    free(buf);
}

/* ================================================================
 *  Interface implementation
 * ================================================================ */

/* Record an event (PLUGIN_OBSERVABILITY interface) */
static void impl_record_event(const char *event_name, const char *data_json) {
    if (!event_name) return;

    record_event_to_ring(event_name, data_json);

    /* Update metric counter */
    metric_t *m = get_or_create_metric(event_name);
    if (m) {
        m->count++;
        m->last_timestamp = (long)time(NULL);
        if (data_json) {
            /* Try to extract a numeric value from data_json */
            const char *val_str = strstr(data_json, "\"value\"");
            if (val_str) {
                const char *colon = strchr(val_str, ':');
                if (colon) {
                    double v = atof(colon + 1);
                    m->sum += v;
                    m->last_value = v;
                }
            }
        }
    }

    /* Auto-save every 10 events (best-effort) */
    if (g_metric_count > 0 && (g_metrics[0].count % 10) == 0)
        state_save();
}

/* Get metrics as JSON (PLUGIN_OBSERVABILITY interface) */
static char *impl_get_metrics(const char *filter_json) {
    (void)filter_json;

    /* Build metrics JSON */
    char buf[8192];
    int pos = snprintf(buf, sizeof(buf),
        "{\"metric_count\":%d,\"event_count\":%d,\"event_wraps\":%d,\"metrics\":[",
        g_metric_count, g_event_count, g_event_wraps);

    for (int i = 0; i < g_metric_count; i++) {
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"name\":\"%s\",\"count\":%ld,\"sum\":%.1f,\"avg\":%.1f,\"last_value\":%.1f}",
            g_metrics[i].name, g_metrics[i].count,
            g_metrics[i].sum,
            g_metrics[i].count > 0 ? g_metrics[i].sum / g_metrics[i].count : 0.0,
            g_metrics[i].last_value);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }

    /* Append recent events */
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "],\"recent_events\":[");
    pos = (int)strlen(buf);

    int start = g_event_count < EVENT_RING_SIZE ? 0 : g_event_head;
    int count = g_event_count < EVENT_RING_SIZE ? g_event_count : EVENT_RING_SIZE;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % EVENT_RING_SIZE;
        if (i > 0) {
            int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos, ",");
            if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
            pos += r;
        }
        int r = snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"name\":\"%s\",\"data\":\"%s\",\"ts\":%ld}",
            g_event_ring[idx].name, g_event_ring[idx].data,
            g_event_ring[idx].timestamp);
        if (r < 0 || (size_t)r >= sizeof(buf) - (size_t)pos) break;
        pos += r;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "]}");

    return strdup(buf);
}

/* ================================================================
 *  Tool interface (optional — get_metrics as tool)
 * ================================================================ */

/* Tool 0: get_observability_metrics */
static char *tool_get_metrics(const char *args_json) {
    return impl_get_metrics(args_json);
}

/* Tool 1: record_event */
static char *tool_record_event(const char *args_json) {
    if (!args_json) return strdup("{\"error\":\"no args\"}");
    /* Parse event_name and data from JSON args */
    const char *name = NULL;
    char name_buf[256] = {0};
    const char *n = strstr(args_json, "\"event_name\"");
    if (n) {
        n += 12; while (*n && *n != ':') n++; if (*n) n++;
        while (*n && *n != '"') n++; if (*n) n++;
        const char *end = strchr(n, '"');
        if (end) {
            size_t len = (size_t)(end - n);
            if (len > 0 && len < sizeof(name_buf)) {
                memcpy(name_buf, n, len);
                name = name_buf;
            }
        }
    }
    impl_record_event(name, args_json);
    return strdup("{\"status\":\"recorded\"}");
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
    return "observability";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "observability";
}

const char *plugin_meta_description(void) {
    return "Metrics and observability — record events, track aggregated counters, query metrics";
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
    g_interface.type = PLUGIN_OBSERVABILITY;

    /* Set observability interface functions */
    g_interface.obs_record_event = impl_record_event;
    g_interface.obs_get_metrics = impl_get_metrics;

    /* Determine state file path */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        snprintf(g_state_path, sizeof(g_state_path),
                 "%s/.hermes/plugins/observability/state.json", home);
    } else {
        snprintf(g_state_path, sizeof(g_state_path),
                 "/tmp/.hermes/plugins/observability/state.json");
    }

    state_load();
    return 0;
}

int plugin_cleanup(void) {
    state_save();
    memset(&g_interface, 0, sizeof(g_interface));
    return 0;
}
