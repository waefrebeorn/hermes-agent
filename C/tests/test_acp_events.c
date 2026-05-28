/*
 * test_acp_events.c — ACP event notifications unit tests.
 *
 * Tests: tool call ID registration/pop, notification builders,
 * plan update builder, edge cases.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_acp_events.c src/acp/events.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_acp_events -lm
 *
 * Run:
 *   /tmp/hermes_test_acp_events
 */

#include "acp/events.h"
#include "acp/server.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * Stubs — replace server.c dependencies
 * ================================================================ */

static json_node_t *g_last_written = NULL;

bool acp_write_message(json_node_t *msg) {
    if (g_last_written) json_free(g_last_written);
    g_last_written = msg ? json_copy(msg) : NULL;
    return true;
}

typedef struct {
    char id[64];
} acp_session_t;

/* ================================================================
 * 1. Tool Call ID Tracking
 * ================================================================ */

static void test_register_null_safety(void) {
    TEST("register NULL session");
    assert(acp_tool_call_id_register(NULL, "test_tool") == NULL);
    PASS();

    TEST("register NULL tool");
    assert(acp_tool_call_id_register("sess1", NULL) == NULL);
    PASS();

    TEST("register both NULL");
    assert(acp_tool_call_id_register(NULL, NULL) == NULL);
    PASS();
}

static void test_pop_null_safety(void) {
    /* Clear state by popping all */
    char *id;
    while ((id = acp_tool_call_id_pop("sess1", "test_tool")) != NULL)
        free(id);

    TEST("pop NULL session");
    assert(acp_tool_call_id_pop(NULL, "test_tool") == NULL);
    PASS();

    TEST("pop NULL tool");
    assert(acp_tool_call_id_pop("sess1", NULL) == NULL);
    PASS();

    TEST("pop non-existent pair");
    assert(acp_tool_call_id_pop("no_such_session", "no_such_tool") == NULL);
    PASS();
}

static void test_register_pop_roundtrip(void) {
    TEST("register then pop");
    const char *id1 = acp_tool_call_id_register("sess1", "search");
    assert(id1 != NULL);
    assert(strlen(id1) > 0);
    /* Verify format: tc-<tool>-<counter>-<timestamp> */
    assert(strstr(id1, "tc-search-") != NULL);

    char *popped = acp_tool_call_id_pop("sess1", "search");
    assert(popped != NULL);
    assert(strcmp(popped, id1) == 0);
    free(popped);
    PASS();

    TEST("pop twice (second fails)");
    char *dup = acp_tool_call_id_pop("sess1", "search");
    assert(dup == NULL);
    PASS();
}

static void test_fifo_ordering(void) {
    TEST("FIFO ordering across 3 calls");
    /* Register 3 tool calls for same session+tools */
    const char *id1 = acp_tool_call_id_register("sess2", "read");
    const char *id2 = acp_tool_call_id_register("sess2", "read");
    const char *id3 = acp_tool_call_id_register("sess2", "read");
    assert(id1 && id2 && id3);

    /* Pop should return oldest first */
    char *p1 = acp_tool_call_id_pop("sess2", "read");
    char *p2 = acp_tool_call_id_pop("sess2", "read");
    char *p3 = acp_tool_call_id_pop("sess2", "read");
    assert(p1 && p2 && p3);

    /* IDs should be in order */
    assert(strcmp(p1, id1) == 0);
    assert(strcmp(p2, id2) == 0);
    assert(strcmp(p3, id3) == 0);

    free(p1); free(p2); free(p3);
    PASS();
}

static void test_session_isolation(void) {
    TEST("isolated sessions don't interfere");
    const char *id_s1 = acp_tool_call_id_register("sess_a", "tool_x");
    const char *id_s2 = acp_tool_call_id_register("sess_b", "tool_y");
    assert(id_s1 && id_s2);

    /* Pop from session B gets B's ID, not A's */
    char *popped_b = acp_tool_call_id_pop("sess_b", "tool_y");
    assert(popped_b != NULL);
    assert(strcmp(popped_b, id_s2) == 0);
    free(popped_b);

    /* Session A still has its entry */
    char *popped_a = acp_tool_call_id_pop("sess_a", "tool_x");
    assert(popped_a != NULL);
    assert(strcmp(popped_a, id_s1) == 0);
    free(popped_a);
    PASS();
}

/* ================================================================
 * 2. Notification Builder — Tool Start
 * ================================================================ */

static void test_build_tool_start(void) {
    json_node_t *n = acp_build_tool_start_notification(
        "sess-test", "tc-123", "search", "{\"q\":\"hello\"}");
    assert(n != NULL);

    TEST("tool start: jsonrpc field");
    const char *rpc = json_object_get_string(n, "jsonrpc", "");
    assert(strcmp(rpc, "2.0") == 0);
    PASS();

    TEST("tool start: method field");
    const char *method = json_object_get_string(n, "method", "");
    assert(strcmp(method, "session_update") == 0);
    PASS();

    TEST("tool start: params.session_id");
    json_node_t *params = json_object_get(n, "params");
    assert(params != NULL);
    const char *sid = json_object_get_string(params, "session_id", "");
    assert(strcmp(sid, "sess-test") == 0);
    PASS();

    TEST("tool start: params.type");
    const char *type = json_object_get_string(params, "type", "");
    assert(strcmp(type, "tool_call_start") == 0);
    PASS();

    TEST("tool start: tool_call.id");
    json_node_t *tc = json_object_get(params, "tool_call");
    assert(tc != NULL);
    const char *tid = json_object_get_string(tc, "id", "");
    assert(strcmp(tid, "tc-123") == 0);
    PASS();

    TEST("tool start: tool_call.name");
    const char *tname = json_object_get_string(tc, "name", "");
    assert(strcmp(tname, "search") == 0);
    PASS();

    TEST("tool start: tool_call.args (parsed as JSON object)");
    json_node_t *args = json_object_get(tc, "args");
    assert(args != NULL);
    assert(args->type == JSON_OBJECT);
    const char *q = json_object_get_string(args, "q", "");
    assert(strcmp(q, "hello") == 0);
    PASS();

    json_free(n);
}

static void test_build_tool_start_null_args(void) {
    TEST("tool start: NULL args");
    json_node_t *n = acp_build_tool_start_notification(
        "sess", "tc-1", "tool", NULL);
    assert(n != NULL);
    json_node_t *params = json_object_get(n, "params");
    json_node_t *tc = json_object_get(params, "tool_call");
    json_node_t *args = json_object_get(tc, "args");
    assert(args == NULL);
    PASS();
    json_free(n);
}

static void test_build_tool_start_empty_args(void) {
    TEST("tool start: empty args string");
    json_node_t *n = acp_build_tool_start_notification(
        "sess", "tc-2", "tool", "");
    assert(n != NULL);
    json_node_t *params = json_object_get(n, "params");
    json_node_t *tc = json_object_get(params, "tool_call");
    json_node_t *args = json_object_get(tc, "args");
    assert(args == NULL);
    PASS();
    json_free(n);
}

/* ================================================================
 * 3. Notification Builder — Tool Complete/Failed
 * ================================================================ */

static void test_build_tool_complete(void) {
    json_node_t *n = acp_build_tool_complete_notification(
        "sess-test", "tc-456", "read", "{\"lines\":42}", false);
    assert(n != NULL);

    TEST("tool complete: method");
    const char *method = json_object_get_string(n, "method", "");
    assert(strcmp(method, "session_update") == 0);
    PASS();

    TEST("tool complete: type = tool_call_complete");
    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    assert(strcmp(type, "tool_call_complete") == 0);
    PASS();

    TEST("tool complete: result parsed as JSON");
    json_node_t *tc = json_object_get(params, "tool_call");
    json_node_t *result = json_object_get(tc, "result");
    assert(result != NULL);
    assert(result->type == JSON_OBJECT);
    PASS();

    TEST("tool complete: result.lines = 42 (number)");
    double lines_v = json_object_get_number(result, "lines", -1.0);
    assert(lines_v == 42.0);
    PASS();

    json_free(n);
}

static void test_build_tool_failed(void) {
    json_node_t *n = acp_build_tool_complete_notification(
        "sess-fail", "tc-789", "write", "{\"error\":\"permission denied\"}", true);
    assert(n != NULL);

    TEST("tool failed: type = tool_call_failed");
    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    assert(strcmp(type, "tool_call_failed") == 0);
    PASS();

    json_free(n);
}

static void test_build_tool_complete_null_result(void) {
    TEST("tool complete: NULL result");
    json_node_t *n = acp_build_tool_complete_notification(
        "sess", "tc-101", "tool", NULL, false);
    assert(n != NULL);
    json_node_t *params = json_object_get(n, "params");
    json_node_t *tc = json_object_get(params, "tool_call");
    json_node_t *result = json_object_get(tc, "result");
    assert(result == NULL);
    PASS();
    json_free(n);
}

/* ================================================================
 * 4. Notification Builder — Plan Update
 * ================================================================ */

static void test_build_plan_update(void) {
    const char *todo_json = "{"
        "\"todos\":["
          "{\"id\":\"T1\",\"content\":\"Task One\",\"status\":\"in_progress\"},"
          "{\"id\":\"T2\",\"content\":\"Task Two\",\"status\":\"completed\"},"
          "{\"id\":\"T3\",\"content\":\"Task Three\",\"status\":\"pending\"}"
        "]}";

    json_node_t *n = acp_build_plan_update_notification("sess-plan", todo_json);
    assert(n != NULL);

    TEST("plan update: method");
    const char *method = json_object_get_string(n, "method", "");
    assert(strcmp(method, "session_update") == 0);
    PASS();

    TEST("plan update: type");
    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    assert(strcmp(type, "plan_update") == 0);
    PASS();

    TEST("plan update: 3 entries");
    json_node_t *entries = json_object_get(params, "entries");
    assert(entries != NULL);
    assert(entries->type == JSON_ARRAY);
    assert(json_len(entries) == 3);
    PASS();

    /* Verify status mappings */
    TEST("plan update: entry 0 = in_progress");
    json_node_t *e0 = json_array_get(entries, 0);
    assert(strcmp(json_object_get_string(e0, "status", ""), "in_progress") == 0);
    assert(strcmp(json_object_get_string(e0, "content", ""), "Task One") == 0);
    PASS();

    TEST("plan update: entry 1 = completed (mapped from completed)");
    json_node_t *e1 = json_array_get(entries, 1);
    assert(strcmp(json_object_get_string(e1, "status", ""), "completed") == 0);
    PASS();

    TEST("plan update: entry 2 = pending");
    json_node_t *e2 = json_array_get(entries, 2);
    assert(strcmp(json_object_get_string(e2, "status", ""), "pending") == 0);
    PASS();

    json_free(n);
}

static void test_build_plan_update_cancelled(void) {
    /* Cancelled status should map to "completed" */
    const char *todo_json = "{"
        "\"todos\":["
          "{\"id\":\"T4\",\"content\":\"Cancelled task\",\"status\":\"cancelled\"}"
        "]}";

    json_node_t *n = acp_build_plan_update_notification("sess-cancelled", todo_json);
    assert(n != NULL);
    json_node_t *params = json_object_get(n, "params");
    json_node_t *entries = json_object_get(params, "entries");
    assert(json_len(entries) == 1);

    TEST("plan update: cancelled → completed");
    json_node_t *e0 = json_array_get(entries, 0);
    assert(strcmp(json_object_get_string(e0, "status", ""), "completed") == 0);
    PASS();

    json_free(n);
}

static void test_build_plan_update_null_session(void) {
    TEST("plan update: NULL session returns NULL");
    json_node_t *n = acp_build_plan_update_notification(NULL, "{}");
    assert(n == NULL);
    PASS();
}

static void test_build_plan_update_null_result(void) {
    TEST("plan update: NULL result returns NULL");
    json_node_t *n = acp_build_plan_update_notification("sess", NULL);
    assert(n == NULL);
    PASS();
}

static void test_build_plan_update_empty_result(void) {
    TEST("plan update: empty result returns NULL");
    json_node_t *n = acp_build_plan_update_notification("sess", "");
    assert(n == NULL);
    PASS();
}

static void test_build_plan_update_no_todos(void) {
    TEST("plan update: no todos key returns NULL");
    json_node_t *n = acp_build_plan_update_notification("sess", "{\"foo\":\"bar\"}");
    assert(n == NULL);
    PASS();
}

static void test_build_plan_update_non_array_todos(void) {
    TEST("plan update: non-array todos returns NULL");
    json_node_t *n = acp_build_plan_update_notification("sess", "{\"todos\":\"not_array\"}");
    assert(n == NULL);
    PASS();
}

/* ================================================================
 * 5. Main callback (with stubs)
 * ================================================================ */

static void test_event_cb_tool_started(void) {
    acp_session_t sess;
    strncpy(sess.id, "sess-cb", sizeof(sess.id) - 1);
    sess.id[sizeof(sess.id) - 1] = '\0';

    /* Manually register an ID so pop works on complete */
    const char *tc_id = acp_tool_call_id_register("sess-cb", "tool_x");
    assert(tc_id != NULL);

    TEST("event cb: tool.started calls acp_write_message");
    int ret = acp_tool_event_cb("tool.started", "tool_x", "{\"key\":\"val\"}", &sess);
    assert(ret == 1);
    assert(g_last_written != NULL);

    /* Verify it's a valid notification */
    const char *method = json_object_get_string(g_last_written, "method", "");
    assert(strcmp(method, "session_update") == 0);
    json_node_t *params = json_object_get(g_last_written, "params");
    const char *type = json_object_get_string(params, "type", "");
    assert(strcmp(type, "tool_call_start") == 0);
    PASS();

    json_free(g_last_written);
    g_last_written = NULL;
}

static void test_event_cb_null_args(void) {
    acp_session_t sess;
    strncpy(sess.id, "sess-cb2", sizeof(sess.id) - 1);

    TEST("event cb: NULL event_type returns 1");
    int ret = acp_tool_event_cb(NULL, "tool", NULL, &sess);
    assert(ret == 1);
    PASS();

    TEST("event cb: NULL tool_name returns 1");
    ret = acp_tool_event_cb("tool.started", NULL, NULL, &sess);
    assert(ret == 1);
    PASS();

    TEST("event cb: NULL userdata returns 1");
    ret = acp_tool_event_cb("tool.started", "tool", NULL, NULL);
    assert(ret == 1);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== ACP Events Tests ===\n\n");

    /* 1. Tool Call ID Tracking */
    printf("--- Tool Call ID Tracking ---\n");
    test_register_null_safety();
    test_pop_null_safety();
    test_register_pop_roundtrip();
    test_fifo_ordering();
    test_session_isolation();

    /* 2. Tool Start Builder */
    printf("\n--- Tool Start Notification Builder ---\n");
    test_build_tool_start();
    test_build_tool_start_null_args();
    test_build_tool_start_empty_args();

    /* 3. Tool Complete/Failed Builder */
    printf("\n--- Tool Complete/Failed Builder ---\n");
    test_build_tool_complete();
    test_build_tool_failed();
    test_build_tool_complete_null_result();

    /* 4. Plan Update Builder */
    printf("\n--- Plan Update Builder ---\n");
    test_build_plan_update();
    test_build_plan_update_cancelled();
    test_build_plan_update_null_session();
    test_build_plan_update_null_result();
    test_build_plan_update_empty_result();
    test_build_plan_update_no_todos();
    test_build_plan_update_non_array_todos();

    /* 5. Main Callback */
    printf("\n--- Event Callback ---\n");
    test_event_cb_tool_started();
    test_event_cb_null_args();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
