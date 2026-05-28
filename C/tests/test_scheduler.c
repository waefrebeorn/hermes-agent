/*
 * test_scheduler.c — Cron schedule parsing unit tests.
 *
 * Tests: parse_cron_field (wildcard, slash-N, numeric, NULL),
 * parse_schedule (5-field, ISO shorthand), should_run (time matching).
 *
 * These tests use functions that were made non-static for testability.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       -I lib/libcron -I src/cron tests/test_scheduler.c src/cron/scheduler.c \
 *       lib/libcron/cron.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_scheduler -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_scheduler
 */

#include "scheduler.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); fflush(stdout); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * 1. parse_cron_field
 * ================================================================ */

static void test_parse_cron_field_null(void) {
    int val = -99, interval = -99;
    TEST("NULL str returns false");
    assert(parse_cron_field(NULL, &val, &interval) == false);
    PASS();
}

static void test_parse_cron_field_empty(void) {
    int val = -99, interval = -99;
    TEST("empty str returns false");
    assert(parse_cron_field("", &val, &interval) == false);
    PASS();
}

static void test_parse_cron_field_wildcard(void) {
    int val = -99, interval = -99;
    TEST("'*' sets value = -1, interval = 0");
    assert(parse_cron_field("*", &val, &interval));
    assert(val == -1);
    assert(interval == 0);
    PASS();
}

static void test_parse_cron_field_slash(void) {
    int val = -99, interval = -99;
    TEST("'*/5' sets value = -1, interval = 5");
    assert(parse_cron_field("*/5", &val, &interval));
    assert(val == -1);
    assert(interval == 5);
    PASS();
}

static void test_parse_cron_field_slash_15(void) {
    int val = -99, interval = -99;
    TEST("'*/15' sets interval = 15");
    assert(parse_cron_field("*/15", &val, &interval));
    assert(val == -1);
    assert(interval == 15);
    PASS();
}

static void test_parse_cron_field_numeric(void) {
    int val = -99, interval = -99;
    TEST("'42' sets value = 42, interval = 0");
    assert(parse_cron_field("42", &val, &interval));
    assert(val == 42);
    assert(interval == 0);
    PASS();
}

static void test_parse_cron_field_zero(void) {
    int val = -99, interval = -99;
    TEST("'0' sets value = 0");
    assert(parse_cron_field("0", &val, &interval));
    assert(val == 0);
    PASS();
}

static void test_parse_cron_field_default_interval(void) {
    int val = -99, interval = -99;
    TEST("'*/0' defaults interval to 5");
    assert(parse_cron_field("*/0", &val, &interval));
    assert(val == -1);
    assert(interval == 5);
    PASS();
}

/* ================================================================
 * 2. parse_schedule
 * ================================================================ */

static void test_parse_schedule_null(void) {
    cron_schedule_t sched;
    TEST("NULL expr returns false");
    assert(parse_schedule(NULL, &sched) == false);
    PASS();
}

static void test_parse_schedule_5field(void) {
    cron_schedule_t sched;
    memset(&sched, 0xFF, sizeof(sched));  /* poison */

    TEST("5-field cron expression");
    assert(parse_schedule("0 9 * * 1-5", &sched));
    assert(sched.minute == 0);
    assert(sched.hour == 9);
    assert(sched.day == -1);    /* * */
    assert(sched.month == -1);  /* * */
    assert(sched.weekday == 1); /* "1-5" → atoi = 1 */
    PASS();
}

static void test_parse_schedule_every_5min(void) {
    cron_schedule_t sched;
    memset(&sched, 0xFF, sizeof(sched));

    TEST("'*/5 * * * *' interval minutes");
    assert(parse_schedule("*/5 * * * *", &sched));
    assert(sched.interval_minutes == 5);
    assert(sched.minute == -1);
    assert(sched.hour == -1);
    assert(sched.day == -1);
    assert(sched.month == -1);
    assert(sched.weekday == -1);
    PASS();
}

static void test_parse_schedule_partial_fields(void) {
    cron_schedule_t sched;
    memset(&sched, 0xFF, sizeof(sched));

    TEST("fewer than 5 fields returns false");
    assert(parse_schedule("0 9", &sched) == false);
    PASS();
}

/* ================================================================
 * 3. should_run
 * ================================================================ */

static void test_should_run_interval_never_ran(void) {
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.interval_minutes = 5;
    sched.minute = -1;

    TEST("interval mode, never ran (last_run=0) returns true");
    assert(should_run(&sched, 1000, 0) == true);
    PASS();
}

static void test_should_run_interval_not_due(void) {
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.interval_minutes = 5;
    sched.minute = -1;

    TEST("interval mode, last_run=now returns false");
    assert(should_run(&sched, 1000, 995) == false);  /* Only 5s ago */
    PASS();
}

static void test_should_run_interval_due(void) {
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.interval_minutes = 5;
    sched.minute = -1;

    TEST("interval mode, 5+ min elapsed returns true");
    assert(should_run(&sched, 1000, 700) == true);  /* 300s = 5min */
    PASS();
}

static void test_should_run_exact_minute(void) {
    /*
     * Set fixed time: 2025-01-15 09:30:00
     * should_run with minute=30, hour=9, day=15, month=1,
     * last_run far in the past
     */
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.minute = 30;
    sched.hour = 9;
    sched.day = 15;
    sched.month = 1;
    sched.weekday = -1;
    sched.interval_minutes = 0;

    /* Construct time: 2025-01-15 09:30:00 UTC */
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = 125;  /* 2025 */
    tm_val.tm_mon = 0;     /* Jan */
    tm_val.tm_mday = 15;
    tm_val.tm_hour = 9;
    tm_val.tm_min = 30;
    tm_val.tm_sec = 0;
    time_t now = timegm(&tm_val);

    TEST("exact minute match returns true");
    assert(should_run(&sched, now, 0) == true);
    PASS();
}

static void test_should_run_wrong_minute(void) {
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.minute = 0;    /* Top of hour */
    sched.hour = -1;
    sched.day = -1;
    sched.month = -1;
    sched.weekday = -1;
    sched.interval_minutes = 0;

    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = 125;
    tm_val.tm_mon = 0;
    tm_val.tm_mday = 15;
    tm_val.tm_hour = 9;
    tm_val.tm_min = 30;  /* Not minute 0 */
    tm_val.tm_sec = 0;
    time_t now = timegm(&tm_val);

    TEST("wrong minute returns false");
    assert(should_run(&sched, now, 0) == false);
    PASS();
}

static void test_should_run_all_wildcards(void) {
    cron_schedule_t sched;
    memset(&sched, 0, sizeof(sched));
    sched.minute = -1;   /* * */
    sched.hour = -1;
    sched.day = -1;
    sched.month = -1;
    sched.weekday = -1;
    sched.interval_minutes = 0;

    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = 125;
    tm_val.tm_mon = 0;
    tm_val.tm_mday = 15;
    tm_val.tm_hour = 9;
    tm_val.tm_min = 30;
    tm_val.tm_sec = 0;
    time_t now = timegm(&tm_val);

    TEST("all wildcards, never ran returns true");
    assert(should_run(&sched, now, 0) == true);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Scheduler/Cron Parser Tests ===\n\n");

    /* Set UTC for deterministic should_run tests */
    setenv("TZ", "UTC", 1);
    tzset();

    printf("--- parse_cron_field ---\n");
    test_parse_cron_field_null();
    test_parse_cron_field_empty();
    test_parse_cron_field_wildcard();
    test_parse_cron_field_slash();
    test_parse_cron_field_slash_15();
    test_parse_cron_field_numeric();
    test_parse_cron_field_zero();
    test_parse_cron_field_default_interval();

    printf("\n--- parse_schedule ---\n");
    test_parse_schedule_null();
    test_parse_schedule_5field();
    test_parse_schedule_every_5min();
    test_parse_schedule_partial_fields();

    printf("\n--- should_run ---\n");
    test_should_run_interval_never_ran();
    test_should_run_interval_not_due();
    test_should_run_interval_due();
    test_should_run_exact_minute();
    test_should_run_wrong_minute();
    test_should_run_all_wildcards();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
