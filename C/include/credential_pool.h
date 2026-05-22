/*
 * credential_pool.h — Credential pool for Hermes C (P82).
 *
 * Manages multiple API keys per provider with round-robin rotation,
 * rate-limit tracking, consecutive-failure escalation, and quota
 * management. Each provider can have N keys; the pool auto-rotates
 * when a key is rate-limited or hits consecutive failures.
 */

#ifndef CREDENTIAL_POOL_H
#define CREDENTIAL_POOL_H

#include "hermes.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Credential Entry — one API key with usage tracking
 * ================================================================ */

#define CREDENTIAL_POOL_NAME_MAX 64
#define CREDENTIAL_POOL_MAX_KEYS 16

typedef enum {
    CRED_OK,             /* Key healthy, usable */
    CRED_RATE_LIMITED,   /* 429 received, waiting for reset */
    CRED_FAILED,         /* Consecutive failures exceeded, skip */
    CRED_EXHAUSTED,      /* Quota exhausted (tokens or requests) */
} credential_status_t;

typedef struct {
    char  api_key[512];           /* The actual key */
    char  label[64];              /* Human label (e.g. "prod-1", "backup") */
    credential_status_t status;
    int   consecutive_failures;   /* Incremented on 4xx/5xx (not 429) */
    int   max_consecutive_failures; /* Defaults to 3, then mark FAILED */

    /* Rate-limit tracking */
    int   rate_limit_remaining;   /* -1 = unknown */
    time_t rate_limit_reset;      /* epoch seconds when RPM resets */
    int   rpm_limit;              /* max requests per minute (from header) */

    /* Quota tracking */
    long long total_tokens_used;  /* lifetime tokens consumed via this key */
    long long total_requests;     /* lifetime request count */
    long long quota_limit;        /* -1 = unlimited */
    time_t   last_used;           /* epoch seconds of last use */
} credential_entry_t;

/* ================================================================
 *  Credential Pool — one pool per provider
 * ================================================================ */

typedef struct {
    char              provider_name[CREDENTIAL_POOL_NAME_MAX];
    credential_entry_t entries[CREDENTIAL_POOL_MAX_KEYS];
    int               entry_count;
    int               current_index;   /* round-robin cursor */

    int               max_retries_per_key; /* calls before rotating (default 1) */
    int               cooloff_seconds;     /* how long to wait after FAILED before retry (default 300) */
} credential_pool_t;

/* ================================================================
 *  API
 * ================================================================ */

/* Create an empty pool for the given provider. Returns NULL on alloc fail. */
credential_pool_t *credential_pool_create(const char *provider_name);

/* Add a key to the pool. label is optional (can be ""). Returns entry index or -1. */
int credential_pool_add_key(credential_pool_t *pool,
                            const char *api_key,
                            const char *label);

/* Get the next available key (round-robin, skipping non-CRED_OK entries).
 * Returns NULL if no usable key. Sets *out_index if non-NULL. */
const char *credential_pool_next_key(const credential_pool_t *pool, int *out_index);

/* Report result after a request using key at entry_index.
 * - http_status: HTTP status code (200, 429, 401, 500, etc.)
 * - tokens_used: tokens consumed (-1 if unknown)
 * - rate_limit_remaining: from x-ratelimit-remaining header (-1 if unknown)
 * - rate_limit_reset: epoch seconds of rate limit reset (0 if unknown)
 * Updates consecutive_failures, status, quota fields.
 * Returns the updated entry status. */
credential_status_t credential_pool_report(credential_pool_t *pool,
                                           int entry_index,
                                           int http_status,
                                           long long tokens_used,
                                           int rate_limit_remaining,
                                           time_t rate_limit_reset);

/* Reset a failed/rate-limited entry back to CRED_OK (e.g. after cooloff).
 * Returns true if the entry exists. */
bool credential_pool_reset(credential_pool_t *pool, int entry_index);

/* Get stats for all entries in the pool. Returns a malloc'd JSON string or NULL.
 * Caller must free(). */
char *credential_pool_stats_json(const credential_pool_t *pool);

/* Free the pool and all entries. */
void credential_pool_free(credential_pool_t *pool);

/* Check if any entry in the pool is usable. */
bool credential_pool_has_available(const credential_pool_t *pool);

/* Global cooloff checker — call periodically (e.g. every turn) to resurrect
 * cooled-off entries. Returns number of entries resurrected. */
int credential_pool_tick(credential_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_POOL_H */
