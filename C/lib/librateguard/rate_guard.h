/**
 * rate_guard.h — Cross-session rate limit guard for provider API calls.
 *
 * Port of Python agent/nous_rate_guard.py.
 *
 * Writes rate limit state to a shared JSON file so all sessions (CLI,
 * gateway, cron) can check whether a provider is currently rate-limited
 * before making requests. Prevents retry amplification when RPH is tapped.
 */
#ifndef HERMES_RATE_GUARD_H
#define HERMES_RATE_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <time.h>

/* ── Constants ──────────────────────────────────────────── */

/** Subdirectory under HERMES_HOME for rate limit state files. */
#define RATE_GUARD_SUBDIR "rate_limits"

/** Default cooldown in seconds when no header data available (5 min). */
#define RATE_GUARD_DEFAULT_COOLDOWN 300.0

/** Buckets with reset < this are treated as transient, not genuine quota. */
#define RATE_GUARD_MIN_RESET_BREAKER 60.0

/* ── Bucket state (mirrors x-ratelimit-* headers) ──────── */

/** Parsed rate-limit bucket for one window. */
typedef struct {
    int   remaining;      /**< Remaining in this window (-1 = unknown) */
    double reset_seconds; /**< Seconds until reset (-1 = unknown) */
    bool   valid;         /**< true if bucket data was present in headers */
} rate_guard_bucket_t;

/** Parsed rate-limit state from response headers. */
typedef struct {
    rate_guard_bucket_t requests_min;    /**< x-ratelimit-remaining-requests */
    rate_guard_bucket_t requests_hour;   /**< x-ratelimit-remaining-requests-1h */
    rate_guard_bucket_t tokens_min;      /**< x-ratelimit-remaining-tokens */
    rate_guard_bucket_t tokens_hour;     /**< x-ratelimit-remaining-tokens-1h */
    int http_status;                     /**< HTTP status code */
} rate_guard_state_t;

/* ── API ────────────────────────────────────────────────── */

/**
 * Record that a provider is rate-limited.
 *
 * Parses reset time from response headers. Falls back to
 * default_cooldown if no reset info available. Writes to a
 * shared JSON file under HERMES_HOME/rate_limits/<name>.json.
 *
 * @param name       Provider name (e.g. "nous", "openrouter")
 * @param state      Parsed rate-limit state from response headers (or NULL)
 * @param cooldown   Fallback cooldown seconds (0 = use default)
 */
void rate_guard_record(const char *name,
                       const rate_guard_state_t *state,
                       double cooldown);

/**
 * Check if a provider is currently rate-limited.
 *
 * @param name  Provider name
 * @return      Seconds remaining until reset, or 0.0 if not rate-limited.
 */
double rate_guard_remaining(const char *name);

/**
 * Clear the rate limit state for a provider.
 *
 * @param name  Provider name
 */
void rate_guard_clear(const char *name);

/**
 * Format remaining seconds into human-readable duration string.
 * Caller must free the returned string.
 *
 * @param seconds  Time in seconds
 * @return         Malloc'd string like "5m 30s" or "2h"
 */
char *rate_guard_format_remaining(double seconds);

/**
 * Check if rate-limit headers indicate a genuine quota exhaustion
 * (not a transient provider-side throttle).
 *
 * A genuine rate limit has at least one bucket with remaining == 0
 * AND reset_seconds >= MIN_RESET_FOR_BREAKER (60s).
 * Transient throttles (upstream jitter, short provider-side caps)
 * have short reset windows and should NOT trip the breaker.
 *
 * @param state  Parsed rate-limit state from response headers
 * @return       true if evidence points at genuine quota exhaustion
 */
bool rate_guard_is_genuine(const rate_guard_state_t *state);

/**
 * Build a rate_guard_state_t from x-ratelimit-* header values.
 *
 * Pass NULL for any unknown/value. All 4 windows are parsed:
 *   remaining_requests, remaining_requests_1h,
 *   remaining_tokens, remaining_tokens_1h
 * plus their reset_* counterparts.
 *
 * @param state           Output struct (filled with parsed data)
 * @param remaining_req   x-ratelimit-remaining-requests  (-1=unknown)
 * @param reset_req       x-ratelimit-reset-requests      (-1=unknown)
 * @param remaining_req1h x-ratelimit-remaining-requests-1h (-1=unknown)
 * @param reset_req1h     x-ratelimit-reset-requests-1h   (-1=unknown)
 * @param remaining_tok   x-ratelimit-remaining-tokens    (-1=unknown)
 * @param reset_tok       x-ratelimit-reset-tokens        (-1=unknown)
 * @param remaining_tok1h x-ratelimit-remaining-tokens-1h (-1=unknown)
 * @param reset_tok1h     x-ratelimit-reset-tokens-1h     (-1=unknown)
 * @param http_status     HTTP status code (0=unknown)
 */
void rate_guard_parse_headers(rate_guard_state_t *state,
                               int remaining_req, double reset_req,
                               int remaining_req1h, double reset_req1h,
                               int remaining_tok, double reset_tok,
                               int remaining_tok1h, double reset_tok1h,
                               int http_status);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_RATE_GUARD_H */
