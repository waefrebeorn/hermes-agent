/*
 * test_acp_events.c — Tests for ACP event notification bridge (events.c).
 *
 * Tests: tool call ID tracking (register, pop), notification builders
 * (tool start, tool complete, plan update), and edge cases.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I../include -I../lib/libjson -I../lib/libplugin \
 *       tests/test_acp_events.c src/acp/events.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_acp_events -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 */
#include "acp/events.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int passed = 0, failed = 0;
static int test_count = 0;

#define TEST(name, expr) do { \
    test_count++; \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* Helper: serialize a JSON node to a malloc'd string */
static char *json_str(json_node_t *node) {
    if (!node) return strdup("(null)");
    return json_serialize(node);
}

/* Helper: check JSON string contains substring */
static int json_contains(json_node_t *node, const char *needle) {
    if (!node || !needle) return 0;
    char *s = json_str(node);
    int ret = (strstr(s, needle) != NULL) ? 1 : 0;
    free(s);
    return ret;
}

/* ================================================================
 *  Tests
 * ================================================================ */

static void test_tool_call_id_register(void) {
    printf("\n--- Tool call ID registration ---\n");

    /* NULL safety */
    TEST_NULL("register(NULL, NULL) returns NULL",
        acp_tool_call_id_register(NULL, NULL));
    TEST_NULL("register('s', NULL) returns NULL",
        acp_tool_call_id_register("session1", NULL));
    TEST_NULL("register(NULL, 't') returns NULL",
        acp_tool_call_id_register(NULL, "tool1"));

    /* Successful registration */
    const char *id1 = acp_tool_call_id_register("session1", "web_search");
    TEST("register returns non-NULL", id1 != NULL);
    if (id1) {
        TEST("call ID starts with tc-", strncmp(id1, "tc-", 3) == 0);
        TEST("call ID contains tool name", strstr(id1, "web_search") != NULL);
    }

    /* Second registration with same session/tool */
    const char *id2 = acp_tool_call_id_register("session1", "web_search");
    TEST("second register returns different ID", id2 != NULL && id2 != id1);
    if (id2) {
        TEST("second ID also starts with tc-", strncmp(id2, "tc-", 3) == 0);
    }
}

static void test_tool_call_id_pop(void) {
    printf("\n--- Tool call ID pop ---\n");

    /* NULL safety */
    TEST_NULL("pop(NULL, NULL) returns NULL",
        acp_tool_call_id_pop(NULL, NULL));
    TEST_NULL("pop('s', NULL) returns NULL",
        acp_tool_call_id_pop("session1", NULL));
    TEST_NULL("pop(NULL, 't') returns NULL",
        acp_tool_call_id_pop(NULL, "tool1"));

    /* Pop unknown session/tool */
    TEST_NULL("pop unknown session", acp_tool_call_id_pop("nosession", "web_search"));
    TEST_NULL("pop unknown tool", acp_tool_call_id_pop("session1", "notool"));

    /* Register and pop round-trip */
    const char *id_reg = acp_tool_call_id_register("session_pop", "terminal");
    TEST("register for pop test", id_reg != NULL);

    char *id_pop = acp_tool_call_id_pop("session_pop", "terminal");
    TEST("pop returns non-NULL", id_pop != NULL);
    if (id_reg && id_pop) {
        TEST_STR_EQ("popped ID matches registered ID", id_pop, id_reg);
    }
    free(id_pop);

    /* Pop twice — second should be NULL */
    TEST_NULL("pop again returns NULL",
        acp_tool_call_id_pop("session_pop", "terminal"));

    /* FIFO order: register 3, pop in order */
    const char *a = acp_tool_call_id_register("fifo_sess", "tool_a");
    const char *b = acp_tool_call_id_register("fifo_sess", "tool_a");
    const char *c = acp_tool_call_id_register("fifo_sess", "tool_a");
    TEST("FIFO register A", a != NULL);
    TEST("FIFO register B", b != NULL);
    TEST("FIFO register C", c != NULL);

    if (a && b && c) {
        char *pa = acp_tool_call_id_pop("fifo_sess", "tool_a");
        TEST("FIFO pop A non-NULL", pa != NULL);
        TEST_STR_EQ("FIFO pop A matches register A", pa, a);
        free(pa);

        char *pb = acp_tool_call_id_pop("fifo_sess", "tool_a");
        TEST("FIFO pop B non-NULL", pb != NULL);
        TEST_STR_EQ("FIFO pop B matches register B", pb, b);
        free(pb);

        char *pc = acp_tool_call_id_pop("fifo_sess", "tool_a");
        TEST("FIFO pop C non-NULL", pc != NULL);
        TEST_STR_EQ("FIFO pop C matches register C", pc, c);
        free(pc);
    }

    /* Multiple session isolation */
    const char *s1 = acp_tool_call_id_register("session_X", "tool_x");
    const char *s2 = acp_tool_call_id_register("session_Y", "tool_y");
    TEST("session X register", s1 != NULL);
    TEST("session Y register", s2 != NULL);

    if (s1 && s2) {
        char *pop_y = acp_tool_call_id_pop("session_Y", "tool_y");
        TEST_STR_EQ("session Y pop matches Y's ID", pop_y, s2);
        free(pop_y);

        /* Pop X from different session should not match Y */
        char *pop_y_again = acp_tool_call_id_pop("session_Y", "tool_y");
        TEST_NULL("session Y pop again null", pop_y_again);

        char *pop_x = acp_tool_call_id_pop("session_X", "tool_x");
        TEST_STR_EQ("session X pop matches X's ID", pop_x, s1);
        free(pop_x);
    }
}

static void test_build_tool_start_notification(void) {
    printf("\n--- Tool start notification builder ---\n");

    /* Normal case */
    json_node_t *n1 = acp_build_tool_start_notification(
        "sess-1", "tc-1", "web_search", "{\"query\":\"hello\"}");
    TEST("start notif non-NULL", n1 != NULL);
    if (n1) {
        TEST("jsonrpc 2.0", json_contains(n1, "\"jsonrpc\":\"2.0\""));
        TEST("method session_update", json_contains(n1, "\"method\":\"session_update\""));
        TEST("params session_id", json_contains(n1, "\"session_id\":\"sess-1\""));
        TEST("type tool_call_start", json_contains(n1, "\"tool_call_start\""));
        TEST("tool call id", json_contains(n1, "\"id\":\"tc-1\""));
        TEST("tool name", json_contains(n1, "\"name\":\"web_search\""));
        TEST("args as object", json_contains(n1, "\"args\":{\"query\":\"hello\"}"));
        json_free(n1);
    }

    /* NULL args — should skip args field */
    json_node_t *n2 = acp_build_tool_start_notification(
        "sess-1", "tc-2", "read_file", NULL);
    TEST("NULL args non-NULL", n2 != NULL);
    if (n2) {
        TEST("NULL args omits args field", !json_contains(n2, "\"args\""));
        json_free(n2);
    }

    /* NULL IDs — should default to empty strings */
    json_node_t *n3 = acp_build_tool_start_notification(
        "sess-1", NULL, "terminal", "{}");
    TEST("NULL call_id non-NULL", n3 != NULL);
    if (n3) {
        TEST("NULL call_id uses empty string", json_contains(n3, "\"id\":\"\""));
        json_free(n3);
    }

    /* Unparseable args — should store as string */
    json_node_t *n4 = acp_build_tool_start_notification(
        "sess-1", "tc-4", "echo", "not json at all");
    TEST("unparseable args non-NULL", n4 != NULL);
    if (n4) {
        TEST("unparseable args as string",
             json_contains(n4, "\"args\":\"not json at all\""));
        json_free(n4);
    }

    /* NULL session_id */
    json_node_t *n5 = acp_build_tool_start_notification(
        NULL, "tc-5", "test", "{}");
    TEST("NULL session_id non-NULL", n5 != NULL);
    if (n5) {
        TEST("NULL session_id passes through",
             json_contains(n5, "\"session_id\":\"\""));
        json_free(n5);
    }
}

static void test_build_tool_complete_notification(void) {
    printf("\n--- Tool complete notification builder ---\n");

    /* Success case */
    json_node_t *n1 = acp_build_tool_complete_notification(
        "sess-1", "tc-1", "web_search", "{\"results\":[1,2,3]}", false);
    TEST("success notif non-NULL", n1 != NULL);
    if (n1) {
        TEST("success: type tool_call_complete",
             json_contains(n1, "\"tool_call_complete\""));
        TEST("success: result as object",
             json_contains(n1, "\"result\":{\"results\":[1,2,3]}"));
        json_free(n1);
    }

    /* Failure case */
    json_node_t *n2 = acp_build_tool_complete_notification(
        "sess-1", "tc-2", "terminal", "{\"error\":\"permission denied\"}", true);
    TEST("failure notif non-NULL", n2 != NULL);
    if (n2) {
        TEST("failure: type tool_call_failed",
             json_contains(n2, "\"tool_call_failed\""));
        json_free(n2);
    }

    /* NULL result — skip result field */
    json_node_t *n3 = acp_build_tool_complete_notification(
        "sess-1", "tc-3", "echo", NULL, false);
    TEST("NULL result non-NULL", n3 != NULL);
    if (n3) {
        TEST("NULL result omits result field",
             !json_contains(n3, "\"result\""));
        json_free(n3);
    }

    /* Unparseable result as string */
    json_node_t *n4 = acp_build_tool_complete_notification(
        "sess-1", "tc-4", "echo", "plain text output", false);
    TEST("unparseable result non-NULL", n4 != NULL);
    if (n4) {
        TEST("unparseable result as string",
             json_contains(n4, "\"result\":\"plain text output\""));
        json_free(n4);
    }
}

static void test_build_plan_update_notification(void) {
    printf("\n--- Plan update notification builder ---\n");

    /* NULL/empty input */
    TEST_NULL("plan_update(NULL, NULL) returns NULL",
        acp_build_plan_update_notification(NULL, NULL));
    TEST_NULL("plan_update('s', NULL) returns NULL",
        acp_build_plan_update_notification("sess-1", NULL));
    TEST_NULL("plan_update(NULL, '') returns NULL",
        acp_build_plan_update_notification(NULL, ""));

    /* Valid todo result with todos array */
    json_node_t *n1 = acp_build_plan_update_notification(
        "sess-1",
        "{\"todos\":["
          "{\"id\":\"t1\",\"content\":\"Task one\",\"status\":\"in_progress\"},"
          "{\"id\":\"t2\",\"content\":\"Task two\",\"status\":\"completed\"},"
          "{\"id\":\"t3\",\"content\":\"Task three\",\"status\":\"pending\"}"
        "]}");
    TEST("valid todo non-NULL", n1 != NULL);
    if (n1) {
        TEST("plan: jsonrpc 2.0", json_contains(n1, "\"jsonrpc\":\"2.0\""));
        TEST("plan: type plan_update", json_contains(n1, "\"plan_update\""));
        TEST("plan: task one in_progress",
             json_contains(n1, "\"content\":\"Task one\"") &&
             json_contains(n1, "\"in_progress\""));
        TEST("plan: task two completed",
             json_contains(n1, "\"content\":\"Task two\"") &&
             json_contains(n1, "\"completed\""));
        TEST("plan: task three pending",
             json_contains(n1, "\"content\":\"Task three\"") &&
             json_contains(n1, "\"pending\""));
        json_free(n1);
    }

    /* Missing todos field */
    TEST_NULL("no todos field returns NULL",
        acp_build_plan_update_notification("sess-1", "{\"no_todos\":[]}"));

    /* Empty todos array */
    json_node_t *n3 = acp_build_plan_update_notification(
        "sess-1", "{\"todos\":[]}");
    TEST("empty todos returns non-NULL", n3 != NULL);
    if (n3) {
        TEST("empty todos has entries",
             json_contains(n3, "\"entries\":[]"));
        json_free(n3);
    }

    /* Malformed JSON */
    json_node_t *n4 = acp_build_plan_update_notification(
        "sess-1", "{bad json}");
    TEST("malformed JSON returns NULL", n4 == NULL);

    /* Non-array todos */
    json_node_t *n5 = acp_build_plan_update_notification(
        "sess-1", "{\"todos\":\"not_an_array\"}");
    TEST("non-array todos returns NULL", n5 == NULL);

    /* Default status mapping: cancelled → completed */
    json_node_t *n6 = acp_build_plan_update_notification(
        "sess-1",
        "{\"todos\":["
          "{\"id\":\"t4\",\"content\":\"Cancelled\",\"status\":\"cancelled\"},"
          "{\"id\":\"t5\",\"content\":\"No status\"}"
        "]}");
    TEST("cancelled status non-NULL", n6 != NULL);
    if (n6) {
        TEST("cancelled maps to completed",
             json_contains(n6, "\"content\":\"Cancelled\"") &&
             json_contains(n6, "\"completed\""));
        TEST("no status defaults to pending",
             json_contains(n6, "\"content\":\"No status\"") &&
             json_contains(n6, "\"pending\""));
        json_free(n6);
    }

    /* Item with no content and no id should be skipped */
    json_node_t *n7 = acp_build_plan_update_notification(
        "sess-1",
        "{\"todos\":["
          "{\"content\":\"Valid item\",\"status\":\"pending\"},"
          "{\"irrelevant\":\"skipped\"}"
        "]}");
    TEST("skip empty item non-NULL", n7 != NULL);
    if (n7) {
        TEST("valid item present",
             json_contains(n7, "\"content\":\"Valid item\""));
        {
            char *s7 = json_str(n7);
            TEST("skipped item absent",
                 s7 && strstr(s7, "skipped") == NULL);
            free(s7);
        }
        json_free(n7);
    }
}

int main(void) {
    printf("=== ACP Event Notification Test Suite ===\n");

    test_tool_call_id_register();
    test_tool_call_id_pop();
    test_build_tool_start_notification();
    test_build_tool_complete_notification();
    test_build_plan_update_notification();

    printf("\n=== Results: %d passed, %d failed (%d assertions) ===\n",
           passed, failed, test_count);
    return failed > 0 ? 1 : 0;
}
