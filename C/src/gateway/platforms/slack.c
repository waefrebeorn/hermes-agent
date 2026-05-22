/*
 * slack.c — Slack Bot API platform adapter for Hermes C.
 * REST-only polling (no Socket Mode).
 * P106: Block Kit, message actions, slash commands, channel join/leave events.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLACK_API "https://slack.com/api"

static char bot_token[512] = "";
static char channel_id[128] = "";
static double last_ts = 0.0;
static char signing_secret[128] = "";

void slack_set_token(const char *token) {
    snprintf(bot_token, sizeof(bot_token), "%s", token);
}

void slack_set_channel(const char *id) {
    snprintf(channel_id, sizeof(channel_id), "%s", id);
}

void slack_set_signing_secret(const char *secret) {
    if (secret) snprintf(signing_secret, sizeof(signing_secret), "%s", secret);
}

/* Build auth header */
static void slack_auth_header(char *buf, size_t cap) {
    snprintf(buf, cap,
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json; charset=utf-8",
             bot_token);
}

/* ================================================================
 *  Send message
 * ================================================================ */

bool slack_send_message(http_client_t *http, const char *text) {
    if (!http || !text || !channel_id[0]) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat.postMessage", SLACK_API);

    json_node_t *body = json_new_object();
    json_object_set(body, "channel", json_new_string(channel_id));
    json_object_set(body, "text", json_new_string(text));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P106: Send message with Block Kit blocks (JSON array) */
bool slack_send_blocks(http_client_t *http, const char *channel,
                        const char *text, json_node_t *blocks) {
    if (!http || !channel) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat.postMessage", SLACK_API);

    json_node_t *body = json_new_object();
    json_object_set(body, "channel", json_new_string(channel));
    if (text)
        json_object_set(body, "text", json_new_string(text));
    if (blocks)
        json_object_set(body, "blocks", json_copy(blocks));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P106: Update a message */
bool slack_update_message(http_client_t *http, const char *channel,
                           const char *ts, const char *text) {
    if (!http || !channel || !ts) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat.update", SLACK_API);

    json_node_t *body = json_new_object();
    json_object_set(body, "channel", json_new_string(channel));
    json_object_set(body, "ts", json_new_string(ts));
    if (text)
        json_object_set(body, "text", json_new_string(text));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P106: Join a channel */
bool slack_join_channel(http_client_t *http, const char *channel) {
    if (!http || !channel) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/conversations.join", SLACK_API);

    json_node_t *body = json_new_object();
    json_object_set(body, "channel", json_new_string(channel));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* P106: Leave a channel */
bool slack_leave_channel(http_client_t *http, const char *channel) {
    if (!http || !channel) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/conversations.leave", SLACK_API);

    json_node_t *body = json_new_object();
    json_object_set(body, "channel", json_new_string(channel));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Poll for new messages via conversations.history
 * ================================================================ */

json_node_t *slack_poll_messages(http_client_t *http) {
    if (!http || !channel_id[0]) return NULL;

    char url[2048];
    if (last_ts > 0.0) {
        snprintf(url, sizeof(url), "%s/conversations.history?channel=%s&oldest=%.6f&limit=10",
                 SLACK_API, channel_id, last_ts + 0.000001);
    } else {
        snprintf(url, sizeof(url), "%s/conversations.history?channel=%s&limit=1&oldest=0",
                 SLACK_API, channel_id);
    }

    char headers[512];
    snprintf(headers, sizeof(headers), "Authorization: Bearer %s", bot_token);

    http_response_t *resp = http_get_with_headers(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) {
            fprintf(stderr, "[slack] API error: %d %s\n", resp->status,
                    resp->body ? resp->body : "(no body)");
            http_response_free(resp);
        }
        return NULL;
    }

    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);

    if (!root || !json_get_bool(root, "ok", false)) {
        free(err);
        json_free(root);
        return NULL;
    }
    free(err);

    json_node_t *msgs = json_object_get(root, "messages");
    if (!msgs || json_len(msgs) == 0) {
        json_free(root);
        return NULL;
    }

    json_node_t *updates = json_new_array();
    size_t n = json_len(msgs);

    for (size_t i = 0; i < n; i++) {
        json_node_t *msg = json_get(msgs, n - 1 - i);
        double ts = json_get_num(msg, "ts", 0);
        if (ts <= last_ts) continue;
        if (ts > last_ts) last_ts = ts;

        /* Skip bot messages */
        json_node_t *bot = json_obj_get(msg, "bot_id");
        if (bot) continue;

        /* Build update wrapper */
        json_node_t *update = json_new_object();
        json_set(update, "update_id", json_number(ts * 1000000));

        json_node_t *message = json_new_object();
        json_set(message, "message_id", json_number(ts * 1000000));

        json_node_t *chat = json_new_object();
        json_set(chat, "id", json_string(channel_id));
        json_set(message, "chat", chat);

        json_set(message, "text", json_get_str(msg, "text", ""));
        json_set(update, "message", message);

        json_append(updates, update);
    }

    json_free(root);
    return updates;
}

/* ================================================================
 *  Extract info from update
 * ================================================================ */

const char *slack_get_chat_id(json_node_t *update) {
    (void)update;
    return channel_id;
}

const char *slack_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}
