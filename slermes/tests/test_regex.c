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
    }

    /* NULL string */
    {
        char *r = regex_extract("(.*)", NULL, 1);
        TEST("extract: NULL string returns NULL", r == NULL);
    }

    /* Empty string */
    {
        char *r = regex_extract("(.*)", "", 1);
        TEST("extract: empty string matches", r && strcmp(r, "") == 0);
        free(r);
    }

    /* Non-existent group number (out of bounds) */
    {
        char *r = regex_extract("(\\w+)", "hello", 99);
        TEST("extract: out-of-bounds group returns NULL", r == NULL);
    }

    /* Negative group number */
    {
        char *r = regex_extract("(\\w+)", "hello", -1);
        TEST("extract: negative group returns NULL", r == NULL);
    }

    /* Multi-group extraction — group 2 */
    {
        char *r = regex_extract("(\\w+) (\\w+)", "hello world", 2);
        TEST("extract: group 2 of multi-group", r && strcmp(r, "world") == 0);
        free(r);
    }

    /* Invalid regex pattern */
    {
        char *r = regex_extract("[invalid", "test", 1);
        TEST("extract: invalid pattern returns NULL", r == NULL);
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

    /* Empty string search */
    {
        hregex_t *re = regex_compile(".*", 0);
        if (re) {
            regex_match_t *m = regex_search(re, "");
            TEST("search: empty string with .* matches", m && m->matched);
            regex_match_free(m);
            regex_free(re);
        }
    }

    /* Case-sensitive search (default) */
    {
        hregex_t *re = regex_compile("WORLD", 0);
        if (re) {
            regex_match_t *m = regex_search(re, "hello world");
            TEST("search: case-sensitive no match", m && !m->matched);
            regex_match_free(m);
            regex_free(re);
        }
    }

    /* Invalid regex pattern in compile */
    {
        hregex_t *re = regex_compile("[invalid", 0);
        TEST("compile: invalid pattern returns NULL", re == NULL);
        regex_free(re);
    }

    /* Empty pattern — library doesn't support empty patterns */
    {
        hregex_t *re = regex_compile("", 0);
        TEST("compile: empty pattern returns NULL", re == NULL);
    }

    /* NULL regex_compile */
    {
        hregex_t *re = regex_compile(NULL, 0);
        TEST("compile: NULL pattern returns NULL", re == NULL);
    }

    /* NULL regex_search */
    {
        regex_match_t *m = regex_search(NULL, "test");
        TEST("search: NULL handle returns NULL", m == NULL);
    }

    /* regex_search with NULL string */
    {
        hregex_t *re = regex_compile(".*", 0);
        if (re) {
            regex_match_t *m = regex_search(re, NULL);
            TEST("search: NULL string returns NULL", m == NULL);
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

    /* Empty replacement string */
    {
        hregex_t *re = regex_compile("world", 0);
        if (re) {
            char *r = regex_replace(re, "hello world", "");
            TEST("replace: empty replacement strips matched text", r && strcmp(r, "hello ") == 0);
            free(r);
            regex_free(re);
        }
    }

    /* Multi-match replacement — library replaces first match only */
    {
        hregex_t *re = regex_compile("[0-9]", 0);
        if (re) {
            char *r = regex_replace(re, "a1b2c3d4", "X");
            TEST("replace: single-match replaces first digit", r && strcmp(r, "aXb2c3d4") == 0);
            free(r);
            regex_free(re);
        }
    }

    /* NULL regex handle */
    {
        char *r = regex_replace(NULL, "test", "replacement");
        TEST("replace: NULL handle returns NULL", r == NULL);
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

    /* NULL replacement string — library passes through, returning NULL replacement text */
    {
        hregex_t *re = regex_compile("world", 0);
        if (re) {
            char *r = regex_replace(re, "hello world", NULL);
            TEST("replace: NULL replacement passthrough", r && strcmp(r, "hello world") == 0);
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
