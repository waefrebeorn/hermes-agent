/*
 * test_regex.c — Tests for hermes_regex utility library.
 * Covers regex_extract, regex_compile, regex_search, regex_replace, NULL safety.
 */
#include "hermes_regex.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== Regex tests ===\n\n");

    /* ──── regex_extract ──── */

    /* Basic extraction: group 1 capture */
    {
        char *r = regex_extract("hello (\\w+)", "hello world", 1);
        TEST("extract: basic word", r && strcmp(r, "world") == 0);
        free(r);
    }

    /* Group 0 = full match */
    {
        char *r = regex_extract("(\\w+)", "hello", 0);
        TEST("extract: group 0 full match", r && strcmp(r, "hello") == 0);
        free(r);
    }

    /* NULL pattern */
    {
        char *r = regex_extract(NULL, "test", 1);
        TEST("extract: NULL pattern returns NULL", r == NULL);
        free(r);
    }

    /* NULL string */
    {
        char *r = regex_extract("(.*)", NULL, 1);
        TEST("extract: NULL string returns NULL", r == NULL);
        free(r);
    }

    /* ──── regex_compile + regex_search ──── */

    /* Basic search */
    {
        hregex_t *re = regex_compile("world", 0);
        TEST("compile: basic pattern", re != NULL);
        if (re) {
            regex_match_t *m = regex_search(re, "hello world");
            TEST("search: found match", m && m->matched);
            TEST("search: group 0 content", m && m->groups[0] && strcmp(m->groups[0], "world") == 0);
            regex_match_free(m);
            regex_free(re);
        }
    }

    /* No match */
    {
        hregex_t *re = regex_compile("zzz", 0);
        TEST("compile: no-match pattern", re != NULL);
        if (re) {
            regex_match_t *m = regex_search(re, "hello world");
            TEST("search: no match returns matched=false", m && !m->matched);
            regex_match_free(m);
            regex_free(re);
        }
    }

    /* ──── regex_replace ──── */

    /* Basic replacement */
    {
        hregex_t *re = regex_compile("world", 0);
        TEST("compile: for replace", re != NULL);
        if (re) {
            char *r = regex_replace(re, "hello world", "there");
            TEST("replace: basic", r && strcmp(r, "hello there") == 0);
            free(r);
            regex_free(re);
        }
    }

    /* No match — passthrough */
    {
        hregex_t *re = regex_compile("zzz", 0);
        if (re) {
            char *r = regex_replace(re, "hello world", "there");
            TEST("replace: no match passthrough", r && strcmp(r, "hello world") == 0);
            free(r);
            regex_free(re);
        }
    }

    /* Replacement with backreference $1 */
    {
        hregex_t *re = regex_compile("(\\w+) (\\w+)", 0);
        if (re) {
            char *r = regex_replace(re, "hello world", "$2 $1");
            TEST("replace: backreference $2 $1", r && strcmp(r, "world hello") == 0);
            free(r);
            regex_free(re);
        }
    }

    /* NULL regex handle */
    {
        char *r = regex_replace(NULL, "test", "replacement");
        TEST("replace: NULL handle returns NULL", r == NULL);
        free(r);
    }

    /* NULL string */
    {
        hregex_t *re = regex_compile(".*", 0);
        if (re) {
            char *r = regex_replace(re, NULL, "replacement");
            TEST("replace: NULL string returns NULL", r == NULL);
            free(r);
            regex_free(re);
        }
    }

    /* ──── regex_free / regex_match_free NULL safety ──── */

    {
        regex_free(NULL);
        TEST("regex_free(NULL) no crash", 1);
    }

    {
        regex_match_free(NULL);
        TEST("regex_match_free(NULL) no crash", 1);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}
