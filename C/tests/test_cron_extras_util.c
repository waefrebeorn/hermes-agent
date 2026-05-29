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

    printf("\n==========================\n");
    printf("Results: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
