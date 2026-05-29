/*
 * test_approval.c — Approval system test suite (G128-G129, G169).
 *
 * Tests: timeout, yolo mode, cache count/clear, session reset.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_approval.c src/tools/approval.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_app -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_app
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_NULL(name, ptr) TEST(name, (ptr) == NULL)
#define TEST_NOT_NULL(name, ptr) TEST(name, (ptr) != NULL)

/* ================================================================
 *  0. Session reset
 * ================================================================ */
static void test_session_reset(void) {
    printf("\n--- Session Reset ---\n");

    /* Should not crash */
    approval_reset_session();
    TEST("reset session no crash", 1);

    /* Resetting twice should also not crash */
    approval_reset_session();
    TEST("reset session twice no crash", 1);
}

/* ================================================================
 *  0c. Cache operations
 * ================================================================ */
static void test_cache_ops(void) {
    printf("\n--- Cache Operations ---\n");

    /* Cache count starts at 0 */
    int before = approval_cache_count();
    TEST_INT_EQ("initial cache count is 0", before, 0);

    /* Cache entry from empty cache returns NULL */
    const char *entry = approval_cache_entry(0);
    TEST_NULL("empty cache entry 0 is NULL", entry);

    entry = approval_cache_entry(999);
    TEST_NULL("empty cache entry 999 is NULL", entry);
}

/* ================================================================
 *  1. Timeout
 * ================================================================ */
static void test_timeout(void) {
    printf("\n--- Timeout ---\n");

    TEST_INT_EQ("default timeout", approval_get_timeout(), 30);

    approval_set_timeout(60);
    TEST_INT_EQ("set to 60", approval_get_timeout(), 60);

    /* Zero is clamped to 30 (default) */
    approval_set_timeout(0);
    TEST_INT_EQ("zero clamped to 30", approval_get_timeout(), 30);

    /* Negative clamped to 30 */
    approval_set_timeout(-5);
    TEST_INT_EQ("negative clamped to 30", approval_get_timeout(), 30);

    approval_set_timeout(300);
    TEST_INT_EQ("set to 300", approval_get_timeout(), 300);

    /* Restore default */
    approval_set_timeout(30);
}

/* ================================================================
 *  2. Yolo mode
 * ================================================================ */
static void test_yolo(void) {
    printf("\n--- Yolo Mode ---\n");

    /* Default: disabled */
    /* No getter for yolo — just verify set doesn't crash */
    approval_set_yolo(false);
    TEST("disable yolo no crash", 1);

    approval_set_yolo(true);
    TEST("enable yolo no crash", 1);

    approval_set_yolo(false);
    TEST("disable again no crash", 1);
}

/* ================================================================
 *  3. Cache count (initial state)
 * ================================================================ */
static void test_cache_count(void) {
    printf("\n--- Cache Count ---\n");

    /* Should start at 0 or whatever state the previous test left */
    /* Just verify it returns non-negative */
    int count = approval_cache_count();
    TEST("cache count non-negative", count >= 0);
    TEST("cache count reasonable", count < 10000);
}

/* ================================================================
 *  4. Cache clear
 * ================================================================ */
static void test_cache_clear(void) {
    printf("\n--- Cache Clear ---\n");

    /* Clear last 1 should not crash */
    approval_cache_clear_last(1);
    TEST("clear last 1 no crash", 1);

    /* Clear last 0 should not crash */
    approval_cache_clear_last(0);
    TEST("clear last 0 no crash", 1);

    /* Clear last -1 (invalid) should not crash */
    approval_cache_clear_last(-1);
    TEST("clear last -1 no crash", 1);

    /* Clear last 1000 (more than exist) should not crash */
    approval_cache_clear_last(1000);
    TEST("clear last 1000 no crash", 1);
}

/* ================================================================
 *  5. Allowlist path
 * ================================================================ */
static void test_allowlist_path(void) {
    printf("\n--- Allowlist Path ---\n");

    approval_set_allowlist_path("/tmp/test_allowlist.json");
    TEST("set allowlist path no crash", 1);

    /* Load from non-existent file — gracefully handles missing */
    approval_load_allowlist();
    TEST("load allowlist no crash", 1);

    /* Save to path */
    approval_save_allowlist();
    TEST("save allowlist no crash", 1);

    /* Reset to default */
    approval_set_allowlist_path(NULL);
    TEST("reset allowlist path no crash", 1);

    /* Cleanup */
    unlink("/tmp/test_allowlist.json");
}

int main(void) {
    printf("=== Approval System Test Suite (G128/G169) ===\n");

    test_session_reset();
    test_cache_ops();
    test_timeout();
    test_yolo();
    test_cache_count();
    test_cache_clear();
    test_allowlist_path();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
