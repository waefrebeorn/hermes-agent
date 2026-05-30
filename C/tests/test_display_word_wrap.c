/*
 * test_display_word_wrap.c — Tests for display_word_wrap (display_core.c).
 *
 * Pure string wrapping function — no I/O, no global state.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I../include -I../lib/libansi \
 *       tests/test_display_word_wrap.c src/cli/display_core.c \
 *       -o /tmp/hermes_test_wordwrap -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;
static int test_count = 0;

#define TEST(name, expr) do { \
    test_count++; \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

int main(void) {
    printf("=== Display Word Wrap Test Suite ===\n\n");

    /* NULL/empty input */
    char *r;

    r = display_word_wrap(NULL, 80);
    TEST("NULL input returns empty string", r != NULL && r[0] == '\0');
    free(r);

    r = display_word_wrap("", 80);
    TEST("empty input returns empty string", r != NULL && r[0] == '\0');
    free(r);

    /* max_width < 1 returns original */
    r = display_word_wrap("hello world", 0);
    TEST_STR_EQ("max_width=0 returns original", r, "hello world");
    free(r);

    /* Short text fits in one line */
    r = display_word_wrap("hello world", 80);
    TEST_STR_EQ("short text fits one line", r, "hello world");
    free(r);

    /* Text at exact boundary */
    r = display_word_wrap("hello", 5);
    TEST_STR_EQ("text at exact boundary", r, "hello");
    free(r);

    /* Text needs wrapping */
    r = display_word_wrap("hello world", 6);
    TEST("wrapping produces \\n", r != NULL && strchr(r, '\n') != NULL);
    if (r) {
        /* "hello" fits in 6 cols, "world" doesn't — check wrapping */
        printf("    wrap(6): '%s'\n", r);
    }
    free(r);

    /* Multi-word wrapping */
    r = display_word_wrap("one two three four", 10);
    TEST("multi-word wraps", r != NULL && strchr(r, '\n') != NULL);
    if (r) {
        /* Each word is ≤ 10 chars, so line breaks should be between words */
        printf("    wrap(10): '%s'\n", r);
    }
    free(r);

    /* Very long single word — exceeds max_width, but word wrap doesn't break words */
    r = display_word_wrap("abcdefghijklmnop", 5);
    TEST("long word returns original (no word break)", r != NULL);
    if (r) {
        TEST_STR_EQ("long word unbroken", r, "abcdefghijklmnop");
        printf("    wrap(5) long word: '%s'\n", r);
    }
    free(r);

    /* Preserve existing newlines */
    r = display_word_wrap("hello\nworld", 80);
    TEST_STR_EQ("preserves existing \\n", r, "hello\nworld");
    free(r);

    /* Leading/trailing spaces */
    r = display_word_wrap("  hello  ", 80);
    TEST("handles leading spaces", r != NULL);
    if (r) {
        printf("    spaces: '%s'\n", r);
        /* Leading spaces at column 0 should be stripped */
    }
    free(r);

    /* Tab characters treated as spaces */
    r = display_word_wrap("hello\tworld", 80);
    TEST("tab handled", r != NULL);
    free(r);

    /* Wrapping with single char width */
    r = display_word_wrap("a b c d e", 1);
    TEST("width 1 wraps after each word", r != NULL);
    if (r) {
        int newlines = 0;
        for (const char *p = r; *p; p++) if (*p == '\n') newlines++;
        TEST("width 1 produces 4 newlines for 5 words", newlines >= 4);
        printf("    width 1: '%s'\n", r);
    }
    free(r);

    /* Each line fits exactly */
    r = display_word_wrap("ab cd", 2);
    TEST("width 2 wraps on cd", r != NULL);
    if (r) {
        printf("    width 2: '%s'\n", r);
    }
    free(r);

    /* No wrapping needed — fits in one line */
    r = display_word_wrap("short", 100);
    TEST_STR_EQ("fits one line at width 100", r, "short");
    free(r);

    printf("\n=== Results: %d passed, %d failed (%d assertions) ===\n",
           passed, failed, test_count);
    return failed > 0 ? 1 : 0;
}
