/*
 * slash_confirm.c — Generic slash-command confirmation primitive (gateway-side).
 * Port of Python tools/slash_confirm.py.
 *
 * Manages pending slash-command confirmations keyed by gateway session_key.
 * Thread-safe with a mutex. Supports timeout-based staleness clearing.
 */

#include "slash_confirm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ── Static state ── */

static slashconfirm_entry_t g_pending[SLASHCONFIRM_MAX_SESSIONS];
static int g_pending_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Internal helpers ── */

static int _find_session(const char *session_key)
{
    for (int i = 0; i < g_pending_count; i++) {
        if (strcmp(g_pending[i].session_key, session_key) == 0)
            return i;
    }
    return -1;
}

static void _remove_at(int idx)
{
    if (idx < 0 || idx >= g_pending_count) return;
    /* Free strings in the entry */
    /* (session_key, confirm_id, command are inline char arrays — no heap alloc) */
    /* Shift remaining entries */
    if (idx < g_pending_count - 1) {
        memmove(&g_pending[idx], &g_pending[idx + 1],
                (g_pending_count - idx - 1) * sizeof(slashconfirm_entry_t));
    }
    g_pending_count--;
}

/* ── Public API ── */

void slashconfirm_register(const char *session_key,
                           const char *confirm_id,
                           const char *command,
                           slashconfirm_handler_t handler)
{
    if (!session_key || !confirm_id || !command) return;

    pthread_mutex_lock(&g_lock);

    /* Overwrite existing entry for same session */
    int idx = _find_session(session_key);
    if (idx >= 0) {
        _remove_at(idx);
    }

    /* Add new entry (or append if we're full) */
    if (g_pending_count >= SLASHCONFIRM_MAX_SESSIONS) {
        /* Replace the oldest entry */
        /* Find oldest by created_at */
        int oldest = 0;
        for (int i = 1; i < g_pending_count; i++) {
            if (g_pending[i].created_at < g_pending[oldest].created_at)
                oldest = i;
        }
        idx = oldest;
    } else {
        idx = g_pending_count;
        g_pending_count++;
    }

    strncpy(g_pending[idx].session_key, session_key,
            SLASHCONFIRM_MAX_KEY - 1);
    g_pending[idx].session_key[SLASHCONFIRM_MAX_KEY - 1] = '\0';

    strncpy(g_pending[idx].confirm_id, confirm_id,
            SLASHCONFIRM_MAX_CONFIRM_ID - 1);
    g_pending[idx].confirm_id[SLASHCONFIRM_MAX_CONFIRM_ID - 1] = '\0';

    strncpy(g_pending[idx].command, command,
            SLASHCONFIRM_MAX_COMMAND - 1);
    g_pending[idx].command[SLASHCONFIRM_MAX_COMMAND - 1] = '\0';

    g_pending[idx].handler = handler;
    g_pending[idx].created_at = (double)time(NULL);

    pthread_mutex_unlock(&g_lock);
}

slashconfirm_entry_t *slashconfirm_get_pending(const char *session_key)
{
    if (!session_key) return NULL;

    pthread_mutex_lock(&g_lock);

    int idx = _find_session(session_key);
    if (idx < 0) {
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    slashconfirm_entry_t *entry = malloc(sizeof(slashconfirm_entry_t));
    if (entry) {
        memcpy(entry, &g_pending[idx], sizeof(slashconfirm_entry_t));
    }

    pthread_mutex_unlock(&g_lock);
    return entry;
}

void slashconfirm_free_entry(slashconfirm_entry_t *entry)
{
    free(entry);
}

void slashconfirm_clear(const char *session_key)
{
    if (!session_key) return;

    pthread_mutex_lock(&g_lock);
    int idx = _find_session(session_key);
    if (idx >= 0) {
        _remove_at(idx);
    }
    pthread_mutex_unlock(&g_lock);
}

bool slashconfirm_clear_if_stale(const char *session_key,
                                  double timeout_seconds)
{
    if (!session_key) return false;

    pthread_mutex_lock(&g_lock);

    int idx = _find_session(session_key);
    if (idx < 0) {
        pthread_mutex_unlock(&g_lock);
        return false;
    }

    double now = (double)time(NULL);
    bool stale = (now - g_pending[idx].created_at) > timeout_seconds;

    if (stale) {
        _remove_at(idx);
    }

    pthread_mutex_unlock(&g_lock);
    return stale;
}

char *slashconfirm_resolve(const char *session_key,
                           const char *confirm_id,
                           const char *choice,
                           double timeout_seconds)
{
    if (!session_key || !confirm_id || !choice) return NULL;

    slashconfirm_entry_t entry_buf;
    slashconfirm_handler_t handler = NULL;
    char command[SLASHCONFIRM_MAX_COMMAND] = "";
    bool found = false;

    pthread_mutex_lock(&g_lock);

    int idx = _find_session(session_key);
    if (idx >= 0) {
        /* Check confirm_id match */
        if (strcmp(g_pending[idx].confirm_id, confirm_id) == 0) {
            /* Check timeout */
            double now = (double)time(NULL);
            if ((now - g_pending[idx].created_at) <= timeout_seconds) {
                /* Save entry data, then remove it */
                memcpy(&entry_buf, &g_pending[idx], sizeof(entry_buf));
                handler = g_pending[idx].handler;
                strncpy(command, g_pending[idx].command, sizeof(command) - 1);
                command[sizeof(command) - 1] = '\0';
                _remove_at(idx);
                found = true;
            } else {
                /* Stale — remove without running */
                _remove_at(idx);
            }
        }
        /* If confirm_id doesn't match, leave the entry (superseded silently) */
    }

    pthread_mutex_unlock(&g_lock);

    if (!found || !handler)
        return NULL;

    /* Run handler */
    char *result = handler(choice);
    return result;  /* caller frees */
}

void slashconfirm_clear_all(void)
{
    pthread_mutex_lock(&g_lock);
    g_pending_count = 0;
    pthread_mutex_unlock(&g_lock);
}
