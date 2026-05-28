/*
 * test_hermes_regex.c — Tests for regex wrapper (lib/libregex).
 */
#include "hermes_regex.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static void test_compile_and_free(void) {
    hregex_t *re = regex_compile("hello", 0);
    TEST("compile basic", re != NULL);
    regex_free(re);

    re = regex_compile("", 0);
    TEST("compile empty pattern returns NULL", re == NULL);

    re = regex_compile(NULL, 0);
    TEST("compile NULL pattern returns NULL", re == NULL);
}

static void test_match(void) {
    hregex_t *re = regex_compile("[0-9]+", 0);
    TEST("compile digits", re != NULL);

    regex_match_t *m = regex_match(re, "abc123def");
    TEST("match digits in string", m != NULL && m->matched);
    TEST("group 0 is the match", m->groups[0] != NULL && strcmp(m->groups[0], "123") == 0);
    regex_match_free(m);

    m = regex_match(re, "no digits");
    TEST("match no digits", m != NULL && !m->matched);
    regex_match_free(m);

    m = regex_match(re, NULL);
    TEST("match NULL str returns NULL", m == NULL);

    regex_free(re);

    m = regex_match(NULL, "test");
    TEST("match NULL re returns NULL", m == NULL);

    re = regex_compile("^hello", 0);
    TEST("compile anchored pattern", re != NULL);
    m = regex_match(re, "hello world");
    TEST("match anchored", m != NULL && m->matched && m->groups[0] != NULL &&
         strcmp(m->groups[0], "hello") == 0);
    regex_match_free(m);

    m = regex_match(re, "say hello");
    TEST("match anchored fails on non-start", m != NULL && !m->matched);
    regex_match_free(m);
    regex_free(re);
}

static void test_search(void) {
    hregex_t *re = regex_compile("world", 0);
    TEST("compile search pattern", re != NULL);

    regex_match_t *m = regex_search(re, "hello world");
    TEST("search finds world", m != NULL && m->matched);
    regex_match_free(m);

    regex_free(re);
}

static void test_case_insensitive(void) {
    hregex_t *re = regex_compile("HELLO", 1);  /* flag bit 0 = ICASE */
    TEST("compile icase", re != NULL);

    regex_match_t *m = regex_match(re, "hello");
    TEST("icase match", m != NULL && m->matched);
    regex_match_free(m);

    m = regex_match(re, "HELLO");
    TEST("icase exact", m != NULL && m->matched);
    regex_match_free(m);

    regex_free(re);
}

static void test_capture_groups(void) {
    hregex_t *re = regex_compile("([a-z]+)@([a-z]+)\\.([a-z]+)", 0);
    TEST("compile email pattern", re != NULL);

    regex_match_t *m = regex_match(re, "user@example.com");
    TEST("match email", m != NULL && m->matched);
    TEST("full match", m->groups[0] != NULL && strcmp(m->groups[0], "user@example.com") == 0);
    TEST("group 1 (user)", m->groups[1] != NULL && strcmp(m->groups[1], "user") == 0);
    TEST("group 2 (domain)", m->groups[2] != NULL && strcmp(m->groups[2], "example") == 0);
    TEST("group 3 (tld)", m->groups[3] != NULL && strcmp(m->groups[3], "com") == 0);
    regex_match_free(m);

    regex_free(re);
}

static void test_replace(void) {
    hregex_t *re = regex_compile("world", 0);
    TEST("compile replace pattern", re != NULL);

    char *result = regex_replace(re, "hello world", "there");
    TEST("replace basic", result != NULL && strcmp(result, "hello there") == 0);
    free(result);

    result = regex_replace(re, "hello world", NULL);
    TEST("replace NULL replacement returns original", result != NULL &&
         strcmp(result, "hello world") == 0);
    free(result);

    regex_free(re);

    result = regex_replace(NULL, "test", "repl");
    TEST("replace NULL re returns NULL", result == NULL);

    re = regex_compile("([a-z]+)@([a-z]+)", 0);
    TEST("compile replace groups", re != NULL);
    result = regex_replace(re, "user@example", "$2@$1");
    TEST("replace with backrefs", result != NULL && strcmp(result, "example@user") == 0);
    free(result);
    regex_free(re);
}

static void test_extract(void) {
    char *result = regex_extract("([0-9]{4})-([0-9]{2})", "Date: 2026-05-28", 1);
    TEST("extract group 1 (year)", result != NULL && strcmp(result, "2026") == 0);
    free(result);

    result = regex_extract("([0-9]{4})-([0-9]{2})", "Date: 2026-05-28", 2);
    TEST("extract group 2 (month)", result != NULL && strcmp(result, "05") == 0);
    free(result);

    result = regex_extract("([0-9]+)", "no digits here", 1);
    TEST("extract no match returns NULL", result == NULL);

    result = regex_extract(NULL, "test", 0);
    TEST("extract NULL pattern returns NULL", result == NULL);

    result = regex_extract("test", NULL, 0);
    TEST("extract NULL str returns NULL", result == NULL);
}

static void test_match_free_null(void) {
    /* Should not crash */
    regex_match_free(NULL);
    TEST("match_free(NULL) no crash", 1);
}

int main(void) {
    printf("=== Regex Library Tests ===\n");

    test_compile_and_free();
    test_match();
    test_search();
    test_case_insensitive();
    test_capture_groups();
    test_replace();
    test_extract();
    test_match_free_null();

    printf("\n%d failures\n", failures);
    return failures > 0 ? 1 : 0;
}
