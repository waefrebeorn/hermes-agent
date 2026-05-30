/**
 * @file hook_registry.c
 * @brief P186: Hook registry implementation.
 *
 * Lightweight event → callback dispatch system.
 * Thread-safe via single mutex.
 *
 * Events are stored in a simple array (no hash table needed for < 32 events).
 * Each event maps to a fixed-size callback array.
 */
#include "hermes_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ── Internal types ─────────────────────────────────────────────── */

typedef struct {
    char             event[HOOK_EVENT_NAME_MAX];
    hook_callback_t  cb;
    void            *userdata;
} hook_entry_t;

typedef struct {
    char            name[HOOK_EVENT_NAME_MAX];
    hook_entry_t    cbs[HOOK_MAX_CBS];
    int             count;
} hook_event_t;

/* ── Global state ───────────────────────────────────────────────── */

static hook_event_t g_events[HOOK_MAX_EVENTS];
static int g_event_count = 0;
static pthread_mutex_t g_hook_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Internal helpers ───────────────────────────────────────────── */

static hook_event_t *find_event(const char *event) {
    for (int i = 0; i < g_event_count; i++) {
        if (strcmp(g_events[i].name, event) == 0)
            return &g_events[i];
    }
    return NULL;
}

static hook_event_t *find_or_create_event(const char *event) {
    hook_event_t *ev = find_event(event);
    if (ev) return ev;
    if (g_event_count >= HOOK_MAX_EVENTS) return NULL;
    ev = &g_events[g_event_count++];
    snprintf(ev->name, sizeof(ev->name), "%s", event);
    ev->count = 0;
    return ev;
}

/* ── Public API ─────────────────────────────────────────────────── */

bool hook_register(const char *event, hook_callback_t cb, void *userdata) {
    if (!event || !cb) return false;

    pthread_mutex_lock(&g_hook_mutex);

    hook_event_t *ev = find_or_create_event(event);
    if (!ev) { pthread_mutex_unlock(&g_hook_mutex); return false; }

    /* Check for duplicates */
    for (int i = 0; i < ev->count; i++) {
        if (ev->cbs[i].cb == cb && ev->cbs[i].userdata == userdata) {
            pthread_mutex_unlock(&g_hook_mutex);
            return true;  /* already registered, idempotent */
        }
    }

    if (ev->count >= HOOK_MAX_CBS) {
        pthread_mutex_unlock(&g_hook_mutex);
        return false;
    }

    snprintf(ev->cbs[ev->count].event, sizeof(ev->cbs[ev->count].event), "%s", event);
    ev->cbs[ev->count].cb = cb;
    ev->cbs[ev->count].userdata = userdata;
    ev->count++;

    pthread_mutex_unlock(&g_hook_mutex);
    return true;
}

bool hook_unregister(const char *event, hook_callback_t cb, void *userdata) {
    if (!event || !cb) return false;

    pthread_mutex_lock(&g_hook_mutex);

    hook_event_t *ev = find_event(event);
    if (!ev) { pthread_mutex_unlock(&g_hook_mutex); return false; }

    for (int i = 0; i < ev->count; i++) {
        if (ev->cbs[i].cb == cb && ev->cbs[i].userdata == userdata) {
            /* Shift remaining entries */
            for (int j = i; j < ev->count - 1; j++)
                ev->cbs[j] = ev->cbs[j + 1];
            ev->count--;
            pthread_mutex_unlock(&g_hook_mutex);
            return true;
        }
    }

    pthread_mutex_unlock(&g_hook_mutex);
    return false;
}

char *hook_invoke(const char *event, const char *payload_json) {
    if (!event) return NULL;
    if (!payload_json) payload_json = "{}";

    pthread_mutex_lock(&g_hook_mutex);

    hook_event_t *ev = find_event(event);
    if (!ev || ev->count == 0) {
        pthread_mutex_unlock(&g_hook_mutex);
        return NULL;
    }

    /* Collect results as JSON array */
    char results_buffer[8192];
    size_t pos = 0;
    int result_count = 0;

    for (int i = 0; i < ev->count; i++) {
        /* Unlock while calling callback (it may re-enter) */
        hook_callback_t cb = ev->cbs[i].cb;
        void *ud = ev->cbs[i].userdata;
        pthread_mutex_unlock(&g_hook_mutex);

        char *res = cb(event, payload_json, ud);

        pthread_mutex_lock(&g_hook_mutex);

        /* Re-find event after releasing lock (may have changed) */
        ev = find_event(event);
        if (!ev) {
            free(res);
            pthread_mutex_unlock(&g_hook_mutex);
            if (result_count > 0) return strdup(results_buffer);
            return NULL;
        }

        if (res) {
            if (result_count == 0) {
                pos += snprintf(results_buffer + pos, sizeof(results_buffer) - pos, "[");
            } else {
                pos += snprintf(results_buffer + pos, sizeof(results_buffer) - pos, ",");
            }
            if (pos < sizeof(results_buffer)) {
                pos += snprintf(results_buffer + pos, sizeof(results_buffer) - pos, "%s", res);
            }
            result_count++;
            free(res);
        }
    }

    pthread_mutex_unlock(&g_hook_mutex);

    if (result_count == 0) return NULL;

    size_t flen = strlen(results_buffer);
    if (flen + 2 < sizeof(results_buffer)) {
        results_buffer[flen] = ']';
        results_buffer[flen + 1] = '\0';
    }
    return strdup(results_buffer);
}

bool hook_has_callbacks(const char *event) {
    if (!event) return false;
    pthread_mutex_lock(&g_hook_mutex);
    hook_event_t *ev = find_event(event);
    bool has = (ev && ev->count > 0);
    pthread_mutex_unlock(&g_hook_mutex);
    return has;
}

int hook_event_count(void) {
    pthread_mutex_lock(&g_hook_mutex);
    int count = g_event_count;
    pthread_mutex_unlock(&g_hook_mutex);
    return count;
}

void hook_reset_all(void) {
    pthread_mutex_lock(&g_hook_mutex);
    for (int i = 0; i < g_event_count; i++)
        g_events[i].count = 0;
    g_event_count = 0;
    pthread_mutex_unlock(&g_hook_mutex);
}

/* ── Result parsing ─────────────────────────────────────────────── */

hook_result_t hook_parse_result(const char *stdout_json) {
    hook_result_t r = {HOOK_DECISION_ALLOW, ""};
    if (!stdout_json || !stdout_json[0]) return r;

    /* Simple string-based parsing (no JSON dep needed for this) */
    if (strstr(stdout_json, "\"decision\":\"block\"") ||
        strstr(stdout_json, "\"action\":\"block\"")) {
        r.decision = HOOK_DECISION_BLOCK;

        /* Extract reason/message */
        const char *reason = strstr(stdout_json, "\"reason\":\"");
        const char *message = strstr(stdout_json, "\"message\":\"");
        const char *src = message ? message : reason;
        if (src) {
            src += strlen("\"message\":\"");
            if (message && !reason) src = message + 11;
            else if (!message && reason) src = reason + 10;
            else if (message && reason) src = message + 11;
            size_t slen = 0;
            const char *end = strchr(src, '"');
            if (end) slen = (size_t)(end - src);
            if (slen > 0 && slen < sizeof(r.message))
                memcpy(r.message, src, slen);
        }
        if (!r.message[0])
            snprintf(r.message, sizeof(r.message), "Blocked by shell hook.");
    }

    if (strstr(stdout_json, "\"context\":\"")) {
        if (r.decision == HOOK_DECISION_ALLOW)
            r.decision = HOOK_DECISION_CONTEXT;
    }

    return r;
}
