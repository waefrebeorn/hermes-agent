/* test_plugin_achievements.c — Tests for achievement tracking plugin.
 * Builds as standalone test by compiling plugin source directly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include plugin source directly for unit testing */
#include "../src/plugins/plugin_achievements.c"

/* Test helpers */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    fprintf(stderr, "  TEST: %s ... ", name); \
} while (0)

#define PASS() do { \
    tests_passed++; \
    fprintf(stderr, "PASS\n"); \
} while (0)

#define FAIL(msg) do { \
    tests_failed++; \
    fprintf(stderr, "FAIL: %s\n", msg); \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* Test that achievement definitions are valid */
static void test_achievement_defs(void) {
    TEST("achievement definitions are valid");
    ASSERT(g_achievement_count > 0, "should have at least 1 achievement");
    ASSERT(g_achievement_count <= MAX_ACHIEVEMENTS, "should not exceed MAX_ACHIEVEMENTS");
    for (int i = 0; i < g_achievement_count; i++) {
        ASSERT(ACHIEVEMENTS[i].id != NULL, "id should not be NULL");
        ASSERT(ACHIEVEMENTS[i].name != NULL, "name should not be NULL");
        ASSERT(ACHIEVEMENTS[i].id[0] != '\0', "id should not be empty");
        ASSERT(ACHIEVEMENTS[i].tier_count > 0, "tier_count should be > 0");
        ASSERT(ACHIEVEMENTS[i].tier_count <= MAX_TIERS, "tier_count should not exceed MAX_TIERS");
    }
    PASS();
}

/* Test metrics_to_json output */
static void test_metrics_json(void) {
    TEST("metrics_to_json returns valid JSON");
    state_load(); /* reset to known state */
    char *json = metrics_to_json();
    ASSERT(json != NULL, "should return non-NULL");
    ASSERT(strstr(json, "total_tool_calls") != NULL, "should contain total_tool_calls");
    ASSERT(strstr(json, "achievements") != NULL, "should contain achievements array");
    ASSERT(strstr(json, "let_him_cook") != NULL, "should contain achievement id");
    free(json);
    PASS();
}

/* Test tier evaluation */
static void test_tier_evaluation(void) {
    TEST("tier evaluation matches thresholds");
    /* Set a specific metric manually via the state */
    g_state.max_tool_calls_in_session = 500;
    evaluate_all();
    ASSERT((g_state.unlocked_mask & 1) != 0, "let_him_cook should be unlocked at 500 (>=200)");
    ASSERT(g_state.achieved_tiers[0] >= 0, "should have tier >= 0");
    ASSERT(g_state.achieved_tiers[0] >= 0 && g_state.achieved_tiers[0] < 5,
           "tier should be in valid range");
    PASS();
}

/* Test secret achievements stay hidden */
static void test_secret_achievements(void) {
    TEST("secret achievements hidden without progress");
    /* port_3000_taken is secret */
    g_state.port_conflict_events = 0;
    evaluate_all();
    int idx = -1;
    for (int i = 0; i < g_achievement_count; i++) {
        if (strcmp(ACHIEVEMENTS[i].id, "port_3000_taken") == 0) { idx = i; break; }
    }
    ASSERT(idx >= 0, "port_3000_taken should be found");
    int unlocked = (g_state.unlocked_mask >> idx) & 1;
    ASSERT(unlocked == 0, "should not be unlocked with 0 events");
    PASS();
}

/* Test secret achievement unlocks with progress */
static void test_secret_unlock(void) {
    TEST("secret achievements unlock at threshold");
    int idx = -1;
    for (int i = 0; i < g_achievement_count; i++) {
        if (strcmp(ACHIEVEMENTS[i].id, "port_3000_taken") == 0) { idx = i; break; }
    }
    ASSERT(idx >= 0, "port_3000_taken should be found");
    g_state.port_conflict_events = 100;
    evaluate_all();
    int unlocked = (g_state.unlocked_mask >> idx) & 1;
    ASSERT(unlocked == 1, "should be unlocked at 100 (>=15)");
    PASS();
}

/* Test state save/load roundtrip */
static void test_state_persistence(void) {
    TEST("state save/load preserves data");
    const char *tmp_path = "/tmp/test_achievements_state.json";
    snprintf(g_state_path, sizeof(g_state_path), "%s", tmp_path);

    g_state.total_tool_calls = 12345;
    g_state.total_errors = 500;
    g_state.unlocked_mask = 0xFF;
    state_save();

    memset(&g_state, 0, sizeof(g_state));
    state_load();

    ASSERT(g_state.total_tool_calls == 12345, "total_tool_calls should be preserved");
    ASSERT(g_state.total_errors == 500, "total_errors should be preserved");
    ASSERT(g_state.unlocked_mask == 0xFF, "unlocked_mask should be preserved");

    unlink(tmp_path);
    PASS();
}

/* Test list_achievements tool */
static void test_list_achievements(void) {
    TEST("list_achievements returns all achievements");
    char *result = tool_list_achievements("{}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "count") != NULL, "should contain count field");
    ASSERT(strstr(result, "let_him_cook") != NULL, "should contain let_him_cook");
    ASSERT(strstr(result, "terminal_goblin") != NULL, "should contain terminal_goblin");
    free(result);
    PASS();
}

/* Test update_metrics tool */
static void test_update_metrics(void) {
    TEST("update_metrics increments tool call counter");
    long before = g_state.total_tool_calls;
    char *result = tool_update_metrics("{\"tool_call\":1}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "updated") != NULL, "should return updated status");
    ASSERT(g_state.total_tool_calls == before + 1,
           "total_tool_calls should increment by 1");
    free(result);
    PASS();
}

/* Test get_metrics returns current state */
static void test_get_metrics(void) {
    TEST("get_metrics returns current metrics");
    char *result = tool_get_metrics("{}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "total_tool_calls") != NULL, "should contain metrics");
    long calls = 0;
    sscanf(result, "{\"total_tool_calls\":%ld", &calls);
    ASSERT(calls == g_state.total_tool_calls, "metric values should match state");
    free(result);
    PASS();
}

/* Test plugin lifecycle */
static void test_plugin_lifecycle(void) {
    TEST("plugin_init initializes interface");
    memset(&g_interface, 0, sizeof(g_interface));
    int ret = plugin_init();
    ASSERT(ret == 0, "init should return 0");
    ASSERT(g_interface.type == PLUGIN_ACHIEVEMENTS, "type should be PLUGIN_ACHIEVEMENTS");
    PASS();
}

int main(void) {
    fprintf(stderr, "Achievements Plugin Tests\n");
    fprintf(stderr, "=========================\n");

    /* Set state path to tmp for persistence test */
    snprintf(g_state_path, sizeof(g_state_path),
             "/tmp/test_achievements_state.json");

    state_load();

    test_achievement_defs();
    test_metrics_json();
    test_tier_evaluation();
    test_secret_achievements();
    test_secret_unlock();
    test_state_persistence();
    test_list_achievements();
    test_update_metrics();
    test_get_metrics();
    test_plugin_lifecycle();

    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);

    /* Cleanup */
    unlink("/tmp/test_achievements_state.json");

    if (tests_failed > 0) return 1;
    return 0;
}
