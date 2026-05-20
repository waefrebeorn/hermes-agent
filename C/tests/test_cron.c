/*
 * test_cron.c — Smoke test for cron expression parser.
 */
#include "cron.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: parse standard expressions */
    char *err = NULL;
    cron_expr_t *c = cron_parse("0 0 * * *", &err);
    TEST("cron_parse '0 0 * * *'", c != NULL && err == NULL);
    free(err);
    cron_free(c);

    /* Test 2: parse special expressions */
    cron_expr_t *ch = cron_parse("@hourly", NULL);
    TEST("cron_parse @hourly", ch != NULL);
    cron_free(ch);

    cron_expr_t *cd = cron_parse("@daily", NULL);
    TEST("cron_parse @daily", cd != NULL);
    cron_free(cd);

    cron_expr_t *cw = cron_parse("@weekly", NULL);
    TEST("cron_parse @weekly", cw != NULL);
    cron_free(cw);

    /* Test 3: wildcard matches everything */
    cron_expr_t *wild = cron_parse("* * * * *", NULL);
    TEST("cron_parse wildcard", wild != NULL);
    if (wild) {
        struct tm tm = {0};
        tm.tm_sec  = 30;
        tm.tm_min  = 15;
        tm.tm_hour = 10;
        tm.tm_mday = 5;
        tm.tm_mon  = 3;
        tm.tm_year = 126;
        tm.tm_wday = 1;
        bool match = cron_match(wild, &tm);
        TEST("cron_match wildcard always true", match == true);
        cron_free(wild);
    }

    /* Test 4: specific time matching */
    cron_expr_t *specific = cron_parse("30 9 * * 1-5", NULL);
    TEST("cron_parse specific", specific != NULL);
    if (specific) {
        struct tm tm = {0};
        tm.tm_sec  = 0;
        tm.tm_min  = 30;
        tm.tm_hour = 9;
        tm.tm_mday = 15;
        tm.tm_mon  = 5;
        tm.tm_year = 126;
        tm.tm_wday = 3; /* Wednesday — matches 1-5 */
        bool match = cron_match(specific, &tm);
        TEST("cron_match 9:30 weekday matches", match == true);

        /* Should not match at wrong time */
        tm.tm_min = 0;
        bool no_match = cron_match(specific, &tm);
        TEST("cron_match 9:00 does not match", no_match == false);

        /* Weekend should not match */
        tm.tm_min = 30;
        tm.tm_wday = 0; /* Sunday */
        bool weekend = cron_match(specific, &tm);
        TEST("cron_match weekend does not match", weekend == false);

        cron_free(specific);
    }

    /* Test 5: describe */
    cron_expr_t *desc_expr = cron_parse("0 9 * * 1", NULL);
    if (desc_expr) {
        char *desc = cron_describe(desc_expr);
        TEST("cron_describe non-NULL", desc != NULL);
        if (desc) {
            TEST("cron_describe not empty", strlen(desc) > 0);
            free(desc);
        }
        cron_free(desc_expr);
    }

    /* Test 6: parse error */
    char *err2 = NULL;
    cron_expr_t *bad = cron_parse("invalid cron expr", &err2);
    TEST("cron_parse invalid", bad == NULL);
    TEST("cron_parse error message", err2 != NULL);
    free(err2);

    /* Test 7: cron_next */
    cron_expr_t *every_min = cron_parse("* * * * *", NULL);
    if (every_min) {
        struct tm from = {0};
        from.tm_sec = 0; from.tm_min = 0; from.tm_hour = 0;
        from.tm_mday = 1; from.tm_mon = 0; from.tm_year = 126;
        struct tm next = {0};
        bool has_next = cron_next(every_min, &from, &next);
        TEST("cron_next returns true", has_next == true);
        cron_free(every_min);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All cron tests PASSED");
    return failures ? 1 : 0;
}
