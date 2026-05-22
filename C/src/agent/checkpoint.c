/*
 * checkpoint.c — P98: Checkpoint manager for agent state.
 *
 * Supports: auto-save every N turns, named checkpoints,
 * rollback to any checkpoint, configurable max snapshots.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* ================================================================
 *  Checkpoint lifecycle
 * ================================================================ */

/* Initialize checkpoint manager */
void checkpoint_init(checkpoint_manager_t *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    mgr->checkpoints = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
    mgr->max_snapshots = 10;      /* default max */
    mgr->auto_save_interval = 5;  /* auto-save every 5 turns */
    mgr->turn_counter = 0;
}

/* Free all checkpoints */
void checkpoint_free(checkpoint_manager_t *mgr) {
    if (!mgr) return;
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->checkpoints[i].messages) {
            for (size_t j = 0; j < mgr->checkpoints[i].count; j++)
                message_free(mgr->checkpoints[i].messages[j]);
            free(mgr->checkpoints[i].messages);
        }
    }
    free(mgr->checkpoints);
    mgr->checkpoints = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
}

/* Set checkpoint manager limits */
void checkpoint_set_limits(checkpoint_manager_t *mgr, int max_snapshots, int auto_save_interval) {
    if (!mgr) return;
    if (max_snapshots > 0) mgr->max_snapshots = max_snapshots;
    if (auto_save_interval > 0) mgr->auto_save_interval = auto_save_interval;
}

/* Generate a checkpoint ID (timestamp-based) */
static void checkpoint_gen_id(char *buf, size_t sz, const char *label) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (label && label[0])
        snprintf(buf, sz, "%04d%02d%02d_%02d%02d%02d_%s",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec, label);
    else
        snprintf(buf, sz, "%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* Evict oldest checkpoint if at capacity */
static void checkpoint_evict_oldest(checkpoint_manager_t *mgr) {
    while (mgr->count > 0 && (int)mgr->count >= mgr->max_snapshots) {
        /* Free oldest (index 0) */
        if (mgr->checkpoints[0].messages) {
            for (size_t j = 0; j < mgr->checkpoints[0].count; j++)
                message_free(mgr->checkpoints[0].messages[j]);
            free(mgr->checkpoints[0].messages);
        }
        memmove(&mgr->checkpoints[0], &mgr->checkpoints[1],
                (mgr->count - 1) * sizeof(checkpoint_t));
        mgr->count--;
    }
}

/* Save a named checkpoint. label can be NULL for auto-save. */
bool checkpoint_save(checkpoint_manager_t *mgr, agent_state_t *state,
                      const char *label) {
    if (!mgr || !state) return false;

    /* Evict oldest if at capacity */
    checkpoint_evict_oldest(mgr);

    /* Grow array if needed */
    if (mgr->count >= mgr->capacity) {
        size_t new_cap = mgr->capacity == 0 ? 8 : mgr->capacity * 2;
        checkpoint_t *new_cps = (checkpoint_t *)realloc(
            mgr->checkpoints, new_cap * sizeof(checkpoint_t));
        if (!new_cps) return false;
        mgr->checkpoints = new_cps;
        mgr->capacity = new_cap;
    }

    checkpoint_t *cp = &mgr->checkpoints[mgr->count];
    memset(cp, 0, sizeof(*cp));

    /* Generate ID */
    checkpoint_gen_id(cp->id, sizeof(cp->id), label);
    if (label)
        snprintf(cp->label, sizeof(cp->label), "%s", label);
    cp->created_at = time(NULL);

    /* Clone messages from state */
    cp->count = state->message_count;
    cp->capacity = cp->count + 16;
    cp->messages = (message_t **)calloc(cp->capacity, sizeof(message_t *));
    if (!cp->messages) return false;

    for (size_t i = 0; i < cp->count; i++) {
        cp->messages[i] = message_clone(state->messages[i]);
        if (!cp->messages[i]) {
            /* Partial clone — free what we have */
            for (size_t j = 0; j < i; j++)
                message_free(cp->messages[j]);
            free(cp->messages);
            cp->messages = NULL;
            cp->count = 0;
            return false;
        }
    }

    mgr->count++;
    return true;
}

/* Restore messages from a checkpoint by ID (or nullptr for most recent).
 * Returns true on success. */
bool checkpoint_restore(checkpoint_manager_t *mgr, agent_state_t *state,
                         const char *checkpoint_id) {
    if (!mgr || !state || mgr->count == 0) return false;

    checkpoint_t *cp = NULL;

    if (checkpoint_id && checkpoint_id[0]) {
        /* Find by ID */
        for (size_t i = 0; i < mgr->count; i++) {
            if (strcmp(mgr->checkpoints[i].id, checkpoint_id) == 0) {
                cp = &mgr->checkpoints[i];
                break;
            }
        }
        if (!cp) return false; /* not found */
    } else {
        /* Most recent checkpoint */
        cp = &mgr->checkpoints[mgr->count - 1];
    }

    /* Clear existing messages in state */
    context_clear(state);

    /* Clone checkpoint messages into state */
    for (size_t i = 0; i < cp->count; i++) {
        message_t *clone = message_clone(cp->messages[i]);
        if (!clone) return false;
        context_push(state, clone);
    }

    return true;
}

/* List saved checkpoints. Returns count written to ids/labels.
 * ids must be array of [64] strings, labels of [128]. */
size_t checkpoint_list(const checkpoint_manager_t *mgr,
                        char (*ids)[64], char (*labels)[128],
                        size_t max_count) {
    if (!mgr || !ids || max_count == 0) return 0;

    size_t n = mgr->count < max_count ? mgr->count : max_count;
    for (size_t i = 0; i < n; i++) {
        snprintf(ids[i], 64, "%s", mgr->checkpoints[i].id);
        if (labels)
            snprintf(labels[i], 128, "%s", mgr->checkpoints[i].label);
    }
    return n;
}

/* Get checkpoint count */
size_t checkpoint_count(const checkpoint_manager_t *mgr) {
    return mgr ? mgr->count : 0;
}

/* Try auto-save — returns true if a checkpoint was saved this call.
 * Only saves every auto_save_interval turns. */
bool checkpoint_try_autosave(checkpoint_manager_t *mgr, agent_state_t *state) {
    if (!mgr || !state) return false;

    /* Don't auto-save empty state */
    if (state->message_count < 2) return false;

    mgr->turn_counter++;
    if (mgr->turn_counter >= mgr->auto_save_interval) {
        mgr->turn_counter = 0;
        return checkpoint_save(mgr, state, "__auto__");
    }
    return false;
}
