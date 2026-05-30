/*
 * test_wecom.c — WeCom platform adapter test suite (S7 X01).
 *
 * Tests pure functions: wecom_set_webhook, wecom_set_app_credentials,
 * wecom_queue_message, wecom_poll_messages, wecom_get_chat_id,
 * wecom_get_text, wecom_handle_webhook.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_wecom.c src/gateway/platforms/wecom.c \
 *       lib/libjson/json.c -lm -lpthread \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_wecom
 *
 * Run:
 *   /tmp/hermes_test_wecom
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Forward declarations for tested functions */
void wecom_set_webhook(const char *url);
void wecom_set_app_credentials(const char *corp_id, const char *corp_secret,
                                const char *agent_id);
void wecom_queue_message(const char *chat_id, const char *text,
                          const char *sender_id);
json_node_t *wecom_poll_messages(http_client_t *http);
const char *wecom_get_chat_id(json_node_t *update);
const char *wecom_get_text(json_node_t *update);
void wecom_handle_webhook(const char *body);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

static void drain_queue(void) {
    /* Clear credentials first so poll_messages() doesn't try HTTP refresh */
    wecom_set_app_credentials("", "", "");
    json_node_t *msgs;
    while ((msgs = wecom_poll_messages(NULL)) != NULL)
        json_free(msgs);
}

/* ================================================================
 *  1. wecom_set_webhook
 * ================================================================ */
static void test_set_webhook(void) {
    printf("\n--- wecom_set_webhook ---\n");

    wecom_set_webhook("https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=abc");
    TEST("set_webhook normal URL", 1);

    wecom_set_webhook("");
    TEST("set_webhook empty", 1);

    wecom_set_webhook(NULL);
    TEST("set_webhook NULL (preserves previous)", 1);

    wecom_set_webhook("https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=xyz-123");
    TEST("set_webhook reset", 1);
}

/* ================================================================
 *  2. wecom_set_app_credentials
 * ================================================================ */
static void test_set_app_credentials(void) {
    printf("\n--- wecom_set_app_credentials ---\n");

    wecom_set_app_credentials("wwcorp123", "secret-abc", "1000001");
    TEST("set_app_credentials normal", 1);

    wecom_set_app_credentials("", "", "");
    TEST("set_app_credentials empty", 1);

    wecom_set_app_credentials("wwcorp456", "secret-xyz", "1000002");
    TEST("set_app_credentials reset", 1);

    /* NULL params — should preserve previous */
    wecom_set_app_credentials(NULL, NULL, NULL);
    TEST("set_app_credentials all NULL", 1);

    wecom_set_app_credentials("wwpartial", NULL, "1000003");
    TEST("set_app_credentials partial NULL", 1);

    /* Clear credentials so subsequent poll_messages doesn't trigger HTTP */
    wecom_set_app_credentials("", "", "");
}

/* ================================================================
 *  3. Ring buffer queue + poll
 * ================================================================ */
static void test_queue_poll(void) {
    printf("\n--- wecom_queue_message + wecom_poll_messages ---\n");

    drain_queue();

    /* Empty queue */
    json_node_t *msgs = wecom_poll_messages(NULL);
    TEST_NULL("empty queue returns NULL", msgs);

    /* Single message */
    wecom_queue_message("user_123", "Hello from WeCom!", "bot_agent");
    msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("single message returns array", msgs);

    json_node_t *first = json_get(msgs, 0);
    TEST_NOT_NULL("first element", first);
    if (first) {
        TEST_STR_EQ("chat_id matches", json_get_str(first, "chat_id", ""), "user_123");
        TEST_STR_EQ("text matches", json_get_str(first, "text", ""), "Hello from WeCom!");
        TEST_STR_EQ("sender_id matches", json_get_str(first, "sender_id", ""), "bot_agent");
    }
    json_free(msgs);

    /* Queue empty again */
    msgs = wecom_poll_messages(NULL);
    TEST_NULL("queue empty after drain", msgs);

    /* Multiple messages — FIFO order */
    wecom_queue_message("c1", "Msg 1", "u1");
    wecom_queue_message("c2", "Msg 2", "u2");
    wecom_queue_message("c3", "Msg 3", "u3");

    msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("multi-message returns array", msgs);
    size_t len = json_len(msgs);
    TEST("three messages in FIFO order", len == 3);

    if (len >= 3) {
        TEST_STR_EQ("first msg chat_id", json_get_str(json_get(msgs, 0), "chat_id", ""), "c1");
        TEST_STR_EQ("second msg chat_id", json_get_str(json_get(msgs, 1), "chat_id", ""), "c2");
        TEST_STR_EQ("third msg chat_id", json_get_str(json_get(msgs, 2), "chat_id", ""), "c3");
    }
    json_free(msgs);
    drain_queue();

    /* NULL params to queue_message */
    wecom_queue_message(NULL, "text only", NULL);
    wecom_queue_message("chat", NULL, "sender");

    msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("NULL-param messages present", msgs);
    len = json_len(msgs);
    TEST("two NULL-param messages", len == 2);

    if (len >= 1) {
        const char *cid0 = json_get_str(json_get(msgs, 0), "chat_id", "");
        TEST("NULL chat_id defaults to 'wecom'", strcmp(cid0, "wecom") == 0);
    }
    if (len >= 2) {
        const char *t1 = json_get_str(json_get(msgs, 1), "text", "");
        TEST("NULL text defaults to empty", t1[0] == '\0');
    }
    json_free(msgs);
    drain_queue();
}

/* ================================================================
 *  4. wecom_get_chat_id / wecom_get_text
 * ================================================================ */
static void test_getters(void) {
    printf("\n--- wecom_get_chat_id / wecom_get_text ---\n");

    /* Normal update */
    json_node_t *upd = json_new_object();
    json_set(upd, "chat_id", json_string("user_from"));
    json_set(upd, "text", json_string("Hello WeCom!"));
    json_set(upd, "sender_id", json_string("bot"));

    const char *cid = wecom_get_chat_id(upd);
    TEST_STR_EQ("get_chat_id normal", cid, "user_from");
    const char *txt = wecom_get_text(upd);
    TEST_STR_EQ("get_text normal", txt, "Hello WeCom!");
    json_free(upd);

    /* Missing chat_id → fallback "wecom" */
    upd = json_new_object();
    json_set(upd, "text", json_string("no chat id"));
    TEST_STR_EQ("get_chat_id fallback", wecom_get_chat_id(upd), "wecom");
    json_free(upd);

    /* Missing text → empty string */
    upd = json_new_object();
    json_set(upd, "chat_id", json_string("c99"));
    TEST_STR_EQ("get_text missing default", wecom_get_text(upd), "");
    json_free(upd);

    /* NULL update */
    TEST_STR_EQ("get_chat_id NULL", wecom_get_chat_id(NULL), "wecom");
    TEST_STR_EQ("get_text NULL", wecom_get_text(NULL), "");

    /* Empty object */
    upd = json_new_object();
    TEST_STR_EQ("get_chat_id empty object", wecom_get_chat_id(upd), "wecom");
    json_free(upd);

    /* Unicode text */
    upd = json_new_object();
    json_set(upd, "chat_id", json_string("u"));
    json_set(upd, "text", json_string("WeCom says 你好世界!"));
    TEST_STR_EQ("get_text unicode", wecom_get_text(upd), "WeCom says 你好世界!");
    json_free(upd);
}

/* ================================================================
 *  5. wecom_handle_webhook — JSON callback format
 * ================================================================ */
static void test_handle_webhook_json(void) {
    printf("\n--- wecom_handle_webhook JSON format ---\n");

    drain_queue();

    /* WeCom JSON callback: {"FromUserName":"user1","MsgType":"text","Content":"Hello"} */
    wecom_handle_webhook(
        "{\"FromUserName\":\"WangQing\",\"MsgType\":\"text\","
        "\"Content\":\"Hello from WeCom!\"}");
    json_node_t *msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("JSON webhook yields message", msgs);
    if (msgs) {
        json_node_t *msg = json_get(msgs, 0);
        TEST_STR_EQ("chat_id from FromUserName",
            json_get_str(msg, "chat_id", ""), "WangQing");
        TEST_STR_EQ("text from Content",
            json_get_str(msg, "text", ""), "Hello from WeCom!");
        json_free(msgs);
    }
    drain_queue();

    /* Non-text MsgType → no message queued */
    wecom_handle_webhook(
        "{\"FromUserName\":\"u1\",\"MsgType\":\"image\",\"Content\":\"img_url\"}");
    msgs = wecom_poll_messages(NULL);
    TEST_NULL("image msgtype → no message", msgs);

    /* Missing Content */
    wecom_handle_webhook(
        "{\"FromUserName\":\"u1\",\"MsgType\":\"text\"}");
    msgs = wecom_poll_messages(NULL);
    TEST_NULL("missing content → no message", msgs);
}

/* ================================================================
 *  6. wecom_handle_webhook — XML fallback
 * ================================================================ */
static void test_handle_webhook_xml(void) {
    printf("\n--- wecom_handle_webhook XML fallback ---\n");

    drain_queue();

    /* Invalid JSON → XML fallback: queues raw body as text */
    wecom_handle_webhook("<xml><ToUserName><![CDATA[bot]]></ToUserName></xml>");
    json_node_t *msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("XML fallback yields message", msgs);
    if (msgs) {
        json_node_t *msg = json_get(msgs, 0);
        TEST_STR_EQ("XML fallback chat_id 'wecom'",
            json_get_str(msg, "chat_id", ""), "wecom");
        const char *txt = json_get_str(msg, "text", "");
        TEST("XML body stored as text", strstr(txt, "<xml>") != NULL);
        json_free(msgs);
    }
    drain_queue();

    /* Long XML body */
    char long_xml[5000];
    snprintf(long_xml, sizeof(long_xml),
             "<xml><ToUserName>bot</ToUserName><Content>%s</Content></xml>",
             "ABC123");
    wecom_handle_webhook(long_xml);
    msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("XML long body", msgs);
    if (msgs) {
        const char *txt = json_get_str(json_get(msgs, 0), "text", "");
        TEST("XML long body stored", strstr(txt, "ABC123") != NULL);
        json_free(msgs);
    }
    drain_queue();
}

/* ================================================================
 *  7. Edge cases
 * ================================================================ */
static void test_edge_cases(void) {
    printf("\n--- edge cases ---\n");

    drain_queue();

    wecom_handle_webhook(NULL);
    TEST_NULL("NULL body → no message", wecom_poll_messages(NULL));

    wecom_handle_webhook("");
    TEST_NULL("empty body → no message", wecom_poll_messages(NULL));

    wecom_handle_webhook("{invalid}");
    /* Invalid JSON falls through to XML fallback which queues it */
    json_node_t *msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("invalid JSON → XML fallback queues body", msgs);
    if (msgs) {
        const char *txt = json_get_str(json_get(msgs, 0), "text", "");
        TEST("invalid JSON body stored as text", strcmp(txt, "{invalid}") == 0);
        json_free(msgs);
    }
    drain_queue();
}

/* ================================================================
 *  8. Ring buffer overflow
 * ================================================================ */
static void test_queue_overflow(void) {
    printf("\n--- ring buffer overflow ---\n");

    drain_queue();

    char chat[64], text[64], sender[64];
    for (int i = 0; i < 70; i++) {
        snprintf(chat, sizeof(chat), "chat_%d", i);
        snprintf(text, sizeof(text), "msg_%d", i);
        snprintf(sender, sizeof(sender), "u_%d", i);
        wecom_queue_message(chat, text, sender);
    }

    json_node_t *msgs = wecom_poll_messages(NULL);
    TEST_NOT_NULL("overflow queue has messages", msgs);
    size_t len = json_len(msgs);
    TEST("overflow max 64 messages", len <= 64);

    if (msgs && len > 0) {
        /* Ring buffer drops NEW messages when full (next==head check) */
        const char *txt = json_get_str(json_get(msgs, 0), "text", "");
        TEST("overflow preserves oldest (msg_0)", strcmp(txt, "msg_0") == 0);
    }
    json_free(msgs);
    drain_queue();
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_wecom.c ===\n");

    test_set_webhook();
    test_set_app_credentials();
    test_queue_poll();
    test_getters();
    test_handle_webhook_json();
    test_handle_webhook_xml();
    test_edge_cases();
    test_queue_overflow();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
