/*
 * fallback_routing.c — Fallback model routing implementation (P83).
 *
 * Ordered fallback list with cool-off tracking and automatic progression.
 */

#include "fallback_routing.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static bool entry_usable(const fallback_entry_t *e, time_t now) {
    return e->cooloff_until == 0 || now >= e->cooloff_until;
}

/* ================================================================
 *  Public API
 * ================================================================ */

fallback_chain_t *fallback_chain_create(const char *primary_model,
                                         const char *primary_provider) {
    fallback_chain_t *chain = (fallback_chain_t *)calloc(1, sizeof(fallback_chain_t));
    if (!chain) return NULL;

    if (primary_model)
        snprintf(chain->primary_model, sizeof(chain->primary_model), "%s", primary_model);
    else
        snprintf(chain->primary_model, sizeof(chain->primary_model), "unknown");

    if (primary_provider)
        snprintf(chain->primary_provider, sizeof(chain->primary_provider), "%s", primary_provider);
    else
        snprintf(chain->primary_provider, sizeof(chain->primary_provider), "openai");

    chain->current_index = -1; /* -1 means using primary */
    chain->using_fallback = false;
    chain->last_failure_time = 0;
    return chain;
}

int fallback_chain_add(fallback_chain_t *chain,
                        const char *model,
                        const char *provider,
                        const char *base_url,
                        credential_pool_t *pool) {
    if (!chain || chain->entry_count >= FALLBACK_MAX_CHAIN) return -1;

    int idx = chain->entry_count;
    fallback_entry_t *e = &chain->entries[idx];

    snprintf(e->model, sizeof(e->model), "%s", model ? model : "fallback");
    snprintf(e->provider, sizeof(e->provider), "%s", provider ? provider : "");
    if (base_url && base_url[0])
        snprintf(e->base_url, sizeof(e->base_url), "%s", base_url);
    e->pool = pool;
    e->cooloff_until = 0;
    e->consecutive_failures = 0;
    e->max_failures_before_cooloff = FALLBACK_DEFAULT_MAX_FAILURES;
    e->cooloff_seconds = FALLBACK_DEFAULT_COOLOFF;

    chain->entry_count++;
    return idx;
}

const char *fallback_chain_current(fallback_chain_t *chain,
                                    char *out_model, size_t model_sz,
                                    char *out_provider, size_t prov_sz,
                                    char *out_base_url, size_t url_sz,
                                    credential_pool_t **out_pool) {
    if (!chain) return NULL;

    time_t now = time(NULL);

    if (!chain->using_fallback) {
        /* Return primary */
        if (out_model) snprintf(out_model, model_sz, "%s", chain->primary_model);
        if (out_provider) snprintf(out_provider, prov_sz, "%s", chain->primary_provider);
        if (out_base_url) out_base_url[0] = '\0';
        if (out_pool) *out_pool = NULL;
        return chain->primary_model;
    }

    /* Find next usable fallback starting from current_index */
    for (int i = 0; i < chain->entry_count; i++) {
        int idx = (chain->current_index + i) % chain->entry_count;
        if (entry_usable(&chain->entries[idx], now)) {
            chain->current_index = idx;
            const fallback_entry_t *e = &chain->entries[idx];
            if (out_model) snprintf(out_model, model_sz, "%s", e->model);
            if (out_provider) snprintf(out_provider, prov_sz, "%s", e->provider);
            if (out_base_url) snprintf(out_base_url, url_sz, "%s", e->base_url);
            if (out_pool) *out_pool = e->pool;
            return e->model;
        }
    }

    /* All fallbacks cooled off — try resurrecting the first one */
    for (int i = 0; i < chain->entry_count; i++) {
        chain->current_index = i;
        const fallback_entry_t *e = &chain->entries[i];
        if (out_model) snprintf(out_model, model_sz, "%s", e->model);
        if (out_provider) snprintf(out_provider, prov_sz, "%s", e->provider);
        if (out_base_url) snprintf(out_base_url, url_sz, "%s", e->base_url);
        if (out_pool) *out_pool = e->pool;
        return e->model; /* Return even if cooled off — better than nothing */
    }

    return NULL; /* No fallbacks configured */
}

bool fallback_chain_report(fallback_chain_t *chain,
                            bool success,
                            int http_status) {
    if (!chain) return false;

    time_t now = time(NULL);

    if (success) {
        /* Reset state on success */
        chain->current_index = -1;
        chain->using_fallback = false;
        chain->last_failure_time = 0;

        /* Clear all cool-offs */
        for (int i = 0; i < chain->entry_count; i++) {
            chain->entries[i].cooloff_until = 0;
            chain->entries[i].consecutive_failures = 0;
        }
        return false; /* No retry needed */
    }

    /* Failure — track and potentially advance fallback */
    chain->last_failure_time = now;

    if (!chain->using_fallback) {
        /* Primary failed — move to first fallback */
        chain->current_index = 0;
        chain->using_fallback = true;
        return chain->entry_count > 0; /* Retry if fallbacks exist */
    }

    /* A fallback failed — mark it and try next */
    int failed_idx = chain->current_index;
    if (failed_idx >= 0 && failed_idx < chain->entry_count) {
        fallback_entry_t *e = &chain->entries[failed_idx];
        e->consecutive_failures++;

        /* Check for auth failure (401/403) — cooloff longer */
        if (http_status == 401 || http_status == 403) {
            e->cooloff_until = now + (time_t)(e->cooloff_seconds * 6); /* 12min for auth failures */
        } else if (http_status == 429) {
            e->cooloff_until = now + 60; /* Rate-limited: 1 min */
        } else if (e->consecutive_failures >= e->max_failures_before_cooloff) {
            e->cooloff_until = now + (time_t)e->cooloff_seconds;
        }
    }

    /* Advance to next fallback */
    chain->current_index = (failed_idx + 1) % chain->entry_count;

    /* Check if all fallbacks have been tried */
    /* If we've wrapped around back to failed_idx, all are exhausted */
    int attempts = 0;
    int idx = chain->current_index;
    while (attempts < chain->entry_count) {
        if (entry_usable(&chain->entries[idx], now))
            return true; /* Found a usable fallback */
        idx = (idx + 1) % chain->entry_count;
        attempts++;
    }

    return false; /* All fallbacks exhausted */
}

void fallback_chain_reset(fallback_chain_t *chain) {
    if (!chain) return;
    chain->current_index = -1;
    chain->using_fallback = false;
    chain->last_failure_time = 0;
    for (int i = 0; i < chain->entry_count; i++) {
        chain->entries[i].cooloff_until = 0;
        chain->entries[i].consecutive_failures = 0;
    }
}

bool fallback_chain_has_available(const fallback_chain_t *chain) {
    if (!chain) return false;
    time_t now = time(NULL);

    /* Primary is always considered available */
    if (!chain->using_fallback) return true;

    for (int i = 0; i < chain->entry_count; i++) {
        if (entry_usable(&chain->entries[i], now))
            return true;
    }
    return false;
}

int fallback_chain_tick(fallback_chain_t *chain) {
    if (!chain) return 0;
    time_t now = time(NULL);
    int resurrected = 0;

    for (int i = 0; i < chain->entry_count; i++) {
        fallback_entry_t *e = &chain->entries[i];
        if (e->cooloff_until > 0 && now >= e->cooloff_until) {
            e->cooloff_until = 0;
            e->consecutive_failures = 0;
            resurrected++;
        }
    }

    return resurrected;
}

void fallback_chain_free(fallback_chain_t *chain) {
    if (!chain) return;
    /* Pool pointers are not owned */
    free(chain);
}

char *fallback_chain_stats_json(const fallback_chain_t *chain) {
    if (!chain) return NULL;

    json_node_t *root = json_object();
    json_set(root, "primary_model", json_string(chain->primary_model));
    json_set(root, "primary_provider", json_string(chain->primary_provider));
    json_set(root, "using_fallback", json_bool(chain->using_fallback));
    json_set(root, "current_index", json_number(chain->current_index));

    json_node_t *entries_arr = json_array();
    for (int i = 0; i < chain->entry_count; i++) {
        const fallback_entry_t *e = &chain->entries[i];
        json_node_t *entry = json_object();
        json_set(entry, "model", json_string(e->model));
        json_set(entry, "provider", json_string(e->provider));
        json_set(entry, "cooling_off", json_bool(e->cooloff_until > 0));
        json_set(entry, "consecutive_failures", json_number(e->consecutive_failures));
        json_append(entries_arr, entry);
    }
    json_set(root, "fallbacks", entries_arr);

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}
