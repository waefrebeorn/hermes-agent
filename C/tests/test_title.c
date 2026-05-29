/*
 * test_title.c — Tests for session title generation.
 *
 * Tests: NULL/empty input, plain text, code block skipping,
 * truncation at 80 chars, sentence boundary detection, trailing period trim.
 */

#include "hermes_agent.h"
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

static void test_null_empty(void) {
    printf("\n--- NULL/empty input ---\n");
    TEST_STR_EQ("NULL input", agent_generate_title(NULL, NULL), "New Session");
    TEST_STR_EQ("empty string", agent_generate_title(NULL, ""), "New Session");
}

static void test_plain_text(void) {
    printf("\n--- Plain text ---\n");
    TEST_STR_EQ("simple sentence",
        agent_generate_title(NULL, "Write a Python script to parse CSV files."),
        "Write a Python script to parse CSV files");
    TEST_STR_EQ("short phrase",
        agent_generate_title(NULL, "Hello world"), "Hello world");
}

static void test_code_block_skipped(void) {
    printf("\n--- Code block skipped ---\n");
    char *r = agent_generate_title(NULL,
        "```python\nprint('hello')\n```\nWhat does this code do?");
    TEST_STR_EQ("code block content skipped", r, "What does this code do?");
    free(r);
}

static void test_newline_handling(void) {
    printf("\n--- Newline handling ---\n");
    TEST_STR_EQ("double newline stops",
        agent_generate_title(NULL, "Line one\n\nLine two"), "Line one");
    TEST_STR_EQ("single newline becomes space",
        agent_generate_title(NULL, "Hello\nworld"), "Hello world");
}

static void test_truncation(void) {
    printf("\n--- 80-char truncation ---\n");
    /* Build a 150-char line */
    char long_input[200];
    memset(long_input, 'x', 85);
    memcpy(long_input + 85, ".", 2);
    long_input[86] = '\0';

    char *r = agent_generate_title(NULL, long_input);
    TEST("truncated to <= 80 chars", r && strlen(r) <= 80);
    free(r);
}

static void test_trailing_period_trim(void) {
    printf("\n--- Trailing period trim ---\n");
    TEST_STR_EQ("single period trimmed",
        agent_generate_title(NULL, "Hello world."), "Hello world");
    TEST_STR_EQ("multiple periods trimmed",
        agent_generate_title(NULL, "Hello..."), "Hello");
}

static void test_leading_whitespace_skip(void) {
    printf("\n--- Leading whitespace ---\n");
    TEST_STR_EQ("leading spaces",
        agent_generate_title(NULL, "    Hello world"), "Hello world");
    TEST_STR_EQ("leading newlines",
        agent_generate_title(NULL, "\n\n\nHello world"), "Hello world");
}

int main(void) {
    printf("=== Title Generation Tests ===\n");

    test_null_empty();
    test_plain_text();
    test_code_block_skipped();
    test_newline_handling();
    test_truncation();
    test_trailing_period_trim();
    test_leading_whitespace_skip();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
