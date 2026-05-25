/*
 * test_interrupt.c — Tests for per-thread interrupt signaling library.
 */

#include "interrupt.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while(0)

static int test_not_interrupted_initially(void) {
    ASSERT(interrupt_is_interrupted() == false, "Should not be interrupted initially");
    return 1;
}

static int test_set_interrupt(void) {
    interrupt_set(true, 0);
    ASSERT(interrupt_is_interrupted() == true, "Should be interrupted after set");
    interrupt_set(false, 0);
    ASSERT(interrupt_is_interrupted() == false, "Should be cleared after unset");
    return 1;
}

static int test_count(void) {
    interrupt_clear_all();
    ASSERT(interrupt_count() == 0, "Count should be 0 after clear_all");
    interrupt_set(true, 0);
    ASSERT(interrupt_count() == 1, "Count should be 1 after set");
    interrupt_set(false, 0);
    ASSERT(interrupt_count() == 0, "Count should be 0 after clear");
    return 1;
}

static int test_clear_all(void) {
    interrupt_set(true, 0);
    interrupt_set(true, 42);  /* fake thread */
    ASSERT(interrupt_count() == 2, "Count should be 2");
    interrupt_clear_all();
    ASSERT(interrupt_count() == 0, "Count should be 0 after clear_all");
    return 1;
}

static int test_specific_thread(void) {
    interrupt_clear_all();
    /* Interrupt thread 42 but not current thread */
    interrupt_set(true, 42);
    ASSERT(interrupt_is_interrupted() == false, "Current thread should not be interrupted");
    ASSERT(interrupt_count() == 1, "Count should be 1");
    /* Clear thread 42 */
    interrupt_set(false, 42);
    ASSERT(interrupt_count() == 0, "Count should be 0 after clear");
    return 1;
}

static int test_specific_thread_zero_means_self(void) {
    interrupt_clear_all();
    interrupt_set(true, 0);
    ASSERT(interrupt_is_interrupted() == true, "thread_id=0 should target self");
    interrupt_set(false, 0);
    return 1;
}

static int test_multiple_interrupted_threads(void) {
    interrupt_clear_all();
    interrupt_set(true, 10);
    interrupt_set(true, 20);
    interrupt_set(true, 30);
    ASSERT(interrupt_count() == 3, "Count should be 3");
    ASSERT(interrupt_is_interrupted() == false, "Current thread not interrupted");
    interrupt_clear_all();
    return 1;
}

static int test_redundant_set(void) {
    interrupt_clear_all();
    interrupt_set(true, 0);
    interrupt_set(true, 0);  /* redundant */
    ASSERT(interrupt_count() == 1, "Count should still be 1");
    interrupt_set(false, 0);
    return 1;
}

int main(void) {
    printf("=== interrupt tests ===\n\n");

    TEST(not_interrupted_initially);
    TEST(set_interrupt);
    TEST(count);
    TEST(clear_all);
    TEST(specific_thread);
    TEST(specific_thread_zero_means_self);
    TEST(multiple_interrupted_threads);
    TEST(redundant_set);

    printf("\n%s\n", tests_failed > 0 ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    printf("Results: %d/%d passed, %d failed, %d total (sum %d)\n",
           tests_passed, tests_run, tests_failed,
           tests_run, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
