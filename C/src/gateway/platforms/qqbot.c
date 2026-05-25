/*
 * qqbot.c — QQ Bot gateway for Hermes C.
 * P113: Full feature parity — markdown, images, keyboard buttons, at-mention.
 *
 * Env vars:
 *   QQ_BOT_WEBHOOK_URL   — QQ Bot webhook URL (for outgoing messages)
 *   QQ_BOT_TOKEN         — QQ Bot auth token
 *   QQ_BOT_APP_ID        — QQ Bot app ID (for sandbox API)
 *   QQ_BOT_API_BASE      — QQ Bot API base URL (default: https://api.sgroup.qq.com)
 *
 * QQ Bot API Docs:
 *   https://bot.q.qq.com/wiki/develop/api/
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_qqbot_webhook[1024] = "";
static char g_qqbot_token[256] = "";
static char g_qqbot_api_base[512] = "https://api.sgroup.qq.com";

void qqbot_set_webhook(const char *url) {
    if (url) snprintf(g_qqbot_webhook, sizeof(g_qqbot_webhook), "%s", url);
}

void qqbot_set_token(const char *token) {
    if (token) snprintf(g_qqbot_token, sizeof(g_qqbot_token), "%s", token);
}

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Internal: build auth headers */
static void build_headers(char *buf, size_t sz) {
    if (g_qqbot_token[0]) {
        snprintf(buf, sz,
                 "Authorization: Bot %s\r\n"
                 "Content-Type: application/json\r\n"
                 "X-Requested-With: XMLHttpRequest\r\n",
                 g_qqbot_token);
    } else {
        snprintf(buf, sz,
                 "Content-Type: application/json\r\n"
                 "X-Requested-With: XMLHttpRequest\r\n");
    }
}

/* Internal: POST JSON to webhook URL */
static bool post_webhook(http_client_t *http, json_node_t *root) {
    if (!g_qqbot_webhook[0]) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[512];
    build_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, g_qqbot_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* Internal: POST to QQ Bot API endpoint (reserved for future API mode) */
static bool __attribute__((unused)) post_api(const char *endpoint, json_node_t *root) {
    (void)endpoint; (void)root;
    if (!g_qqbot_token[0]) return false;

    http_client_t *http = http_client_new(10);
    if (!http) { json_free(root); return false; }

    char url[1024];
    snprintf(url, sizeof(url), "%s%s", g_qqbot_api_base, endpoint);

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { http_client_free(http); return false; }

    char headers[512];
    build_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    http_client_free(http);
    return ok;
}

/* ================================================================
 *  P113: Send plain text message
 * ================================================================ */
bool qqbot_send_message(http_client_t *http, const char *text) {
    if (!text) return false;

    /* QQ Bot uses "content" field for text messages */
    json_node_t *root = json_new_object();
    json_set(root, "content", json_string(text));
    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send markdown message
 *  Uses QQ Bot markdown message type: msg_type=2, markdown content
 * ================================================================ */
bool qqbot_send_markdown(http_client_t *http, const char *text) {
    if (!text) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_number(2));
    json_node_t *markdown = json_new_object();
    json_set(markdown, "content", json_string(text));
    json_set(root, "markdown", markdown);

    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send image message
 *  Uses QQ Bot image: msg_type=3, image with URL
 * ================================================================ */
bool qqbot_send_image(http_client_t *http, const char *image_url) {
    if (!image_url) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_number(3));
    json_node_t *image = json_new_object();
    json_set(image, "url", json_string(image_url));
    json_set(root, "image", image);

    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send message with keyboard (interactive buttons)
 *  keyboard_json: JSON object with "rows" array, e.g.
 *  {"rows":[{"buttons":[{"id":"1","label":"Click me","action":0}]}]}
 * ================================================================ */
bool qqbot_send_with_keyboard(http_client_t *http, const char *text,
                               const char *keyboard_json) {
    if (!text) return false;

    json_node_t *root = json_new_object();
    json_set(root, "content", json_string(text));

    if (keyboard_json && *keyboard_json) {
        json_node_t *kb = json_parse(keyboard_json, NULL);
        if (kb) json_set(root, "keyboard", kb);
    }

    return post_webhook(http, root);
}

/* ================================================================
 *  P113: Send message with @-mention
 *  at_user_id: QQ user ID to @mention (use "@everyone" for all)
 * ================================================================ */
bool qqbot_send_with_at(http_client_t *http, const char *text,
                         const char *at_user_id) {
    if (!text) return false;

    json_node_t *root = json_new_object();
    /* Construct message with mention */
    char with_at[4096];
    if (at_user_id && *at_user_id) {
        if (strcmp(at_user_id, "@everyone") == 0) {
            snprintf(with_at, sizeof(with_at), "@everyone %s", text);
        } else {
            snprintf(with_at, sizeof(with_at), "<@%s> %s", at_user_id, text);
        }
    } else {
        snprintf(with_at, sizeof(with_at), "%s", text);
    }

    json_set(root, "content", json_string(with_at));

    json_node_t *mention = json_new_object();
    if (at_user_id && *at_user_id) {
        if (strcmp(at_user_id, "@everyone") == 0) {
            json_node_t *all = json_new_object();
            json_set(all, "id", json_string("all"));
            json_set(mention, "everyone", json_bool(true));
        } else {
            json_set(mention, "user_id", json_string(at_user_id));
        }
    }
    json_set(root, "mention", mention);

    return post_webhook(http, root);
}

/* ================================================================
 *  Webhook message queue — stores messages received via HTTP callback
 * ================================================================ */

#define QQ_QUEUE_MAX 64

typedef struct {
    char chat_id[128];
    char text[4096];
    char sender_id[128];
    time_t timestamp;
} qq_msg_t;

static qq_msg_t g_qq_queue[QQ_QUEUE_MAX];
static int g_qq_head = 0;
static int g_qq_tail = 0;
static pthread_mutex_t g_qq_lock = PTHREAD_MUTEX_INITIALIZER;

void qqbot_queue_message(const char *chat_id, const char *text,
                          const char *sender_id) {
    pthread_mutex_lock(&g_qq_lock);
    int next = (g_qq_tail + 1) % QQ_QUEUE_MAX;
    if (next != g_qq_head) {
        snprintf(g_qq_queue[g_qq_tail].chat_id, sizeof(g_qq_queue[g_qq_tail].chat_id), "%s", chat_id ? chat_id : "qqbot");
        snprintf(g_qq_queue[g_qq_tail].text, sizeof(g_qq_queue[g_qq_tail].text), "%s", text ? text : "");
        snprintf(g_qq_queue[g_qq_tail].sender_id, sizeof(g_qq_queue[g_qq_tail].sender_id), "%s", sender_id ? sender_id : "");
        g_qq_queue[g_qq_tail].timestamp = time(NULL);
        g_qq_tail = next;
    }
    pthread_mutex_unlock(&g_qq_lock);
}

/* Handle incoming QQ Bot webhook POST.
 * Supports both older OneBot (post_type/message_type) and
 * newer QQ Guild API (op/d) formats. */
void qqbot_handle_webhook(const char *body) {
    if (!body || !body[0]) return;

    char *err = NULL;
    json_t *root = json_parse(body, &err);
    if (!root) { free(err); return; }

    /* Check for OneBot format: { "post_type": "message", ... } */
    const char *post_type = json_get_str(root, "post_type", "");
    if (strcmp(post_type, "message") == 0) {
        const char *msg_type = json_get_str(root, "message_type", "");
        const char *text = json_get_str(root, "message", "");
        char chat_id[256];
        if (strcmp(msg_type, "group") == 0) {
            /* Group message: use group_id as chat_id */
            long gid = (long)json_get_num(root, "group_id", 0);
            snprintf(chat_id, sizeof(chat_id), "group_%ld", gid);
        } else {
            /* Private message: use user_id */
            long uid = (long)json_get_num(root, "user_id", 0);
            snprintf(chat_id, sizeof(chat_id), "user_%ld", uid);
        }
        if (text && text[0]) {
            qqbot_queue_message(chat_id, text, "");
        }
        json_free(root);
        return;
    }

    /* Check for QQ Guild API format: { "op": 0, "d": { ... } }
     * where d has channel_id, author, content */
    int op = (int)json_get_num(root, "op", -1);
    if (op == 0) {
        json_t *d = json_obj_get(root, "d");
        if (d) {
            const char *content = json_get_str(d, "content", "");
            const char *channel_id = json_get_str(d, "channel_id", "");
            json_t *author = json_obj_get(d, "author");
            const char *author_id = author ? json_get_str(author, "id", "") : "";
            if (channel_id[0] && content[0]) {
                qqbot_queue_message(channel_id, content, author_id);
            }
        }
    }

    json_free(root);
}

json_node_t *qqbot_poll_messages(http_client_t *http) {
    (void)http;

    pthread_mutex_lock(&g_qq_lock);
    if (g_qq_head == g_qq_tail) {
        pthread_mutex_unlock(&g_qq_lock);
        return NULL;
    }

    json_node_t *results = json_array();
    while (g_qq_head != g_qq_tail) {
        json_node_t *msg = json_new_object();
        json_set(msg, "chat_id", json_string(g_qq_queue[g_qq_head].chat_id));
        json_set(msg, "text", json_string(g_qq_queue[g_qq_head].text));
        json_set(msg, "sender_id", json_string(g_qq_queue[g_qq_head].sender_id));
        json_append(results, msg);
        g_qq_head = (g_qq_head + 1) % QQ_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_qq_lock);
    return results;
}

const char *qqbot_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "qqbot");
}

const char *qqbot_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
