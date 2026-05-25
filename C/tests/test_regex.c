/* test_regex.c -- J16: regex wrapper tests (compile, match, search, replace, extract). */
#include "hermes_regex.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== J16: libregex Tests ===\n\n");

    /* --- compile --- */
    printf("--- compile ---\n");
    hregex_t *re = regex_compile("hello", 0);
    TEST("compile hello", re != NULL);
    TEST("compile NULL", regex_compile(NULL, 0) == NULL);
    TEST("compile empty", regex_compile("", 0) == NULL);

    /* --- match --- */
    printf("\n--- match ---\n");
    regex_match_t *m = regex_match(re, "hello world");
    TEST("match hello", m && m->matched);
    TEST("group 0", m && m->groups[0] && strcmp(m->groups[0], "hello") == 0);
    regex_match_free(m);

    m = regex_match(re, "world");
    TEST("no match", m && !m->matched);
    regex_match_free(m);

    m = regex_match(NULL, "hello");
    TEST("NULL re", m == NULL);

    /* --- search --- */
    printf("\n--- search ---\n");
    m = regex_search(re, "say hello world");
    TEST("search finds hello", m && m->matched);
    regex_match_free(m);

    /* --- capture groups --- */
    printf("\n--- capture groups ---\n");
    hregex_t *re_cap = regex_compile("([a-z]+)@([a-z]+)\\.com", 0);
    TEST("compile capture", re_cap != NULL);
    m = regex_search(re_cap, "user@example.com");
    TEST("capture matched", m && m->matched);
    if (m) {
        TEST("group 1 user", m->groups[1] && strcmp(m->groups[1], "user") == 0);
        TEST("group 2 domain", m->groups[2] && strcmp(m->groups[2], "example") == 0);
    }
    regex_match_free(m);
    regex_free(re_cap);

    /* --- case insensitive --- */
    printf("\n--- case insensitive ---\n");
    hregex_t *re_ci = regex_compile("hello", 1);
    TEST("compile ci", re_ci != NULL);
    m = regex_match(re_ci, "HELLO");
    TEST("ci match", m && m->matched);
    regex_match_free(m);
    regex_free(re_ci);

    /* --- replace --- */
    printf("\n--- replace ---\n");
    hregex_t *re_r = regex_compile("world", 0);
    char *replaced = regex_replace(re_r, "hello world", "there");
    TEST("replace", replaced && strcmp(replaced, "hello there") == 0);
    free(replaced);
    regex_free(re_r);

    /* --- backreference replace --- */
    hregex_t *re_br = regex_compile("(\\w+)@(\\w+)", 0);
    replaced = regex_replace(re_br, "user@example.com", "$1 at $2");
    TEST("backref replace", replaced && strcmp(replaced, "user at example.com") == 0);
    free(replaced);
    regex_free(re_br);

    /* --- replace no match --- */
    re_r = regex_compile("xyz", 0);
    replaced = regex_replace(re_r, "hello world", "nope");
    TEST("replace no match returns orig", replaced && strcmp(replaced, "hello world") == 0);
    free(replaced);
    regex_free(re_r);

    /* --- extract --- */
    printf("\n--- extract ---\n");
    char *extracted = regex_extract("name=(\\w+)", "name=john;age=30", 1);
    TEST("extract group 1", extracted && strcmp(extracted, "john") == 0);
    free(extracted);

    extracted = regex_extract("(\\d+)", "abc", 1);
    TEST("extract no match", extracted == NULL);

    extracted = regex_extract(NULL, "hello", 1);
    TEST("extract NULL pattern", extracted == NULL);

    /* --- free NULL safety --- */
    printf("\n--- free NULL safety ---\n");
    regex_free(NULL);
    regex_match_free(NULL);
    TEST("free NULL no crash", 1);

    regex_free(re);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
