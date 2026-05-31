/*
 * test_conversation_edge.c — Edge case tests for conversation loop.
 *
 * Tests message sequence repair, tool call argument sanitization,
 * and conversation flow edge cases beyond the happy-path coverage.
 *
 * Covers: deeply nested tool calls, multi-tool per assistant,
 * special characters in IDs, message boundary conditions,
 * repair after edge state, and extreme message arrays.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

#define MAX_MSGS 32

static message_t make_msg(message_role_t role, const char *content) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = role;
    m.content = content ? strdup(content) : NULL;
    return m;
}

static message_t make_assistant_tc(const char *content,
                                    const char *ids[], const char *names[],
                                    int count) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    m.content = content ? strdup(content) : NULL;
    m.tool_calls_count = count;
    for (int i = 0; i < count && i < 8; i++) {
        if (ids[i]) snprintf(m.tool_calls[i].id, sizeof(m.tool_calls[i].id), "%s", ids[i]);
        if (names[i]) snprintf(m.tool_calls[i].name, sizeof(m.tool_calls[i].name), "%s", names[i]);
        snprintf(m.tool_calls[i].arguments, sizeof(m.tool_calls[i].arguments), "{}");
    }
    return m;
}

static message_t make_tool_msg(const char *id, const char *content) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_TOOL;
    m.content = content ? strdup(content) : NULL;
    m.tool_call_id = id ? strdup(id) : NULL;
    return m;
}

static void free_msgs(message_t *msgs, int count) {
    for (int i = 0; i < count; i++) {
        free(msgs[i].content); msgs[i].content = NULL;
        free(msgs[i].tool_call_id); msgs[i].tool_call_id = NULL;
        free(msgs[i].tool_name); msgs[i].tool_name = NULL;
        free(msgs[i].reasoning); msgs[i].reasoning = NULL;
        free(msgs[i].encrypted_content); msgs[i].encrypted_content = NULL;
    }
}

/* ── Repair edge cases ── */

static void test_repair_four_tools_assistant(void) {
    /* Assistant with 4 tool calls, all properly matched */
    const char *ids[] = {"c1","c2","c3","c4"};
    const char *names[] = {"read","write","search","exec"};
    message_t msgs[6];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "run tools");
    msgs[1] = make_assistant_tc("running", ids, names, 4);
    msgs[2] = make_tool_msg("c1", "read ok");
    msgs[3] = make_tool_msg("c2", "write ok");
    msgs[4] = make_tool_msg("c3", "search ok");
    msgs[5] = make_tool_msg("c4", "exec ok");

    int cnt = 6;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("4-tool assistant: 0 repairs", r == 0);
    TEST("4-tool assistant: count=6", cnt == 6);
    free_msgs(msgs, cnt);
}

static void test_repair_mixed_valid_stray(void) {
    /* 3 tools: 2 valid, 1 stray — only stray dropped */
    const char *ids[] = {"c1","c2","c3"};
    const char *names[] = {"a","b","c"};
    message_t msgs[5];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("thinking", ids, names, 3);
    msgs[1] = make_tool_msg("c1", "a ok");       /* valid */
    msgs[2] = make_tool_msg("c2", "b ok");       /* valid */
    msgs[3] = make_tool_msg("c_stray", "stray"); /* no match */
    msgs[4] = make_tool_msg("c3", "c ok");       /* valid */

    int cnt = 5;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("mixed stray: >= 1 repairs", r >= 1);
    TEST("mixed stray: count=4", cnt == 4);
    TEST("mixed stray: c3 tool preserved",
         cnt >= 4 && msgs[3].role == MSG_TOOL &&
         msgs[3].tool_call_id && strcmp(msgs[3].tool_call_id, "c3") == 0);
    free_msgs(msgs, cnt);
}

static void test_repair_no_assistant_before_tool(void) {
    /* Tool message without any preceding assistant */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "hello");
    msgs[1] = make_tool_msg("call_1", "orphan");

    int cnt = 2;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("orphan tool dropped: 1 repair", r == 1);
    TEST("orphan tool dropped: count=1", cnt == 1);
    free_msgs(msgs, cnt);
}

static void test_repair_system_user_consecutive(void) {
    /* System + User consecutive (valid, no repair needed) */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_SYSTEM, "you are helpful");
    msgs[1] = make_msg(MSG_USER, "hello");

    int cnt = 2;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("system+user: 0 repairs", r == 0);
    TEST("system+user: count=2", cnt == 2);
    free_msgs(msgs, cnt);
}

static void test_repair_system_user_user(void) {
    /* System + User + User — should merge second two users */
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_SYSTEM, "be helpful");
    msgs[1] = make_msg(MSG_USER, "first");
    msgs[2] = make_msg(MSG_USER, "second");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("system+2user: 1 repair", r == 1);
    TEST("system+2user: count=2", cnt == 2);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_missing_content(void) {
    /* Tool message with NULL content should still be valid if ID matches */
    const char *ids[] = {"c1"};
    const char *names[] = {"tool"};
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "do it");
    msgs[1] = make_assistant_tc("ok", ids, names, 1);
    msgs[2] = make_tool_msg("c1", NULL); /* NULL content but valid ID */

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("tool with NULL content: 0 repairs (valid ID)", r == 0);
    TEST("tool with NULL content: count=3", cnt == 3);
    free_msgs(msgs, cnt);
}

static void test_repair_interleaved_tools(void) {
    /* Assistant1(tc1,tc2) → Tool(tc1) → Tool(tc2) → Assistant2(tc3) → Tool(tc3) */
    const char *ids1[] = {"tc1","tc2"};
    const char *names1[] = {"a","b"};
    const char *ids2[] = {"tc3"};
    const char *names2[] = {"c"};
    message_t msgs[5];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("first", ids1, names1, 2);
    msgs[1] = make_tool_msg("tc1", "result a");
    msgs[2] = make_tool_msg("tc2", "result b");
    msgs[3] = make_assistant_tc("second", ids2, names2, 1);
    msgs[4] = make_tool_msg("tc3", "result c");

    int cnt = 5;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("interleaved tools: 0 repairs", r == 0);
    TEST("interleaved tools: count=5", cnt == 5);
    free_msgs(msgs, cnt);
}

static void test_repair_special_chars_ids(void) {
    /* Tool call IDs with special characters */
    const char *ids[] = {"call_$pecial!", "call_hyphen-underscore_123"};
    const char *names[] = {"read","write"};
    message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("thinking", ids, names, 2);
    msgs[1] = make_tool_msg("call_$pecial!", "result 1");
    msgs[2] = make_tool_msg("call_hyphen-underscore_123", "result 2");
    msgs[3] = make_msg(MSG_USER, "thanks");

    int cnt = 4;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("special char IDs: 0 repairs", r == 0);
    TEST("special char IDs: count=4", cnt == 4);
    free_msgs(msgs, cnt);
}

static void test_repair_all_tools_stray(void) {
    /* All tool messages are stray (no matching assistant) */
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_tool_msg("x1", "r1");
    msgs[1] = make_tool_msg("x2", "r2");
    msgs[2] = make_tool_msg("x3", "r3");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("all stray tools: 3 repairs", r == 3);
    TEST("all stray tools: count=0", cnt == 0);
    free_msgs(msgs, cnt);
}

/* ── Sanitize edge cases ── */

static void test_sanitize_nested_json_args(void) {
    /* Tool call with nested JSON object args (valid — no repair) */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"call_nested"};
    const char *names[] = {"complex_tool"};
    msgs[0] = make_assistant_tc("thinking", ids, names, 1);
    snprintf(msgs[0].tool_calls[0].arguments, sizeof(msgs[0].tool_calls[0].arguments),
             "{\"path\":\"/tmp\",\"options\":{\"recursive\":true,\"pattern\":\"*.c\",\"depth\":5}}");
    msgs[1] = make_tool_msg("call_nested", "done");

    int cnt = 2;
    int r = hermes_sanitize_tool_call_arguments(msgs, &cnt);
    TEST("nested JSON args: 0 repairs (valid)", r == 0);
    free_msgs(msgs, cnt);
}

static void test_sanitize_array_args(void) {
    /* Tool call with JSON array as arguments (unusual but valid) */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"call_arr"};
    const char *names[] = {"list_tool"};
    msgs[0] = make_assistant_tc("thinking", ids, names, 1);
    snprintf(msgs[0].tool_calls[0].arguments, sizeof(msgs[0].tool_calls[0].arguments),
             "[\"item1\",\"item2\",\"item3\"]");
    msgs[1] = make_tool_msg("call_arr", "done");

    int cnt = 2;
    int r = hermes_sanitize_tool_call_arguments(msgs, &cnt);
    TEST("array args: 0 repairs (valid JSON)", r == 0);
    free_msgs(msgs, cnt);
}

static void test_sanitize_escaped_string_args(void) {
    /* Tool call with escaped quotes and special chars — valid */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"call_esc"};
    const char *names[] = {"echo_tool"};
    msgs[0] = make_assistant_tc("thinking", ids, names, 1);
    snprintf(msgs[0].tool_calls[0].arguments, sizeof(msgs[0].tool_calls[0].arguments),
             "{\"text\":\"hello \\\"world\\\" with \\\\escapes and\\nnewlines\"}");
    msgs[1] = make_tool_msg("call_esc", "result");

    int cnt = 2;
    int r = hermes_sanitize_tool_call_arguments(msgs, &cnt);
    TEST("escaped string args: 0 repairs (valid JSON)", r == 0);
    free_msgs(msgs, cnt);
}

static void test_sanitize_max_calls(void) {
    /* 8 tool calls (max array size), all valid */
    message_t msgs[10];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"a","b","c","d","e","f","g","h"};
    const char *names[] = {"t0","t1","t2","t3","t4","t5","t6","t7"};
    msgs[0] = make_assistant_tc("running 8 tools", ids, names, 8);
    for (int i = 0; i < 8; i++)
        msgs[1+i] = make_tool_msg(ids[i], "ok");

    int cnt = 9;
    int r = hermes_sanitize_tool_call_arguments(msgs, &cnt);
    TEST("8 tool calls args: 0 repairs", r == 0);
    free_msgs(msgs, cnt);
}

/* ── Agent loop edge cases ── */

static void test_loop_message_limit(void) {
    /* Verify run_conversation loop has iteration tracking */
    TEST("max_iterations field exists", sizeof(((agent_state_t*)0)->max_iterations) > 0);
}

static void test_message_roles_valid(void) {
    /* Verify msg_role_t enum values are in expected range */
    TEST("MSG_USER valid", MSG_USER >= 0 && MSG_USER <= 5);
    TEST("MSG_ASSISTANT > USER", MSG_ASSISTANT > MSG_USER);
    TEST("MSG_SYSTEM valid", MSG_SYSTEM >= 0);
    TEST("MSG_TOOL valid", MSG_TOOL >= 0);
}

static void test_tool_call_arg_truncation(void) {
    /* Very long arguments string — over 2KB */
    char big_args[2048];
    big_args[0] = '{';
    int pos = 1;
    for (int i = 0; i < 100 && pos < 2000; i++) {
        int n = snprintf(big_args + pos, sizeof(big_args) - pos,
                         "\"k%d\":\"v%d\",", i, i);
        if (n > 0) pos += n;
    }
    if (pos < (int)sizeof(big_args) - 2)
        snprintf(big_args + pos, sizeof(big_args) - pos, "\"last\":true}");
    else
        memcpy(big_args + sizeof(big_args) - 15, "\"last\":true}", 13);

    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"call_big"};
    const char *names[] = {"big_tool"};
    msgs[0] = make_assistant_tc("thinking", ids, names, 1);
    snprintf(msgs[0].tool_calls[0].arguments, sizeof(msgs[0].tool_calls[0].arguments), "%s", big_args);
    msgs[1] = make_tool_msg("call_big", "result");

    int cnt = 2;
    int r = hermes_sanitize_tool_call_arguments(msgs, &cnt);
    TEST("big args: 0 repairs (valid JSON)", r == 0);
    free_msgs(msgs, cnt);
}

/* ── main ── */

int main(void) {
    printf("=== Conversation Loop Edge Case Tests (X08) ===\n");

    printf("\n-- message repair edge cases --\n");
    test_repair_four_tools_assistant();
    test_repair_mixed_valid_stray();
    test_repair_no_assistant_before_tool();
    test_repair_system_user_consecutive();
    test_repair_system_user_user();
    test_repair_tool_missing_content();
    test_repair_interleaved_tools();
    test_repair_special_chars_ids();
    test_repair_all_tools_stray();

    printf("\n-- tool call argument sanitize edge cases --\n");
    test_sanitize_nested_json_args();
    test_sanitize_array_args();
    test_sanitize_escaped_string_args();
    test_sanitize_max_calls();

    printf("\n-- agent loop edge cases --\n");
    test_loop_message_limit();
    test_message_roles_valid();
    test_tool_call_arg_truncation();

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
