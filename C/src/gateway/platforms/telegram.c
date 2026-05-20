/*
 * telegram.c — Telegram Bot API platform adapter for Hermes C.
 * Handles message formatting, sending, and receiving.
 * Uses the Telegram Bot API via HTTP.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Telegram API URLs
 * ================================================================ */

static char api_base[512] = "https://api.telegram.org/bot";

void telegram_set_token(const char *token) {
    snprintf(api_base, sizeof(api_base), "https://api.telegram.org/bot%s", token);
}

/* ================================================================
 *  Send message
 * ================================================================ */

bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode)
{
    if (!http || !chat_id || !text) return false;

    char url[512];
    snprintf(url, sizeof(url), "%s/sendMessage", api_base);

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "text", json_new_string(text));
    if (parse_mode)
        json_object_set(body, "parse_mode", json_new_string(parse_mode));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(http, HTTP_POST, url, payload);
    free(payload);

    bool ok = resp && resp->status_code == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Send typing indicator
 * ================================================================ */

bool telegram_send_chat_action(http_client_t *http, const char *chat_id,
                                const char *action)
{
    if (!http || !chat_id || !action) return false;

    char url[512];
    snprintf(url, sizeof(url), "%s/sendChatAction", api_base);

    json_node_t *body = json_new_object();
    json_object_set(body, "chat_id", json_new_string(chat_id));
    json_object_set(body, "action", json_new_string(action));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(http, HTTP_POST, url, payload);
    free(payload);

    bool ok = resp && resp->status_code == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Get updates (used by poll loop in server.c)
 * ================================================================ */

json_node_t *telegram_get_updates(http_client_t *http, int offset, int timeout)
{
    char url[512];
    snprintf(url, sizeof(url), "%s/getUpdates", api_base);

    json_node_t *body = json_new_object();
    json_object_set(body, "offset", json_new_number((double)offset));
    json_object_set(body, "timeout", json_new_number((double)timeout));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(http, HTTP_POST, url, payload);
    free(payload);

    if (!resp) return NULL;

    char *err = NULL;
    json_node_t *root = NULL;
    if (resp->body)
        root = json_parse(resp->body, &err);

    http_response_free(resp);
    free(err);
    return root;
}

/* ================================================================
 *  Extract message info from update
 * ================================================================ */

const char *telegram_get_chat_id(json_node_t *update) {
    static char buf[32];
    json_node_t *msg = json_object_get(update, "message");
    if (!msg) return NULL;
    json_node_t *chat = json_object_get(msg, "chat");
    if (!chat) return NULL;
    double id = json_object_get_number(chat, "id", 0);
    snprintf(buf, sizeof(buf), "%.0f", id);
    return buf;
}

const char *telegram_get_text(json_node_t *update) {
    json_node_t *msg = json_object_get(update, "message");
    if (!msg) return NULL;
    return json_object_get_string(msg, "text", NULL);
}
