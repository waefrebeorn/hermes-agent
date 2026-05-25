/*
 * credential_pool.c — Credential pool implementation (P82).
 *
 * Multi-key rotation per provider with rate-limit tracking,
 * consecutive-failure backoff, and quota management.
 */

#include "credential_pool.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Default max retries before rotating */
#define DEFAULT_MAX_CONSECUTIVE_FAILURES 3
#define DEFAULT_COOLOFF_SECONDS 300
#define DEFAULT_RETRIES_PER_KEY 1

static bool entry_usable(const credential_entry_t *e, time_t now) {
    switch (e->status) {
    case CRED_OK:
        return true;
    case CRED_RATE_LIMITED:
        /* Check if rate limit has expired */
        if (e->rate_limit_reset > 0 && now >= e->rate_limit_reset)
            return true;
        return false;
    case CRED_FAILED:
        /* Check cooloff: if enough time passed since last_used, try again */
        if (e->last_used > 0 && (now - e->last_used) >= DEFAULT_COOLOFF_SECONDS)
            return true;
        return false;
    case CRED_EXHAUSTED:
        return false;
    }
    return false;
}

/* ================================================================
 *  Public API
 * ================================================================ */

credential_pool_t *credential_pool_create(const char *provider_name) {
    credential_pool_t *pool = (credential_pool_t *)calloc(1, sizeof(credential_pool_t));
    if (!pool) return NULL;

    /* Seed random for weighted selection (one-shot, ok if redundant) */
    static bool seeded = false;
    if (!seeded) { srand((unsigned int)(time(NULL) ^ (uintptr_t)pool)); seeded = true; }

    if (provider_name)
        snprintf(pool->provider_name, sizeof(pool->provider_name), "%s", provider_name);
    else
        snprintf(pool->provider_name, sizeof(pool->provider_name), "default");

    pool->current_index = 0;
    pool->max_retries_per_key = DEFAULT_RETRIES_PER_KEY;
    pool->cooloff_seconds = DEFAULT_COOLOFF_SECONDS;
    return pool;
}

int credential_pool_add_key(credential_pool_t *pool,
                             const char *api_key,
                             const char *label) {
    if (!pool || !api_key) return -1;
    if (pool->entry_count >= CREDENTIAL_POOL_MAX_KEYS) return -1;

    int idx = pool->entry_count;
    credential_entry_t *e = &pool->entries[idx];

    snprintf(e->api_key, sizeof(e->api_key), "%s", api_key);
    if (label && label[0])
        snprintf(e->label, sizeof(e->label), "%s", label);
    else
        snprintf(e->label, sizeof(e->label), "key-%d", idx);

    e->status = CRED_OK;
    e->consecutive_failures = 0;
    e->max_consecutive_failures = DEFAULT_MAX_CONSECUTIVE_FAILURES;
    e->rate_limit_remaining = -1;
    e->rate_limit_reset = 0;
    e->rpm_limit = 0;
    e->total_tokens_used = 0;
    e->total_requests = 0;
    e->quota_limit = -1;
    e->last_used = 0;
    e->weight = 1;  /* B11: default weight = normal */

    pool->entry_count++;
    return idx;
}

/* B11: Set weight for a specific entry */
bool credential_pool_set_weight(credential_pool_t *pool, int entry_index, int weight) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return false;
    pool->entries[entry_index].weight = weight < 0 ? 0 : weight;
    return true;
}

const char *credential_pool_next_key(const credential_pool_t *pool, int *out_index) {
    if (!pool || pool->entry_count == 0) return NULL;

    time_t now = time(NULL);
    int n = pool->entry_count;

    /* B11: Check if any entry has non-default weight (weight != 1) */
    bool has_weights = false;
    int total_weight = 0;
    for (int i = 0; i < n; i++) {
        if (pool->entries[i].weight != 1) { has_weights = true; }
        if (entry_usable(&pool->entries[i], now) && pool->entries[i].weight > 0)
            total_weight += pool->entries[i].weight;
    }

    if (has_weights && total_weight > 0) {
        /* Weighted random selection */
        int roll = rand() % total_weight;
        int accum = 0;
        for (int i = 0; i < n; i++) {
            if (!entry_usable(&pool->entries[i], now) || pool->entries[i].weight <= 0)
                continue;
            accum += pool->entries[i].weight;
            if (roll < accum) {
                if (out_index) *out_index = i;
                ((credential_pool_t *)pool)->current_index = (i + 1) % n;
                return pool->entries[i].api_key;
            }
        }
    }

    /* Round-robin search (fallback for uniform weights or empty usable after weighting) */
    for (int i = 0; i < n; i++) {
        int idx = (pool->current_index + i) % n;
        if (entry_usable(&pool->entries[idx], now)) {
            if (out_index) *out_index = idx;
            ((credential_pool_t *)pool)->current_index = (idx + 1) % n;
            return pool->entries[idx].api_key;
        }
    }

    /* No usable key — try resurrecting any cooled-off failed entry */
    for (int i = 0; i < n; i++) {
        if (pool->entries[i].status == CRED_FAILED) {
            if (out_index) *out_index = i;
            ((credential_pool_t *)pool)->current_index = (i + 1) % n;
            return pool->entries[i].api_key;
        }
    }

    return NULL; /* All keys exhausted */
}

credential_status_t credential_pool_report(credential_pool_t *pool,
                                            int entry_index,
                                            int http_status,
                                            long long tokens_used,
                                            int rate_limit_remaining,
                                            time_t rate_limit_reset) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return CRED_FAILED;

    credential_entry_t *e = &pool->entries[entry_index];
    e->last_used = time(NULL);
    e->total_requests++;

    if (tokens_used > 0)
        e->total_tokens_used += tokens_used;

    /* Track rate-limit headers */
    if (rate_limit_remaining >= 0)
        e->rate_limit_remaining = rate_limit_remaining;
    if (rate_limit_reset > 0)
        e->rate_limit_reset = rate_limit_reset;

    /* Categorize HTTP status */
    if (http_status >= 200 && http_status < 300) {
        /* Success */
        e->consecutive_failures = 0;
        e->status = CRED_OK;
        /* Advance round-robin cursor for next call */
        pool->current_index = (entry_index + 1) % pool->entry_count;
    } else if (http_status == 429) {
        /* Rate limited */
        e->status = CRED_RATE_LIMITED;
        e->consecutive_failures++;
        if (rate_limit_reset == 0) {
            /* No reset header — assume 60s cooloff */
            e->rate_limit_reset = time(NULL) + 60;
        }
    } else if (http_status == 401 || http_status == 403) {
        /* Auth failure — key is dead */
        e->status = CRED_EXHAUSTED;
        e->consecutive_failures++;
    } else if (http_status >= 500) {
        /* Server error — increment failures */
        e->consecutive_failures++;
        if (e->consecutive_failures >= e->max_consecutive_failures) {
            e->status = CRED_FAILED;
        }
    } else if (http_status >= 400) {
        /* Client error (not auth/rate-limit) */
        e->consecutive_failures++;
        if (e->consecutive_failures >= e->max_consecutive_failures) {
            e->status = CRED_FAILED;
        }
    }

    /* Check if quota exhausted */
    if (e->quota_limit > 0 && e->total_tokens_used >= e->quota_limit) {
        e->status = CRED_EXHAUSTED;
    }

    return e->status;
}

bool credential_pool_reset(credential_pool_t *pool, int entry_index) {
    if (!pool || entry_index < 0 || entry_index >= pool->entry_count)
        return false;

    credential_entry_t *e = &pool->entries[entry_index];
    e->status = CRED_OK;
    e->consecutive_failures = 0;
    e->rate_limit_remaining = -1;
    e->rate_limit_reset = 0;
    return true;
}

char *credential_pool_stats_json(const credential_pool_t *pool) {
    if (!pool) return NULL;

    json_node_t *root = json_object();
    json_node_t *entries_arr = json_array();

    for (int i = 0; i < pool->entry_count; i++) {
        const credential_entry_t *e = &pool->entries[i];
        json_node_t *entry = json_object();

        json_set(entry, "label", json_string(e->label));
        json_set(entry, "weight", json_number(e->weight));
        json_set(entry, "status", json_string(
            e->status == CRED_OK ? "ok" :
            e->status == CRED_RATE_LIMITED ? "rate_limited" :
            e->status == CRED_FAILED ? "failed" : "exhausted"));
        json_set(entry, "consecutive_failures", json_number(e->consecutive_failures));
        json_set(entry, "total_tokens_used", json_number((double)e->total_tokens_used));
        json_set(entry, "total_requests", json_number((double)e->total_requests));

        if (e->rate_limit_remaining >= 0)
            json_set(entry, "rate_limit_remaining", json_number(e->rate_limit_remaining));
        if (e->quota_limit > 0)
            json_set(entry, "quota_limit", json_number((double)e->quota_limit));

        json_append(entries_arr, entry);
    }

    json_set(root, "provider", json_string(pool->provider_name));
    json_set(root, "entries", entries_arr);
    json_set(root, "current_index", json_number(pool->current_index));

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

void credential_pool_free(credential_pool_t *pool) {
    if (!pool) return;
    /* No dynamic allocations per-entry (all fixed-size arrays) */
    free(pool);
}

bool credential_pool_has_available(const credential_pool_t *pool) {
    if (!pool || pool->entry_count == 0) return false;
    time_t now = time(NULL);
    for (int i = 0; i < pool->entry_count; i++) {
        if (entry_usable(&pool->entries[i], now))
            return true;
    }
    return false;
}

int credential_pool_tick(credential_pool_t *pool) {
    if (!pool) return 0;
    time_t now = time(NULL);
    int resurrected = 0;

    for (int i = 0; i < pool->entry_count; i++) {
        credential_entry_t *e = &pool->entries[i];

        /* Resurrect rate-limited entries past their reset time */
        if (e->status == CRED_RATE_LIMITED &&
            e->rate_limit_reset > 0 &&
            now >= e->rate_limit_reset) {
            e->status = CRED_OK;
            e->consecutive_failures = 0;
            resurrected++;
        }

        /* Resurrect failed entries past cooloff */
        if (e->status == CRED_FAILED &&
            e->last_used > 0 &&
            (now - e->last_used) >= (time_t)pool->cooloff_seconds) {
            e->status = CRED_OK;
            e->consecutive_failures = 0;
            resurrected++;
        }
    }

    return resurrected;
}
