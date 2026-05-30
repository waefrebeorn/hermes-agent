/*
 * test_agent_message_sanitize.c — Test suite for hermes_message_sanitize().
 *
 * Phase 216: L26 build_assistant_message sanitization pipeline port.
 * Tests: surrogate sanitization, think block stripping, secret redaction.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test count tracking */
static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name, cond) do { \
    g_total++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

static void test_null_safety(void) {
    printf("  null safety...\n");
    TEST("NULL msg returns false", hermes_message_sanitize(NULL) == false);
}

static void test_surrogate_sanitization(void) {
    printf("  surrogate sanitization...\n");
    /* Build a message with surrogate characters in content */
    message_t *msg = message_new(MSG_ASSISTANT, "hello\xED\xA0\x80world");
    TEST("msg created", msg != NULL);
    bool result = hermes_message_sanitize(msg);
    TEST("sanitize returns true", result == true);
    TEST("surrogate replaced", msg->content && strstr(msg->content, "\xEF\xBF\xBD") != NULL);
    TEST("no raw surrogate", msg->content && strstr(msg->content, "\xED\xA0") == NULL);
    message_free(msg);

    /* Surrogate in reasoning */
    msg = message_new_assistant("hello", NULL, NULL, "think\xED\xA0\x80ing", NULL);
    TEST("reasoning msg created", msg != NULL);
    result = hermes_message_sanitize(msg);
    TEST("reasoning surrogate sanitized", result == true);
    TEST("reasoning surrogate replaced", msg->reasoning && strstr(msg->reasoning, "\xEF\xBF\xBD") != NULL);
    message_free(msg);

    /* Clean content — no surrogates, no changes */
    msg = message_new(MSG_ASSISTANT, "clean text");
    result = hermes_message_sanitize(msg);
    TEST("clean content unchanged", result == true);
    TEST("clean text preserved", msg->content && strcmp(msg->content, "clean text") == 0);
    message_free(msg);
}

static void test_think_block_stripping(void) {
    printf("  think block stripping...\n");

    /* Closed <think> pair */
    message_t *msg = message_new(MSG_ASSISTANT, "visible<think>hidden</think>end");
    TEST("think msg created", msg != NULL);
    hermes_message_sanitize(msg);
    TEST("think block stripped", msg->content && strstr(msg->content, "hidden") == NULL);
    TEST("visible content preserved", msg->content && strstr(msg->content, "visible") != NULL);
    TEST("end content preserved", msg->content && strstr(msg->content, "end") != NULL);
    message_free(msg);

    /* Case-insensitive: <THINK> */
    msg = message_new(MSG_ASSISTANT, "a<THINK>secret</THINK>b");
    hermes_message_sanitize(msg);
    TEST("uppercase think stripped", msg->content && strstr(msg->content, "secret") == NULL);
    message_free(msg);

    /* <thinking> variant */
    msg = message_new(MSG_ASSISTANT, "a<thinking>hide</thinking>b");
    hermes_message_sanitize(msg);
    TEST("thinking block stripped", msg->content && strstr(msg->content, "hide") == NULL);
    message_free(msg);

    /* <reasoning> variant */
    msg = message_new(MSG_ASSISTANT, "a<reasoning>think</reasoning>b");
    hermes_message_sanitize(msg);
    TEST("reasoning block stripped", msg->content && strstr(msg->content, "think") == NULL);
    message_free(msg);

    /* Multiple blocks */
    msg = message_new(MSG_ASSISTANT, "a<think>1</think>b<think>2</think>c");
    hermes_message_sanitize(msg);
    TEST("multiple blocks stripped", msg->content && strcmp(msg->content, "abc") == 0);
    message_free(msg);

    /* No think blocks — passthrough */
    msg = message_new(MSG_ASSISTANT, "plain text");
    hermes_message_sanitize(msg);
    TEST("plain text preserved", msg->content && strcmp(msg->content, "plain text") == 0);
    message_free(msg);

    /* Empty content */
    msg = message_new(MSG_ASSISTANT, "");
    hermes_message_sanitize(msg);
    TEST("empty content stays empty", msg->content && strcmp(msg->content, "") == 0);
    message_free(msg);

    /* Unterminated open tag at start */
    msg = message_new(MSG_ASSISTANT, "<think>unterminated remaining");
    hermes_message_sanitize(msg);
    TEST("unterminated at start stripped", msg->content && strlen(msg->content) == 0);
    message_free(msg);

    /* Tool-call XML block */
    msg = message_new(MSG_ASSISTANT, "before<tool_call>args</tool_call>after");
    hermes_message_sanitize(msg);
    TEST("tool_call XML stripped", msg->content && strstr(msg->content, "tool_call") == NULL);
    TEST("tool_call before/after preserved", msg->content && strcmp(msg->content, "beforeafter") == 0);
    message_free(msg);
}

static void test_secret_redaction(void) {
    printf("  secret redaction...\n");

    /* Redact API key in content using key=value format (hermes_redact expects structured data) */
    message_t *msg = message_new(MSG_ASSISTANT, "api_key=sk-abc123xyz7890123456789012345");
    hermes_message_sanitize(msg);
    TEST("sk- redacted", msg->content && strstr(msg->content, "sk-abc123xyz7890123456789012345") == NULL);
    TEST("redacted content not empty", msg->content && strlen(msg->content) > 0);
    message_free(msg);

    /* Redact in tool call arguments (hermes_redact matches key=value format) */
    tool_call_t tc;
    memset(&tc, 0, sizeof(tc));
    strcpy(tc.id, "call_1");
    strcpy(tc.name, "terminal");
    strcpy(tc.arguments,
        "api_key=sk-abc...7890");

    msg = message_new_assistant_with_toolcalls("tool use", &tc, 1, NULL, NULL);
    TEST("toolcall msg created", msg != NULL);
    hermes_message_sanitize(msg);
    TEST("tool call args redacted", strstr(msg->tool_calls[0].arguments, "sk-abc...7890") == NULL);
    message_free(msg);

    /* Redact in both content and tool call */
    memset(&tc, 0, sizeof(tc));
    strcpy(tc.id, "call_2");
    strcpy(tc.name, "web_get");
    strcpy(tc.arguments,
        "token=ghp_se...3210");
    msg = message_new_assistant_with_toolcalls(
        "token=ghp_my...4321",
        &tc, 1, NULL, NULL);
    hermes_message_sanitize(msg);
    TEST("both content and args redacted",
         strstr(msg->content, "ghp_my...4321") == NULL &&
         strstr(msg->tool_calls[0].arguments, "ghp_se...3210") == NULL);
    message_free(msg);
}

static void test_no_tool_calls(void) {
    printf("  no tool calls...\n");
    message_t *msg = message_new_assistant("hello", NULL, NULL, NULL, NULL);
    TEST("simple assistant msg created", msg != NULL);
    TEST("no tool calls count", msg->tool_calls_count == 0);
    bool result = hermes_message_sanitize(msg);
    TEST("sanitize returns true", result == true);
    message_free(msg);
}

static void test_clean_passthrough(void) {
    printf("  clean passthrough...\n");
    message_t *msg = message_new_assistant_with_toolcalls("clean text", NULL, 0, NULL, NULL);
    TEST("clean msg created", msg != NULL);
    bool result = hermes_message_sanitize(msg);
    TEST("clean msg returns true", result == true);
    TEST("clean content unchanged", msg->content && strcmp(msg->content, "clean text") == 0);
    TEST("tool_calls count zero", msg->tool_calls_count == 0);
    message_free(msg);
}

int main(void) {
    printf("=== Agent Message Sanitize Tests ===\n\n");

    test_null_safety();
    test_surrogate_sanitization();
    test_think_block_stripping();
    test_secret_redaction();
    test_no_tool_calls();
    test_clean_passthrough();

    printf("\n---\n");
    printf("Total: %d  Pass: %d  Fail: %d\n", g_total, g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
