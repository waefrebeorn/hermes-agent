/*
 * test_system_prompt_continuation.c — Tests for agent_get_continuation_prompt().
 *
 * Tests the three variants: partial stub + dropped tools, partial stub only,
 * and default truncation prompt.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_system_prompt_continuation.c \
 *       src/agent/system_prompt.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_continuation -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_continuation
 */

#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR(name, result, expected) do { \
    bool ok = (result) && strcmp(result, expected) == 0; \
    if (ok) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s — expected \"%s\", got \"%s\" (line %d)\n", name, expected, (result ? result : "NULL"), __LINE__); } \
} while(0)

/* ================================================================
 * 1. Default: output length limit exceeded (no partial stub, no tools)
 * ================================================================ */

static void test_default_truncation(void) {
    printf("\n--- Default truncation ---\n");
    char *prompt = agent_get_continuation_prompt(false, NULL);
    TEST("returns non-NULL", prompt != NULL);
    TEST_STR("contains truncation message", prompt,
        "[System: Your previous response was truncated by the output "
        "length limit. Continue exactly where you left off. Do not "
        "restart or repeat prior text. Finish the answer directly.]");
    free(prompt);
}

/* ================================================================
 * 2. Partial stub only (network error, no dropped tools)
 * ================================================================ */

static void test_partial_stub_only(void) {
    printf("\n--- Partial stub (network error) ---\n");
    char *prompt = agent_get_continuation_prompt(true, NULL);
    TEST("returns non-NULL", prompt != NULL);
    TEST_STR("contains network error message", prompt,
        "[System: The previous response was cut off by a "
        "network error mid-stream. Continue exactly where "
        "you left off. Do not restart or repeat prior text. "
        "Finish the answer directly.]");
    free(prompt);

    /* Empty string dropped_tools should also fall through to this */
    prompt = agent_get_continuation_prompt(true, "");
    TEST("empty dropped_tools falls through", prompt != NULL);
    TEST_STR("empty tools gives network message", prompt,
        "[System: The previous response was cut off by a "
        "network error mid-stream. Continue exactly where "
        "you left off. Do not restart or repeat prior text. "
        "Finish the answer directly.]");
    free(prompt);
}

/* ================================================================
 * 3. Partial stub with dropped tools
 * ================================================================ */

static void test_partial_with_dropped_tools(void) {
    printf("\n--- Partial stub with dropped tools ---\n");
    char *prompt = agent_get_continuation_prompt(true, "[\"read_file\", \"write_file\"]");
    TEST("returns non-NULL", prompt != NULL);
    TEST("contains tool names", prompt && strstr(prompt, "read_file") != NULL);
    TEST("contains multiple tools", prompt && strstr(prompt, "write_file") != NULL);
    TEST("contains size guidance", prompt && strstr(prompt, "~8K") != NULL);
    TEST("contains Do NOT retry", prompt && strstr(prompt, "Do NOT retry") != NULL);
    free(prompt);
}

/* ================================================================
 * 4. Partial stub with many dropped tools (max 3)
 * ================================================================ */

static void test_partial_with_many_tools(void) {
    printf("\n--- Partial stub with many tools (max 3) ---\n");
    char *prompt = agent_get_continuation_prompt(true,
        "[\"read_file\", \"write_file\", \"patch\", \"search_files\"]");
    TEST("returns non-NULL", prompt != NULL);
    TEST("contains first tool", prompt && strstr(prompt, "read_file") != NULL);
    TEST("contains second tool", prompt && strstr(prompt, "write_file") != NULL);
    TEST("contains third tool", prompt && strstr(prompt, "patch") != NULL);
    TEST("does NOT contain fourth tool", prompt && strstr(prompt, "search_files") == NULL);
    free(prompt);
}

/* ================================================================
 * 5. Edge cases
 * ================================================================ */

static void test_edge_cases(void) {
    printf("\n--- Edge cases ---\n");

    /* NULL partial stub + NULL tools — should get default truncation */
    char *prompt = agent_get_continuation_prompt(false, NULL);
    TEST("default with NULL tools", prompt != NULL);
    TEST("default message has truncation", prompt && strstr(prompt, "truncated") != NULL);
    free(prompt);

    /* Not a partial stub but has tool names — should get default */
    prompt = agent_get_continuation_prompt(false, "[\"patch\"]");
    TEST("not partial with tools is default", prompt != NULL);
    TEST("not partial has truncation", prompt && strstr(prompt, "truncated") != NULL);
    free(prompt);

    /* Invalid JSON in dropped_tools — should fall through to partial stub */
    prompt = agent_get_continuation_prompt(true, "not-json");
    TEST("invalid JSON falls through", prompt != NULL);
    TEST("invalid JSON gives network message", prompt && strstr(prompt, "network error") != NULL);
    free(prompt);

    /* Empty JSON array — should fall through to partial stub */
    prompt = agent_get_continuation_prompt(true, "[]");
    TEST("empty array falls through", prompt != NULL);
    TEST("empty array gives network message", prompt && strstr(prompt, "network error") != NULL);
    free(prompt);
}

int main(void) {
    printf("=== System Prompt Continuation Tests ===\n");

    test_default_truncation();
    test_partial_stub_only();
    test_partial_with_dropped_tools();
    test_partial_with_many_tools();
    test_edge_cases();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
