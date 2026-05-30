/*
 * test_cron.c — Tests for libcron (cron expression parser).
 *
 * Tests: parse various formats, match, next, describe, free.
 * Known-answer tests against standard cron expressions.
 */
#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

static void set_tm(struct tm *tm, int year, int mon, int mday,
                   int hour, int min, int sec) {
    memset(tm, 0, sizeof(*tm));
    tm->tm_year = year - 1900;
    tm->tm_mon  = mon - 1;
    tm->tm_mday = mday;
    tm->tm_hour = hour;
    tm->tm_min  = min;
    tm->tm_sec  = sec;
    tm->tm_isdst = -1;
    mktime(tm);  /* normalize */
}

/* ================================================================
 *  Parse Tests
 * ================================================================ */

static void test_parse_valid(void) {
    /* Standard 5-field */
    cron_expr_t *c = cron_parse("0 0 * * *", NULL);
    TEST("every midnight", c != NULL);
    cron_free(c);

    /* Wildcards */
    c = cron_parse("* * * * *", NULL);
    TEST("every minute", c != NULL);
    cron_free(c);

    /* Step values */
    c = cron_parse("*/5 * * * *", NULL);
    TEST("every 5 min", c != NULL);
    cron_free(c);

    /* Specific values */
    c = cron_parse("30 9 * * 1-5", NULL);
    TEST("weekdays 9:30", c != NULL);
    cron_free(c);

    /* List values */
    c = cron_parse("0,15,30,45 * * * *", NULL);
    TEST("every quarter hour", c != NULL);
    cron_free(c);

    /* Month names */
    c = cron_parse("0 0 1 JAN *", NULL);
    TEST("month name JAN", c != NULL);
    cron_free(c);

    /* Day names */
    c = cron_parse("0 0 * * MON-FRI", NULL);
    TEST("day names MON-FRI", c != NULL);
    cron_free(c);

    /* @-specials */
    c = cron_parse("@hourly", NULL);
    TEST("@hourly", c != NULL);
    cron_free(c);

    c = cron_parse("@daily", NULL);
    TEST("@daily", c != NULL);
    cron_free(c);

    c = cron_parse("@weekly", NULL);
    TEST("@weekly", c != NULL);
    cron_free(c);
}

static void test_parse_invalid(void) {
    char *err = NULL;

    /* Empty string */
    cron_expr_t *c = cron_parse("", &err);
    TEST("empty fails", c == NULL);
    cron_free(c);
    free(err); err = NULL;

    /* Garbage (not enough fields) */
    c = cron_parse("not-a-cron", &err);
    TEST("garbage fails", c == NULL);
    cron_free(c);
    free(err); err = NULL;

    /* Invalid @-special */
    c = cron_parse("@yearly-extra", &err);
    TEST("invalid @special fails", c == NULL);
    cron_free(c);
    free(err); err = NULL;
}

/* ================================================================
 *  Match Tests
 * ================================================================ */

static void test_match(void) {
    cron_expr_t *c = cron_parse("0 9 * * 1-5", NULL);  /* weekdays 9:00 */
    TEST("cron parsed", c != NULL);
    if (!c) return;

    struct tm tm;
    /* Monday 9:00 -> should match */
    set_tm(&tm, 2026, 5, 25, 9, 0, 0);  /* Monday */
    TEST("Mon 9:00 matches", cron_match(c, &tm));

    /* Monday 9:01 -> should NOT match */
    set_tm(&tm, 2026, 5, 25, 9, 1, 0);
    TEST("Mon 9:01 no match", !cron_match(c, &tm));

    /* Saturday 9:00 -> should NOT match */
    set_tm(&tm, 2026, 5, 30, 9, 0, 0);  /* Saturday */
    TEST("Sat 9:00 no match", !cron_match(c, &tm));

    cron_free(c);
}

/* ================================================================
 *  Next Tests
 * ================================================================ */

static void test_next(void) {
    cron_expr_t *c = cron_parse("0 9 * * 1-5", NULL);  /* weekdays 9:00 */
    TEST("cron parsed", c != NULL);
    if (!c) return;

    struct tm from, next;
    /* Friday 8:30 -> next should be Friday 9:00 */
    set_tm(&from, 2026, 5, 29, 8, 30, 0);  /* Friday */
    bool ok = cron_next(c, &from, &next);
    TEST("next found", ok);
    if (ok) {
        TEST("next hour=9", next.tm_hour == 9);
        TEST("next min=0", next.tm_min == 0);
        TEST("next same day", next.tm_mday == 29);
    }

    /* Friday 17:00 -> next should be Monday 9:00 */
    set_tm(&from, 2026, 5, 29, 17, 0, 0);  /* Friday */
    ok = cron_next(c, &from, &next);
    TEST("next after weekend found", ok);
    if (ok) {
        TEST("next hour=9", next.tm_hour == 9);
        TEST("next min=0", next.tm_min == 0);
        TEST("next is Monday", next.tm_mday != 29);  /* not Friday */
    }

    cron_free(c);
}

/* ================================================================
 *  Describe Tests
 * ================================================================ */

static void test_describe(void) {
    cron_expr_t *c = cron_parse("0 9 * * 1-5", NULL);
    TEST("cron parsed", c != NULL);
    if (!c) return;

    char *desc = cron_describe(c);
    TEST("describe non-null", desc != NULL);
    if (desc) {
        TEST("describe has 9", strstr(desc, "9") != NULL);
        TEST("describe has time '0 9'", strstr(desc, "0 9") != NULL);
        free(desc);
    }
    cron_free(c);

    /* Test @-specials description */
    c = cron_parse("@hourly", NULL);
    TEST("@hourly parsed", c != NULL);
    if (c) {
        desc = cron_describe(c);
        TEST("@hourly describe non-null", desc != NULL);
        if (desc) {
            TEST("@hourly describes as '@hourly'", strstr(desc, "@hourly") != NULL);
            free(desc);
        }
        cron_free(c);
    }
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    test_parse_valid();
    test_parse_invalid();
    test_match();
    test_next();
    test_describe();

    printf("cron: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
