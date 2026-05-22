#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * 1. cron_parse — special strings
 * ================================================================ */
static void test_parse_special(void) {
    TEST("@yearly");
    cron_expr_t *c = cron_parse("@yearly", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("@annually");
    c = cron_parse("@annually", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("@monthly");
    c = cron_parse("@monthly", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("@weekly");
    c = cron_parse("@weekly", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("@daily");
    c = cron_parse("@daily", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("@hourly");
    c = cron_parse("@hourly", NULL);
    assert(c != NULL); cron_free(c);
    PASS();
}

/* ================================================================
 * 2. cron_parse — wildcard, specific, step, range, names
 * ================================================================ */
static void test_parse_valid(void) {
    TEST("* * * * * (every minute)");
    cron_expr_t *c = cron_parse("* * * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 0 * * * (midnight daily)");
    c = cron_parse("0 0 * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("30 9 * * 1-5 (weekdays 9:30)");
    c = cron_parse("30 9 * * 1-5", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("*/5 * * * * (every 5 minutes)");
    c = cron_parse("*/5 * * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 */2 * * * (every 2 hours)");
    c = cron_parse("0 */2 * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 0 1 JAN * (Jan 1st)");
    c = cron_parse("0 0 1 JAN *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 0 * * MON-FRI (weekdays)");
    c = cron_parse("0 0 * * MON-FRI", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 0 1 jul,jan * (comma months)");
    c = cron_parse("0 0 1 jul,jan *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0,15,30,45 * * * * (comma mins)");
    c = cron_parse("0,15,30,45 * * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("0 9,18 * * 1-5 (comma hours + range)");
    c = cron_parse("0 9,18 * * 1-5", NULL);
    assert(c != NULL); cron_free(c);
    PASS();

    TEST("*/15 * * * * (every 15 min)");
    c = cron_parse("*/15 * * * *", NULL);
    assert(c != NULL); cron_free(c);
    PASS();
}

/* ================================================================
 * 3. cron_parse — error cases
 * ================================================================ */
static void test_parse_errors(void) {
    char *err = NULL;

    TEST("NULL expression");
    cron_expr_t *c = cron_parse(NULL, &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();

    TEST("empty string");
    c = cron_parse("", &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();

    TEST("too few fields (4)");
    c = cron_parse("0 0 * *", &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();

    TEST("too few fields (2)");
    c = cron_parse("0 0", &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();

    TEST("invalid month name");
    c = cron_parse("0 0 1 XXX *", &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();

    TEST("negative step");
    c = cron_parse("*/-1 * * * *", &err);
    assert(c == NULL);
    assert(err != NULL);
    free(err); err = NULL;
    PASS();
}

/* ================================================================
 * 4. cron_match
 * ================================================================ */
static void test_match(void) {
    /* Every minute */
    cron_expr_t *c = cron_parse("* * * * *", NULL);
    assert(c);

    struct tm tm = {0};
    tm.tm_year = 126; tm.tm_mon = 4; tm.tm_mday = 24;
    tm.tm_hour = 10; tm.tm_min = 30;
    tm.tm_wday = 0;
    mktime(&tm);

    TEST("wildcard matches any time");
    assert(cron_match(c, &tm));
    PASS();
    cron_free(c);

    /* Daily at midnight */
    c = cron_parse("0 0 * * *", NULL);
    assert(c);

    TEST("daily midnight — correct time");
    tm.tm_hour = 0; tm.tm_min = 0;
    assert(cron_match(c, &tm));
    PASS();

    TEST("daily midnight — wrong hour");
    tm.tm_hour = 1;
    assert(!cron_match(c, &tm));
    PASS();

    TEST("daily midnight — wrong minute");
    tm.tm_hour = 0; tm.tm_min = 5;
    assert(!cron_match(c, &tm));
    PASS();
    cron_free(c);

    /* Weekday 9:30 */
    c = cron_parse("30 9 * * 1-5", NULL);
    assert(c);

    TEST("weekday 9:30 — Monday correct");
    tm.tm_hour = 9; tm.tm_min = 30;
    tm.tm_wday = 1;
    assert(cron_match(c, &tm));
    PASS();

    TEST("weekday 9:30 — Sunday wrong");
    tm.tm_wday = 0;
    assert(!cron_match(c, &tm));
    PASS();

    TEST("weekday 9:30 — wrong minute");
    tm.tm_wday = 1; tm.tm_min = 31;
    assert(!cron_match(c, &tm));
    PASS();
    cron_free(c);

    /* Every 5 minutes */
    c = cron_parse("*/5 * * * *", NULL);
    assert(c);

    TEST("every 5 min — 0 matches");
    tm.tm_min = 0;
    assert(cron_match(c, &tm));
    PASS();

    TEST("every 5 min — 15 matches");
    tm.tm_min = 15;
    assert(cron_match(c, &tm));
    PASS();

    TEST("every 5 min — 17 doesn't match");
    tm.tm_min = 17;
    assert(!cron_match(c, &tm));
    PASS();
    cron_free(c);

    /* Specific date: 0 0 1 1 * (Jan 1) */
    c = cron_parse("0 0 1 1 *", NULL);
    assert(c);

    TEST("Jan 1 midnight — correct");
    tm.tm_mon = 0; tm.tm_mday = 1;
    tm.tm_hour = 0; tm.tm_min = 0;
    assert(cron_match(c, &tm));
    PASS();

    TEST("Jan 1 midnight — wrong month");
    tm.tm_mon = 5;
    assert(!cron_match(c, &tm));
    PASS();

    TEST("Jan 1 midnight — wrong day");
    tm.tm_mon = 0; tm.tm_mday = 2;
    assert(!cron_match(c, &tm));
    PASS();
    cron_free(c);
}

/* ================================================================
 * 5. cron_match — null safety
 * ================================================================ */
static void test_match_null(void) {
    struct tm tm = {0};
    mktime(&tm);

    TEST("NULL cron returns false");
    assert(!cron_match(NULL, &tm));
    PASS();

    TEST("NULL tm returns false");
    cron_expr_t *c = cron_parse("* * * * *", NULL);
    assert(c);
    assert(!cron_match(c, NULL));
    PASS();
    cron_free(c);
}

/* ================================================================
 * 6. cron_next
 * ================================================================ */
static void test_next(void) {
    struct tm from, out;
    memset(&from, 0, sizeof(from));

    /* @hourly: from 2026-05-24 10:30 → next is 11:00 */
    cron_expr_t *c = cron_parse("@hourly", NULL);
    assert(c);

    from.tm_year = 126; from.tm_mon = 4; from.tm_mday = 24;
    from.tm_hour = 10; from.tm_min = 30;
    from.tm_isdst = -1;
    mktime(&from);

    TEST("@hourly next from 10:30 → 11:00");
    assert(cron_next(c, &from, &out));
    assert(out.tm_hour == 11);
    assert(out.tm_min == 0);
    PASS();
    cron_free(c);

    /* Daily at midnight: from 2026-05-24 23:30 → next is 2026-05-25 00:00 */
    c = cron_parse("0 0 * * *", NULL);
    assert(c);

    memset(&from, 0, sizeof(from));
    from.tm_year = 126; from.tm_mon = 4; from.tm_mday = 24;
    from.tm_hour = 23; from.tm_min = 30;
    from.tm_isdst = -1;
    mktime(&from);

    TEST("daily midnight — next from 23:30 → next day 00:00");
    assert(cron_next(c, &from, &out));
    assert(out.tm_mday == 25);
    assert(out.tm_hour == 0);
    assert(out.tm_min == 0);
    PASS();
    cron_free(c);

    /* Every 5 minutes: from 10:32 → next is 10:35 */
    c = cron_parse("*/5 * * * *", NULL);
    assert(c);

    memset(&from, 0, sizeof(from));
    from.tm_year = 126; from.tm_mon = 4; from.tm_mday = 24;
    from.tm_hour = 10; from.tm_min = 32;
    from.tm_isdst = -1;
    mktime(&from);

    TEST("*/5 next from 10:32 → 10:35");
    assert(cron_next(c, &from, &out));
    assert(out.tm_hour == 10);
    assert(out.tm_min == 35);
    PASS();
    cron_free(c);

    /* Every 2 hours: from 10:30 → next is 12:00 */
    memset(&from, 0, sizeof(from));
    c = cron_parse("0 */2 * * *", NULL);
    assert(c);

    from.tm_year = 126; from.tm_mon = 4; from.tm_mday = 24;
    from.tm_hour = 10; from.tm_min = 30;
    from.tm_isdst = -1;
    mktime(&from);

    TEST("*/2 next from 10:30 → 12:00");
    assert(cron_next(c, &from, &out));
    assert(out.tm_hour == 12);
    assert(out.tm_min == 0);
    PASS();
    cron_free(c);
}

/* ================================================================
 * 7. cron_next — exact match time skips forward
 * ================================================================ */
static void test_next_exact(void) {
    cron_expr_t *c = cron_parse("0 10 * * *", NULL);
    assert(c);

    struct tm from, out;
    memset(&from, 0, sizeof(from));
    from.tm_year = 126; from.tm_mon = 4; from.tm_mday = 24;
    from.tm_hour = 10; from.tm_min = 0;
    from.tm_isdst = -1;
    mktime(&from);

    TEST("daily 10:00 — from exact 10:00 → next day 10:00");
    assert(cron_next(c, &from, &out));
    assert(out.tm_hour == 10);
    assert(out.tm_min == 0);
    assert(out.tm_mday != from.tm_mday || out.tm_mon != from.tm_mon);
    PASS();
    cron_free(c);
}

/* ================================================================
 * 8. cron_next — null safety
 * ================================================================ */
static void test_next_null(void) {
    struct tm from, out;
    memset(&from, 0, sizeof(from));

    TEST("NULL cron returns false");
    assert(!cron_next(NULL, &from, &out));
    PASS();

    TEST("NULL from returns false");
    cron_expr_t *c = cron_parse("* * * * *", NULL);
    assert(c);
    assert(!cron_next(c, NULL, &out));
    PASS();
    cron_free(c);

    TEST("NULL out returns false");
    c = cron_parse("* * * * *", NULL);
    assert(c);
    assert(!cron_next(c, &from, NULL));
    PASS();
    cron_free(c);
}

/* ================================================================
 * 9. cron_describe
 * ================================================================ */
static void test_describe(void) {
    char *desc;

    TEST("* * * * * → 'Every minute'");
    cron_expr_t *c = cron_parse("* * * * *", NULL);
    assert(c);
    desc = cron_describe(c);
    assert(strstr(desc, "Every minute") != NULL);
    free(desc); cron_free(c);
    PASS();

    TEST("0 * * * * → 'Cron:' (describe fallthrough)");
    c = cron_parse("0 * * * *", NULL);
    assert(c);
    desc = cron_describe(c);
    assert(strstr(desc, "Cron:") != NULL);
    free(desc); cron_free(c);
    PASS();

    TEST("0 9 * * * → 'At 09:00'");
    c = cron_parse("0 9 * * *", NULL);
    assert(c);
    desc = cron_describe(c);
    assert(strstr(desc, "At") != NULL);
    free(desc); cron_free(c);
    PASS();

    TEST("NULL → 'invalid'");
    desc = cron_describe(NULL);
    assert(strcmp(desc, "invalid") == 0);
    free(desc);
    PASS();
}

/* ================================================================
 * 10. cron_free — null safety
 * ================================================================ */
static void test_free_null(void) {
    TEST("cron_free(NULL) no crash");
    cron_free(NULL);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void) {
    printf("=== Cron Library Tests ===\n");

    test_parse_special();
    test_parse_valid();
    test_parse_errors();
    test_match();
    test_match_null();
    test_next();
    test_next_exact();
    test_next_null();
    test_describe();
    test_free_null();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
