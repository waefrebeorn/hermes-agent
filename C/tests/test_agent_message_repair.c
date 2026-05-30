/*
 * test_agent_message_repair.c — Tests for hermes_repair_message_sequence().
 *
 * Port of Python agent_runtime_helpers.repair_message_sequence().
 *
 * Tests:
 * - Empty message list
 * - Stray tool message (no matching assistant tool_call)
 * - Valid tool message (matching assistant tool_call_id)
 * - Consecutive user messages merged with "\n\n"
 * - User message after tool message clears known IDs
 * - Multiple repairs in one call
 * - NULL / edge cases
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Helper: create a message with given role and content.
   Caller must free content pointer (strdup'd). */
static message_t make_msg(message_role_t role, const char *content) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.role = role;
    msg.content = content ? strdup(content) : NULL;
    return msg;
}

/* Helper: create an assistant message with tool calls */
static message_t make_assistant_with_tool(const char *content,
                                           const char *tool_call_id,
                                           const char *tool_name) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.role = MSG_ASSISTANT;
    msg.content = content ? strdup(content) : NULL;
    if (tool_call_id) {
        msg.tool_calls_count = 1;
        snprintf(msg.tool_calls[0].id, sizeof(msg.tool_calls[0].id), "%s", tool_call_id);
        snprintf(msg.tool_calls[0].name, sizeof(msg.tool_calls[0].name), "%s", tool_name ? tool_name : "test_tool");
        snprintf(msg.tool_calls[0].arguments, sizeof(msg.tool_calls[0].arguments), "{}");
    }
    return msg;
}

/* Helper: create a tool result message */
static message_t make_tool_msg(const char *tool_call_id, const char *content) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.role = MSG_TOOL;
    msg.content = content ? strdup(content) : NULL;
    msg.tool_call_id = tool_call_id ? strdup(tool_call_id) : NULL;
    return msg;
}

/* Free a message's allocated fields */
static void free_msg(message_t *msg) {
    if (!msg) return;
    free(msg->content); msg->content = NULL;
    free(msg->tool_call_id); msg->tool_call_id = NULL;
    free(msg->tool_name); msg->tool_name = NULL;
    free(msg->reasoning); msg->reasoning = NULL;
    free(msg->encrypted_content); msg->encrypted_content = NULL;
}

/* ================================================================ */

static void test_empty_or_null(void) {
    printf("\n--- empty/null ---\n");
    int count = 0;
    int r = hermes_repair_message_sequence(NULL, NULL);
    TEST("NULL messages + NULL count returns 0", r == 0);

    r = hermes_repair_message_sequence(NULL, &count);
    TEST("NULL messages returns 0", r == 0);

    message_t msgs[10];
    memset(msgs, 0, sizeof(msgs));
    r = hermes_repair_message_sequence(msgs, &count);
    TEST("count=0 returns 0", r == 0);
}

static void test_stray_tool_dropped(void) {
    printf("\n--- stray tool dropped ---\n");
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "hello");
    msgs[1] = make_assistant_with_tool("thinking", "call_123", "read_file");
    msgs[2] = make_tool_msg("call_999", "result"); /* stray — no matching ID */

    int count = 3;
    int r = hermes_repair_message_sequence(msgs, &count);
    TEST("stray tool dropped, 1 repair", r == 1);
    TEST("count reduced to 2", count == 2);
    TEST("message 0 is still user", msgs[0].role == MSG_USER);
    TEST("message 1 is still assistant", msgs[1].role == MSG_ASSISTANT);

    for (int i = 0; i < count; i++) free_msg(&msgs[i]);
}

static void test_valid_tool_preserved(void) {
    printf("\n--- valid tool preserved ---\n");
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_with_tool("thinking", "call_123", "read_file");
    msgs[1] = make_tool_msg("call_123", "result"); /* matches */
    msgs[2] = make_msg(MSG_USER, "thanks");

    int count = 3;
    int r = hermes_repair_message_sequence(msgs, &count);
    TEST("valid tool preserved, 0 repairs", r == 0);
    TEST("count still 3", count == 3);
    TEST("tool message preserved", msgs[1].role == MSG_TOOL);

    for (int i = 0; i < count; i++) free_msg(&msgs[i]);
}

static void test_consecutive_users_merged(void) {
    printf("\n--- consecutive users merged ---\n");
    message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "first message");
    msgs[1] = make_assistant_with_tool("ok", "call_1", "tool");
    msgs[2] = make_tool_msg("call_1", "done");
    msgs[3] = make_msg(MSG_USER, "details");

    /* Insert second consecutive user by creating a 5-element array */
    message_t msgs2[5];
    memset(msgs2, 0, sizeof(msgs2));
    msgs2[0] = make_msg(MSG_USER, "first");
    msgs2[1] = make_msg(MSG_USER, "second"); /* consecutive user */
    msgs2[2] = make_assistant_with_tool("ok", "call_1", "tool");
    msgs2[3] = make_tool_msg("call_1", "done");
    msgs2[4] = make_msg(MSG_USER, "third");

    int count = 5;
    int r = hermes_repair_message_sequence(msgs2, &count);
    TEST("consecutive users merged, 1 repair", r >= 1);
    /* First two users should be merged */
    TEST("count reduced to 4", count == 4);
    if (count >= 1) {
        TEST("merged user content has newline separator",
             strstr(msgs2[0].content, "\n\n") != NULL);
    }

    /* Only free up to the new count — entries beyond were overwritten */
    for (int i = 0; i < count; i++) free_msg(&msgs2[i]);
}

static void test_user_clears_known_ids(void) {
    printf("\n--- user clears known IDs ---\n");
    message_t msgs[5];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_assistant_with_tool("thinking", "call_old", "old_tool");
    msgs[1] = make_tool_msg("call_old", "result");
    msgs[2] = make_msg(MSG_USER, "user interrupts");
    msgs[3] = make_assistant_with_tool("response", "call_new", "new_tool");
    msgs[4] = make_tool_msg("call_old", "stale"); /* old ID after user — should be dropped */

    int count = 5;
    int r = hermes_repair_message_sequence(msgs, &count);
    TEST("stale tool after user interrupt dropped, 1 repair", r >= 1);

    for (int i = 0; i < count; i++) free_msg(&msgs[i]);
}

static void test_three_consecutive_users(void) {
    printf("\n--- three consecutive users ---\n");
    message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0] = make_msg(MSG_USER, "a");
    msgs[1] = make_msg(MSG_USER, "b");
    msgs[2] = make_msg(MSG_USER, "c");

    int count = 3;
    int r = hermes_repair_message_sequence(msgs, &count);
    TEST("three consecutive users merged (2 repairs)", r == 2);
    TEST("count reduced to 1", count == 1);
    if (count == 1) {
        TEST("all content merged",
             strcmp(msgs[0].content, "a\n\nb\n\nc") == 0);
    }

    for (int i = 0; i < count; i++) free_msg(&msgs[i]);
}

int main(void) {
    printf("=== Agent Message Repair Test Suite ===\n");

    test_empty_or_null();
    test_stray_tool_dropped();
    test_valid_tool_preserved();
    test_consecutive_users_merged();
    test_user_clears_known_ids();
    test_three_consecutive_users();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
