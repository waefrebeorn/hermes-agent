/*
 * test_rate_limit.c — Tests for lib/libratelimit/rate_limit.c
 */

#include "rate_limit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define ASSERT_STREQ(a, b, msg) do { \
    if (strcmp((a), (b)) != 0) { \
        char _buf[256]; \
        snprintf(_buf, sizeof(_buf), "%s: expected \"%s\", got \"%s\"", msg, (b), (a)); \
        FAIL(_buf); return; \
    } \
} while(0)

/* ─── Bucket tests ─────────────────────────────────────── */

static void test_bucket_init_zero(void)
{
    TEST("bucket init zero");
    rate_limit_bucket_t b;
    rate_limit_bucket_init(&b);
    ASSERT(b.limit == 0 && b.remaining == 0, "all fields should be zero");
    PASS();
}

static void test_bucket_used_clamped(void)
{
    TEST("bucket used clamped at zero");
    rate_limit_bucket_t b = { .limit = 100, .remaining = 200, .reset_seconds = 60, .captured_at = 1000 };
    int u = rate_limit_bucket_used(&b);
    ASSERT(u == 0, "used should be 0 when remaining > limit");
    PASS();
}

static void test_bucket_usage_pct_50(void)
{
    TEST("bucket usage pct 50%");
    rate_limit_bucket_t b = { .limit = 100, .remaining = 50, .reset_seconds = 60, .captured_at = 1000 };
    double p = rate_limit_bucket_usage_pct(&b);
    ASSERT(fabs(p - 50.0) < 0.001, "should be 50%");
    PASS();
}

static void test_bucket_null_safe(void)
{
    TEST("bucket null safe");
    ASSERT(rate_limit_bucket_used(NULL) == 0, "used");
    ASSERT(fabs(rate_limit_bucket_usage_pct(NULL)) < 0.001, "pct");
    ASSERT(fabs(rate_limit_bucket_remaining_seconds(NULL)) < 0.001, "rem");
    PASS();
}

/* ─── State tests ──────────────────────────────────────── */

static void test_state_init(void)
{
    TEST("state init");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "test-provider");
    ASSERT(!rate_limit_state_has_data(&s), "no data yet");
    ASSERT(strcmp(s.provider, "test-provider") == 0, "provider set");
    PASS();
}

static void test_state_age_inf(void)
{
    TEST("state age inf without data");
    rate_limit_state_t s;
    rate_limit_state_init(&s, NULL);
    double a = rate_limit_state_age_seconds(&s);
    ASSERT(isinf(a), "should be INFINITY");
    PASS();
}

/* ─── Header parsing tests ─────────────────────────────── */

static void test_parse_no_headers(void)
{
    TEST("parse no rate-limit headers");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "test");
    const char *keys[] = {"content-type", "date"};
    const char *vals[] = {"application/json", "Mon, 01 Jan 2024"};
    bool ok = rate_limit_parse_headers(&s, keys, vals, 2, "test");
    ASSERT(!ok, "should return false");
    ASSERT(!rate_limit_state_has_data(&s), "no data");
    PASS();
}

static void test_parse_full_headers(void)
{
    TEST("parse full rate-limit headers");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "nous");
    const char *keys[] = {
        "x-ratelimit-limit-requests", "x-ratelimit-remaining-requests", "x-ratelimit-reset-requests",
        "x-ratelimit-limit-requests-1h", "x-ratelimit-remaining-requests-1h", "x-ratelimit-reset-requests-1h",
        "x-ratelimit-limit-tokens", "x-ratelimit-remaining-tokens", "x-ratelimit-reset-tokens",
        "x-ratelimit-limit-tokens-1h", "x-ratelimit-remaining-tokens-1h", "x-ratelimit-reset-tokens-1h",
    };
    const char *vals[] = {
        "100", "75", "30",
        "1000", "800", "3600",
        "100000", "50000", "45",
        "1000000", "750000", "7200",
    };
    bool ok = rate_limit_parse_headers(&s, keys, vals, 12, "nous");
    ASSERT(ok, "should return true");
    ASSERT(rate_limit_state_has_data(&s), "has data");
    ASSERT(strcmp(s.provider, "nous") == 0, "provider set");
    ASSERT(s.requests_min.limit == 100, "req min limit");
    ASSERT(s.requests_min.remaining == 75, "req min remaining");
    ASSERT(fabs(s.requests_min.reset_seconds - 30.0) < 0.001, "req min reset");
    ASSERT(s.tokens_min.limit == 100000, "tok min limit");
    ASSERT(s.tokens_min.remaining == 50000, "tok min remaining");
    PASS();
}

/* ─── Formatting tests ─────────────────────────────────── */

static void test_fmt_count_thousands(void)
{
    TEST("fmt count 33599 -> 33.6K");
    char buf[32];
    rate_limit_fmt_count(buf, sizeof(buf), 33599);
    ASSERT_STREQ(buf, "33.6K", "33.6K");
    PASS();
}

static void test_fmt_count_millions(void)
{
    TEST("fmt count 7999856 -> 8.0M");
    char buf[32];
    rate_limit_fmt_count(buf, sizeof(buf), 7999856);
    ASSERT_STREQ(buf, "8.0M", "8.0M");
    PASS();
}

static void test_fmt_count_small(void)
{
    TEST("fmt count 799 -> 799");
    char buf[32];
    rate_limit_fmt_count(buf, sizeof(buf), 799);
    ASSERT_STREQ(buf, "799", "799");
    PASS();
}

static void test_fmt_seconds_minutes(void)
{
    TEST("fmt seconds 134 -> 2m 14s");
    char buf[32];
    rate_limit_fmt_seconds(buf, sizeof(buf), 134);
    ASSERT_STREQ(buf, "2m 14s", "2m 14s");
    PASS();
}

static void test_fmt_seconds_hours(void)
{
    TEST("fmt seconds 3720 -> 1h 2m");
    char buf[32];
    rate_limit_fmt_seconds(buf, sizeof(buf), 3720);
    ASSERT_STREQ(buf, "1h 2m", "1h 2m");
    PASS();
}

static void test_fmt_seconds_just_seconds(void)
{
    TEST("fmt seconds 58 -> 58s");
    char buf[32];
    rate_limit_fmt_seconds(buf, sizeof(buf), 58);
    ASSERT_STREQ(buf, "58s", "58s");
    PASS();
}

static void test_bucket_line_no_data(void)
{
    TEST("bucket line no data");
    char buf[256];
    rate_limit_bucket_t b;
    rate_limit_bucket_init(&b);
    int n = rate_limit_bucket_line(buf, sizeof(buf), "Requests/min", &b, 14);
    ASSERT(n > 0, "should produce output");
    ASSERT(strstr(buf, "(no data)") != NULL, "should say no data");
    PASS();
}

static void test_format_display_no_data(void)
{
    TEST("format display no data");
    char *s = rate_limit_format_display(NULL);
    ASSERT(s != NULL, "should not be null");
    ASSERT(strstr(s, "No rate limit data") != NULL, "should say no data");
    free(s);
    PASS();
}

static void test_format_display_with_data(void)
{
    TEST("format display with data");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "nous");
    const char *keys[] = {
        "x-ratelimit-limit-requests", "x-ratelimit-remaining-requests", "x-ratelimit-reset-requests",
        "x-ratelimit-limit-tokens", "x-ratelimit-remaining-tokens", "x-ratelimit-reset-tokens",
    };
    const char *vals[] = {"100", "50", "60", "100000", "95000", "30"};
    rate_limit_parse_headers(&s, keys, vals, 6, "nous");
    char *display = rate_limit_format_display(&s);
    ASSERT(display != NULL, "should produce display");
    ASSERT(strstr(display, "nous") != NULL, "should contain provider");
    ASSERT(strstr(display, "50.0%") != NULL || strstr(display, "50 %") != NULL, "should show 50%");
    free(display);
    PASS();
}

static void test_format_compact_no_data(void)
{
    TEST("format compact no data");
    char *s = rate_limit_format_compact(NULL);
    ASSERT(s != NULL, "should not be null");
    ASSERT(strstr(s, "No rate limit data") != NULL, "should say no data");
    free(s);
    PASS();
}

static void test_format_compact_with_data(void)
{
    TEST("format compact with data");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "test");
    const char *keys[] = {
        "x-ratelimit-limit-requests", "x-ratelimit-remaining-requests",
        "x-ratelimit-limit-tokens", "x-ratelimit-remaining-tokens",
    };
    const char *vals[] = {"100", "75", "100000", "50000"};
    rate_limit_parse_headers(&s, keys, vals, 4, "test");
    char *compact = rate_limit_format_compact(&s);
    ASSERT(compact != NULL, "should produce compact");
    ASSERT(strstr(compact, "RPM:") != NULL, "should contain RPM");
    ASSERT(strstr(compact, "TPM:") != NULL, "should contain TPM");
    free(compact);
    PASS();
}

/* ─── Warning tests ────────────────────────────────────── */

static void test_warning_at_80pct(void)
{
    TEST("warning at 80% usage");
    rate_limit_state_t s;
    rate_limit_state_init(&s, "test");
    const char *keys[] = {
        "x-ratelimit-limit-requests", "x-ratelimit-remaining-requests", "x-ratelimit-reset-requests",
    };
    const char *vals[] = {"100", "15", "30"};  /* 85% used */
    rate_limit_parse_headers(&s, keys, vals, 3, "test");
    char *display = rate_limit_format_display(&s);
    ASSERT(display != NULL, "display should exist");
    /* 85/100 = 85% >= 80% → should trigger ⚠ warning */
    ASSERT(strstr(display, "⚠") != NULL, "should have warning symbol");
    free(display);
    PASS();
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    printf("=== rate_limit tests ===\n\n");

    /* Bucket */
    test_bucket_init_zero();
    test_bucket_used_clamped();
    test_bucket_usage_pct_50();
    test_bucket_null_safe();

    /* State */
    test_state_init();
    test_state_age_inf();

    /* Parsing */
    test_parse_no_headers();
    test_parse_full_headers();

    /* Formatting */
    test_fmt_count_thousands();
    test_fmt_count_millions();
    test_fmt_count_small();
    test_fmt_seconds_minutes();
    test_fmt_seconds_hours();
    test_fmt_seconds_just_seconds();
    test_bucket_line_no_data();
    test_format_display_no_data();
    test_format_display_with_data();
    test_format_compact_no_data();
    test_format_compact_with_data();

    /* Warnings */
    test_warning_at_80pct();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
