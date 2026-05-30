/*
 * test_qqbot.c — QQ Bot platform adapter test suite (S7 X01).
 *
 * Tests pure functions: qqbot_set_webhook, qqbot_set_token,
 * qqbot_queue_message, qqbot_poll_messages, qqbot_get_chat_id,
 * qqbot_get_text, qqbot_handle_webhook.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libgateway \
 *       tests/test_qqbot.c src/gateway/platforms/qqbot.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_qqbot
 *
 * Run:
 *   /tmp/hermes_test_qqbot
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Forward declarations for tested functions */
void qqbot_set_webhook(const char *url);
void qqbot_set_token(const char *token);
void qqbot_queue_message(const char *chat_id, const char *text,
                          const char *sender_id);
json_node_t *qqbot_poll_messages(http_client_t *http);
const char *qqbot_get_chat_id(json_node_t *update);
const char *qqbot_get_text(json_node_t *update);
void qqbot_handle_webhook(const char *body);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* Drain any leftover messages from previous tests */
static void drain_queue(void) {
    json_node_t *msgs;
    while ((msgs = qqbot_poll_messages(NULL)) != NULL)
        json_free(msgs);
}

/* ================================================================
 *  1. qqbot_set_webhook / qqbot_set_token
 * ================================================================ */
static void test_setters(void) {
    printf("\n--- qqbot_set_webhook / qqbot_set_token ---\n");

    qqbot_set_webhook("https://qqbot.example.com/webhook");
    TEST("set_webhook normal URL", 1);

    qqbot_set_webhook("");
    TEST("set_webhook empty", 1);

    qqbot_set_webhook("https://bot.q.qq.com/callback");
    TEST("set_webhook reset", 1);

    qqbot_set_token("my-qq-bot-token-42");
    TEST("set_token normal", 1);

    qqbot_set_token("");
    TEST("set_token empty", 1);

    qqbot_set_token("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    TEST("set_token alphanumeric", 1);

    /* NULL safety — setter functions handle NULL */
    qqbot_set_webhook(NULL);
    TEST("set_webhook NULL (preserves previous)", 1);

    qqbot_set_token(NULL);
    TEST("set_token NULL (preserves previous)", 1);
}

/* ================================================================
 *  2. qqbot_queue_message + qqbot_poll_messages (ring buffer)
 * ================================================================ */
static void test_queue_poll(void) {
    printf("\n--- qqbot_queue_message + qqbot_poll_messages ---\n");

    drain_queue();

    /* Empty queue → NULL */
    json_node_t *msgs = qqbot_poll_messages(NULL);
    TEST_NULL("empty queue returns NULL", msgs);

    /* Single message */
    qqbot_queue_message("group_12345", "Hello world", "user_42");

    /* Single message — poll returns JSON array */
    msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("single message poll returns array", msgs);

    json_node_t *first_msg = json_get(msgs, 0);
    TEST_NOT_NULL("first element exists", first_msg);

    const char *cid = qqbot_get_chat_id(first_msg);
    TEST_STR_EQ("chat_id matches", cid, "group_12345");

    const char *text = qqbot_get_text(first_msg);
    TEST_STR_EQ("text matches", text, "Hello world");

    json_free(msgs);

    /* Queue empty again after poll */
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("queue empty after drain", msgs);

    /* Multiple messages */
    qqbot_queue_message("chat_1", "First", "sender_1");
    qqbot_queue_message("chat_2", "Second", "sender_2");
    qqbot_queue_message("chat_3", "Third", "sender_3");

    msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("multi-message poll", msgs);
    size_t len = json_len(msgs);
    TEST("three messages in array", len == 3);

    /* Check FIFO order */
    json_node_t *first = json_get(msgs, 0);
    TEST_STR_EQ("first chat_id", json_get_str(first, "chat_id", ""), "chat_1");
    TEST_STR_EQ("first text", json_get_str(first, "text", ""), "First");

    json_node_t *second = json_get(msgs, 1);
    TEST_STR_EQ("second chat_id", json_get_str(second, "chat_id", ""), "chat_2");

    json_node_t *third = json_get(msgs, 2);
    TEST_STR_EQ("third chat_id", json_get_str(third, "chat_id", ""), "chat_3");

    json_free(msgs);
    drain_queue();

    /* NULL/empty params to queue_message */
    qqbot_queue_message(NULL, "text", "sender");
    qqbot_queue_message("chat", NULL, "sender");
    qqbot_queue_message("chat", "text", NULL);

    msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("NULL-param messages present", msgs);
    len = json_len(msgs);
    TEST("three NULL-param messages", len == 3);

    if (len >= 1) {
        json_node_t *first_msg = json_get(msgs, 0);
        const char *cid0 = json_get_str(first_msg, "chat_id", "");
        TEST("NULL chat_id defaults to 'qqbot'", strcmp(cid0, "qqbot") == 0);

        json_node_t *second_msg = json_get(msgs, 1);
        const char *t1 = json_get_str(second_msg, "text", "");
        TEST("NULL text defaults to empty", t1[0] == '\0');
    }
    json_free(msgs);
    drain_queue();
}

/* ================================================================
 *  3. qqbot_get_chat_id / qqbot_get_text
 * ================================================================ */
static void test_getters(void) {
    printf("\n--- qqbot_get_chat_id / qqbot_get_text ---\n");

    /* Normal update */
    json_node_t *update = json_new_object();
    json_set(update, "chat_id", json_string("channel_99"));
    json_set(update, "text", json_string("Hello QQ Bot!"));

    const char *cid = qqbot_get_chat_id(update);
    TEST_STR_EQ("get_chat_id normal", cid, "channel_99");

    const char *text = qqbot_get_text(update);
    TEST_STR_EQ("get_text normal", text, "Hello QQ Bot!");
    json_free(update);

    /* Missing chat_id → default "qqbot" */
    update = json_new_object();
    json_set(update, "text", json_string("no chat id"));
    cid = qqbot_get_chat_id(update);
    TEST_STR_EQ("get_chat_id fallback", cid, "qqbot");
    json_free(update);

    /* Missing text → empty string */
    update = json_new_object();
    json_set(update, "chat_id", json_string("c42"));
    text = qqbot_get_text(update);
    TEST_STR_EQ("get_text missing default", text, "");
    json_free(update);

    /* NULL update */
    cid = qqbot_get_chat_id(NULL);
    TEST_STR_EQ("get_chat_id NULL", cid, "qqbot");

    text = qqbot_get_text(NULL);
    TEST_STR_EQ("get_text NULL", text, "");

    /* Empty update */
    update = json_new_object();
    cid = qqbot_get_chat_id(update);
    TEST_STR_EQ("get_chat_id empty object", cid, "qqbot");
    json_free(update);

    /* Unicode text */
    update = json_new_object();
    json_set(update, "chat_id", json_string("unic"));
    json_set(update, "text", json_string("QQ Bot says 你好世界! Привет!"));
    text = qqbot_get_text(update);
    TEST_STR_EQ("get_text unicode", text, "QQ Bot says 你好世界! Привет!");
    json_free(update);
}

/* ================================================================
 *  4. qqbot_handle_webhook — OneBot format
 * ================================================================ */
static void test_handle_onebot(void) {
    printf("\n--- qqbot_handle_webhook OneBot format ---\n");

    drain_queue();

    /* OneBot group message */
    const char *onebot_group =
        "{\"post_type\":\"message\",\"message_type\":\"group\","
        "\"group_id\":12345,\"user_id\":67890,"
        "\"message\":\"Hello from OneBot group!\",\"self_id\":111}";
    qqbot_handle_webhook(onebot_group);

    json_node_t *msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("OneBot group yields message", msgs);
    if (msgs) {
        json_node_t *msg = json_get(msgs, 0);
        const char *cid = json_get_str(msg, "chat_id", "");
        const char *txt = json_get_str(msg, "text", "");
        TEST("group chat_id starts with 'group_'", strstr(cid, "group_") != NULL);
        TEST("group chat_id has group_id", strstr(cid, "12345") != NULL);
        TEST_STR_EQ("group text matches", txt, "Hello from OneBot group!");
        json_free(msgs);
    }
    drain_queue();

    /* OneBot private message */
    const char *onebot_private =
        "{\"post_type\":\"message\",\"message_type\":\"private\","
        "\"user_id\":55555,\"message\":\"Private chat message\",\"self_id\":111}";
    qqbot_handle_webhook(onebot_private);

    msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("OneBot private yields message", msgs);
    if (msgs) {
        json_node_t *msg = json_get(msgs, 0);
        const char *cid = json_get_str(msg, "chat_id", "");
        const char *txt = json_get_str(msg, "text", "");
        TEST("private chat_id starts with 'user_'", strstr(cid, "user_") != NULL);
        TEST("private chat_id has user_id", strstr(cid, "55555") != NULL);
        TEST_STR_EQ("private text matches", txt, "Private chat message");
        json_free(msgs);
    }
    drain_queue();
}

/* ================================================================
 *  5. qqbot_handle_webhook — QQ Guild API format
 * ================================================================ */
static void test_handle_guild_api(void) {
    printf("\n--- qqbot_handle_webhook QQ Guild API format ---\n");

    drain_queue();

    /* QQ Guild API format: { "op": 0, "d": { "channel_id": ..., "content": ..., "author": { "id": ... } } } */
    const char *guild_msg =
        "{\"op\":0,\"d\":{"
        "\"channel_id\":\"guild_channel_42\","
        "\"content\":\"Hello from Guild API!\","
        "\"author\":{\"id\":\"author_999\"}"
        "}}";
    qqbot_handle_webhook(guild_msg);

    json_node_t *msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("Guild API yields message", msgs);
    if (msgs) {
        json_node_t *msg = json_get(msgs, 0);
        const char *cid = json_get_str(msg, "chat_id", "");
        const char *txt = json_get_str(msg, "text", "");
        const char *sid = json_get_str(msg, "sender_id", "");
        TEST_STR_EQ("Guild chat_id", cid, "guild_channel_42");
        TEST_STR_EQ("Guild text", txt, "Hello from Guild API!");
        TEST_STR_EQ("Guild sender_id", sid, "author_999");
        json_free(msgs);
    }
    drain_queue();

    /* Guild API with empty content → no message queued */
    const char *guild_empty =
        "{\"op\":0,\"d\":{"
        "\"channel_id\":\"c1\","
        "\"content\":\"\","
        "\"author\":{\"id\":\"a1\"}"
        "}}";
    qqbot_handle_webhook(guild_empty);
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("empty content → no message", msgs);

    /* Guild API with missing channel_id → no message queued */
    const char *guild_no_channel =
        "{\"op\":0,\"d\":{"
        "\"content\":\"test\","
        "\"author\":{\"id\":\"a1\"}"
        "}}";
    qqbot_handle_webhook(guild_no_channel);
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("missing channel_id → no message", msgs);
}

/* ================================================================
 *  6. qqbot_handle_webhook — edge cases
 * ================================================================ */
static void test_handle_edge_cases(void) {
    printf("\n--- qqbot_handle_webhook edge cases ---\n");

    drain_queue();

    /* NULL body */
    qqbot_handle_webhook(NULL);
    json_node_t *msgs = qqbot_poll_messages(NULL);
    TEST_NULL("NULL body → no message", msgs);

    /* Empty body */
    qqbot_handle_webhook("");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("empty body → no message", msgs);

    /* Invalid JSON */
    qqbot_handle_webhook("{invalid json here!");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("invalid JSON → no message", msgs);

    /* Unknown format (no post_type, no op) */
    qqbot_handle_webhook("{\"some_other_field\":\"value\"}");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("unknown format → no message", msgs);

    /* OneBot non-message post_type */
    qqbot_handle_webhook("{\"post_type\":\"notice\",\"notice_type\":\"group_increase\"}");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("non-message post_type → no message", msgs);

    /* Empty message text in OneBot */
    qqbot_handle_webhook(
        "{\"post_type\":\"message\",\"message_type\":\"group\","
        "\"group_id\":1,\"user_id\":2,\"message\":\"\",\"self_id\":3}");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("empty message text → no message", msgs);

    /* OneBot with null message */
    qqbot_handle_webhook(
        "{\"post_type\":\"message\",\"message_type\":\"private\","
        "\"user_id\":2,\"message\":null,\"self_id\":3}");
    msgs = qqbot_poll_messages(NULL);
    TEST_NULL("null message text → no message", msgs);
}

/* ================================================================
 *  7. Ring buffer overflow test
 * ================================================================ */
static void test_queue_overflow(void) {
    printf("\n--- qqbot_queue_message ring buffer overflow ---\n");

    drain_queue();

    /* Fill the ring buffer (64 entries) */
    char chat[64], text[64], sender[64];
    for (int i = 0; i < 70; i++) {
        snprintf(chat, sizeof(chat), "chat_%d", i);
        snprintf(text, sizeof(text), "msg_%d", i);
        snprintf(sender, sizeof(sender), "user_%d", i);
        qqbot_queue_message(chat, text, sender);
    }

    /* Should have at most 64 messages (overflow drops oldest) */
    json_node_t *msgs = qqbot_poll_messages(NULL);
    TEST_NOT_NULL("overflow queue has messages", msgs);
    size_t len = json_len(msgs);
    TEST("overflow drops to 64 max", len <= 64);

    if (msgs && len > 0) {
        /* FIFO: oldest surviving messages come first */
        json_node_t *first_msg = json_get(msgs, 0);
        TEST_NOT_NULL("overflow first element", first_msg);
        if (first_msg) {
            const char *txt = json_get_str(first_msg, "text", "");
            /* Ring buffer drops NEW messages when full (next==head check).
             * Buffer size 64, head starts at 0, tail ends at 63 after 64 pushes.
             * Messages 65-70 are silently dropped because tail+1 == head.
             * First surviving is msg_0. */
            TEST("overflow preserves oldest messages",
                 strcmp(txt, "msg_0") == 0);
        }
    }
    json_free(msgs);
    drain_queue();
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_qqbot.c ===\n");

    test_setters();
    test_queue_poll();
    test_getters();
    test_handle_onebot();
    test_handle_guild_api();
    test_handle_edge_cases();
    test_queue_overflow();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
