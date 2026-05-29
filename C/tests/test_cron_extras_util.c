/*
 * test_cron_extras_util.c — Tests for P176 cron utility functions.
 *
 * Tests: cron_canonical_skills, cron_normalize_value, cron_normalize_deliver.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_cron_extras_util.c src/cron/cron_extras.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_cron_util -lm
 *
 * Run:
 *   /tmp/hermes_test_cron_util
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper: free a NULL-terminated string array */
static void free_strlist(char **list) {
    if (!list) return;
    for (size_t i = 0; list[i]; i++) free(list[i]);
    free(list);
}

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 *  cron_canonical_skills tests
 * ================================================================ */

static void test_canonical_skills_null_skills(void) {
    TEST("null skills, NULL skill -> empty");
    size_t cnt;
    char **r = cron_canonical_skills(NULL, NULL, &cnt);
    if (cnt == 0) { PASS(); return; }
    FAIL("expected empty"); free_strlist(r);
}

static void test_canonical_skills_single_skill(void) {
    TEST("single skill string");
    size_t cnt;
    char **r = cron_canonical_skills("my_skill", NULL, &cnt);
    if (r && cnt == 1 && strcmp(r[0], "my_skill") == 0) { PASS(); free_strlist(r); return; }
    FAIL("expected [my_skill]"); free_strlist(r);
}

static void test_canonical_skills_skills_array(void) {
    TEST("skills JSON array");
    json_t *arr = json_array();
    json_append(arr, json_string("skill_a"));
    json_append(arr, json_string("skill_b"));
    size_t cnt;
    char **r = cron_canonical_skills(NULL, arr, &cnt);
    json_free(arr);
    if (r && cnt == 2 && strcmp(r[0], "skill_a") == 0 && strcmp(r[1], "skill_b") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [skill_a, skill_b]"); free_strlist(r);
}

static void test_canonical_skills_dedup(void) {
    TEST("dedup identical skills");
    json_t *arr = json_array();
    json_append(arr, json_string("x"));
    json_append(arr, json_string("x"));
    json_append(arr, json_string("y"));
    size_t cnt;
    char **r = cron_canonical_skills(NULL, arr, &cnt);
    json_free(arr);
    if (r && cnt == 2 && strcmp(r[0], "x") == 0 && strcmp(r[1], "y") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [x, y]"); free_strlist(r);
}

static void test_canonical_skills_empty_array_fallback(void) {
    TEST("empty array falls back to skill");
    json_t *arr = json_array();
    size_t cnt;
    char **r = cron_canonical_skills("fallback", arr, &cnt);
    json_free(arr);
    if (r && cnt == 1 && strcmp(r[0], "fallback") == 0) {
        PASS(); free_strlist(r); return;
    }
    FAIL("expected [fallback]"); free_strlist(r);
}

/* ================================================================
 *  cron_normalize_value tests
 * ================================================================ */

static void test_normalize_value_null(void) {
    TEST("NULL input -> NULL");
    char *r = cron_normalize_value(NULL, false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_empty(void) {
    TEST("empty string -> NULL");
    char *r = cron_normalize_value("", false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_whitespace(void) {
    TEST("whitespace only -> NULL");
    char *r = cron_normalize_value("   \t\n  ", false);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_value_trim(void) {
    TEST("trimmed value");
    char *r = cron_normalize_value("  hello  ", false);
    if (r && strcmp(r, "hello") == 0) { PASS(); free(r); return; }
    FAIL("expected hello"); free(r);
}

static void test_normalize_value_trailing_slash(void) {
    TEST("strip trailing slash");
    char *r = cron_normalize_value("/path/to/dir/", true);
    if (r && strcmp(r, "/path/to/dir") == 0) { PASS(); free(r); return; }
    FAIL("expected /path/to/dir"); free(r);
}

static void test_normalize_value_trailing_slash_no_strip(void) {
    TEST("no strip when flag false");
    char *r = cron_normalize_value("  value/  ", false);
    if (r && strcmp(r, "value/") == 0) { PASS(); free(r); return; }
    FAIL("expected value/"); free(r);
}

static void test_normalize_value_only_slash(void) {
    TEST("only slashes -> NULL");
    char *r = cron_normalize_value("///", true);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

/* ================================================================
 *  cron_normalize_deliver tests
 * ================================================================ */

static void test_normalize_deliver_null(void) {
    TEST("NULL -> NULL");
    char *r = cron_normalize_deliver(NULL);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_deliver_string(void) {
    TEST("string -> trimmed string");
    json_t *s = json_string("  telegram  ");
    char *r = cron_normalize_deliver(s);
    json_free(s);
    if (r && strcmp(r, "telegram") == 0) { PASS(); free(r); return; }
    FAIL("expected telegram"); free(r);
}

static void test_normalize_deliver_array(void) {
    TEST("array -> comma-joined");
    json_t *arr = json_array();
    json_append(arr, json_string("telegram"));
    json_append(arr, json_string("local"));
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (r && strcmp(r, "telegram,local") == 0) { PASS(); free(r); return; }
    FAIL("expected telegram,local"); free(r);
}

static void test_normalize_deliver_empty_array(void) {
    TEST("empty array -> NULL");
    json_t *arr = json_array();
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (!r) { PASS(); return; }
    FAIL("expected NULL"); free(r);
}

static void test_normalize_deliver_trimmed_array(void) {
    TEST("array items trimmed");
    json_t *arr = json_array();
    json_append(arr, json_string("  origin  "));
    json_append(arr, json_string("  all  "));
    char *r = cron_normalize_deliver(arr);
    json_free(arr);
    if (r && strcmp(r, "origin,all") == 0) { PASS(); free(r); return; }
    FAIL("expected origin,all"); free(r);
}

/* ================================================================
 *  cron_parse_duration tests
 * ================================================================ */

static int test_parse_duration(const char *input) {
    if (!input || !input[0]) return -1;
    /* Skip leading whitespace */
    while (*input == ' ' || *input == '\t') input++;
    if (!*input) return -1;
    char *end = NULL;
    long val = strtol(input, &end, 10);
    if (end == input || val <= 0 || val > 1000000) return -1;
    const char *unit = end;
    while (*unit == ' ' || *unit == '\t') unit++;
    if (!*unit) return -1;
    char unit_lower[16];
    size_t ui = 0;
    while (unit[ui] && ui < 15) { unit_lower[ui] = (unit[ui] >= 'A' && unit[ui] <= 'Z') ? unit[ui] + 32 : unit[ui]; ui++; }
    unit_lower[ui] = '\0';
    int mult = 0;
    if (strcmp(unit_lower, "m")==0||strcmp(unit_lower,"min")==0||strcmp(unit_lower,"mins")==0||strcmp(unit_lower,"minute")==0||strcmp(unit_lower,"minutes")==0) mult=1;
    else if (strcmp(unit_lower, "h")==0||strcmp(unit_lower,"hr")==0||strcmp(unit_lower,"hrs")==0||strcmp(unit_lower,"hour")==0||strcmp(unit_lower,"hours")==0) mult=60;
    else if (strcmp(unit_lower, "d")==0||strcmp(unit_lower,"day")==0||strcmp(unit_lower,"days")==0) mult=1440;
    if (mult == 0) return -1;
    long r = val * mult;
    if (r > 1000000 || r < 0) return -1;
    return (int)r;
}

static void test_duration_30m(void) {
    TEST("30m -> 30");
    int r = test_parse_duration("30m");
    if (r == 30) { PASS(); return; }
    FAIL("expected 30"); printf("  got %d\n", r);
}

static void test_duration_2h(void) {
    TEST("2h -> 120");
    int r = test_parse_duration("2h");
    if (r == 120) { PASS(); return; }
    FAIL("expected 120"); printf("  got %d\n", r);
}

static void test_duration_1d(void) {
    TEST("1d -> 1440");
    int r = test_parse_duration("1d");
    if (r == 1440) { PASS(); return; }
    FAIL("expected 1440"); printf("  got %d\n", r);
}

static void test_duration_variants(void) {
    TEST("30 minutes -> 30");
    int r = test_parse_duration("30 minutes");
    if (r == 30) { PASS(); return; }
    FAIL("expected 30"); printf("  got %d\n", r);
}

static void test_duration_2hours(void) {
    TEST("2hours -> 120");
    int r = test_parse_duration("2hours");
    if (r == 120) { PASS(); return; }
    FAIL("expected 120"); printf("  got %d\n", r);
}

static void test_duration_5days(void) {
    TEST("5 days -> 7200");
    int r = test_parse_duration("5 days");
    if (r == 7200) { PASS(); return; }
    FAIL("expected 7200"); printf("  got %d\n", r);
}

static void test_duration_invalid(void) {
    TEST("invalid returns -1");
    int r = test_parse_duration("xyz");
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

static void test_duration_null(void) {
    TEST("NULL -> -1");
    int r = test_parse_duration(NULL);
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

static void test_duration_empty(void) {
    TEST("empty -> -1");
    int r = test_parse_duration("");
    if (r == -1) { PASS(); return; }
    FAIL("expected -1"); printf("  got %d\n", r);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("Cron Utility Tests (P176)\n");
    printf("==========================\n\n");

    printf("--- cron_canonical_skills ---\n");
    test_canonical_skills_null_skills();
    test_canonical_skills_single_skill();
    test_canonical_skills_skills_array();
    test_canonical_skills_dedup();
    test_canonical_skills_empty_array_fallback();

    printf("\n--- cron_normalize_value ---\n");
    test_normalize_value_null();
    test_normalize_value_empty();
    test_normalize_value_whitespace();
    test_normalize_value_trim();
    test_normalize_value_trailing_slash();
    test_normalize_value_trailing_slash_no_strip();
    test_normalize_value_only_slash();

    printf("\n--- cron_normalize_deliver ---\n");
    test_normalize_deliver_null();
    test_normalize_deliver_string();
    test_normalize_deliver_array();
    test_normalize_deliver_empty_array();
    test_normalize_deliver_trimmed_array();

    printf("\n--- cron_parse_duration ---\n");
    test_duration_30m();
    test_duration_2h();
    test_duration_1d();
    test_duration_variants();
    test_duration_2hours();
    test_duration_5days();
    test_duration_invalid();
    test_duration_null();
    test_duration_empty();

    printf("\n==========================\n");
    printf("Results: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
