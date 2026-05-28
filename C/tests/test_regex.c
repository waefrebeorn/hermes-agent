/*
 * test_regex.c — C regex wrapper test suite (J16).
 *
 * Tests: regex_compile, regex_match, regex_search,
 *        regex_replace, regex_extract, regex_free, regex_match_free.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libregex \
 *       tests/test_regex.c lib/libregex/hermes_regex.c \
 *       -o /tmp/hermes_test_regex -lm
 *
 * Run:
 *   /tmp/hermes_test_regex
 */

#include "hermes_regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* ================================================================
 *  1. regex_compile
 * ================================================================ */
static void test_compile(void) {
    printf("\n--- regex_compile ---\n");

    hregex_t *re = regex_compile("hello", 0);
    TEST_NOT_NULL("compile 'hello'", re);
    regex_free(re);

    re = regex_compile("", 0);
    TEST_NULL("compile empty pattern returns NULL", re);

    re = regex_compile(NULL, 0);
    TEST_NULL("compile NULL returns NULL", re);

    re = regex_compile("[a-z]+", 1); /* flag 1 = case insensitive */
    TEST_NOT_NULL("compile with case-insensitive flag", re);
    regex_free(re);
}

/* ================================================================
 *  2. regex_match
 * ================================================================ */
static void test_match(void) {
    printf("\n--- regex_match ---\n");

    hregex_t *re = regex_compile("world", 0);
    TEST_NOT_NULL("compile 'world'", re);
    if (!re) return;

    regex_match_t *m = regex_match(re, "hello world");
    TEST_NOT_NULL("match returns non-NULL", m);
    if (m) {
        TEST("match finds 'world' in 'hello world'", m->matched);
        TEST_STR_EQ("match group 0 is 'world'", m->groups[0], "world");
        regex_match_free(m);
    }

    m = regex_match(re, "hello there");
    TEST_NOT_NULL("match with no match returns non-NULL", m);
    if (m) {
        TEST("match correctly returns false for no match", !m->matched);
        regex_match_free(m);
    }

    regex_free(re);
}

/* ================================================================
 *  3. regex_search (same as match for POSIX)
 * ================================================================ */
static void test_search(void) {
    printf("\n--- regex_search ---\n");

    hregex_t *re = regex_compile("fox", 0);
    TEST_NOT_NULL("compile 'fox'", re);
    if (!re) return;

    regex_match_t *m = regex_search(re, "the quick brown fox jumps");
    TEST_NOT_NULL("search returns non-NULL", m);
    if (m) {
        TEST("search finds 'fox'", m->matched);
        TEST_STR_EQ("search group 0 is 'fox'", m->groups[0], "fox");
        regex_match_free(m);
    }

    regex_free(re);
}

/* ================================================================
 *  4. regex_extract
 * ================================================================ */
static void test_extract(void) {
    printf("\n--- regex_extract ---\n");

    char *result = regex_extract("name=(\\w+)", "name=alice", 0);
    TEST_STR_EQ("extract group 0", result, "name=alice");
    free(result);

    result = regex_extract("name=(\\w+)", "name=alice", 1);
    TEST_STR_EQ("extract group 1", result, "alice");
    free(result);

    result = regex_extract("name=(\\w+)", "nope", 1);
    TEST_NULL("extract no match returns NULL", result);

    result = regex_extract(NULL, "test", 0);
    TEST_NULL("extract NULL pattern returns NULL", result);

    result = regex_extract("test", NULL, 0);
    TEST_NULL("extract NULL str returns NULL", result);
}

/* ================================================================
 *  5. regex_replace
 * ================================================================ */
static void test_replace(void) {
    printf("\n--- regex_replace ---\n");

    hregex_t *re = regex_compile("\\w+", 0);
    TEST_NOT_NULL("compile word pattern", re);
    if (!re) return;

    char *result = regex_replace(re, "hello", "world");
    TEST("replace first match", result != NULL);
    if (result) {
        TEST_STR_EQ("replace 'hello' with 'world'", result, "world");
        free(result);
    }

    result = regex_replace(re, "hello world", "hi");
    if (result) {
        TEST_STR_EQ("replace first match only", result, "hi world");
        free(result);
    }

    regex_free(re);

    /* Replace with group reference */
    re = regex_compile("(\\w+) (\\w+)", 0);
    TEST_NOT_NULL("compile two-word pattern", re);
    if (!re) return;

    result = regex_replace(re, "hello world", "$2 $1");
    if (result) {
        TEST_STR_EQ("replace with group swap", result, "world hello");
        free(result);
    }

    regex_free(re);
}

/* ================================================================
 *  6. NULL/safety tests
 * ================================================================ */
static void test_null_safety(void) {
    printf("\n--- NULL/safety ---\n");

    /* These should crash: */
    regex_free(NULL);
    TEST("regex_free(NULL) no crash", 1);

    regex_match_free(NULL);
    TEST("regex_match_free(NULL) no crash", 1);

    hregex_t *re = regex_compile("test", 0);
    TEST_NOT_NULL("compile 'test' for null-safety", re);
    if (re) {
        regex_match_t *m = regex_match(re, NULL);
        TEST_NULL("regex_match(NULL str) returns NULL", m);

        char *r = regex_replace(re, NULL, "repl");
        TEST_NULL("regex_replace(NULL str) returns NULL", r);

        r = regex_replace(re, "hello", NULL);
        TEST_NOT_NULL("regex_replace(NULL repl) returns strdup", r);
        if (r) {
            TEST_STR_EQ("replace with NULL repl returns original", r, "hello");
            free(r);
        }

        regex_free(re);
    }
}

int main(void) {
    printf("=== Regex Wrapper Test Suite (J16) ===\n");

    test_compile();
    test_match();
    test_search();
    test_extract();
    test_replace();
    test_null_safety();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
