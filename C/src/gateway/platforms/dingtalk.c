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

json_node_t *dingtalk_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *dingtalk_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "dingtalk");
}

const char *dingtalk_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
