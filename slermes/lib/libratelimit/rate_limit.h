#ifndef HERMES_RATE_LIMIT_H
#define HERMES_RATE_LIMIT_H

/*
 * rate_limit.h — Rate limit tracking for inference API responses.
 * Port of Python agent/rate_limit_tracker.py.
 *
 * Captures x-ratelimit-* headers from provider responses and provides
 * formatted display for the /usage slash command.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <time.h>

/* ─── RateLimitBucket ──────────────────────────────────── */

/** One rate-limit window (e.g. requests per minute). */
typedef struct {
    int          limit;            /**< Cap for this window */
    int          remaining;        /**< Remaining in this window */
    double       reset_seconds;    /**< Seconds until reset (from capture time) */
    double       captured_at;      /**< time() when captured */
} rate_limit_bucket_t;

/** Initialize a bucket with zero/default values. */
void rate_limit_bucket_init(rate_limit_bucket_t *b);

/** Compute used = limit - remaining. Returns 0 if limit <= 0. */
int  rate_limit_bucket_used(const rate_limit_bucket_t *b);

/** Compute usage percentage. Returns 0.0 if limit <= 0. */
double rate_limit_bucket_usage_pct(const rate_limit_bucket_t *b);

/** Estimated seconds remaining until reset, adjusted for elapsed time. */
double rate_limit_bucket_remaining_seconds(const rate_limit_bucket_t *b);

/* ─── RateLimitState ───────────────────────────────────── */

/** Full rate-limit state parsed from response headers. */
typedef struct {
    rate_limit_bucket_t requests_min;    /**< Requests/min */
    rate_limit_bucket_t requests_hour;   /**< Requests/hr */
    rate_limit_bucket_t tokens_min;      /**< Tokens/min */
    rate_limit_bucket_t tokens_hour;     /**< Tokens/hr */
    double              captured_at;     /**< time() when captured */
    char                provider[64];    /**< Provider name */
} rate_limit_state_t;

/** Initialize state with zero/default values. */
void rate_limit_state_init(rate_limit_state_t *s, const char *provider);

/** True if state has data (captured_at > 0). */
bool rate_limit_state_has_data(const rate_limit_state_t *s);

/** Seconds since capture. Returns INFINITY if no data. */
double rate_limit_state_age_seconds(const rate_limit_state_t *s);

/* ─── Header parsing ───────────────────────────────────── */

/**
 * Parse x-ratelimit-* headers from a flat key-value map.
 * Header keys should already be lowercased.
 * Returns true if at least one rate-limit header was found.
 */
bool rate_limit_parse_headers(
    rate_limit_state_t *state,
    const char * const *header_keys,
    const char * const *header_values,
    int header_count,
    const char *provider
);

/* ─── Formatting ───────────────────────────────────────── */

/** Format a count: 7999856 -> "8.0M", 33599 -> "33.6K", 799 -> "799". */
void rate_limit_fmt_count(char *buf, size_t sz, int n);

/** Format seconds: 58 -> "58s", 134 -> "2m 14s", 3720 -> "1h 2m". */
void rate_limit_fmt_seconds(char *buf, size_t sz, double seconds);

/**
 * Format one bucket as a single display line.
 * Returns number of chars written (excluding null).
 */
int rate_limit_bucket_line(char *buf, size_t sz,
                           const char *label, const rate_limit_bucket_t *b,
                           int label_width);

/**
 * Format full rate-limit display for terminal/chat.
 * Returns heap-allocated string; caller must free.
 */
char *rate_limit_format_display(const rate_limit_state_t *state);

/**
 * Format one-line compact summary for status bars.
 * Returns heap-allocated string; caller must free.
 */
char *rate_limit_format_compact(const rate_limit_state_t *state);

/** Testing support — reset cached internal state. */
void rate_limit_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_RATE_LIMIT_H */
