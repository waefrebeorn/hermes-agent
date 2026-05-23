/* test_plugin_observability.c — Tests for observability plugin.
 * Builds as standalone test by compiling plugin source directly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include plugin source directly for unit testing */
#include "../src/plugins/plugin_observability.c"

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

/* Reset state between tests */
static void reset_state(void) {
    memset(g_metrics, 0, sizeof(g_metrics));
    g_metric_count = 0;
    memset(g_event_ring, 0, sizeof(g_event_ring));
    g_event_head = 0;
    g_event_count = 0;
    g_event_wraps = 0;
}

/* Test recording an event */
static void test_record_event(void) {
    TEST("record_event creates metric counter");
    reset_state();
    impl_record_event("terminal_call", NULL);
    ASSERT(g_metric_count == 1, "should create 1 metric");
    ASSERT(strcmp(g_metrics[0].name, "terminal_call") == 0, "metric name should match");
    ASSERT(g_metrics[0].count == 1, "count should be 1");
    PASS();
}

/* Test recording multiple events */
static void test_record_multiple(void) {
    TEST("record_event increments counters");
    reset_state();
    impl_record_event("terminal_call", NULL);
    impl_record_event("terminal_call", NULL);
    impl_record_event("tool_call", NULL);
    ASSERT(g_metric_count == 2, "should have 2 distinct metrics");
    metric_t *m = find_metric("terminal_call");
    ASSERT(m != NULL, "terminal_call metric should exist");
    ASSERT(m->count == 2, "terminal_call count should be 2");
    m = find_metric("tool_call");
    ASSERT(m != NULL, "tool_call metric should exist");
    ASSERT(m->count == 1, "tool_call count should be 1");
    PASS();
}

/* Test recording with value extraction */
static void test_record_with_value(void) {
    TEST("record_event extracts value from data_json");
    reset_state();
    impl_record_event("api_latency", "{\"value\":42.5}");
    metric_t *m = find_metric("api_latency");
    ASSERT(m != NULL, "metric should exist");
    ASSERT(m->sum == 42.5, "sum should be extracted value");
    ASSERT(m->last_value == 42.5, "last_value should be extracted");
    PASS();
}

/* Test get_metrics returns valid JSON */
static void test_get_metrics_json(void) {
    TEST("get_metrics returns valid JSON");
    reset_state();
    impl_record_event("test_event", NULL);
    char *json = impl_get_metrics(NULL);
    ASSERT(json != NULL, "should return non-NULL");
    ASSERT(strstr(json, "metric_count") != NULL, "should contain metric_count");
    ASSERT(strstr(json, "test_event") != NULL, "should contain test_event");
    ASSERT(strstr(json, "recent_events") != NULL, "should contain recent_events");
    free(json);
    PASS();
}

/* Test event ring buffer wrapping */
static void test_ring_buffer_wrap(void) {
    TEST("ring buffer wraps correctly");
    reset_state();
    for (int i = 0; i < EVENT_RING_SIZE * 2; i++) {
        char name[32];
        snprintf(name, sizeof(name), "event_%d", i);
        impl_record_event(name, NULL);
    }
    ASSERT(g_event_count == EVENT_RING_SIZE, "event count should be ring size");
    ASSERT(g_event_wraps > 0, "should have wrapped");
    /* Most recent event should be at head-1 */
    int last_idx = (g_event_head - 1 + EVENT_RING_SIZE) % EVENT_RING_SIZE;
    ASSERT(strcmp(g_event_ring[last_idx].name, "event_127") == 0,
           "last event should be event_127");
    PASS();
}

/* Test null safety */
static void test_null_safety(void) {
    TEST("record_event handles NULL args");
    reset_state();
    impl_record_event(NULL, NULL);  /* should not crash */
    ASSERT(g_metric_count == 0, "no metric created for NULL name");
    impl_record_event("safe", NULL);
    impl_record_event(NULL, "{\"data\":1}");  /* should not crash */
    ASSERT(g_metric_count == 1, "only one metric created");
    PASS();
}

/* Test get_metrics with no data */
static void test_get_metrics_empty(void) {
    TEST("get_metrics with no data returns empty state");
    reset_state();
    char *json = impl_get_metrics(NULL);
    ASSERT(json != NULL, "should return non-NULL");
    ASSERT(strstr(json, "\"metric_count\":0") != NULL, "should have 0 metrics");
    free(json);
    PASS();
}

/* Test obs_get_metrics interface pointer */
static void test_interface_fn_ptr(void) {
    TEST("obs_get_metrics function pointer works");
    reset_state();
    ASSERT(g_interface.obs_get_metrics != NULL, "obs_get_metrics ptr should be set");
    char *result = g_interface.obs_get_metrics(NULL);
    ASSERT(result != NULL, "should return non-NULL");
    free(result);
    PASS();
}

/* Test obs_record_event interface pointer */
static void test_interface_fn_record(void) {
    TEST("obs_record_event function pointer works");
    reset_state();
    ASSERT(g_interface.obs_record_event != NULL, "obs_record_event ptr should be set");
    g_interface.obs_record_event("iface_test", "{\"via\":\"ptr\"}");
    ASSERT(g_metric_count == 1, "should have 1 metric via interface ptr");
    PASS();
}

/* Test plugin lifecycle sets interface */
static void test_plugin_lifecycle(void) {
    TEST("plugin_init sets interface correctly");
    reset_state();
    memset(&g_interface, 0, sizeof(g_interface));
    int ret = plugin_init();
    ASSERT(ret == 0, "init should return 0");
    ASSERT(g_interface.type == PLUGIN_OBSERVABILITY, "type should be PLUGIN_OBSERVABILITY");
    ASSERT(g_interface.obs_record_event != NULL, "obs_record_event should be set");
    ASSERT(g_interface.obs_get_metrics != NULL, "obs_get_metrics should be set");
    PASS();
}

int main(void) {
    fprintf(stderr, "Observability Plugin Tests\n");
    fprintf(stderr, "==========================\n");

    /* Set state path to tmp */
    snprintf(g_state_path, sizeof(g_state_path),
             "/tmp/test_observability_state.json");

    /* Initialize once for interface tests */
    plugin_init();

    test_record_event();
    test_record_multiple();
    test_record_with_value();
    test_get_metrics_json();
    test_ring_buffer_wrap();
    test_null_safety();
    test_get_metrics_empty();
    test_interface_fn_ptr();
    test_interface_fn_record();
    test_plugin_lifecycle();

    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);

    /* Cleanup */
    unlink("/tmp/test_observability_state.json");

    if (tests_failed > 0) return 1;
    return 0;
}
