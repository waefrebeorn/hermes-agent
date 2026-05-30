/* test_dingtalk.c — Tests for DingTalk gateway platform (setup, queue, webhook). */
#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

/* Extern functions from dingtalk.c */
extern void dingtalk_set_webhook(const char *url);
extern void dingtalk_set_app_credentials(const char *app_id, const char *app_secret);
extern void dingtalk_queue_message(const char *chat_id, const char *text,
                                    const char *sender_id);
extern void dingtalk_handle_webhook(const char *body);
extern json_node_t *dingtalk_poll_messages(http_client_t *http);
extern const char *dingtalk_get_chat_id(json_node_t *update);
extern const char *dingtalk_get_text(json_node_t *update);
extern bool dingtalk_send_message(http_client_t *http, const char *text);

int main(void) {
    printf("=== DingTalk Gateway Tests ===\n\n");

    /* Setup */
    printf("--- Setup ---\n");
    {
        dingtalk_set_webhook("https://oapi.dingtalk.com/robot/send?access_token=test123");
        dingtalk_set_app_credentials("app123", "secret456");
        /* No return value to test, just ensure no crash */
        TEST("setup no crash", 1);
    }

    /* Queue and poll */
    printf("--- Queue / Poll ---\n");
    {
        /* Empty queue should return NULL */
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("empty queue returns NULL", msgs == NULL);

        /* Push a message */
        dingtalk_queue_message("chat_abc", "hello world", "user_42");

        /* Poll should return one message */
        msgs = dingtalk_poll_messages(NULL);
        TEST("poll returns non-NULL", msgs != NULL);
        if (msgs) {
            TEST("poll has 1 item", json_len(msgs) == 1);
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *chat = dingtalk_get_chat_id(msg);
                const char *text = dingtalk_get_text(msg);
                TEST("chat_id matches", chat && strcmp(chat, "chat_abc") == 0);
                TEST("text matches", text && strcmp(text, "hello world") == 0);
            }
            json_free(msgs);
        }

        /* Poll again should return NULL (queue drained) */
        msgs = dingtalk_poll_messages(NULL);
        TEST("queue drained returns NULL", msgs == NULL);
    }

    /* Webhook handling */
    printf("--- Webhook Handler ---\n");
    {
        /* NULL body */
        dingtalk_handle_webhook(NULL);
        TEST("NULL body no crash", 1);

        /* Invalid JSON */
        dingtalk_handle_webhook("not json");
        TEST("invalid json no crash", 1);

        /* Valid DingTalk callback format */
        dingtalk_handle_webhook("{"
            "\"conversationId\":\"cid_abc123\","
            "\"senderId\":\"uid_456\","
            "\"msgtype\":\"text\","
            "\"text\":{\"content\":\"test message\"}"
        "}");

        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("webhook poll returns non-NULL", msgs != NULL);
        if (msgs) {
            TEST("webhook has 1 item", json_len(msgs) == 1);
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *chat = dingtalk_get_chat_id(msg);
                const char *text = dingtalk_get_text(msg);
                TEST("webhook chat_id matches", chat && strcmp(chat, "cid_abc123") == 0);
                TEST("webhook text matches", text && strcmp(text, "test message") == 0);
            }
            json_free(msgs);
        }

        /* Non-text message type */
        dingtalk_handle_webhook("{"
            "\"conversationId\":\"cid_def\","
            "\"senderId\":\"uid_789\","
            "\"msgtype\":\"image\","
            "\"image\":{\"url\":\"http://example.com/img.jpg\"}"
        "}");
        msgs = dingtalk_poll_messages(NULL);
        TEST("non-text webhook returns NULL (no text content)", msgs == NULL);
    }

    /* Multiple messages in queue */
    printf("--- Multiple Messages ---\n");
    {
        dingtalk_queue_message("chat_1", "msg1", "user_1");
        dingtalk_queue_message("chat_2", "msg2", "user_2");
        dingtalk_queue_message("chat_3", "msg3", "user_3");

        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("multiple poll returns non-NULL", msgs != NULL);
        if (msgs) {
            TEST("multiple has 3 items", json_len(msgs) == 3);
            json_free(msgs);
        }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
