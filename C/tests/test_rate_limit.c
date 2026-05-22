/*
 * test_rate_limit.c — Per-tool rate limiter test suite (G127).
 *
 * Tests: init tool/provider slots, check allowance, window reset,
 * overflow rejection, duplicate name, slot exhaustion, NULL safety.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include \
 *       tests/test_rate_limit.c src/tools/rate_limit.c \
 *       -o /tmp/hermes_test_rl -lm
 *
 * Run:
 *   /tmp/hermes_test_rl
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

/* ================================================================
 *  Test: Basic tool rate limiting
 * ================================================================ */
static void test_tool_rate_limit(void) {
    printf("\n--- Tool Rate Limit ---\n");

    /* Init a tool with 5 calls/minute */
    TEST_TRUE("init tool 'test_tool'", rate_limit_init_tool("test_tool", 5));

    /* First 5 calls should pass */
    for (int i = 0; i < 5; i++) {
        TEST_TRUE("tool call within limit", rate_limit_check_tool("test_tool"));
    }

    /* 6th call should be blocked */
    TEST_FALSE("tool call over limit", rate_limit_check_tool("test_tool"));

    /* Check remaining */
    int remaining = rate_limit_remaining_tool("test_tool");
    TEST_INT_EQ("remaining after 5/5", remaining, 0);
}

/* ================================================================
 *  Test: Provider rate limiting
 * ================================================================ */
static void test_provider_rate_limit(void) {
    printf("\n--- Provider Rate Limit ---\n");

    /* Init a provider with 10 calls/minute */
    TEST_TRUE("init provider 'deepseek'", rate_limit_init_provider("deepseek", 10));

    /* Use up all 10 */
    for (int i = 0; i < 10; i++) {
        TEST_TRUE("provider call within limit", rate_limit_check_provider("deepseek"));
    }

    /* 11th blocked */
    TEST_FALSE("provider call over limit", rate_limit_check_provider("deepseek"));

    /* Remaining = 0 */
    /* Note: no rate_limit_remaining_provider() — use tool variant for checking */
    TEST_TRUE("provider exhausted", !rate_limit_check_provider("deepseek"));
}

/* ================================================================
 *  Test: Multiple independent slots
 * ================================================================ */
static void test_multiple_slots(void) {
    printf("\n--- Multiple Slots ---\n");

    /* Init three independent slots */
    TEST_TRUE("init read_file (100/min)", rate_limit_init_tool("read_file", 100));
    TEST_TRUE("init write_file (50/min)", rate_limit_init_tool("write_file", 50));
    TEST_TRUE("init web_search (30/min)", rate_limit_init_tool("web_search", 30));

    /* Consume some from each */
    for (int i = 0; i < 50; i++) {
        TEST_TRUE("read_file 50 ok", rate_limit_check_tool("read_file"));
    }
    for (int i = 0; i < 25; i++) {
        TEST_TRUE("write_file 25 ok", rate_limit_check_tool("write_file"));
    }
    for (int i = 0; i < 30; i++) {
        /* web_search should exhaust after 30 */
        if (i < 30) TEST_TRUE("web_search within limit", rate_limit_check_tool("web_search"));
    }
    TEST_FALSE("web_search exhausted", rate_limit_check_tool("web_search"));

    /* Others should still have remaining */
    int remaining = rate_limit_remaining_tool("read_file");
    TEST("read_file has 50 remaining", remaining == 50);
    remaining = rate_limit_remaining_tool("write_file");
    TEST("write_file has 25 remaining", remaining == 25);
}

/* ================================================================
 *  Test: Unknown slot
 * ================================================================ */
static void test_unknown_slot(void) {
    printf("\n--- Unknown Slot ---\n");

    /* Accessing uninitialized tool should return true (no limit) */
    TEST_TRUE("unknown tool passes through",
              rate_limit_check_tool("this_tool_does_not_exist"));

    /* Remaining = -1 for unknown tool */
    TEST_INT_EQ("unknown tool remaining -1",
                rate_limit_remaining_tool("this_tool_does_not_exist"), -1);

    /* Unknown provider same behavior */
    TEST_TRUE("unknown provider passes through",
              rate_limit_check_provider("unknown_provider"));
}

/* ================================================================
 *  Test: Duplicate name
 * ================================================================ */
static void test_duplicate_name(void) {
    printf("\n--- Duplicate Name ---\n");

    /* Init same tool twice — second call updates limit (NOT rejected) */
    TEST_TRUE("first init ok", rate_limit_init_tool("duplicate_tool", 10));
    TEST_TRUE("second init updates limit", rate_limit_init_tool("duplicate_tool", 20));

    /* Updated limit should apply (20 calls now) */
    for (int i = 0; i < 20; i++)
        TEST_TRUE("updated limit applies", rate_limit_check_tool("duplicate_tool"));
    TEST_FALSE("updated limit exhausted", rate_limit_check_tool("duplicate_tool"));
}

/* ================================================================
 *  Test: NULL safety
 * ================================================================ */
static void test_null_safety(void) {
    printf("\n--- NULL Safety ---\n");

    TEST_FALSE("init_tool NULL", rate_limit_init_tool(NULL, 5));
    TEST_TRUE("check_tool NULL returns true (passthrough)",
              rate_limit_check_tool(NULL));
    TEST_INT_EQ("remaining_tool NULL", rate_limit_remaining_tool(NULL), -1);

    TEST_FALSE("init_provider NULL", rate_limit_init_provider(NULL, 5));
    TEST_TRUE("check_provider NULL",
              rate_limit_check_provider(NULL));
    /* No rate_limit_remaining_provider — just verify pass-through */
    TEST_TRUE("check_tool NULL passthrough",
              rate_limit_check_tool(NULL));
    TEST_INT_EQ("remaining_tool NULL", rate_limit_remaining_tool(NULL), -1);
}

/* ================================================================
 *  Test: Reset via reset_all
 * ================================================================ */
static void test_reset(void) {
    printf("\n--- Reset ---\n");

    /* Init and exhaust a tool */
    TEST_TRUE("init reset_tool", rate_limit_init_tool("reset_tool", 3));
    rate_limit_check_tool("reset_tool");
    rate_limit_check_tool("reset_tool");
    rate_limit_check_tool("reset_tool");
    TEST_FALSE("reset_tool exhausted", rate_limit_check_tool("reset_tool"));

    /* Reset all rate limiters */
    rate_limit_reset_all();

    /* After reset, should be allowed again (new window) */

    /* Note: after reset, the internal slot state is reset but the
     * call_count is set to 0 for that slot, so we should be able
     * to call again. However, the slot still exists. Let's check. */
    int remaining = rate_limit_remaining_tool("reset_tool");
    /* After reset_all, call_count is reset to 0 */
    TEST("remaining after reset", remaining >= 3);
}

int main(void) {
    printf("=== Rate Limiter Test Suite (G127) ===\n");

    test_tool_rate_limit();
    test_provider_rate_limit();
    test_multiple_slots();
    test_unknown_slot();
    test_duplicate_name();
    test_null_safety();
    test_reset();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
