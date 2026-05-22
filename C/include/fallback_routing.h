/*
 * fallback_routing.h — Fallback model routing for Hermes C (P83).
 *
 * Ordered fallback list with cool-off tracking. When the primary model
 * fails (connection error, rate-limit, server error), the fallback chain
 * is tried in order. Failed fallbacks are cooled off (not retried for
 * N seconds) to prevent rapid failure cascades.
 */

#ifndef FALLBACK_ROUTING_H
#define FALLBACK_ROUTING_H

#include "hermes.h"
#include "credential_pool.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Fallback Entry — one model/provider fallback option
 * ================================================================ */

#define FALLBACK_MAX_CHAIN 8
#define FALLBACK_DEFAULT_COOLOFF 120  /* 2 minutes default cooloff */
#define FALLBACK_DEFAULT_MAX_FAILURES 3

typedef struct {
    char  model[128];           /* model name to fallback to */
    char  provider[64];         /* provider name */
    char  base_url[512];        /* optional override base URL */
    credential_pool_t *pool;    /* optional credential pool (not owned) */

    /* Cool-off tracking */
    time_t cooloff_until;       /* 0 = not cooling off */
    int    consecutive_failures;
    int    max_failures_before_cooloff; /* default 3 */
    int    cooloff_seconds;              /* default 120 */
} fallback_entry_t;

/* ================================================================
 *  Fallback Chain — ordered list for one provider's fallback
 * ================================================================ */

typedef struct {
    char  primary_model[128];   /* the original model requested */
    char  primary_provider[64]; /* the original provider */
    fallback_entry_t entries[FALLBACK_MAX_CHAIN];
    int   entry_count;

    /* Runtime state */
    int   current_index;        /* which fallback we last tried (-1 = primary) */
    time_t last_failure_time;   /* when the primary last failed */
    bool   using_fallback;      /* true if we're currently on a fallback */
} fallback_chain_t;

/* ================================================================
 *  API
 * ================================================================ */

/* Create a fallback chain for the given primary model/provider.
 * Returns NULL on alloc fail. */
fallback_chain_t *fallback_chain_create(const char *primary_model,
                                         const char *primary_provider);

/* Add a fallback entry. Returns entry index or -1 on full chain. */
int fallback_chain_add(fallback_chain_t *chain,
                        const char *model,
                        const char *provider,
                        const char *base_url,
                        credential_pool_t *pool);

/* Get the current target (model + provider + optional pool) based on fallback state.
 * Returns a descriptive string for logging (static buffer).
 * Sets *out_model, *out_provider, *out_base_url, *out_pool if non-NULL.
 * Returns NULL if all fallbacks are cooled off. */
const char *fallback_chain_current(fallback_chain_t *chain,
                                    char *out_model, size_t model_sz,
                                    char *out_provider, size_t prov_sz,
                                    char *out_base_url, size_t url_sz,
                                    credential_pool_t **out_pool);

/* Report result of a call using the current target.
 * - success: true if HTTP 2xx, false otherwise
 * - http_status: HTTP status code
 * On failure, advances to next fallback (if available) or marks primary as failed.
 * On success, resets failure state.
 * Returns true if we should retry (i.e., there's another fallback to try). */
bool fallback_chain_report(fallback_chain_t *chain,
                            bool success,
                            int http_status);

/* Reset the chain back to primary (e.g. for a new session). */
void fallback_chain_reset(fallback_chain_t *chain);

/* Check if the chain has ANY usable target (including cooled-off fallbacks
 * that may have resurrected). */
bool fallback_chain_has_available(const fallback_chain_t *chain);

/* Tick — call periodically to resurrect cooled-off entries.
 * Returns number of entries resurrected. */
int fallback_chain_tick(fallback_chain_t *chain);

/* Free the chain. Does NOT free credential pools (not owned). */
void fallback_chain_free(fallback_chain_t *chain);

/* Get stats as malloc'd JSON string. Caller must free(). */
char *fallback_chain_stats_json(const fallback_chain_t *chain);

#ifdef __cplusplus
}
#endif

#endif /* FALLBACK_ROUTING_H */
