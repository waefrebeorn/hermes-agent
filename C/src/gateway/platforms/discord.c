/*
 * discord.c — Discord Bot API platform adapter for Hermes C.
 * REST-only polling (no WebSocket). Polls recent messages in a channel.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISCORD_API "https://discord.com/api/v10"

static char bot_token[512] = "";
static char channel_id[128] = "";
static long long last_message_id = 0;

void discord_set_token(const char *token) {
    snprintf(bot_token, sizeof(bot_token), "%s", token);
}

void discord_set_channel(const char *id) {
    snprintf(channel_id, sizeof(channel_id), "%s", id);
}

/* Build combined headers: auth + content-type */
static void discord_build_headers(char *buf, size_t cap, bool with_body) {
    if (with_body) {
        snprintf(buf, cap,
                 "Authorization: Bot %s\r\n"
                 "Content-Type: application/json",
                 bot_token);
    } else {
        snprintf(buf, cap,
                 "Authorization: Bot %s",
                 bot_token);
    }
}

/* ================================================================
 *  Send message
 * ================================================================ */

bool discord_send_message(http_client_t *http, const char *text) {
    if (!http || !text || !channel_id[0]) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/messages", DISCORD_API, channel_id);

    json_node_t *body = json_new_object();
    json_object_set(body, "content", json_new_string(text));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    discord_build_headers(headers, sizeof(headers), true);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Send typing indicator
 * ================================================================ */

void discord_send_typing(http_client_t *http) {
    if (!http || !channel_id[0]) return;

    char url[1024];
    snprintf(url, sizeof(url), "%s/channels/%s/typing", DISCORD_API, channel_id);

    char headers[1024];
    discord_build_headers(headers, sizeof(headers), false);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, NULL, 0);
    if (resp) http_response_free(resp);
}

/* ================================================================
 *  Poll for new messages (via REST: GET /channels/{id}/messages)
 * ================================================================ */

json_node_t *discord_poll_messages(http_client_t *http) {
    if (!http || !channel_id[0]) return NULL;

    char url[1024];
    if (last_message_id > 0) {
        snprintf(url, sizeof(url), "%s/channels/%s/messages?limit=5&after=%lld",
                 DISCORD_API, channel_id, last_message_id);
    } else {
        snprintf(url, sizeof(url), "%s/channels/%s/messages?limit=1",
                 DISCORD_API, channel_id);
    }

    char headers[512];
    discord_build_headers(headers, sizeof(headers), false);

    http_response_t *resp = http_get_with_headers(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        return NULL;
    }

    char *err = NULL;
    json_node_t *msgs = json_parse(resp->body, &err);
    http_response_free(resp);
    free(err);

    if (!msgs || json_len(msgs) == 0) {
        json_free(msgs);
        return NULL;
    }

    /* Normalize to Telegram-like update format */
    json_node_t *updates = json_new_array();
    size_t n = json_len(msgs);

    for (size_t i = 0; i < n; i++) {
        json_node_t *msg = json_get(msgs, n - 1 - i); /* oldest first */
        long long mid = (long long)json_get_num(msg, "id", 0);

        if (mid <= last_message_id) continue;
        if (mid > last_message_id) last_message_id = mid;

        /* Skip bot's own messages */
        json_node_t *author = json_obj_get(msg, "author");
        if (author) {
            bool is_bot = json_get_bool(author, "bot", false);
            if (is_bot) continue;
        }

        /* Build update wrapper */
        json_node_t *update = json_new_object();
        json_set(update, "update_id", json_number((double)mid));

        json_node_t *message = json_new_object();
        json_set(message, "message_id", json_number((double)mid));

        json_node_t *chat = json_new_object();
        json_set(chat, "id", json_number((double)mid));
        json_set(message, "chat", chat);

        json_set(message, "text", json_string(json_get_str(msg, "content", "")));
        json_set(update, "message", message);

        json_append(updates, update);
    }

    json_free(msgs);
    return updates;
}

/* ================================================================
 *  Extract info from update
 * ================================================================ */

const char *discord_get_chat_id(json_node_t *update) {
    (void)update;
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s", channel_id);
    return buf;
}

const char *discord_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}
