/*
 * test_budget_tracker.c — Tests for budget_tracker module (P84/G24/G26).
 * Tests: init, limits, reporting, warnings, exceeded, remaining, iteration budget.
 * Compile: gcc -O2 -Wall -Wextra -I../include -I../lib/libjson -I../lib/libplugin
 *     test_budget_tracker.c ../src/agent/budget_tracker.c
 *     ../lib/libjson/json.c ../src/agent/provider_metadata.c
 *     -o /tmp/t_budget -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "budget_tracker.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

#define ASSERT_NEAR(a, b, eps) TEST(#a " ~= " #b, fabs((a) - (b)) <= (eps))

int main(void) {
    printf("=== Budget Tracker Tests ===\n");

    /* Test 1: init sets defaults */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        TEST("init clears struct", bt.total_input_tokens == 0);
        TEST("init max_iterations=0", bt.max_iterations == 0);
        TEST("init warn_at_pct=0.8", bt.warn_at_pct == 0.8);
        TEST("init hard_limit=false", bt.hard_limit == false);
        TEST("init max_tool_calls=0", bt.max_tool_calls_per_turn == 0);
    }

    /* Test 2: init(NULL) no crash */
    {
        budget_tracker_init(NULL);
        TEST("init(NULL) no crash", 1);
    }

    /* Test 3: set_limits works */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 1000, 500, 0.05, 10);
        TEST("set max_input=1000", bt.max_input_tokens == 1000);
        TEST("set max_output=500", bt.max_output_tokens == 500);
        TEST("set max_cost=0.05", bt.max_cost_usd == 0.05);
        TEST("set max_turns=10", bt.max_turns == 10);
    }

    /* Test 4: set_limits(NULL) no crash */
    {
        budget_tracker_set_limits(NULL, 100, 100, 1.0, 5);
        TEST("set_limits(NULL) no crash", 1);
    }

    /* Test 5: report_turn updates totals */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_report_turn(&bt, 100, 50, 0.001, "gpt-4");
        TEST("report adds input", bt.total_input_tokens == 100);
        TEST("report adds output", bt.total_output_tokens == 50);
        TEST("report adds cost", bt.total_cost_usd == 0.001);
        TEST("report increments turns", bt.turn_count == 1);
        TEST("report sets last_input", bt.last_input_tokens == 100);
        TEST("report sets last_model", strcmp(bt.last_model, "gpt-4") == 0);
    }

    /* Test 6: report_turn(NULL) no crash */
    {
        budget_tracker_report_turn(NULL, 100, 50, 0.001, "gpt-4");
        TEST("report_turn(NULL) no crash", 1);
    }

    /* Test 7: is_exceeded false by default */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        TEST("not exceeded by default", !budget_tracker_is_exceeded(&bt));
    }

    /* Test 8: is_exceeded true when over limit */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 100, 0, 0, 0);
        budget_tracker_report_turn(&bt, 200, 0, 0, "test");
        TEST("exceeded when over input limit", budget_tracker_is_exceeded(&bt));
    }

    /* Test 9: remaining_input/output/cost correct */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 1000, 500, 1.0, 10);
        TEST("remaining_input init", budget_tracker_remaining_input(&bt) == 1000);
        TEST("remaining_output init", budget_tracker_remaining_output(&bt) == 500);
        ASSERT_NEAR(budget_tracker_remaining_cost(&bt), 1.0, 0.001);
        TEST("remaining_turns init", budget_tracker_remaining_turns(&bt) == 10);

        budget_tracker_report_turn(&bt, 200, 50, 0.1, "test");
        TEST("remaining_input after use", budget_tracker_remaining_input(&bt) == 800);
        TEST("remaining_output after use", budget_tracker_remaining_output(&bt) == 450);
        ASSERT_NEAR(budget_tracker_remaining_cost(&bt), 0.9, 0.001);
        TEST("remaining_turns after use", budget_tracker_remaining_turns(&bt) == 9);
    }

    /* Test 10: remaining with no limits returns -1 (unlimited) */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        TEST("remaining_input -1 for unlimited", budget_tracker_remaining_input(&bt) == -1);
        TEST("remaining_output -1 for unlimited", budget_tracker_remaining_output(&bt) == -1);
        TEST("remaining_cost -1 for unlimited", budget_tracker_remaining_cost(&bt) == -1.0);
        TEST("remaining_turns -1 for unlimited", budget_tracker_remaining_turns(&bt) == -1);
    }

    /* Test 11: get_warning at 80% threshold */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 100, 0, 0, 0);
        budget_tracker_report_turn(&bt, 85, 0, 0, "test");
        const char *warn = budget_tracker_get_warning(&bt);
        TEST("warning emitted near limit", warn != NULL);
    }

    /* Test 12: get_warning returns NULL when under threshold */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 1000, 0, 0, 0);
        budget_tracker_report_turn(&bt, 100, 0, 0, "test");
        const char *warn = budget_tracker_get_warning(&bt);
        TEST("no warning under threshold", warn == NULL);
    }

    /* Test 13: get_warning clears flag (only warns once) */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 100, 0, 0, 0);
        budget_tracker_report_turn(&bt, 95, 0, 0, "test");
        const char *w1 = budget_tracker_get_warning(&bt);
        const char *w2 = budget_tracker_get_warning(&bt);
        TEST("first warning non-NULL", w1 != NULL);
        TEST("second warning NULL (cleared)", w2 == NULL);
    }

    /* Test 14: per-turn tool call tracking */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_per_turn_limit(&bt, 3);
        TEST("per-turn limit set", bt.max_tool_calls_per_turn == 3);
        TEST("turn not exceeded initially", !budget_tracker_turn_exceeded(&bt));

        budget_tracker_increment_tool_call(&bt);
        budget_tracker_increment_tool_call(&bt);
        budget_tracker_increment_tool_call(&bt);
        TEST("3 calls = count=3", bt.turn_tool_calls == 3);
        TEST("turn exceeded at limit", budget_tracker_turn_exceeded(&bt));

        budget_tracker_reset_turn_tools(&bt);
        TEST("reset clears count", bt.turn_tool_calls == 0);
        TEST("turn not exceeded after reset", !budget_tracker_turn_exceeded(&bt));
    }

    /* Test 15: set_per_turn_limit(NULL) no crash */
    {
        budget_tracker_set_per_turn_limit(NULL, 5);
        TEST("set_per_turn_limit(NULL) no crash", 1);
    }

    /* Test 16: hard limit mode */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_hard_limit(&bt, true);
        TEST("hard_limit set", bt.hard_limit == true);
        TEST("not hard exceeded below limit", !budget_tracker_is_hard_exceeded(&bt));

        budget_tracker_set_limits(&bt, 100, 0, 0, 0);
        budget_tracker_report_turn(&bt, 200, 0, 0, "test");
        TEST("hard exceeded when over limit", budget_tracker_is_hard_exceeded(&bt));
    }

    /* Test 17: stats_json produces valid output */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 1000, 500, 1.0, 10);
        budget_tracker_report_turn(&bt, 100, 50, 0.001, "gpt-4");

        char *json = budget_tracker_stats_json(&bt);
        TEST("stats_json non-NULL", json != NULL);
        if (json) {
            TEST("stats_json contains total_input", strstr(json, "total_input") != NULL);
            TEST("stats_json contains total_cost", strstr(json, "total_cost") != NULL);
            TEST("stats_json contains turn_count", strstr(json, "turn_count") != NULL);
            free(json);
        }
    }

    /* Test 18: stats_json(NULL) returns NULL */
    {
        char *json = budget_tracker_stats_json(NULL);
        TEST("stats_json(NULL) returns NULL", json == NULL);
    }

    /* Test 19: reset clears all */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_limits(&bt, 1000, 500, 1.0, 10);
        budget_tracker_report_turn(&bt, 100, 50, 0.001, "gpt-4");
        budget_tracker_reset(&bt);
        TEST("reset clears input", bt.total_input_tokens == 0);
        TEST("reset clears turns", bt.turn_count == 0);
        TEST("reset clears cost", bt.total_cost_usd == 0.0);
        TEST("reset preserves limits", bt.max_input_tokens == 1000);
    }

    /* Test 20: iteration budget */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_iteration_limit(&bt, 3);
        TEST("max_iterations set", bt.max_iterations == 3);
        TEST("iterations_used init", budget_tracker_iterations_used(&bt) == 0);
        TEST("iterations_remaining init", budget_tracker_iterations_remaining(&bt) == 3);

        TEST("consume 1 works", budget_tracker_consume_iteration(&bt));
        TEST("consume 2 works", budget_tracker_consume_iteration(&bt));
        TEST("consume 3 works", budget_tracker_consume_iteration(&bt));
        TEST("consume 4 fails (exhausted)", !budget_tracker_consume_iteration(&bt));
        TEST("iterations_used = 3", budget_tracker_iterations_used(&bt) == 3);
        TEST("iterations_remaining = 0", budget_tracker_iterations_remaining(&bt) == 0);

        budget_tracker_refund_iteration(&bt);
        TEST("refund restores to 2 remaining", budget_tracker_iterations_remaining(&bt) == 1);
        TEST("refund then consume works", budget_tracker_consume_iteration(&bt));
    }

    /* Test 21: consume_iteration with unlimited (0) always works */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        for (int i = 0; i < 100; i++)
            if (!budget_tracker_consume_iteration(&bt))
                { TEST("unlimited consume never fails", 0); break; }
        TEST("unlimited remaining = -1", budget_tracker_iterations_remaining(&bt) == -1);
    }

    /* Test 22: NULL safety for remaining/getters */
    {
        TEST("remaining_input(NULL)=-1", budget_tracker_remaining_input(NULL) == -1);
        TEST("remaining_output(NULL)=-1", budget_tracker_remaining_output(NULL) == -1);
        TEST("remaining_cost(NULL)=-1", budget_tracker_remaining_cost(NULL) == -1.0);
        TEST("remaining_turns(NULL)=-1", budget_tracker_remaining_turns(NULL) == -1);
        TEST("iterations_remaining(NULL)=-1", budget_tracker_iterations_remaining(NULL) == -1);
        TEST("is_exceeded(NULL)=false", !budget_tracker_is_exceeded(NULL));
        TEST("is_hard_exceeded(NULL)=false", !budget_tracker_is_hard_exceeded(NULL));
        TEST("turn_exceeded(NULL)=false", !budget_tracker_turn_exceeded(NULL));
        TEST("iterations_used(NULL)=0", budget_tracker_iterations_used(NULL) == 0);
        TEST("consume_iteration(NULL)=false", !budget_tracker_consume_iteration(NULL));
        budget_tracker_refund_iteration(NULL);
        TEST("refund_iteration(NULL) no crash", 1);
        budget_tracker_reset(NULL);
        TEST("reset(NULL) no crash", 1);
        const char *w = budget_tracker_get_warning(NULL);
        TEST("get_warning(NULL)=NULL", w == NULL);
    }

    /* Test 23: Hard limit with grace call mode */
    {
        budget_tracker_t bt;
        budget_tracker_init(&bt);
        budget_tracker_set_hard_limit(&bt, false);
        budget_tracker_set_limits(&bt, 100, 0, 0, 0);
        budget_tracker_report_turn(&bt, 200, 0, 0, "test");
        TEST("exceeded in grace mode", budget_tracker_is_exceeded(&bt));
        TEST("not hard exceeded in grace mode", !budget_tracker_is_hard_exceeded(&bt));
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All budget_tracker tests PASSED");
    return failures ? 1 : 0;
}
