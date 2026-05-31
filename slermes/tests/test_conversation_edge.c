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

/* ── Additional message sequence edge cases ── */

static void test_repair_consecutive_assistants(void) {
    /* Two consecutive assistant messages — repair behaves predictably */
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "hello");
    msgs[1] = make_assistant_tc("first", NULL, NULL, 0);
    msgs[2] = make_assistant_tc("second", NULL, NULL, 0);

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("consecutive assistants: no crash", r >= 0);
    TEST("consecutive assistants: count decreased or equal", cnt <= 3);
    free_msgs(msgs, cnt);
}

static void test_repair_assistant_before_system(void) {
    /* Assistant before system (invalid order) — should drop assistant */
    message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_ASSISTANT, "response before system");
    msgs[1] = make_msg(MSG_SYSTEM, "you are helpful");

    int cnt = 2;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("assistant before system: repair", r >= 0);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_after_tool_consecutive(void) {
    /* Two tool results for the same call ID (edge: duplicate) */
    const char *ids[] = {"call_x"};
    const char *names[] = {"tool_x"};
    message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("running", ids, names, 1);
    msgs[1] = make_tool_msg("call_x", "first result");
    msgs[2] = make_tool_msg("call_x", "second result"); /* duplicate */
    msgs[3] = make_msg(MSG_USER, "continue");

    int cnt = 4;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    /* Second tool with same ID is kept since it matches the assistant */
    TEST("duplicate tool call ID: no crash", r >= 0);
    free_msgs(msgs, cnt);
}

static void test_repair_system_only(void) {
    /* Only a system message, no user */
    message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_SYSTEM, "you are helpful");

    int cnt = 1;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("system only: no crash", r >= 0);
    TEST("system only: count unchanged", cnt == 1);
    free_msgs(msgs, cnt);
}

static void test_repair_assistant_only(void) {
    /* Only an assistant message, no user/system */
    message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("hello!", NULL, NULL, 0);

    int cnt = 1;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("assistant only: no crash", r >= 0);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_only(void) {
    /* Only a tool message with no assistant */
    message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_tool_msg("orphan", "no assistant");

    int cnt = 1;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("tool only: dropped (1 repair)", r == 1);
    TEST("tool only: count=0", cnt == 0);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_call_null_name(void) {
    /* Assistant with null tool name but valid ID */
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    const char *ids[] = {"call_1"};
    const char *names[] = {""};  /* empty name, not NULL */
    msgs[0] = make_assistant_tc("thinking", ids, names, 1);
    msgs[1] = make_tool_msg("call_1", "result");
    msgs[2] = make_msg(MSG_USER, "thanks");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("null tool name: 0 repairs (names ignored in matching)", r == 0);
    free_msgs(msgs, cnt);
}

static void test_repair_extremely_long_tool_id(void) {
    /* Reasonably long tool call ID — within tool_call_t.id[64] limit */
    char long_id[60];
    memset(long_id, 'x', sizeof(long_id) - 1);
    long_id[sizeof(long_id) - 1] = '\0';
    const char *ids[] = {long_id};
    const char *names[] = {"tool"};
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("running", ids, names, 1);
    msgs[1] = make_tool_msg(long_id, "done");
    msgs[2] = make_msg(MSG_USER, "ok");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("long tool ID: 0 repairs (valid match)", r == 0);
    TEST("long tool ID: count=3", cnt == 3);
    free_msgs(msgs, cnt);
}

/* ── NULL safety edge cases ── */

static void test_repair_null_safety(void) {
    /* NULL pointer safety for repair and sanitize */
    int cnt = 5;
    TEST("repair: NULL msgs, NULL count is safe",
         hermes_repair_message_sequence(NULL, NULL) == 0);
    TEST("repair: NULL msgs is safe",
         hermes_repair_message_sequence(NULL, &cnt) == 0);
    /* NULL count with valid msgs */
    message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "test");
    TEST("repair: NULL count is safe",
         hermes_repair_message_sequence(msgs, NULL) == 0);
    free_msgs(msgs, 1);

    TEST("sanitize: NULL msgs, NULL count is safe",
         hermes_sanitize_tool_call_arguments(NULL, NULL) == 0);
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "test");
    cnt = 1;
    TEST("sanitize: NULL count is safe",
         hermes_sanitize_tool_call_arguments(msgs, NULL) == 0);
    free_msgs(msgs, 1);
}

static void test_repair_negative_count(void) {
    /* Negative count should not crash */
    message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "test");
    int cnt = -1;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("negative count: no crash", r >= 0);
    free_msgs(msgs, 1);
}

static void test_repair_zero_count(void) {
    /* Zero count should not crash */
    int cnt = 0;
    int r = hermes_repair_message_sequence(NULL, &cnt);
    TEST("zero count: no crash", r == 0);
}

/* ── hermes_message_sanitize tests ── */

static void test_sanitize_null_msg(void) {
    TEST("sanitize NULL msg", hermes_message_sanitize(NULL) == false);
}

static void test_sanitize_empty_message(void) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_USER;
    m.content = NULL;
    TEST("sanitize empty msg (no content)", hermes_message_sanitize(&m) == false);
}

static void test_sanitize_think_blocks(void) {
    /* Content with <think>...</think> tags should be stripped */
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    m.content = strdup("Let me think...<think>This is internal reasoning</think> Answer is 42");
    bool r = hermes_message_sanitize(&m);
    TEST("sanitize think blocks: returns true", r == true);
    TEST("sanitize think blocks: tag stripped",
         m.content && strstr(m.content, "Let me think...") != NULL &&
         strstr(m.content, "Answer is 42") != NULL &&
         strstr(m.content, "This is internal") == NULL);
    free(m.content); m.content = NULL;
}

static void test_sanitize_think_alone(void) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    m.content = strdup("Just a message with no tags");
    bool r = hermes_message_sanitize(&m);
    TEST("sanitize no-op: returns true (no think/redact needed)", r == true);
    TEST("sanitize no-op: content unchanged",
         m.content && strcmp(m.content, "Just a message with no tags") == 0);
    free(m.content); m.content = NULL;
}

static void test_sanitize_redact_simple(void) {
    /* api_key= prefix triggers key-value redaction with max_show=4 prefix chars kept */
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    m.content = strdup("api_key=abcdefghijklmnop12345");
    bool r = hermes_message_sanitize(&m);
    TEST("sanitize redact: returns true", r == true);
    TEST("sanitize redact: value redacted (4 prefix chars kept)",
         m.content && strstr(m.content, "api_key=abcd***REDACTED***") != NULL);
    free(m.content); m.content = NULL;
}

static void test_sanitize_surrogate_content(void) {
    /* Lone surrogates in content should be cleaned */
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    /* Construct content with a lone high surrogate */
    char *buf = malloc(32);
    buf[0] = 'h'; buf[1] = 'i'; buf[2] = 0xED; buf[3] = 0xA0; buf[4] = 0x80;
    buf[5] = '!'; buf[6] = '\0';
    m.content = buf;
    bool r = hermes_message_sanitize(&m);
    TEST("sanitize surrogates: returns true", r == true);
    TEST("sanitize surrogates: lone surrogate removed or replaced",
         m.content && strlen(m.content) >= 2);
    free(m.content); m.content = NULL;
}

static void test_sanitize_tool_args_redact(void) {
    /* Tool call arguments containing 'apikey' prefix should be redacted */
    message_t m;
    memset(&m, 0, sizeof(m));
    m.role = MSG_ASSISTANT;
    m.content = strdup("running tool");
    m.tool_calls_count = 1;
    snprintf(m.tool_calls[0].id, sizeof(m.tool_calls[0].id), "call_1");
    snprintf(m.tool_calls[0].name, sizeof(m.tool_calls[0].name), "read");
    /* Use 'apikey:' pattern which should trigger key-value redaction */
    snprintf(m.tool_calls[0].arguments, sizeof(m.tool_calls[0].arguments),
             "apikey:abcdefghijklmnop12345");
    bool r = hermes_message_sanitize(&m);
    TEST("sanitize tool args: returns true", r == true);
    TEST("sanitize tool args: secret redacted",
         strstr(m.tool_calls[0].arguments, "***REDACTED***") != NULL);
    free(m.content); m.content = NULL;
}

/* ── message_free edge cases ── */

static void test_message_free_null(void) {
    /* message_free must not crash on NULL */
    message_free(NULL);
    TEST("message_free(NULL): no crash", true);
}

static void test_message_free_empty(void) {
    /* message_free with all NULL fields — no double-free */
    message_t *m = (message_t *)calloc(1, sizeof(message_t));
    TEST("message_free: alloc ok", m != NULL);
    m->role = MSG_USER;
    message_free(m);
    TEST("message_free(empty msg): no crash", true);
}

static void test_message_free_with_content(void) {
    message_t *m = (message_t *)calloc(1, sizeof(message_t));
    TEST("message_free content: alloc ok", m != NULL);
    m->role = MSG_USER;
    m->content = strdup("hello");
    message_free(m);
    TEST("message_free(content): freed content handled", true);
}

/* ── Additional repair edge cases ── */

static void test_repair_consecutive_user_msgs(void) {
    /* Two consecutive user messages should merge */
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "first");
    msgs[1] = make_msg(MSG_USER, "second");
    msgs[2] = make_msg(MSG_USER, "third");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("3 consecutive users: repair occurs", r > 0);
    TEST("3 consecutive users: count < 3", cnt < 3);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_id_cross_assistant(void) {
    /* Tool IDs belong to different assistant turns (with user message between) */
    const char *ids1[] = {"call_a"};
    const char *names1[] = {"tool_a"};
    const char *ids2[] = {"call_b"};
    const char *names2[] = {"tool_b"};
    message_t msgs[6];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "first request");
    msgs[1] = make_assistant_tc("first response", ids1, names1, 1);
    msgs[2] = make_tool_msg("call_a", "result for first");
    msgs[3] = make_msg(MSG_USER, "second request");
    msgs[4] = make_assistant_tc("second response", ids2, names2, 1);
    msgs[5] = make_tool_msg("call_b", "result for second");

    int cnt = 6;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    /* Both tools match their respective assistants — valid */
    TEST("cross-assistant tool IDs: 0 repairs", r == 0);
    TEST("cross-assistant tool IDs: count=6", cnt == 6);
    free_msgs(msgs, cnt);
}

static void test_repair_empty_tool_call_id(void) {
    /* Tool call with empty string ID */
    const char *ids[] = {""};
    const char *names[] = {"tool"};
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("running", ids, names, 1);
    msgs[1] = make_tool_msg("", "result with empty ID");
    msgs[2] = make_msg(MSG_USER, "thanks");

    int cnt = 3;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    /* Empty IDs may or may not be matched — just must not crash */
    TEST("empty tool ID: no crash", r >= 0);
    free_msgs(msgs, cnt);
}

static void test_repair_user_after_tool(void) {
    /* Normal flow: assistant→tool→user (valid, no repair needed) */
    const char *ids[] = {"call_1"};
    const char *names[] = {"tool"};
    message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "do something");
    msgs[1] = make_assistant_tc("ok", ids, names, 1);
    msgs[2] = make_tool_msg("call_1", "done");
    msgs[3] = make_msg(MSG_USER, "continue");

    int cnt = 4;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("user after tool: 0 repairs", r == 0);
    TEST("user after tool: count=4", cnt == 4);
    free_msgs(msgs, cnt);
}

static void test_repair_mixed_roles_large(void) {
    /* Complex sequence: user, assistant(tc1), tool(tc1), user, assistant, user */
    const char *ids[] = {"tc1"};
    const char *names[] = {"calc"};
    message_t msgs[6];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "calc 1+1");
    msgs[1] = make_assistant_tc("running", ids, names, 1);
    msgs[2] = make_tool_msg("tc1", "2");
    msgs[3] = make_msg(MSG_USER, "now calc 2+2");
    msgs[4] = make_assistant_tc("answer: 4", NULL, NULL, 0);
    msgs[5] = make_msg(MSG_USER, "thanks");

    int cnt = 6;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("mixed roles large: 0 repairs", r == 0);
    TEST("mixed roles large: count=6", cnt == 6);
    free_msgs(msgs, cnt);
}

static void test_repair_tool_mismatch_all(void) {
    /* All tool calls have IDs that don't match any assistant tool call */
    const char *ids[] = {"call_a", "call_b"};
    const char *names[] = {"tool1", "tool2"};
    message_t msgs[5];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_tc("running", ids, names, 2);
    msgs[1] = make_tool_msg("call_x", "no match 1");
    msgs[2] = make_tool_msg("call_y", "no match 2");
    msgs[3] = make_tool_msg("call_z", "no match 3");
    msgs[4] = make_msg(MSG_USER, "continue");

    int cnt = 5;
    int r = hermes_repair_message_sequence(msgs, &cnt);
    TEST("all tool IDs mismatched: 3 repairs", r == 3);
    TEST("all tool IDs mismatched: count=2", cnt == 2);
    free_msgs(msgs, cnt);
}

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

    printf("\n-- additional message sequence edge cases --\n");
    test_repair_consecutive_assistants();
    test_repair_assistant_before_system();
    test_repair_tool_after_tool_consecutive();
    test_repair_system_only();
    test_repair_assistant_only();
    test_repair_tool_only();
    test_repair_tool_call_null_name();
    test_repair_extremely_long_tool_id();

    printf("\n-- null safety edge cases --\n");
    test_repair_null_safety();
    test_repair_negative_count();
    test_repair_zero_count();

    printf("\n-- hermes_message_sanitize tests --\n");
    test_sanitize_null_msg();
    test_sanitize_empty_message();
    test_sanitize_think_blocks();
    test_sanitize_think_alone();
    test_sanitize_redact_simple();
    test_sanitize_surrogate_content();
    test_sanitize_tool_args_redact();

    printf("\n-- message_free edge cases --\n");
    test_message_free_null();
    test_message_free_empty();
    test_message_free_with_content();

    printf("\n-- additional repair edge cases --\n");
    test_repair_consecutive_user_msgs();
    test_repair_tool_id_cross_assistant();
    test_repair_empty_tool_call_id();
    test_repair_user_after_tool();
    test_repair_mixed_roles_large();
    test_repair_tool_mismatch_all();

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
