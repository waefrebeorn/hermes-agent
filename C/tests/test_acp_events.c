/*
 * test_acp_events.c — Tests for ACP event notification bridge.
 *
 * Tests: tool_call_id_register, tool_call_id_pop, notification builders.
 * Uses -Wl,--unresolved-symbols=ignore-all for acp_write_message.
 */
#include "acp/events.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

/* == Tool Call ID Tests == */

static void test_id_register(void) {
    const char *id1 = acp_tool_call_id_register("sess-1", "read_file");
    TEST("register returns non-NULL for valid input", id1 != NULL);
    TEST("register returns non-empty ID", id1 && strlen(id1) > 0);
    TEST("register starts with tc-read_file-", id1 && strncmp(id1, "tc-read_file-", 13) == 0);

    const char *id2 = acp_tool_call_id_register("sess-1", "read_file");
    TEST("second register returns different ID", id1 && id2 && strcmp(id1, id2) != 0);

    const char *id3 = acp_tool_call_id_register("sess-1", "write_file");
    TEST("register different tool returns distinct ID", id2 && id3 && strcmp(id2, id3) != 0);

    const char *id_null = acp_tool_call_id_register(NULL, "tool");
    TEST("register with NULL session returns NULL", id_null == NULL);

    const char *id_null2 = acp_tool_call_id_register("sess", NULL);
    TEST("register with NULL tool returns NULL", id_null2 == NULL);
}

static void test_id_pop(void) {
    const char *id1 = acp_tool_call_id_register("pop-sess", "compute");
    TEST("pop prereg: register returns non-NULL", id1 != NULL);

    char *popped = acp_tool_call_id_pop("pop-sess", "compute");
    TEST("pop returns non-NULL for registered ID", popped != NULL);
    TEST_STR_EQ("pop returns same ID as registered", id1, popped);
    free(popped);

    char *pop2 = acp_tool_call_id_pop("pop-sess", "compute");
    TEST("pop returns NULL for already-popped ID", pop2 == NULL);

    char *pop3 = acp_tool_call_id_pop("nonexistent", "compute");
    TEST("pop returns NULL for wrong session", pop3 == NULL);

    char *pop4 = acp_tool_call_id_pop("pop-sess", "wrong_tool");
    TEST("pop returns NULL for wrong tool", pop4 == NULL);
}

static void test_id_fifo_order(void) {
    const char *id1 = acp_tool_call_id_register("fifo", "task");
    const char *id2 = acp_tool_call_id_register("fifo", "task");
    TEST("fifo: both registers succeed", id1 && id2);

    char *first = acp_tool_call_id_pop("fifo", "task");
    char *second = acp_tool_call_id_pop("fifo", "task");
    TEST("fifo: first pop returns non-NULL", first != NULL);
    TEST("fifo: second pop returns non-NULL", second != NULL);
    TEST("fifo: first == oldest (id1)", first && id1 && strcmp(first, id1) == 0);
    TEST("fifo: second == newest (id2)", second && id2 && strcmp(second, id2) == 0);
    free(first);
    free(second);
}

/* == Notification Builder Tests == */

static void test_build_tool_start(void) {
    json_node_t *n = acp_build_tool_start_notification("sess-1", "tc-1", "read_file", "{\"path\":\"/tmp\"}");
    TEST("start notif non-NULL", n != NULL);

    const char *method = json_object_get_string(n, "method", "");
    TEST_STR_EQ("start notif method is session_update", method, "session_update");

    json_node_t *params = json_object_get(n, "params");
    TEST("start notif has params", params != NULL);
    const char *type = json_object_get_string(params, "type", "");
    TEST_STR_EQ("start type is tool_call_start", type, "tool_call_start");

    const char *sid = json_object_get_string(params, "session_id", "");
    TEST_STR_EQ("start session_id matches", sid, "sess-1");

    json_node_t *tc = json_object_get(params, "tool_call");
    TEST("start has tool_call object", tc != NULL);
    const char *tc_id = json_object_get_string(tc, "id", "");
    TEST_STR_EQ("tool_call.id matches", tc_id, "tc-1");
    const char *tc_name = json_object_get_string(tc, "name", "");
    TEST_STR_EQ("tool_call.name matches", tc_name, "read_file");
    json_node_t *args = json_object_get(tc, "args");
    TEST("tool_call has args", args != NULL);
    const char *path = json_object_get_string(args, "path", "");
    TEST_STR_EQ("args.path is /tmp", path, "/tmp");

    json_free(n);
}

static void test_build_tool_complete(void) {
    json_node_t *n = acp_build_tool_complete_notification(
        "sess-1", "tc-2", "write_file", "{\"success\":true}", false);
    TEST("complete notif non-NULL", n != NULL);

    const char *method = json_object_get_string(n, "method", "");
    TEST_STR_EQ("complete method", method, "session_update");

    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    TEST_STR_EQ("complete type is tool_call_complete", type, "tool_call_complete");

    json_node_t *tc = json_object_get(params, "tool_call");
    const char *tc_id = json_object_get_string(tc, "id", "");
    TEST_STR_EQ("complete tool_call.id", tc_id, "tc-2");

    /* Result is parsed as JSON object, check its field */
    json_node_t *result = json_object_get(tc, "result");
    TEST("complete has result object", result != NULL);
    bool success_val = json_object_get_bool(result, "success", false);
    TEST("complete result.success is true", success_val);

    json_free(n);
}

static void test_build_tool_failed(void) {
    json_node_t *n = acp_build_tool_complete_notification(
        "sess-1", "tc-3", "read_file", "error: not found", true);
    TEST("failed notif non-NULL", n != NULL);

    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    TEST_STR_EQ("failed type is tool_call_failed", type, "tool_call_failed");

    json_node_t *tc = json_object_get(params, "tool_call");
    const char *result = json_object_get_string(tc, "result", "");
    TEST_STR_EQ("failed result is raw string", result, "error: not found");

    json_free(n);
}

static void test_build_plan_update(void) {
    json_node_t *n = acp_build_plan_update_notification("sess-1",
        "{\"todos\":[{\"id\":\"t1\",\"content\":\"Task 1\",\"status\":\"pending\"},"
        "{\"id\":\"t2\",\"content\":\"Task 2\",\"status\":\"completed\"}]}");
    TEST("plan update non-NULL for valid input", n != NULL);

    json_node_t *params = json_object_get(n, "params");
    const char *type = json_object_get_string(params, "type", "");
    TEST_STR_EQ("plan type is plan_update", type, "plan_update");

    json_node_t *entries = json_object_get(params, "entries");
    TEST("plan has entries array", entries != NULL);
    TEST("entries length is 2", json_len(entries) == 2);

    json_node_t *e1 = json_array_get(entries, 0);
    const char *c1 = json_object_get_string(e1, "content", "");
    TEST_STR_EQ("entry1 content", c1, "Task 1");
    const char *s1 = json_object_get_string(e1, "status", "");
    TEST_STR_EQ("entry1 status mapped to pending", s1, "pending");

    json_node_t *e2 = json_array_get(entries, 1);
    const char *c2 = json_object_get_string(e2, "content", "");
    TEST_STR_EQ("entry2 content", c2, "Task 2");
    const char *s2 = json_object_get_string(e2, "status", "");
    TEST_STR_EQ("entry2 status mapped to completed", s2, "completed");

    json_free(n);
}

static void test_build_plan_update_in_progress(void) {
    json_node_t *n = acp_build_plan_update_notification("sess-1",
        "{\"todos\":[{\"id\":\"t3\",\"content\":\"In progress\",\"status\":\"in_progress\"}]}");
    TEST("plan update in_progress non-NULL", n != NULL);

    json_node_t *params = json_object_get(n, "params");
    json_node_t *entries = json_object_get(params, "entries");
    json_node_t *e1 = json_array_get(entries, 0);
    const char *s1 = json_object_get_string(e1, "status", "");
    TEST_STR_EQ("in_progress status preserved", s1, "in_progress");

    json_free(n);
}

static void test_build_plan_update_cancelled(void) {
    json_node_t *n = acp_build_plan_update_notification("sess-1",
        "{\"todos\":[{\"id\":\"t4\",\"content\":\"Cancelled\",\"status\":\"cancelled\"}]}");
    TEST("plan update cancelled non-NULL", n != NULL);

    json_node_t *params = json_object_get(n, "params");
    json_node_t *entries = json_object_get(params, "entries");
    json_node_t *e1 = json_array_get(entries, 0);
    const char *s1 = json_object_get_string(e1, "status", "");
    TEST_STR_EQ("cancelled mapped to completed", s1, "completed");

    json_free(n);
}

static void test_build_plan_update_empty(void) {
    json_node_t *n = acp_build_plan_update_notification("sess-1",
        "{\"todos\":[]}");
    TEST("plan update empty todos non-NULL", n != NULL);
    json_node_t *params = json_object_get(n, "params");
    json_node_t *entries = json_object_get(params, "entries");
    TEST("plan update empty entries array", entries != NULL);
    TEST("entries length is 0", json_len(entries) == 0);
    json_free(n);
}

static void test_build_plan_update_invalid(void) {
    json_node_t *n1 = acp_build_plan_update_notification(NULL, "{\"todos\":[]}");
    TEST("plan update NULL session returns NULL", n1 == NULL);

    json_node_t *n2 = acp_build_plan_update_notification("sess", NULL);
    TEST("plan update NULL result returns NULL", n2 == NULL);

    json_node_t *n3 = acp_build_plan_update_notification("sess", "");
    TEST("plan update empty result returns NULL", n3 == NULL);

    json_node_t *n4 = acp_build_plan_update_notification("sess", "not json");
    TEST("plan update invalid JSON returns NULL", n4 == NULL);

    json_node_t *n5 = acp_build_plan_update_notification("sess", "{\"key\":\"value\"}");
    TEST("plan update no todos returns NULL", n5 == NULL);
}

int main(void) {
    printf("=== ACP Events Tests ===\n");

    printf("--- Tool Call ID Registration ---\n");
    test_id_register();
    test_id_pop();
    test_id_fifo_order();

    printf("--- Notification Builders ---\n");
    test_build_tool_start();
    test_build_tool_complete();
    test_build_tool_failed();
    test_build_plan_update();
    test_build_plan_update_in_progress();
    test_build_plan_update_cancelled();
    test_build_plan_update_empty();
    test_build_plan_update_invalid();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
