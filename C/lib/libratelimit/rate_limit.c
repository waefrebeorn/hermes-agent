/*
 * rate_limit.c — Rate limit tracking for inference API responses.
 * Port of Python agent/rate_limit_tracker.py.
 *
 * Captures x-ratelimit-* headers from provider responses and provides
 * formatted display for the /usage slash command.
 */

#include "rate_limit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ─── RateLimitBucket ──────────────────────────────────── */

void rate_limit_bucket_init(rate_limit_bucket_t *b)
{
    if (!b) return;
    b->limit = 0;
    b->remaining = 0;
    b->reset_seconds = 0.0;
    b->captured_at = 0.0;
}

int rate_limit_bucket_used(const rate_limit_bucket_t *b)
{
    if (!b || b->limit <= 0) return 0;
    int u = b->limit - b->remaining;
    return u > 0 ? u : 0;
}

double rate_limit_bucket_usage_pct(const rate_limit_bucket_t *b)
{
    if (!b || b->limit <= 0) return 0.0;
    int used = rate_limit_bucket_used(b);
    return ((double)used / (double)b->limit) * 100.0;
}

double rate_limit_bucket_remaining_seconds(const rate_limit_bucket_t *b)
{
    if (!b || b->captured_at <= 0.0) return 0.0;
    double elapsed = difftime(time(NULL), b->captured_at);
    double rem = b->reset_seconds - elapsed;
    return rem > 0.0 ? rem : 0.0;
}

/* ─── RateLimitState ───────────────────────────────────── */

void rate_limit_state_init(rate_limit_state_t *s, const char *provider)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    if (provider)
        strncpy(s->provider, provider, sizeof(s->provider) - 1);
}

bool rate_limit_state_has_data(const rate_limit_state_t *s)
{
    return s && s->captured_at > 0.0;
}

double rate_limit_state_age_seconds(const rate_limit_state_t *s)
{
    if (!s || s->captured_at <= 0.0)
        return INFINITY;
    return difftime(time(NULL), s->captured_at);
}

/* ─── Header parsing ───────────────────────────────────── */

static int safe_int(const char *value, int default_val)
{
    if (!value || !value[0]) return default_val;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value) return default_val;
    return (int)v;
}

static double safe_double(const char *value, double default_val)
{
    if (!value || !value[0]) return default_val;
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value) return default_val;
    return v;
}

/* Find a header value by key (case-sensitive, already lowercased). */
static const char *find_header(const char * const *keys,
                                const char * const *values,
                                int count, const char *target)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(keys[i], target) == 0)
            return values[i];
    }
    return NULL;
}

static void parse_bucket(rate_limit_bucket_t *b,
                          const char * const *keys,
                          const char * const *values,
                          int count,
                          const char *resource, const char *suffix)
{
    char key[128];
    double now = (double)time(NULL);

    snprintf(key, sizeof(key), "x-ratelimit-limit-%s%s", resource, suffix ? suffix : "");
    b->limit = safe_int(find_header(keys, values, count, key), 0);

    snprintf(key, sizeof(key), "x-ratelimit-remaining-%s%s", resource, suffix ? suffix : "");
    b->remaining = safe_int(find_header(keys, values, count, key), 0);

    snprintf(key, sizeof(key), "x-ratelimit-reset-%s%s", resource, suffix ? suffix : "");
    b->reset_seconds = safe_double(find_header(keys, values, count, key), 0.0);

    b->captured_at = now;
}

bool rate_limit_parse_headers(
    rate_limit_state_t *state,
    const char * const *header_keys,
    const char * const *header_values,
    int header_count,
    const char *provider)
{
    if (!state || !header_keys || !header_values) return false;

    /* Quick check: at least one rate-limit header */
    bool has_any = false;
    for (int i = 0; i < header_count; i++) {
        size_t len = strlen(header_keys[i]);
        if (len > 12 && strncmp(header_keys[i], "x-ratelimit-", 12) == 0) {
            has_any = true;
            break;
        }
    }
    if (!has_any) return false;

    double now = (double)time(NULL);

    rate_limit_bucket_init(&state->requests_min);
    rate_limit_bucket_init(&state->requests_hour);
    rate_limit_bucket_init(&state->tokens_min);
    rate_limit_bucket_init(&state->tokens_hour);

    parse_bucket(&state->requests_min, header_keys, header_values, header_count, "requests", "");
    parse_bucket(&state->requests_hour, header_keys, header_values, header_count, "requests", "-1h");
    parse_bucket(&state->tokens_min, header_keys, header_values, header_count, "tokens", "");
    parse_bucket(&state->tokens_hour, header_keys, header_values, header_count, "tokens", "-1h");

    state->captured_at = now;
    if (provider)
        strncpy(state->provider, provider, sizeof(state->provider) - 1);

    return true;
}

/* ─── Formatting ───────────────────────────────────────── */

void rate_limit_fmt_count(char *buf, size_t sz, int n)
{
    if (!buf || sz == 0) return;

    if (n >= 1000000) {
        snprintf(buf, sz, "%.1fM", n / 1000000.0);
    } else if (n >= 1000) {
        snprintf(buf, sz, "%.1fK", n / 1000.0);
    } else {
        snprintf(buf, sz, "%d", n);
    }
}

void rate_limit_fmt_seconds(char *buf, size_t sz, double seconds)
{
    if (!buf || sz == 0) return;

    int s = (int)fmax(0.0, seconds);
    if (s < 60) {
        snprintf(buf, sz, "%ds", s);
    } else if (s < 3600) {
        int m = s / 60;
        int sec = s % 60;
        if (sec > 0)
            snprintf(buf, sz, "%dm %ds", m, sec);
        else
            snprintf(buf, sz, "%dm", m);
    } else {
        int h = s / 3600;
        int m = (s % 3600) / 60;
        if (m > 0)
            snprintf(buf, sz, "%dh %dm", h, m);
        else
            snprintf(buf, sz, "%dh", h);
    }
}

static void bar_ascii(char *buf, size_t sz, double pct, int width)
{
    int filled = (int)(pct / 100.0 * width);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    int empty = width - filled;
    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < filled && pos < (int)sz - 2; i++)
        buf[pos++] = (char)0xDB; /* █ */
    for (int i = 0; i < empty && pos < (int)sz - 2; i++)
        buf[pos++] = (char)0xB0; /* ░ */
    buf[pos++] = ']';
    buf[pos] = '\0';
}

int rate_limit_bucket_line(char *buf, size_t sz,
                            const char *label,
                            const rate_limit_bucket_t *b,
                            int label_width)
{
    if (!buf || sz == 0) return 0;

    if (!b || b->limit <= 0) {
        return snprintf(buf, sz, "  %-*s  (no data)", label_width, label ? label : "");
    }

    double pct = rate_limit_bucket_usage_pct(b);
    int used = rate_limit_bucket_used(b);
    char used_str[32], limit_str[32], remain_str[32], reset_str[64];
    rate_limit_fmt_count(used_str, sizeof(used_str), used);
    rate_limit_fmt_count(limit_str, sizeof(limit_str), b->limit);
    rate_limit_fmt_count(remain_str, sizeof(remain_str), b->remaining);
    rate_limit_fmt_seconds(reset_str, sizeof(reset_str),
                           rate_limit_bucket_remaining_seconds(b));

    char bar_buf[32];
    bar_ascii(bar_buf, sizeof(bar_buf), pct, 20);

    return snprintf(buf, sz,
        "  %-*s %s %5.1f%%  %s/%s used  (%s left, resets in %s)",
        label_width, label ? label : "",
        bar_buf, pct,
        used_str, limit_str,
        remain_str, reset_str);
}

char *rate_limit_format_display(const rate_limit_state_t *state)
{
    if (!state || !rate_limit_state_has_data(state)) {
        return strdup("No rate limit data yet — make an API request first.");
    }

    /* Build line-by-line */
    double age = rate_limit_state_age_seconds(state);
    char freshness[64];
    if (age < 5.0) {
        snprintf(freshness, sizeof(freshness), "just now");
    } else if (age < 60.0) {
        snprintf(freshness, sizeof(freshness), "%ds ago", (int)age);
    } else {
        char dur[32];
        rate_limit_fmt_seconds(dur, sizeof(dur), age);
        snprintf(freshness, sizeof(freshness), "%s ago", dur);
    }

    /* Use 2048 buffer — plenty for display output */
    char buf[4096];
    int pos = 0;

    const char *provider = state->provider[0] ? state->provider : "Provider";
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "%s Rate Limits (captured %s):\n\n",
                    provider, freshness);

    char line[512];
    rate_limit_bucket_line(line, sizeof(line),
                           "Requests/min", &state->requests_min, 14);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n", line);

    rate_limit_bucket_line(line, sizeof(line),
                           "Requests/hr", &state->requests_hour, 14);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n\n", line);

    rate_limit_bucket_line(line, sizeof(line),
                           "Tokens/min", &state->tokens_min, 14);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n", line);

    rate_limit_bucket_line(line, sizeof(line),
                           "Tokens/hr", &state->tokens_hour, 14);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n", line);

    /* Warnings for hot buckets */
    struct { const char *label; const rate_limit_bucket_t *bucket; } buckets[] = {
        {"requests/min", &state->requests_min},
        {"requests/hr",  &state->requests_hour},
        {"tokens/min",   &state->tokens_min},
        {"tokens/hr",    &state->tokens_hour},
    };
    bool has_warning = false;
    for (int i = 0; i < 4; i++) {
        if (buckets[i].bucket->limit > 0 &&
            rate_limit_bucket_usage_pct(buckets[i].bucket) >= 80.0) {
            if (!has_warning) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");
                has_warning = true;
            }
            rate_limit_fmt_seconds(line, sizeof(line),
                rate_limit_bucket_remaining_seconds(buckets[i].bucket));
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "  ⚠ %s at %.0f%% — resets in %s\n",
                buckets[i].label,
                rate_limit_bucket_usage_pct(buckets[i].bucket),
                line);
        }
    }

    return strdup(buf);
}

char *rate_limit_format_compact(const rate_limit_state_t *state)
{
    if (!state || !rate_limit_state_has_data(state)) {
        return strdup("No rate limit data.");
    }

    char buf[1024];
    int pos = 0;
    int written;

    const rate_limit_bucket_t *rm = &state->requests_min;
    const rate_limit_bucket_t *rh = &state->requests_hour;
    const rate_limit_bucket_t *tm = &state->tokens_min;
    const rate_limit_bucket_t *th = &state->tokens_hour;

    if (rm->limit > 0) {
        written = snprintf(buf + pos, sizeof(buf) - pos,
                          "RPM: %d/%d", rm->remaining, rm->limit);
        if (written > 0) pos += written;
    }
    if (rh->limit > 0) {
        char rem_str[16], lim_str[16], reset_str[32];
        rate_limit_fmt_count(rem_str, sizeof(rem_str), rh->remaining);
        rate_limit_fmt_count(lim_str, sizeof(lim_str), rh->limit);
        rate_limit_fmt_seconds(reset_str, sizeof(reset_str),
                               rate_limit_bucket_remaining_seconds(rh));
        written = snprintf(buf + pos, sizeof(buf) - pos,
                          "%sRPH: %s/%s (resets %s)",
                          pos > 0 ? " | " : "",
                          rem_str, lim_str, reset_str);
        if (written > 0) pos += written;
    }
    if (tm->limit > 0) {
        char rem_str[16], lim_str[16];
        rate_limit_fmt_count(rem_str, sizeof(rem_str), tm->remaining);
        rate_limit_fmt_count(lim_str, sizeof(lim_str), tm->limit);
        written = snprintf(buf + pos, sizeof(buf) - pos,
                          "%sTPM: %s/%s",
                          pos > 0 ? " | " : "",
                          rem_str, lim_str);
        if (written > 0) pos += written;
    }
    if (th->limit > 0) {
        char rem_str[16], lim_str[16], reset_str[32];
        rate_limit_fmt_count(rem_str, sizeof(rem_str), th->remaining);
        rate_limit_fmt_count(lim_str, sizeof(lim_str), th->limit);
        rate_limit_fmt_seconds(reset_str, sizeof(reset_str),
                               rate_limit_bucket_remaining_seconds(th));
        written = snprintf(buf + pos, sizeof(buf) - pos,
                          "%sTPH: %s/%s (resets %s)",
                          pos > 0 ? " | " : "",
                          rem_str, lim_str, reset_str);
        if (written > 0) pos += written;
    }

    return strdup(buf);
}

void rate_limit_reset(void)
{
    /* Currently no global cached state to reset.
     * Placeholder for future if needed. */
}
