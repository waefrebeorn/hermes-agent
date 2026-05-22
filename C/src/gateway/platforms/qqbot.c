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

json_node_t *qqbot_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *qqbot_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "qqbot");
}

const char *qqbot_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
