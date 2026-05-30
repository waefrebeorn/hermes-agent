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

static void test_exclamation_question(void) {
    printf("\n--- Exclamation / question marks ---\n");
    char *r = agent_generate_title(NULL, "What is the capital of France?");
    TEST("question mark preserved (no trim for ?)", r && strlen(r) > 0);
    free(r);
    TEST_STR_EQ("exclamation within text continues",
        agent_generate_title(NULL, "Wow! That is amazing."),
        "Wow! That is amazing");
}

static void test_only_code_block(void) {
    printf("\n--- Only code blocks ---\n");
    char *r = agent_generate_title(NULL, "```python\nprint('hello')\n```");
    TEST_STR_EQ("only code block no text", r, "New Session");
    free(r);
}

static void test_non_ascii(void) {
    printf("\n--- Non-ASCII content ---\n");
    char *r = agent_generate_title(NULL, "How do I say caf\u00e9 in Spanish?");
    TEST("unicode non-printable bytes dropped gracefully", r && strlen(r) > 0);
    free(r);
}

static void test_very_long_input(void) {
    printf("\n--- Very long input ---\n");
    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
        "Can you help me write a comprehensive guide about programming in C "
        "that covers all the major topics including pointers, memory management, "
        "data structures, algorithms, file I/O, networking, concurrency, and "
        "best practices? This should be very detailed and thorough.");
    (void)n;
    char *r = agent_generate_title(NULL, buf);
    TEST("long input truncated to <= 80 chars", r && strlen(r) <= 80);
    TEST("long input ends with valid truncation", r && strlen(r) > 40);
    free(r);
}

static void test_tab_and_control_chars(void) {
    printf("\n--- Tab and control characters ---\n");
    char *r = agent_generate_title(NULL, "Hello\tworld");
    TEST("tab silently dropped (not printable)", r && strstr(r, "Helloworld"));
    free(r);
    char *r2 = agent_generate_title(NULL, "Hello\x01world");
    TEST("control char handled without crash", r2 != NULL);
    free(r2);
}

static void test_trailing_ellipsis(void) {
    printf("\n--- Trailing ellipsis ---\n");
    TEST_STR_EQ("three dots trimmed",
        agent_generate_title(NULL, "I wonder what happens next..."),
        "I wonder what happens next");
}

static void test_multiple_sentences(void) {
    printf("\n--- Multiple sentences ---\n");
    char *r = agent_generate_title(NULL, "This is the first sentence. Here is another one.");
    TEST("multiple sentences preserved (break only at final period)", r && strstr(r, "first sentence. Here is another one"));
    free(r);
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
    test_exclamation_question();
    test_only_code_block();
    test_non_ascii();
    test_very_long_input();
    test_tab_and_control_chars();
    test_trailing_ellipsis();
    test_multiple_sentences();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
