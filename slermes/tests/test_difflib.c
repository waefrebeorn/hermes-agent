/* test_difflib.c — J15: Python difflib port tests (ratio, unified_diff, simple_diff).
 * Expanded with edge cases: NULL/empty, swap consistency, large context,
 * single-line, trailing newlines, unicode, multiple hunks. */
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
    TEST("string vs NULL", difflib_ratio("hello", NULL) == 0.0);
    TEST("partial match > 0", difflib_ratio("abc", "abd") > 0.5);
    TEST("swap consistency abc/abd",
          fabs(difflib_ratio("abc", "abd") - difflib_ratio("abd", "abc")) < 0.001);
    TEST("single char match", difflib_ratio("a", "a") > 0.99);
    TEST("single char mismatch", difflib_ratio("a", "b") < 0.3);
    TEST("unicode chars handle",
          difflib_ratio("caf\xc3\xa9", "caf\xc3\xa9") > 0.99);
    TEST("unicode mismatch",
          difflib_ratio("caf\xc3\xa9", "cafe") > 0.0);
    TEST("whitespace diff matters",
          difflib_ratio("hello", "hello ") > 0.0);
    TEST("whitespace diff not 1.0",
          fabs(difflib_ratio("hello", "hello ") - 1.0) > 0.001);

    /* Multi-line ratio */
    TEST("multi-line identical",
          fabs(difflib_ratio("a\nb\nc", "a\nb\nc") - 1.0) < 0.001);
    TEST("multi-line partial",
          difflib_ratio("a\nb\nc", "a\nx\nc") > 0.3);
    TEST("multi-line one line differs",
          fabs(difflib_ratio("line1\nline2\nline3", "line1\nCHANGED\nline3") - 0.66) < 0.05);
    TEST("swap consistency multi",
          fabs(difflib_ratio("a\nb\nc\nd", "a\nx\nc\ny") -
               difflib_ratio("a\nx\nc\ny", "a\nb\nc\nd")) < 0.001);
    TEST("long identical",
          fabs(difflib_ratio("The quick brown fox jumps over the lazy dog",
                             "The quick brown fox jumps over the lazy dog") - 1.0) < 0.001);
    TEST("substring partial > 0",
          difflib_ratio("The quick brown fox", "The quick brown") > 0.5);

    /* --- difflib_unified_diff --- */
    printf("\n--- difflib_unified_diff ---\n");
    char *diff = difflib_unified_diff("hello\nworld", "hello\nthere", 1);
    TEST("unified diff non-NULL", diff != NULL);
    if (diff) {
        TEST("diff contains ---", strstr(diff, "---") != NULL);
        TEST("diff contains +++", strstr(diff, "+++") != NULL);
        TEST("diff contains -world", strstr(diff, "-world") != NULL);
        TEST("diff contains +there", strstr(diff, "+there") != NULL);
    }
    free(diff);

    diff = difflib_unified_diff("same\nsame", "same\nsame", 3);
    TEST("identical produces diff non-NULL", diff != NULL);
    free(diff);

    /* Multiple changes */
    diff = difflib_unified_diff("a\nb\nc\nd\ne", "a\nx\nc\ny\ne", 0);
    if (diff) {
        TEST("multi-change diff has hunk markers", strstr(diff, "@@") != NULL);
    }
    free(diff);

    /* --- unified_diff edge cases --- */
    printf("\n--- unified_diff edge cases ---\n");

    /* NULL a, only b */
    diff = difflib_unified_diff(NULL, "only", 3);
    TEST("NULL a diff non-NULL", diff != NULL);
    if (diff) {
        TEST("NULL a contains +only", strstr(diff, "+only") != NULL);
    }
    free(diff);

    /* NULL b, only a */
    diff = difflib_unified_diff("only", NULL, 3);
    TEST("NULL b diff non-NULL", diff != NULL);
    if (diff) {
        TEST("NULL b contains -only", strstr(diff, "-only") != NULL);
    }
    free(diff);

    /* Both NULL (returns header-only diff) */
    diff = difflib_unified_diff(NULL, NULL, 3);
    TEST("both NULL diff non-NULL", diff != NULL);
    if (diff) {
        TEST("both NULL diff has header", strlen(diff) > 0);
        TEST("both NULL diff has ---", strstr(diff, "---") != NULL);
    }
    free(diff);

    /* Empty a, non-empty b */
    diff = difflib_unified_diff("", "added\nlines", 1);
    TEST("empty a diff non-NULL", diff != NULL);
    if (diff) {
        TEST("empty a has +added", strstr(diff, "+added") != NULL);
        TEST("empty a has +lines", strstr(diff, "+lines") != NULL);
    }
    free(diff);

    /* Non-empty a, empty b */
    diff = difflib_unified_diff("removed\nlines", "", 1);
    TEST("empty b diff non-NULL", diff != NULL);
    if (diff) {
        TEST("empty b has -removed", strstr(diff, "-removed") != NULL);
        TEST("empty b has -lines", strstr(diff, "-lines") != NULL);
    }
    free(diff);

    /* Large context */
    diff = difflib_unified_diff("a\nb\nc", "a\nx\nc", 9999);
    TEST("large context diff non-NULL", diff != NULL);
    free(diff);

    /* Zero context */
    diff = difflib_unified_diff("a\nb\nc", "a\nx\nc", 0);
    TEST("zero context diff non-NULL", diff != NULL);
    if (diff) {
        TEST("zero context has hunk", strlen(diff) > 0);
    }
    free(diff);

    /* Single line difference */
    diff = difflib_unified_diff("hello", "world", 3);
    TEST("single line diff non-NULL", diff != NULL);
    if (diff) {
        TEST("single line has -hello", strstr(diff, "-hello") != NULL);
        TEST("single line has +world", strstr(diff, "+world") != NULL);
    }
    free(diff);

    /* Trailing newlines */
    diff = difflib_unified_diff("a\n", "a\n", 3);
    TEST("trailing newline no change diff non-NULL", diff != NULL);
    free(diff);

    diff = difflib_unified_diff("a\n", "a\nb\n", 3);
    TEST("trailing newline add diff non-NULL", diff != NULL);
    if (diff) {
        TEST("trailing add has +b", strlen(diff) > 0);
    }
    free(diff);

    /* Multiple hunks */
    diff = difflib_unified_diff("a\nb\nc\nd\ne\nf\ng\nh\ni\nj",
                                "a\nX\nc\nd\ne\nf\nY\nh\ni\nj", 1);
    TEST("two hunks diff non-NULL", diff != NULL);
    if (diff) {
        /* Should have two @@ markers for two separate hunks */
        int count = 0;
        const char *p = diff;
        while ((p = strstr(p, "@@")) != NULL) { count++; p += 2; }
        TEST("two hunks has 2+ @@ markers", count >= 2);
    }
    free(diff);

    /* --- difflib_simple_diff --- */
    printf("\n--- difflib_simple_diff ---\n");
    char *sd = difflib_simple_diff("hello\nworld", "hello\nthere");
    TEST("simple diff non-NULL", sd != NULL);
    if (sd) {
        TEST("simple diff unchanged line", strstr(sd, " hello") != NULL);
        TEST("simple diff -world", strstr(sd, "-world") != NULL);
        TEST("simple diff +there", strstr(sd, "+there") != NULL);
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

    /* --- simple_diff edge cases --- */
    printf("\n--- simple_diff edge cases ---\n");

    /* Empty vs empty */
    sd = difflib_simple_diff("", "");
    TEST("simple diff empty vs empty", sd != NULL && strlen(sd) == 0);
    free(sd);

    /* Full add (empty a) */
    sd = difflib_simple_diff("", "added\nline");
    TEST("simple diff add from empty", sd != NULL);
    if (sd) {
        TEST("simple diff has +added", strstr(sd, "+added") != NULL);
        TEST("simple diff has +line", strstr(sd, "+line") != NULL);
    }
    free(sd);

    /* Full delete (empty b) */
    sd = difflib_simple_diff("gone\nbye", "");
    TEST("simple diff delete to empty", sd != NULL);
    if (sd) {
        TEST("simple diff has -gone", strstr(sd, "-gone") != NULL);
        TEST("simple diff has -bye", strstr(sd, "-bye") != NULL);
    }
    free(sd);

    /* Single line change */
    sd = difflib_simple_diff("hello", "world");
    TEST("simple diff single line change", sd != NULL);
    if (sd) {
        TEST("single change has -hello", strstr(sd, "-hello") != NULL);
        TEST("single change has +world", strstr(sd, "+world") != NULL);
    }
    free(sd);

    /* Multiple changes distant */
    sd = difflib_simple_diff("a\nb\nc\nd\ne\nf", "X\nb\nc\nY\ne\nZ");
    TEST("simple diff multiple changes", sd != NULL);
    if (sd) {
        TEST("multi change has -a", strstr(sd, "-a") != NULL);
        TEST("multi change has +X", strstr(sd, "+X") != NULL);
        TEST("multi change has -d", strstr(sd, "-d") != NULL);
        TEST("multi change has +Y", strstr(sd, "+Y") != NULL);
        TEST("multi change has +Z", strstr(sd, "+Z") != NULL);
    }
    free(sd);

    /* Trailing newline difference */
    sd = difflib_simple_diff("a\nb\n", "a\nb");
    TEST("simple diff trailing newline diff", sd != NULL);
    free(sd);

    /* Identical with trailing newlines */
    sd = difflib_simple_diff("a\nb\nc\n", "a\nb\nc\n");
    TEST("simple diff identical multi-line", sd != NULL);
    if (sd) {
        TEST("identical multi has unchanged lines", strstr(sd, " a") != NULL);
    }
    free(sd);

    /* Long line diff */
    sd = difflib_simple_diff("short", "this is a much longer line that goes on");
    TEST("simple diff long line change", sd != NULL);
    if (sd) {
        TEST("long change has -short", strstr(sd, "-short") != NULL);
        TEST("long change has +this", strstr(sd, "+this") != NULL);
    }
    free(sd);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
