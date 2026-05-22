/*
 * test_budget_tracker.c — Budget tracker test suite (G158).
 *
 * Tests: init, limits, turn reporting, exceeded detection,
 * remaining budget, warnings, stats JSON, reset, NULL safety.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_budget_tracker.c \
 *       src/agent/budget_tracker.c src/agent/provider_metadata.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_budget -lm
 *
 * Run:
 *   /tmp/hermes_test_budget
 */

#include "budget_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* ================================================================
 *  1. Init tests
 * ================================================================ */
static void test_init(void) {
    printf("\n--- Init ---\n");

    budget_tracker_t bt;
    memset(&bt, 0xAA, sizeof(bt)); /* Fill with garbage */

    budget_tracker_init(&bt);

    TEST_INT_EQ("max_input_tokens defaults to 0", bt.max_input_tokens, 0);
    TEST_INT_EQ("max_output_tokens defaults to 0", bt.max_output_tokens, 0);
    TEST("max_cost_usd defaults to 0.0", bt.max_cost_usd == 0.0);
    TEST_INT_EQ("max_turns defaults to 0", bt.max_turns, 0);
    TEST_INT_EQ("total_input_tokens starts 0", bt.total_input_tokens, 0);
    TEST_INT_EQ("total_output_tokens starts 0", bt.total_output_tokens, 0);
    TEST("total_cost_usd starts 0.0", bt.total_cost_usd == 0.0);
    TEST_INT_EQ("turn_count starts 0", bt.turn_count, 0);
    TEST("warn_at_pct defaults to 0.8", bt.warn_at_pct == 0.8);
    TEST("warned_input false", !bt.warned_input);
    TEST("warned_output false", !bt.warned_output);
    TEST("warned_cost false", !bt.warned_cost);
    TEST("warned_turns false", !bt.warned_turns);

    /* NULL safety */
    budget_tracker_init(NULL);
    TEST("init NULL no crash", 1);
}

/* ================================================================
 *  2. Set limits tests
 * ================================================================ */
static void test_set_limits(void) {
    printf("\n--- Set Limits ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);

    budget_tracker_set_limits(&bt, 1000, 500, 0.05, 10);
    TEST_INT_EQ("max_input set", bt.max_input_tokens, 1000);
    TEST_INT_EQ("max_output set", bt.max_output_tokens, 500);
    TEST("max_cost set", bt.max_cost_usd == 0.05);
    TEST_INT_EQ("max_turns set", bt.max_turns, 10);

    /* Reset with 0 = unlimited */
    budget_tracker_set_limits(&bt, 0, 0, 0, 0);
    TEST_INT_EQ("unlimited input", bt.max_input_tokens, 0);
    TEST_INT_EQ("unlimited output", bt.max_output_tokens, 0);
    TEST("unlimited cost", bt.max_cost_usd == 0.0);
    TEST_INT_EQ("unlimited turns", bt.max_turns, 0);

    /* NULL safety */
    budget_tracker_set_limits(NULL, 100, 100, 1.0, 5);
    TEST("set_limits NULL no crash", 1);
}

/* ================================================================
 *  3. Report turn — basic accumulation
 * ================================================================ */
static void test_report_turn(void) {
    printf("\n--- Report Turn ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);
    budget_tracker_set_limits(&bt, 10000, 5000, 1.0, 20);

    /* Report first turn */
    budget_tracker_report_turn(&bt, 100, 50, 0.01, "deepseek-v4");
    TEST_INT_EQ("total_input after 1 turn", bt.total_input_tokens, 100);
    TEST_INT_EQ("total_output after 1 turn", bt.total_output_tokens, 50);
    TEST("total_cost after 1 turn", bt.total_cost_usd == 0.01);
    TEST_INT_EQ("turn_count after 1", bt.turn_count, 1);
    TEST_INT_EQ("last_input", bt.last_input_tokens, 100);
    TEST_INT_EQ("last_output", bt.last_output_tokens, 50);
    TEST("last_cost", bt.last_cost_usd == 0.01);
    TEST_STR_EQ("last_model", bt.last_model, "deepseek-v4");

    /* Report second turn */
    budget_tracker_report_turn(&bt, 200, 75, 0.02, "gpt-4o");
    TEST_INT_EQ("total_input after 2 turns", bt.total_input_tokens, 300);
    TEST_INT_EQ("total_output after 2 turns", bt.total_output_tokens, 125);
    TEST("total_cost after 2 turns", bt.total_cost_usd == 0.03);
    TEST_INT_EQ("turn_count after 2", bt.turn_count, 2);
    TEST_STR_EQ("last_model updated", bt.last_model, "gpt-4o");

    /* Report with NULL model (should not crash, model field unchanged) */
    budget_tracker_report_turn(&bt, 50, 25, 0.005, NULL);
    TEST_INT_EQ("total_input after NULL model", bt.total_input_tokens, 350);
    TEST("last_model unchanged after NULL model",
         strcmp(bt.last_model, "gpt-4o") == 0);

    /* Report with zero values */
    budget_tracker_report_turn(&bt, 0, 0, 0.0, "noop");
    TEST_INT_EQ("zero tokens still counted", bt.total_input_tokens, 350);
    TEST_INT_EQ("turn_count incremented", bt.turn_count, 4);

    /* NULL safety */
    budget_tracker_report_turn(NULL, 100, 50, 0.01, "test");
    TEST("report_turn NULL no crash", 1);
}

/* ================================================================
 *  4. Exceeded detection
 * ================================================================ */
static void test_is_exceeded(void) {
    printf("\n--- Is Exceeded ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);

    /* No limits — never exceeded */
    TEST("no limits → not exceeded", !budget_tracker_is_exceeded(&bt));

    /* Set limits */
    budget_tracker_set_limits(&bt, 1000, 500, 0.10, 5);

    /* Under limits */
    budget_tracker_report_turn(&bt, 100, 50, 0.01, "model");
    TEST("under limit → not exceeded", !budget_tracker_is_exceeded(&bt));

    /* Exceed input */
    budget_tracker_set_limits(&bt, 100, 1000, 1.0, 10);
    budget_tracker_report_turn(&bt, 150, 0, 0, "model");
    TEST("input exceeded", budget_tracker_is_exceeded(&bt));

    /* Reset and test output exceeded */
    budget_tracker_reset(&bt);
    budget_tracker_set_limits(&bt, 10000, 100, 1.0, 10);
    budget_tracker_report_turn(&bt, 10, 200, 0, "model");
    TEST("output exceeded", budget_tracker_is_exceeded(&bt));

    /* Reset and test cost exceeded */
    budget_tracker_reset(&bt);
    budget_tracker_set_limits(&bt, 10000, 10000, 0.05, 10);
    budget_tracker_report_turn(&bt, 10, 50, 0.06, "model");
    TEST("cost exceeded", budget_tracker_is_exceeded(&bt));

    /* Reset and test turns exceeded */
    budget_tracker_reset(&bt);
    budget_tracker_set_limits(&bt, 10000, 10000, 1.0, 3);
    budget_tracker_report_turn(&bt, 10, 50, 0.01, "model");
    budget_tracker_report_turn(&bt, 10, 50, 0.01, "model");
    budget_tracker_report_turn(&bt, 10, 50, 0.01, "model");
    TEST("turns at limit → exceeded", budget_tracker_is_exceeded(&bt));

    /* Exceeded on multiple dimensions */
    budget_tracker_report_turn(&bt, 10000, 10000, 0, "model");
    TEST("still exceeded after more turns", budget_tracker_is_exceeded(&bt));

    /* NULL safety */
    TEST("is_exceeded NULL returns false", !budget_tracker_is_exceeded(NULL));
}

/* ================================================================
 *  5. Remaining budget
 * ================================================================ */
static void test_remaining(void) {
    printf("\n--- Remaining ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);

    /* No limits — returns -1 */
    TEST_INT_EQ("remaining_input no limit", budget_tracker_remaining_input(&bt), -1);
    TEST_INT_EQ("remaining_output no limit", budget_tracker_remaining_output(&bt), -1);
    TEST("remaining_cost no limit", budget_tracker_remaining_cost(&bt) == -1.0);
    TEST_INT_EQ("remaining_turns no limit", budget_tracker_remaining_turns(&bt), -1);

    /* With limits */
    budget_tracker_set_limits(&bt, 1000, 500, 0.10, 10);
    budget_tracker_report_turn(&bt, 200, 100, 0.03, "model");

    TEST_INT_EQ("remaining_input", budget_tracker_remaining_input(&bt), 800);
    TEST_INT_EQ("remaining_output", budget_tracker_remaining_output(&bt), 400);
    TEST("remaining_cost", budget_tracker_remaining_cost(&bt) == 0.07);
    TEST_INT_EQ("remaining_turns", budget_tracker_remaining_turns(&bt), 9);

    /* Over budget — negative values */
    budget_tracker_set_limits(&bt, 100, 100, 1.0, 10);
    budget_tracker_report_turn(&bt, 200, 200, 0, "model");
    TEST("remaining_input negative after over", budget_tracker_remaining_input(&bt) < 0);
    TEST("remaining_output negative after over", budget_tracker_remaining_output(&bt) < 0);

    /* NULL safety */
    TEST_INT_EQ("remaining_input NULL", budget_tracker_remaining_input(NULL), -1);
    TEST_INT_EQ("remaining_output NULL", budget_tracker_remaining_output(NULL), -1);
    TEST("remaining_cost NULL", budget_tracker_remaining_cost(NULL) == -1.0);
    TEST_INT_EQ("remaining_turns NULL", budget_tracker_remaining_turns(NULL), -1);
}

/* ================================================================
 *  6. Warning messages
 * ================================================================ */
static void test_warnings(void) {
    printf("\n--- Warnings ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);

    /* No limits — no warning */
    TEST_NULL("no limits → no warning", budget_tracker_get_warning(&bt));

    /* Set limits, warn_at_pct=0.8 */
    budget_tracker_set_limits(&bt, 1000, 500, 0.10, 10);

    /* Under 80% — no warning */
    budget_tracker_report_turn(&bt, 100, 50, 0.01, "model");
    TEST_NULL("under 80% → no warning", budget_tracker_get_warning(&bt));

    /* Report to hit input warn threshold (800 = 80% of 1000) */
    budget_tracker_report_turn(&bt, 700, 0, 0, "model");
    const char *w = budget_tracker_get_warning(&bt);
    TEST_NOT_NULL("input warning emitted", w);
    TEST("input warning text contains 'Input'",
         w && strstr(w, "Input") != NULL);
    TEST("input warning text contains '200' (remaining)",
         w && strstr(w, "200") != NULL);

    /* Only emitted once */
    TEST_NULL("input warning second call returns NULL",
              budget_tracker_get_warning(&bt));

    /* Report to hit output warn threshold (400 = 80% of 500) */
    budget_tracker_report_turn(&bt, 0, 350, 0, "model");
    w = budget_tracker_get_warning(&bt);
    TEST_NOT_NULL("output warning emitted", w);
    TEST("output warning contains 'Output'",
         w && strstr(w, "Output") != NULL);

    /* Only emitted once */
    TEST_NULL("output warning second call returns NULL",
              budget_tracker_get_warning(&bt));

    /* Test cost warning */
    budget_tracker_reset(&bt);
    budget_tracker_set_limits(&bt, 10000, 10000, 1.0, 100);
    budget_tracker_report_turn(&bt, 100, 100, 0.90, "model");
    w = budget_tracker_get_warning(&bt);
    TEST_NOT_NULL("cost warning emitted", w);
    TEST("cost warning contains 'Cost'",
         w && strstr(w, "Cost") != NULL);

    /* Test turn warning */
    budget_tracker_reset(&bt);
    budget_tracker_set_limits(&bt, 10000, 10000, 10.0, 10);
    for (int i = 0; i < 8; i++)
        budget_tracker_report_turn(&bt, 10, 10, 0, "model");
    w = budget_tracker_get_warning(&bt);
    TEST_NOT_NULL("turn warning emitted", w);
    TEST("turn warning contains 'Turn'",
         w && strstr(w, "Turn") != NULL);

    /* NULL safety */
    TEST_NULL("get_warning NULL", budget_tracker_get_warning(NULL));
}

/* ================================================================
 *  7. Reset
 * ================================================================ */
static void test_reset(void) {
    printf("\n--- Reset ---\n");

    budget_tracker_t bt;
    budget_tracker_init(&bt);
    budget_tracker_set_limits(&bt, 1000, 500, 0.10, 10);

    /* Use it */
    budget_tracker_report_turn(&bt, 200, 100, 0.03, "deepseek");
    budget_tracker_report_turn(&bt, 300, 150, 0.02, "gpt-4o");
    TEST("used up some budget", bt.total_input_tokens == 500);

    /* Reset — limits preserved, totals zeroed */
    budget_tracker_reset(&bt);
    TEST_INT_EQ("input limit preserved", bt.max_input_tokens, 1000);
    TEST_INT_EQ("output limit preserved", bt.max_output_tokens, 500);
    TEST("cost limit preserved", bt.max_cost_usd == 0.10);
    TEST_INT_EQ("turn limit preserved", bt.max_turns, 10);
    TEST("warn_at_pct preserved", bt.warn_at_pct == 0.8);
    TEST_INT_EQ("total_input zeroed", bt.total_input_tokens, 0);
    TEST_INT_EQ("total_output zeroed", bt.total_output_tokens, 0);
    TEST("total_cost zeroed", bt.total_cost_usd == 0.0);
    TEST_INT_EQ("turn_count zeroed", bt.turn_count, 0);

    /* NULL safety */
    budget_tracker_reset(NULL);
    TEST("reset NULL no crash", 1);
}

/* ================================================================
 *  8. Stats JSON
 * ================================================================ */
static void test_stats_json(void) {
    printf("\n--- Stats JSON ---\n");

    /* NULL tracker */
    char *json = budget_tracker_stats_json(NULL);
    TEST_NULL("stats_json NULL returns NULL", json);
    free(json);

    budget_tracker_t bt;
    budget_tracker_init(&bt);
    budget_tracker_set_limits(&bt, 1000, 500, 0.10, 10);
    budget_tracker_report_turn(&bt, 100, 50, 0.01, "test-model");

    json = budget_tracker_stats_json(&bt);
    TEST_NOT_NULL("stats_json returns non-NULL", json);
    if (json) {
        TEST("json contains turn_count", strstr(json, "turn_count") != NULL);
        TEST("json contains total_input_tokens", strstr(json, "total_input_tokens") != NULL);
        TEST("json contains total_output_tokens", strstr(json, "total_output_tokens") != NULL);
        TEST("json contains total_cost_usd", strstr(json, "total_cost_usd") != NULL);
        TEST("json contains last_model", strstr(json, "last_model") != NULL);
        TEST("json contains test-model", strstr(json, "test-model") != NULL);
        TEST("json contains limits", strstr(json, "limits") != NULL);
        TEST("json contains warnings", strstr(json, "warnings") != NULL);
        TEST("json contains exceeded", strstr(json, "exceeded") != NULL);
        TEST("json contains 100 (input)", strstr(json, "100") != NULL);
        TEST("json contains 50 (output)", strstr(json, "50") != NULL);
        free(json);
    }
}

/* ================================================================
 *  9. Estimate cost
 * ================================================================ */
static void test_estimate_cost(void) {
    printf("\n--- Estimate Cost ---\n");

    /* Delegates to model_estimate_cost — smoke test only */
    double cost = budget_tracker_estimate_cost("deepseek-v4", 1000, 500);
    TEST("estimate_cost returns non-negative", cost >= 0.0);

    /* Unknown model uses fallback (not 0) — non-zero for non-zero tokens */
    cost = budget_tracker_estimate_cost("nonexistent-model-xyz", 100, 100);
    TEST("unknown model returns fallback cost", cost > 0.0);

    /* NULL model uses fallback — non-zero for non-zero tokens */
    cost = budget_tracker_estimate_cost(NULL, 100, 100);
    TEST("NULL model returns fallback cost", cost > 0.0);
}

int main(void) {
    printf("=== Budget Tracker Test Suite (G158) ===\n");

    test_init();
    test_set_limits();
    test_report_turn();
    test_is_exceeded();
    test_remaining();
    test_warnings();
    test_reset();
    test_stats_json();
    test_estimate_cost();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
