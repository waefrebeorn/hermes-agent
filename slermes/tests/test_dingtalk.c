/* test_dingtalk.c — Tests for DingTalk gateway platform (setup, queue, webhook, senders). */
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

/* Wrapper to reset queue state between test sections */
static void drain_queue(void) {
    json_node_t *m;
    do { m = dingtalk_poll_messages(NULL); if (m) json_free(m); } while (m);
}

int main(void) {
    printf("=== DingTalk Gateway Tests ===\n\n");

    /* Setup */
    printf("--- Setup ---\n");
    {
        dingtalk_set_webhook("https://oapi.dingtalk.com/robot/send?access_token=test123");
        dingtalk_set_app_credentials("app123", "secret456");
        TEST("setup no crash", 1);
    }

    /* Setup edge cases */
    printf("--- Setup Edge Cases ---\n");
    {
        dingtalk_set_webhook(NULL);
        TEST("set webhook NULL no crash", 1);
        dingtalk_set_webhook("");
        TEST("set webhook empty no crash", 1);
        dingtalk_set_app_credentials(NULL, NULL);
        TEST("set credentials both NULL no crash", 1);
        dingtalk_set_app_credentials("", "");
        TEST("set credentials both empty no crash", 1);
        /* Restore valid values */
        dingtalk_set_webhook("https://oapi.dingtalk.com/robot/send?access_token=test123");
        dingtalk_set_app_credentials("app123", "secret456");
        TEST("re-set valid values no crash", 1);
    }

    /* NULL / missing field extractors */
    printf("--- Extractor Edge Cases ---\n");
    {
        const char *cid = dingtalk_get_chat_id(NULL);
        TEST("get_chat_id(NULL) returns 'dingtalk' fallback", cid && strcmp(cid, "dingtalk") == 0);

        const char *txt = dingtalk_get_text(NULL);
        TEST("get_text(NULL) returns ''", txt && txt[0] == '\0');

        json_node_t *no_chat_id = json_new_object();
        json_set(no_chat_id, "text", json_string("hello"));
        cid = dingtalk_get_chat_id(no_chat_id);
        TEST("get_chat_id missing field returns 'dingtalk'", cid && strcmp(cid, "dingtalk") == 0);
        json_free(no_chat_id);

        json_node_t *no_text = json_new_object();
        json_set(no_text, "chat_id", json_string("chat_x"));
        txt = dingtalk_get_text(no_text);
        TEST("get_text missing field returns ''", txt && txt[0] == '\0');
        json_free(no_text);

        json_node_t *empty_obj = json_new_object();
        cid = dingtalk_get_chat_id(empty_obj);
        TEST("get_chat_id empty object returns 'dingtalk'", cid && strcmp(cid, "dingtalk") == 0);
        txt = dingtalk_get_text(empty_obj);
        TEST("get_text empty object returns ''", txt && txt[0] == '\0');
        json_free(empty_obj);
    }

    /* Queue and poll */
    printf("--- Queue / Poll ---\n");
    {
        drain_queue();
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("empty queue returns NULL", msgs == NULL);

        dingtalk_queue_message("chat_abc", "hello world", "user_42");
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

        msgs = dingtalk_poll_messages(NULL);
        TEST("queue drained returns NULL", msgs == NULL);
    }

    /* Queue with NULL/empty params */
    printf("--- Queue Edge Cases ---\n");
    {
        drain_queue();
        dingtalk_queue_message(NULL, NULL, NULL);
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("queue NULL params no crash", msgs != NULL);
        if (msgs) {
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *chat = dingtalk_get_chat_id(msg);
                const char *text = dingtalk_get_text(msg);
                TEST("NULL chat_id becomes 'dingtalk'", chat && strcmp(chat, "dingtalk") == 0);
                TEST("NULL text becomes ''", text && text[0] == '\0');
            }
            json_free(msgs);
        }

        drain_queue();
        dingtalk_queue_message("", "", "");
        msgs = dingtalk_poll_messages(NULL);
        TEST("empty params enqueues", msgs != NULL);
        if (msgs) {
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *chat = dingtalk_get_chat_id(msg);
                const char *text = dingtalk_get_text(msg);
                TEST("empty chat_id stored as ''", chat && chat[0] == '\0');
                TEST("empty text stored as ''", text && text[0] == '\0');
            }
            json_free(msgs);
        }
    }

    /* Queue overflow */
    printf("--- Queue Overflow ---\n");
    {
        drain_queue();
        char buf[128];
        for (int i = 0; i < 67; i++) {
            snprintf(buf, sizeof(buf), "msg_%d", i);
            dingtalk_queue_message(buf, buf, "overflow");
        }
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("overflow poll returns non-NULL", msgs != NULL);
        if (msgs) {
            TEST("overflow retains 63 messages (oldest dropped)", json_len(msgs) == 63);
            if (json_len(msgs) == 63) {
                json_node_t *first = json_get(msgs, 0);
                json_node_t *last = json_get(msgs, 62);
                if (first) {
                    const char *first_chat = dingtalk_get_chat_id(first);
                    TEST("overflow first is msg_0 (ring buffer: oldest preserved, newest dropped)", first_chat && strcmp(first_chat, "msg_0") == 0);
                }
                if (last) {
                    const char *last_chat = dingtalk_get_chat_id(last);
                    TEST("overflow last is msg_62 (63rd of 67 pushes fits; 64th onward dropped)", last_chat && strcmp(last_chat, "msg_62") == 0);
                }
            }
            json_free(msgs);
        }
    }

    /* Drain between sections */
    drain_queue();

    /* Unicode / special characters */
    printf("--- Unicode / Special Chars ---\n");
    {
        drain_queue();
        dingtalk_queue_message("chat_uni", "hello émöji 🎉 test", "user_uni");
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("unicode poll returns non-NULL", msgs != NULL);
        if (msgs) {
            TEST("unicode has 1 item", json_len(msgs) == 1);
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *text = dingtalk_get_text(msg);
                TEST("unicode text preserved", text && strstr(text, "🎉") != NULL);
            }
            json_free(msgs);
        }
    }

    /* Webhook handling */
    printf("--- Webhook Handler ---\n");
    {
        drain_queue();
        dingtalk_handle_webhook(NULL);
        TEST("NULL body no crash", 1);

        dingtalk_handle_webhook("");
        TEST("empty body no crash", 1);

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
        drain_queue();
        dingtalk_handle_webhook("{"
            "\"conversationId\":\"cid_def\","
            "\"senderId\":\"uid_789\","
            "\"msgtype\":\"image\","
            "\"image\":{\"url\":\"http://example.com/img.jpg\"}"
        "}");
        msgs = dingtalk_poll_messages(NULL);
        TEST("non-text webhook returns NULL (no text content)", msgs == NULL);

        /* Webhook with conversationTitle (group chat) */
        dingtalk_handle_webhook("{"
            "\"conversationId\":\"cid_group\","
            "\"senderId\":\"uid_group\","
            "\"msgtype\":\"text\","
            "\"text\":{\"content\":\"group message\"},"
            "\"conversationTitle\":\"Team Chat\""
        "}");
        msgs = dingtalk_poll_messages(NULL);
        TEST("group chat webhook returns non-NULL", msgs != NULL);
        if (msgs) {
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *text = dingtalk_get_text(msg);
                TEST("group chat text preserved", text && strcmp(text, "group message") == 0);
            }
            json_free(msgs);
        }

        /* Webhook with missing conversationId */
        drain_queue();
        dingtalk_handle_webhook("{"
            "\"senderId\":\"uid_nocid\","
            "\"msgtype\":\"text\","
            "\"text\":{\"content\":\"no cid\"}"
        "}");
        msgs = dingtalk_poll_messages(NULL);
        TEST("missing conversationId in webhook returns NULL (no chat_id)", msgs == NULL);
    }

    /* Multiple messages in queue */
    printf("--- Multiple Messages ---\n");
    {
        drain_queue();
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

    /* Long text boundary */
    printf("--- Long Text Boundary ---\n");
    {
        drain_queue();
        /* DINGTALK_QUEUE_MAX text field is 4096 bytes. Test with near-boundary text */
        char long_text[4096];
        memset(long_text, 'A', sizeof(long_text) - 1);
        long_text[sizeof(long_text) - 1] = '\0';
        dingtalk_queue_message("chat_long", long_text, "user_long");
        json_node_t *msgs = dingtalk_poll_messages(NULL);
        TEST("long text poll returns non-NULL", msgs != NULL);
        if (msgs) {
            json_node_t *msg = json_get(msgs, 0);
            if (msg) {
                const char *text = dingtalk_get_text(msg);
                TEST("long text stored (truncated to 4095)", text && strlen(text) == 4095);
            }
            json_free(msgs);
        }
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
