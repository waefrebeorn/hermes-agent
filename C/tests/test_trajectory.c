/*
 * test_trajectory.c — Tests for trajectory scratchpad tag helpers.
 *
 * Tests: tag conversion (REASONING_SCRATCHPAD → think),
 * incomplete scratchpad detection, NULL/empty/NUL-tagged edge cases.
 */

#include "hermes_trajectory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) do { \
    const char *_a = (a); const char *_b = (b); \
    if (_a && _b && strcmp(_a, _b) == 0) { passed++; printf("  PASS: %s\n", name); } \
    else { \
        failed++; \
        printf("  FAIL: %s (line %d) — got \"%s\", expected \"%s\"\n", \
               name, __LINE__, _a ? _a : "(null)", _b ? _b : "(null)"); \
    } \
} while(0)

static void test_convert_null_empty(void) {
    printf("\n--- Convert: NULL/empty ---\n");
    TEST("NULL returns NULL", hermes_convert_scratchpad_to_think(NULL) == NULL);
    char *r = hermes_convert_scratchpad_to_think("");
    TEST_STR_EQ("empty returns empty", r, "");
    free(r);
}

static void test_convert_no_tag(void) {
    printf("\n--- Convert: no tag (passthrough) ---\n");
    char *r = hermes_convert_scratchpad_to_think("hello world");
    TEST_STR_EQ("plain text unchanged", r, "hello world");
    free(r);

    r = hermes_convert_scratchpad_to_think("multiple\nlines\nhere");
    TEST_STR_EQ("multi-line unchanged", r, "multiple\nlines\nhere");
    free(r);
}

static void test_convert_single_tag(void) {
    printf("\n--- Convert: single tags ---\n");
    char *r = hermes_convert_scratchpad_to_think(
        "<REASONING_SCRATCHPAD>some reasoning</REASONING_SCRATCHPAD>");
    TEST_STR_EQ("open/close tags converted", r,
        "<think>some reasoning</think>");
    free(r);

    r = hermes_convert_scratchpad_to_think("<REASONING_SCRATCHPAD>deep thoughts");
    TEST_STR_EQ("open tag only", r, "<think>deep thoughts");
    free(r);
}

static void test_convert_multiple_tags(void) {
    printf("\n--- Convert: multiple tag pairs ---\n");
    char *r = hermes_convert_scratchpad_to_think(
        "a<REASONING_SCRATCHPAD>b</REASONING_SCRATCHPAD>c"
        "<REASONING_SCRATCHPAD>d</REASONING_SCRATCHPAD>e");
    TEST_STR_EQ("two tag pairs", r, "a<think>b</think>c<think>d</think>e");
    free(r);
}

static void test_convert_mixed_content(void) {
    printf("\n--- Convert: mixed content ---\n");
    char *r = hermes_convert_scratchpad_to_think(
        "Before <REASONING_SCRATCHPAD>inner</REASONING_SCRATCHPAD> after.");
    TEST_STR_EQ("text before/after tags", r,
        "Before <think>inner</think> after.");
    free(r);

    r = hermes_convert_scratchpad_to_think(
        "<think>already think</think>");
    TEST_STR_EQ("already think tags unchanged",
        r, "<think>already think</think>");
    free(r);
}

static void test_has_incomplete(void) {
    printf("\n--- Has incomplete scratchpad ---\n");
    TEST("NULL returns false", !hermes_has_incomplete_scratchpad(NULL));
    TEST("empty returns false", !hermes_has_incomplete_scratchpad(""));
    TEST("no tag returns false",
        !hermes_has_incomplete_scratchpad("hello world"));

    TEST("open without close returns true",
        hermes_has_incomplete_scratchpad(
            "<REASONING_SCRATCHPAD>unclosed"));
    TEST("closed pair returns false",
        !hermes_has_incomplete_scratchpad(
            "<REASONING_SCRATCHPAD>closed</REASONING_SCRATCHPAD>"));

    TEST("close without open returns false",
        !hermes_has_incomplete_scratchpad(
            "text</REASONING_SCRATCHPAD>"));

    TEST("two opens one close returns false (close present)",
        !hermes_has_incomplete_scratchpad(
            "<REASONING_SCRATCHPAD>a</REASONING_SCRATCHPAD>"
            "<REASONING_SCRATCHPAD>b"));

    TEST("two pairs complete returns false",
        !hermes_has_incomplete_scratchpad(
            "<REASONING_SCRATCHPAD>a</REASONING_SCRATCHPAD>"
            "<REASONING_SCRATCHPAD>b</REASONING_SCRATCHPAD>"));
}

int main(void) {
    printf("=== Trajectory Tests ===\n");

    test_convert_null_empty();
    test_convert_no_tag();
    test_convert_single_tag();
    test_convert_multiple_tags();
    test_convert_mixed_content();
    test_has_incomplete();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
