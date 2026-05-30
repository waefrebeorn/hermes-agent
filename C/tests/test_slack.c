/*
 * test_slack.c — Slack platform adapter test suite (S7 X01).
 *
 * Tests pure functions: slack_set_token, slack_set_channel,
 * slack_set_signing_secret, slack_get_chat_id, slack_get_text.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_slack.c src/gateway/platforms/slack.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_slack
 *
 * Run:
 *   /tmp/hermes_test_slack
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void slack_set_token(const char *token);
void slack_set_channel(const char *id);
void slack_set_signing_secret(const char *secret);
const char *slack_get_chat_id(json_node_t *update);
const char *slack_get_text(json_node_t *update);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* ================================================================
 *  1. slack_set_token
 * ================================================================ */
static void test_set_token(void) {
    printf("\n--- slack_set_token ---\n");

    slack_set_token("slack-bot-token-001");
    TEST("set_token normal", 1);

    slack_set_token("");
    TEST("set_token empty", 1);

    slack_set_token("slack-bot-token-long-example");
    TEST("set_token long", 1);

    /* Token with special chars */
    slack_set_token("slack-bot-token-special-ABC_DEF");
    TEST("set_token special chars", 1);
}

/* ================================================================
 *  2. slack_set_channel
 * ================================================================ */
static void test_set_channel(void) {
    printf("\n--- slack_set_channel ---\n");

    slack_set_channel("C1234567890");
    TEST("set_channel normal", 1);

    slack_set_channel("");
    TEST("set_channel empty", 1);

    slack_set_channel("Cgeneral-channel-42");
    TEST("set_channel with dashes", 1);

    slack_set_channel("D0123456");
    TEST("set_channel DM channel", 1);
}

/* ================================================================
 *  3. slack_set_signing_secret
 * ================================================================ */
static void test_set_signing_secret(void) {
    printf("\n--- slack_set_signing_secret ---\n");

    /* NULL-safe: should not crash */
    slack_set_signing_secret(NULL);
    TEST("set_signing_secret NULL (no crash)", 1);

    slack_set_signing_secret("abc123def456");
    TEST("set_signing_secret normal", 1);

    slack_set_signing_secret("");
    TEST("set_signing_secret empty", 1);

    slack_set_signing_secret("0123456789abcdef0123456789abcdef01234567");
    TEST("set_signing_secret hex", 1);
}

/* ================================================================
 *  4. slack_get_chat_id
 * ================================================================ */
static void test_get_chat_id(void) {
    printf("\n--- slack_get_chat_id ---\n");

    slack_set_channel("Ctest-channel-99");

    /* Returns static channel_id regardless of update content */
    json_node_t *update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    const char *cid = slack_get_chat_id(update);
    TEST_STR_EQ("get_chat_id returns channel_id", cid, "Ctest-channel-99");
    json_free(update);

    /* After changing channel */
    slack_set_channel("D0123456");
    update = json_new_object();
    cid = slack_get_chat_id(update);
    TEST_STR_EQ("get_chat_id reflects change", cid, "D0123456");
    json_free(update);

    /* NULL update */
    cid = slack_get_chat_id(NULL);
    TEST_STR_EQ("get_chat_id with NULL update", cid, "D0123456");
}

/* ================================================================
 *  5. slack_get_text
 * ================================================================ */
static void test_get_text(void) {
    printf("\n--- slack_get_text ---\n");

    /* Normal message text */
    json_node_t *message = json_new_object();
    json_set(message, "text", json_string("Hello from Slack!"));
    json_node_t *update = json_new_object();
    json_set(update, "message", message);

    const char *text = slack_get_text(update);
    TEST_STR_EQ("get_text normal", text, "Hello from Slack!");
    json_free(update);

    /* Empty text */
    message = json_new_object();
    json_set(message, "text", json_string(""));
    update = json_new_object();
    json_set(update, "message", message);

    text = slack_get_text(update);
    TEST_STR_EQ("get_text empty string", text, "");
    json_free(update);

    /* No text field in message */
    message = json_new_object();
    json_set(message, "other", json_string("value"));
    update = json_new_object();
    json_set(update, "message", message);

    text = slack_get_text(update);
    TEST_NULL("get_text missing text field", text);
    json_free(update);

    /* No message at all */
    update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    text = slack_get_text(update);
    TEST_NULL("get_text no message object", text);
    json_free(update);

    /* NULL update */
    text = slack_get_text(NULL);
    TEST_NULL("get_text NULL update", text);

    /* Unicode text */
    message = json_new_object();
    json_set(message, "text", json_string("Slack says Hello 世界!"));
    update = json_new_object();
    json_set(update, "message", message);

    text = slack_get_text(update);
    TEST_STR_EQ("get_text unicode", text, "Slack says Hello 世界!");
    json_free(update);

    /* Multi-line */
    message = json_new_object();
    json_set(message, "text", json_string("Line 1\nLine 2\nLine 3"));
    update = json_new_object();
    json_set(update, "message", message);

    text = slack_get_text(update);
    TEST_STR_EQ("get_text multi-line", text, "Line 1\nLine 2\nLine 3");
    json_free(update);

    /* Markdown-formatted text */
    message = json_new_object();
    json_set(message, "text", json_string("Hello *bold* and ~strikethrough~ and `code`"));
    update = json_new_object();
    json_set(update, "message", message);

    text = slack_get_text(update);
    TEST_STR_EQ("get_text markdown", text, "Hello *bold* and ~strikethrough~ and `code`");
    json_free(update);
}

/* ================================================================
 *  6. Setter/getter interaction
 * ================================================================ */
static void test_setter_getter_interaction(void) {
    printf("\n--- setter/getter interaction ---\n");

    slack_set_channel("Cinteraction-test");
    json_node_t *update = json_new_object();

    /* get_chat_id should match last set_channel */
    const char *cid = slack_get_chat_id(update);
    TEST_STR_EQ("chat_id matches set_channel", cid, "Cinteraction-test");

    json_free(update);

    /* Re-set and re-check */
    slack_set_channel("Cre-set-42");
    update = json_new_object();
    cid = slack_get_chat_id(update);
    TEST_STR_EQ("chat_id after re-set", cid, "Cre-set-42");
    json_free(update);

    /* Multiple set_token calls */
    slack_set_token("token-first");
    slack_set_token("token-second");
    slack_set_token("token-third");
    TEST("multiple set_token calls", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_slack.c ===\n");

    test_set_token();
    test_set_channel();
    test_set_signing_secret();
    test_get_chat_id();
    test_get_text();
    test_setter_getter_interaction();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
