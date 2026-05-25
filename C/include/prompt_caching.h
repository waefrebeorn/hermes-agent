/*
 * prompt_caching.h — Anthropic prompt caching strategy (system_and_3).
 *
 * Port of Python agent/prompt_caching.py (79 lines).
 * Places up to 4 cache_control breakpoints: system + last 3 non-system.
 *
 * MIT License — WuBu Hermes Project
 */

#ifndef PROMPT_CACHING_H
#define PROMPT_CACHING_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of messages we can handle */
#define PC_MAX_MESSAGES 1024

/** Maximum message content length */
#define PC_MAX_CONTENT 65536

/** A single message with cache_control info (simplified for serialization). */
typedef struct {
    int role;           /**< 0=system, 1=user, 2=assistant, 3=tool */
    char content[PC_MAX_CONTENT]; /**< String content (simplified) */
    bool has_cache;     /**< true if cache_control should be set */
    /** cache_control type: "ephemeral" or empty */
    char cache_type[32];
    /** cache_control ttl: "1h" or empty */
    char cache_ttl[32];
} pc_message_t;

/**
 * Apply system_and_3 caching strategy to messages for Anthropic models.
 * Places up to 4 cache_control breakpoints: system + last 3 non-system.
 *
 * @param messages    Array of messages (modified in-place)
 * @param count       Number of messages
 * @param cache_ttl   TTL: "5m" (default) or "1h"
 * @param native_anthropic If true, tool messages get cache_control too
 */
void apply_anthropic_cache_control(pc_message_t *messages, int *count,
                                   const char *cache_ttl,
                                   bool native_anthropic);

/** Track a cache hit. Increments hit counter. */
void cache_track_hit(void);

/** Track a cache miss. Increments miss counter. */
void cache_track_miss(void);

/** Reset all cache statistics counters to zero. */
void cache_reset_stats(void);

/**
 * Get cache statistics as a JSON string.
 * Returns malloc'd JSON like {"hits":N,"misses":N,"total":N,"hit_rate":0.0}.
 * Caller must free().
 */
char *cache_get_stats_json(void);

/** Get total cache requests (hits + misses). */
int cache_get_total(void);

/** Get total cache hits. */
int cache_get_hits(void);

/** Get total cache misses. */
int cache_get_misses(void);

#ifdef __cplusplus
}
#endif

#endif /* PROMPT_CACHING_H */
