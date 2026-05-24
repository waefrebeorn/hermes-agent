/*
 * prompt_caching.c — Anthropic prompt caching strategy (system_and_3).
 *
 * Port of Python agent/prompt_caching.py (79 lines).
 * Pure functions — no state, no dependencies beyond stdlib.
 *
 * Strategy: up to 4 cache_control breakpoints —
 *   system prompt (if present) + last 3 non-system messages
 *
 * MIT License — WuBu Hermes Project
 */

#include "prompt_caching.h"
#include <string.h>
#include <stdio.h>

/* Apply cache_control to a single message */
static void apply_cache_marker(pc_message_t *msg,
                               const char *cache_type,
                               const char *cache_ttl,
                               bool native_anthropic) {
    if (!msg) return;

    /* For tool messages, only add cache_control in native_anthropic mode */
    if (msg->role == 3 && !native_anthropic)
        return;

    msg->has_cache = true;
    snprintf(msg->cache_type, sizeof(msg->cache_type), "%s", cache_type);
    if (cache_ttl && cache_ttl[0])
        snprintf(msg->cache_ttl, sizeof(msg->cache_ttl), "%s", cache_ttl);
}

/* Build the cache_control marker */
static void build_marker(const char *cache_ttl,
                         char *cache_type_out, size_t type_sz,
                         char *cache_ttl_out, size_t ttl_sz) {
    snprintf(cache_type_out, type_sz, "ephemeral");
    cache_ttl_out[0] = '\0';
    if (cache_ttl && strcmp(cache_ttl, "1h") == 0) {
        snprintf(cache_ttl_out, ttl_sz, "1h");
    }
}

void apply_anthropic_cache_control(pc_message_t *messages, int *count,
                                   const char *cache_ttl,
                                   bool native_anthropic) {
    if (!messages || !count || *count <= 0) return;

    /* Default TTL */
    const char *ttl = cache_ttl;
    if (!ttl || !ttl[0]) ttl = "5m";

    char cache_type[32], cache_ttl_val[32];
    build_marker(ttl, cache_type, sizeof(cache_type),
                 cache_ttl_val, sizeof(cache_ttl_val));

    int breakpoints_used = 0;

    /* First message: if it's system, add cache marker */
    if (*count > 0 && messages[0].role == 0) {
        apply_cache_marker(&messages[0], cache_type,
                           cache_ttl_val[0] ? cache_ttl_val : NULL,
                           native_anthropic);
        breakpoints_used++;
    }

    /* Last 3 non-system messages get cache markers */
    int remaining = 4 - breakpoints_used;
    if (remaining <= 0) return;

    /* Walk backwards, collect non-system indices */
    int non_sys_indices[PC_MAX_MESSAGES];
    int ns_count = 0;
    for (int i = *count - 1; i >= 0 && ns_count < remaining; i--) {
        if (messages[i].role != 0) { /* not system */
            non_sys_indices[ns_count++] = i;
        }
    }

    /* Apply markers (they're already in reverse order from the walk) */
    for (int i = 0; i < ns_count; i++) {
        apply_cache_marker(&messages[non_sys_indices[i]], cache_type,
                           cache_ttl_val[0] ? cache_ttl_val : NULL,
                           native_anthropic);
    }
}
