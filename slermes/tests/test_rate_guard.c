/*
 * test_rate_guard.c — Tests for librateguard (cross-session rate limit guard).
 * Tests: record/remaining/clear cycle, genuine detection, format.
 */

#include "rate_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  format_remaining tests
 * ================================================================ */

static void test_format_seconds(void)
{
    char *r = rate_guard_format_remaining(0);
    TEST("0s format not null", r != NULL);
    if (r) { TEST("0s", strcmp(r, "0s") == 0); free(r); }
}

static void test_format_under_minute(void)
{
    char *r = rate_guard_format_remaining(45.0);
    TEST("45s format not null", r != NULL);
    if (r) { TEST("45s", strcmp(r, "45s") == 0); free(r); }
}

static void test_format_minutes(void)
{
    char *r = rate_guard_format_remaining(125.0);
    TEST("2m5s format not null", r != NULL);
    if (r) { TEST("2m5s", strcmp(r, "2m 5s") == 0); free(r); }
}

static void test_format_exact_minute(void)
{
    char *r = rate_guard_format_remaining(300.0);
    TEST("5m format not null", r != NULL);
    if (r) { TEST("5m", strcmp(r, "5m") == 0); free(r); }
}

static void test_format_hours(void)
{
    char *r = rate_guard_format_remaining(7500.0);
    TEST("2h5m format not null", r != NULL);
    if (r) { TEST("2h5m", strcmp(r, "2h 5m") == 0); free(r); }
}

static void test_format_exact_hour(void)
{
    char *r = rate_guard_format_remaining(7200.0);
    TEST("2h format not null", r != NULL);
    if (r) { TEST("2h", strcmp(r, "2h") == 0); free(r); }
}

static void test_format_negative(void)
{
    char *r = rate_guard_format_remaining(-10.0);
    TEST("negative format not null", r != NULL);
    if (r) { TEST("negative -> 0s", strcmp(r, "0s") == 0); free(r); }
}

/* ================================================================
 *  is_genuine tests
 * ================================================================ */

static void test_genuine_null_state(void)
{
    TEST("null state is not genuine", !rate_guard_is_genuine(NULL));
}

static void test_genuine_exhausted_long_window(void)
{
    rate_guard_state_t state = {0};
    rate_guard_parse_headers(&state, 0, 120.0, -1, -1, -1, -1, -1, -1, 429);
    TEST("exhausted + 120s reset IS genuine",
         rate_guard_is_genuine(&state));
}

static void test_genuine_not_exhausted(void)
{
    rate_guard_state_t state = {0};
    rate_guard_parse_headers(&state, 5, 120.0, -1, -1, -1, -1, -1, -1, 200);
    TEST("remaining>0 is NOT genuine",
         !rate_guard_is_genuine(&state));
}

static void test_genuine_short_window(void)
{
    rate_guard_state_t state = {0};
    rate_guard_parse_headers(&state, 0, 10.0, -1, -1, -1, -1, -1, -1, 429);
    TEST("exhausted + 10s reset is NOT genuine (transient)",
         !rate_guard_is_genuine(&state));
}

static void test_genuine_no_headers(void)
{
    rate_guard_state_t state = {0};
    rate_guard_parse_headers(&state, -1, -1, -1, -1, -1, -1, -1, -1, 429);
    TEST("no bucket data is NOT genuine",
         !rate_guard_is_genuine(&state));
}

static void test_genuine_hour_bucket(void)
{
    rate_guard_state_t state = {0};
    rate_guard_parse_headers(&state, -1, -1, 0, 3600.0, -1, -1, -1, -1, 429);
    TEST("hour bucket exhausted + 3600s IS genuine",
         rate_guard_is_genuine(&state));
}

/* ================================================================
 *  Record/remaining/clear integration tests
 * ================================================================ */

static void test_record_and_remaining(void)
{
    /* Clean state */
    rate_guard_clear("test_provider");

    /* Should be 0 when nothing recorded */
    double rem = rate_guard_remaining("test_provider");
    TEST("no record = 0 remaining", rem == 0.0);

    /* Record a short cooldown */
    rate_guard_record("test_provider", NULL, 10.0);
    rem = rate_guard_remaining("test_provider");
    TEST("recorded 10s cooldown > 0", rem > 0.0);
    TEST("recorded cooldown <= 10.5", rem <= 10.5);

    /* Clear */
    rate_guard_clear("test_provider");
    rem = rate_guard_remaining("test_provider");
    TEST("after clear = 0 remaining", rem == 0.0);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== Rate Guard Library Tests ===\n");

    /* Format tests */
    test_format_seconds();
    test_format_under_minute();
    test_format_minutes();
    test_format_exact_minute();
    test_format_hours();
    test_format_exact_hour();
    test_format_negative();

    /* Genuine detection tests */
    test_genuine_null_state();
    test_genuine_exhausted_long_window();
    test_genuine_not_exhausted();
    test_genuine_short_window();
    test_genuine_no_headers();
    test_genuine_hour_bucket();

    /* Integration tests */
    test_record_and_remaining();

    printf("Tests: %d  Passed: %d  Failed: %d\n", tests, passed, failed);
    return failed > 0 ? 1 : 0;
}
