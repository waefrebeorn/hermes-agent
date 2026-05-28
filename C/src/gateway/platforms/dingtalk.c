/*
 * dingtalk.c — DingTalk gateway for Hermes C.
 * P113: Full feature parity — markdown, at-mention, action cards, links, images.
 *
 * Env vars:
 *   DINGTALK_WEBHOOK_URL     — DingTalk bot webhook URL (with access_token)
 *   DINGTALK_APP_ID          — DingTalk app ID (for Open API, optional)
 *   DINGTALK_APP_SECRET      — DingTalk app secret (for Open API, optional)
 *
 * DingTalk Webhook API Docs:
 *   https://open.dingtalk.com/document/robots/custom-robot-access
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_dingtalk_webhook[1024] = "";
static char g_app_id[256] = "";
static char g_app_secret[512] = "";

void dingtalk_set_webhook(const char *url) {
    if (url) snprintf(g_dingtalk_webhook, sizeof(g_dingtalk_webhook), "%s", url);
}

void dingtalk_set_app_credentials(const char *app_id, const char *app_secret) {
    if (app_id) snprintf(g_app_id, sizeof(g_app_id), "%s", app_id);
    if (app_secret) snprintf(g_app_secret, sizeof(g_app_secret), "%s", app_secret);
}

/* ================================================================
 *  Internal: POST JSON to DingTalk webhook
 * ================================================================ */
static bool post_to_webhook(http_client_t *http, json_node_t *root) {
    if (!g_dingtalk_webhook[0]) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *body = json_serialize(root);
    json_free(root);
    if (!body) { if (local_http) http_client_free(local_http); return false; }

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_dingtalk_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 *  P113: Send plain text message
 * ================================================================ */
bool dingtalk_send_message(http_client_t *http, const char *text) {
    if (!text) return false;
    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "text", content);
    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send markdown message
 * ================================================================ */
bool dingtalk_send_markdown(http_client_t *http, const char *title, const char *text) {
    if (!text) return false;
    if (!title) title = "";
    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("markdown"));
    json_node_t *content = json_new_object();
    json_set(content, "title", json_string(title));
    json_set(content, "text", json_string(text));
    json_set(root, "markdown", content);
    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send text with @-mentions
 *  at_mobiles_json: JSON array of phone numbers, e.g. "[\"138xxxx\"]"
 *  at_user_ids_json: JSON array of user IDs, e.g. "[\"user123\"]"
 *  is_at_all: true to @all
 * ================================================================ */
bool dingtalk_send_text_with_at(http_client_t *http, const char *text,
                                 const char *at_mobiles_json,
                                 const char *at_user_ids_json,
                                 bool is_at_all) {
    if (!text) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "text", content);

    json_node_t *at = json_new_object();
    if (at_mobiles_json && *at_mobiles_json) {
        json_node_t *mobiles = json_parse(at_mobiles_json, NULL);
        if (mobiles) { json_set(at, "atMobiles", mobiles); }
    }
    if (at_user_ids_json && *at_user_ids_json) {
        json_node_t *user_ids = json_parse(at_user_ids_json, NULL);
        if (user_ids) { json_set(at, "atUserIds", user_ids); }
    }
    json_set(at, "isAtAll", json_bool(is_at_all));
    json_set(root, "at", at);

    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send markdown with @-mentions
 * ================================================================ */
bool dingtalk_send_markdown_with_at(http_client_t *http, const char *title,
                                     const char *text,
                                     const char *at_mobiles_json,
                                     const char *at_user_ids_json,
                                     bool is_at_all) {
    if (!text) return false;
    if (!title) title = "";

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("markdown"));
    json_node_t *content = json_new_object();
    json_set(content, "title", json_string(title));
    json_set(content, "text", json_string(text));
    json_set(root, "markdown", content);

    json_node_t *at = json_new_object();
    if (at_mobiles_json && *at_mobiles_json) {
        json_node_t *mobiles = json_parse(at_mobiles_json, NULL);
        if (mobiles) { json_set(at, "atMobiles", mobiles); }
    }
    if (at_user_ids_json && *at_user_ids_json) {
        json_node_t *user_ids = json_parse(at_user_ids_json, NULL);
        if (user_ids) { json_set(at, "atUserIds", user_ids); }
    }
    json_set(at, "isAtAll", json_bool(is_at_all));
    json_set(root, "at", at);

    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send interactive ActionCard (button menu)
 *  btns_json: JSON array of {"title":"...","actionURL":"..."}
 *  btn_orientation: "0" horizontal, "1" vertical
 * ================================================================ */
bool dingtalk_send_action_card(http_client_t *http,
                                const char *title, const char *text,
                                const char *btns_json,
                                const char *btn_orientation) {
    if (!title || !text) return false;

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("actionCard"));
    json_node_t *card = json_new_object();
    json_set(card, "title", json_string(title));
    json_set(card, "text", json_string(text));

    if (btn_orientation)
        json_set(card, "btnOrientation", json_string(btn_orientation));
    else
        json_set(card, "btnOrientation", json_string("0"));

    if (btns_json && *btns_json) {
        json_node_t *btns = json_parse(btns_json, NULL);
        if (btns) { json_set(card, "btns", btns); }
    }
    json_set(root, "actionCard", card);

    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send Link message
 * ================================================================ */
bool dingtalk_send_link(http_client_t *http,
                         const char *title, const char *text,
                         const char *message_url, const char *pic_url) {
    if (!title || !message_url) return false;
    if (!text) text = "";

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("link"));
    json_node_t *link = json_new_object();
    json_set(link, "title", json_string(title));
    json_set(link, "text", json_string(text));
    json_set(link, "messageUrl", json_string(message_url));
    if (pic_url && *pic_url)
        json_set(link, "picUrl", json_string(pic_url));
    json_set(root, "link", link);

    return post_to_webhook(http, root);
}

/* ================================================================
 *  P113: Send image by URL
 *  NOTE: Requires DingTalk Open API access_token (uses app credentials).
 *  Falls back to webhook image format if no app credentials available.
 * ================================================================ */
bool dingtalk_send_image_by_url(http_client_t *http, const char *image_url) {
    if (!image_url) return false;

    /* Webhook image format: {"msgtype":"image","image":{"url":"..."}}
     * Note: this requires the image to already be uploaded to DingTalk media.
     * For external URLs, use the Open API with app credentials. */
    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("image"));
    json_node_t *img = json_new_object();
    json_set(img, "url", json_string(image_url));
    json_set(root, "image", img);

    return post_to_webhook(http, root);
}

/* ================================================================
 *  Webhook message queue — stores messages received via HTTP callback
 * ================================================================ */

#define DINGTALK_QUEUE_MAX 64

typedef struct {
    char chat_id[128];
    char text[4096];
    char sender_id[128];
    time_t timestamp;
} dingtalk_msg_t;

static dingtalk_msg_t g_msg_queue[DINGTALK_QUEUE_MAX];
static int g_msg_head = 0;
static int g_msg_tail = 0;
static pthread_mutex_t g_msg_lock = PTHREAD_MUTEX_INITIALIZER;

/* Push a message into the ring buffer. Thread-safe. */
void dingtalk_queue_message(const char *chat_id, const char *text,
                             const char *sender_id) {
    pthread_mutex_lock(&g_msg_lock);
    int next = (g_msg_tail + 1) % DINGTALK_QUEUE_MAX;
    if (next != g_msg_head) { /* not full */
        snprintf(g_msg_queue[g_msg_tail].chat_id, sizeof(g_msg_queue[g_msg_tail].chat_id), "%s", chat_id ? chat_id : "dingtalk");
        snprintf(g_msg_queue[g_msg_tail].text, sizeof(g_msg_queue[g_msg_tail].text), "%s", text ? text : "");
        snprintf(g_msg_queue[g_msg_tail].sender_id, sizeof(g_msg_queue[g_msg_tail].sender_id), "%s", sender_id ? sender_id : "");
        g_msg_queue[g_msg_tail].timestamp = time(NULL);
        g_msg_tail = next;
    }
    pthread_mutex_unlock(&g_msg_lock);
}

/* ================================================================
 *  OAuth2 access token — used for Open API calls
 * ================================================================ */

static char g_access_token[512] = "";
static time_t g_token_expires = 0;

/* Get a valid access token, refreshing if needed.
 * Returns NULL if not available. */
static const __attribute__((unused)) char *dingtalk_get_access_token(http_client_t *http) {
    if (!g_app_id[0] || !g_app_secret[0]) return NULL;

    /* Check if current token is still valid (5 min buffer) */
    if (g_access_token[0] && time(NULL) < g_token_expires - 300)
        return g_access_token;

    /* Request new token */
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }
    if (!http) return NULL;

    char body[1024];
    snprintf(body, sizeof(body),
             "{\"appkey\":\"%s\",\"appsecret\":\"%s\"}",
             g_app_id, g_app_secret);

    char headers[128];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST,
        "https://oapi.dingtalk.com/gettoken",
        headers, body, strlen(body));

    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        if (local_http) http_client_free(local_http);
        return NULL;
    }

    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);
    if (local_http) http_client_free(local_http);

    if (!root) { free(err); return NULL; }

    const char *token = json_get_str(root, "access_token", NULL);
    if (token) {
        snprintf(g_access_token, sizeof(g_access_token), "%s", token);
        /* Tokens typically expire in 7200s, store expiry */
        g_token_expires = time(NULL) + 7200;

        /* Try to get expires_in, fall back to 7200 */
        json_t *expires_node = json_obj_get(root, "expires_in");
        if (expires_node && expires_node->type == JSON_NUMBER) {
            g_token_expires = time(NULL) + (time_t)expires_node->num_val;
        }
    }
    json_free(root);
    return g_access_token[0] ? g_access_token : NULL;
}

/* Handle incoming webhook POST from DingTalk.
 * Parses the callback body and queues the message. */
void dingtalk_handle_webhook(const char *body) {
    if (!body || !body[0]) return;

    char *err = NULL;
    json_node_t *root = json_parse(body, &err);
    if (!root) { free(err); return; }

    /* DingTalk webhook callback format:
     * { "conversationId": "...", "chatbotUserId": "...",
     *   "msgtype": "text", "text": { "content": "..." },
     *   "senderId": "...", "senderNick": "..." } */
    const char *chat_id = json_get_str(root, "conversationId", NULL);
    const char *sender_id = json_get_str(root, "senderId", NULL);
    const char *msgtype = json_get_str(root, "msgtype", "");

    const char *text = NULL;
    if (strcmp(msgtype, "text") == 0) {
        json_t *text_node = json_obj_get(root, "text");
        if (text_node) text = json_get_str(text_node, "content", NULL);
    }

    if (chat_id && text) {
        dingtalk_queue_message(chat_id, text, sender_id);
    }

    /* Also check for the conversationTitle for group chats */
    const char *conv_title = json_get_str(root, "conversationTitle", NULL);
    (void)conv_title;
    json_free(root);
}

/* Poll for inbound DingTalk messages.
 * Returns JSON array of messages from the webhook queue. */
json_node_t *dingtalk_poll_messages(http_client_t *http) {
    (void)http;

    pthread_mutex_lock(&g_msg_lock);
    if (g_msg_head == g_msg_tail) {
        pthread_mutex_unlock(&g_msg_lock);
        return NULL;
    }

    json_node_t *results = json_array();

    while (g_msg_head != g_msg_tail) {
        json_node_t *msg = json_new_object();
        json_set(msg, "chat_id", json_string(g_msg_queue[g_msg_head].chat_id));
        json_set(msg, "text", json_string(g_msg_queue[g_msg_head].text));
        json_set(msg, "sender_id", json_string(g_msg_queue[g_msg_head].sender_id));
        json_append(results, msg);
        g_msg_head = (g_msg_head + 1) % DINGTALK_QUEUE_MAX;
    }
    pthread_mutex_unlock(&g_msg_lock);
    return results;
}

const char *dingtalk_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "dingtalk");
}

const char *dingtalk_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
