/*
 * test_iteration_budget.c — Tests for iteration budget (port of Python
 * agent/iteration_budget.py).
 *
 * Tests: consume until exhausted, refund, unlimited mode, edge cases.
 */

#include "budget_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Stub for model_estimate_cost — not called by iteration budget tests
 * but linked from budget_tracker.c when --gc-sections isn't supported. */
double model_estimate_cost(const char *model, long long in_tok, long long out_tok) {
    (void)model; (void)in_tok; (void)out_tok;
    return 0.0;
}

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Helper to create a budget tracker with iteration limits */
static budget_tracker_t *make_tracker(int max_iterations) {
    budget_tracker_t *bt = (budget_tracker_t *)calloc(1, sizeof(budget_tracker_t));
    assert(bt);
    budget_tracker_init(bt);
    budget_tracker_set_iteration_limit(bt, max_iterations);
    return bt;
}

static void test_consume_until_exhausted(void) {
    printf("\n--- Consume until exhausted ---\n");
    budget_tracker_t *bt = make_tracker(3);

    TEST("consume 1 returns true", budget_tracker_consume_iteration(bt));
    TEST("used == 1", budget_tracker_iterations_used(bt) == 1);

    TEST("consume 2 returns true", budget_tracker_consume_iteration(bt));
    TEST("used == 2", budget_tracker_iterations_used(bt) == 2);

    TEST("consume 3 returns true", budget_tracker_consume_iteration(bt));
    TEST("used == 3", budget_tracker_iterations_used(bt) == 3);

    TEST("consume 4 returns false (exhausted)", !budget_tracker_consume_iteration(bt));
    TEST("used still 3", budget_tracker_iterations_used(bt) == 3);

    TEST("is_exceeded true", budget_tracker_is_exceeded(bt));
    TEST("remaining == 0", budget_tracker_iterations_remaining(bt) == 0);

    free(bt);
}

static void test_refund(void) {
    printf("\n--- Refund ---\n");
    budget_tracker_t *bt = make_tracker(3);

    /* Use 2, refund 1, use 2 more = should exhaust */
    budget_tracker_consume_iteration(bt);
    budget_tracker_consume_iteration(bt);
    TEST("used == 2", budget_tracker_iterations_used(bt) == 2);

    budget_tracker_refund_iteration(bt);
    TEST("after refund, used == 1", budget_tracker_iterations_used(bt) == 1);

    budget_tracker_consume_iteration(bt);
    budget_tracker_consume_iteration(bt);
    TEST("used == 3 after refund+2", budget_tracker_iterations_used(bt) == 3);

    TEST("next consume false", !budget_tracker_consume_iteration(bt));

    /* Refund should work even at limit */
    budget_tracker_refund_iteration(bt);
    TEST("refund at limit, used == 2", budget_tracker_iterations_used(bt) == 2);

    /* Should be able to consume once more */
    TEST("consume after refund", budget_tracker_consume_iteration(bt));
    TEST("used == 3", budget_tracker_iterations_used(bt) == 3);

    free(bt);
}

static void test_refund_safety(void) {
    printf("\n--- Refund safety (no negative) ---\n");
    budget_tracker_t *bt = make_tracker(5);

    /* Refund at 0 should not go negative */
    budget_tracker_refund_iteration(bt);
    TEST("refund at 0, used == 0", budget_tracker_iterations_used(bt) == 0);

    /* Multiple refunds at 0 */
    budget_tracker_refund_iteration(bt);
    budget_tracker_refund_iteration(bt);
    TEST("multiple refunds at 0, used == 0", budget_tracker_iterations_used(bt) == 0);

    free(bt);
}

static void test_unlimited(void) {
    printf("\n--- Unlimited mode ---\n");
    budget_tracker_t *bt = make_tracker(0);  /* 0 = unlimited */

    TEST("remaining == -1 (unlimited)", budget_tracker_iterations_remaining(bt) == -1);

    /* Should never exhaust */
    for (int i = 0; i < 10000; i++) {
        if (!budget_tracker_consume_iteration(bt)) {
            printf("  FAIL: exhausted at %d with unlimited budget\n", i);
            failed++;
            break;
        }
    }
    TEST("used == 10000", budget_tracker_iterations_used(bt) == 10000);
    TEST("not exceeded", !budget_tracker_is_exceeded(bt));

    free(bt);
}

static void test_null_safety(void) {
    printf("\n--- NULL safety ---\n");
    TEST("consume on NULL returns false", !budget_tracker_consume_iteration(NULL));
    TEST("refund on NULL is safe", 1);  /* no crash */
    TEST("used on NULL returns 0", budget_tracker_iterations_used(NULL) == 0);
    TEST("remaining on NULL returns -1", budget_tracker_iterations_remaining(NULL) == -1);
    budget_tracker_set_iteration_limit(NULL, 10);
    TEST("set_limit on NULL is safe", 1);  /* no crash */
}

static void test_reset_preserves_limit(void) {
    printf("\n--- Reset preserves iteration limit ---\n");
    budget_tracker_t *bt = make_tracker(5);

    budget_tracker_consume_iteration(bt);
    budget_tracker_consume_iteration(bt);
    TEST("used == 2 before reset", budget_tracker_iterations_used(bt) == 2);

    budget_tracker_reset(bt);
    TEST("used == 0 after reset", budget_tracker_iterations_used(bt) == 0);
    TEST("limit still 5 after reset", budget_tracker_iterations_remaining(bt) == 5);

    free(bt);
}

int main(void) {
    printf("=== Iteration Budget Tests ===\n");

    test_consume_until_exhausted();
    test_refund();
    test_refund_safety();
    test_unlimited();
    test_null_safety();
    test_reset_preserves_limit();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
