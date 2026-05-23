/* test_difflib.c — J15: Python difflib port tests (ratio, unified_diff, simple_diff). */
#include "difflib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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
    printf("=== J15: libdifflib Tests ===\n\n");

    /* --- difflib_ratio --- */
    printf("--- difflib_ratio ---\n");
    TEST("identical strings", fabs(difflib_ratio("hello", "hello") - 1.0) < 0.001);
    TEST("completely different", difflib_ratio("abc", "xyz") < 0.5);
    TEST("empty vs empty", fabs(difflib_ratio("", "") - 1.0) < 0.001);
    TEST("empty vs non-empty", difflib_ratio("", "hello") == 0.0);
    TEST("NULL vs NULL", fabs(difflib_ratio(NULL, NULL) - 1.0) < 0.001);
    TEST("NULL vs string", difflib_ratio(NULL, "hello") == 0.0);
    TEST("partial match > 0", difflib_ratio("abc", "abd") > 0.5);

    /* Multi-line ratio */
    TEST("multi-line identical", fabs(difflib_ratio("a\nb\nc", "a\nb\nc") - 1.0) < 0.001);
    TEST("multi-line partial", difflib_ratio("a\nb\nc", "a\nx\nc") > 0.3);

    /* --- difflib_unified_diff --- */
    printf("\n--- difflib_unified_diff ---\n");
    char *diff = difflib_unified_diff("hello\nworld", "hello\nthere", 1);
    TEST("unified diff non-NULL", diff != NULL);
    if (diff) {
        TEST("diff contains identifier", strstr(diff, "---") != NULL);
        TEST("diff contains +++", strstr(diff, "+++") != NULL);
        TEST("diff contains -world", strstr(diff, "-world") != NULL);
        TEST("diff contains +there", strstr(diff, "+there") != NULL);
    }
    free(diff);

    diff = difflib_unified_diff("same\nsame", "same\nsame", 3);
    TEST("identical produces diff with header only (or empty)", diff != NULL);
    free(diff);

    /* Multiple changes */
    diff = difflib_unified_diff("a\nb\nc\nd\ne", "a\nx\nc\ny\ne", 0);
    if (diff) {
        TEST("multi-change diff has markers", strstr(diff, "@@") != NULL);
    }
    free(diff);

    /* --- difflib_simple_diff --- */
    printf("\n--- difflib_simple_diff ---\n");
    char *sd = difflib_simple_diff("hello\nworld", "hello\nthere");
    TEST("simple diff non-NULL", sd != NULL);
    if (sd) {
        TEST("simple diff contains unchanged line", strstr(sd, " hello") != NULL);
        TEST("simple diff contains -world", strstr(sd, "-world") != NULL);
        TEST("simple diff contains +there", strstr(sd, "+there") != NULL);
    }
    free(sd);

    sd = difflib_simple_diff(NULL, "only");
    TEST("simple diff NULL a", sd && strstr(sd, "only") != NULL);
    free(sd);

    sd = difflib_simple_diff("only", NULL);
    TEST("simple diff NULL b", sd && strstr(sd, "only") != NULL);
    free(sd);

    sd = difflib_simple_diff(NULL, NULL);
    TEST("simple diff both NULL", sd && sd[0] == '\0');
    free(sd);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
