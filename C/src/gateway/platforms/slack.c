/*
 * slack.c — Slack Bot API platform adapter for Hermes C.
 * REST-only polling (no WebSocket/Socket Mode).
 * Polls channel messages via conversations.history.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLACK_API "https://slack.com/api"

static char bot_token[512] = "";
static char channel_id[128] = "";
static double last_ts = 0.0;  /* Slack uses floating-point timestamps */

void slack_set_token(const char *token) {
    snprintf(bot_token, sizeof(bot_token), "%s", token);
}

void slack_set_channel(const char *id) {
    snprintf(channel_id, sizeof(channel_id), "%s", id);
}

/* Build auth header */
static void slack_auth_header(char *buf, size_t cap) {
    snprintf(buf, cap, "Authorization: Bearer %s\r\n"
                       "Content-Type: application/json", bot_token);
}

/* ================================================================
 *  Send message to channel
 * ================================================================ */

bool slack_send_message(http_client_t *http, const char *text) {
    if (!http || !text || !channel_id[0]) return false;

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat.postMessage", SLACK_API);

    json_node_t *body = json_new_object();
    json_set(body, "channel", json_string(channel_id));
    json_set(body, "text", json_string(text));

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
 *  Returns JSON array of updates in Telegram-like format.
 * ================================================================ */

json_node_t *slack_poll_messages(http_client_t *http) {
    if (!http || !channel_id[0]) return NULL;

    char url[1024];
    if (last_ts > 0.0) {
        /* Fetch messages since last_ts */
        int n = snprintf(url, sizeof(url), "%s/conversations.history", SLACK_API);
        if (n < 0 || (size_t)n >= sizeof(url)) return NULL;
        snprintf(url + n, sizeof(url) - n,
                 "?channel=%s&oldest=%.6f&limit=10",
                 channel_id, last_ts + 0.000001);
    } else {
        /* First poll — get latest message */
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
    free(err);

    if (!root) return NULL;

    /* Check Slack API ok flag */
    bool api_ok = json_get_bool(root, "ok", false);
    if (!api_ok) {
        const char *error = json_get_str(root, "error", "unknown");
        fprintf(stderr, "[slack] API error: %s\n", error);
        json_free(root);
        return NULL;
    }

    json_node_t *msgs = json_obj_get(root, "messages");
    if (!msgs || json_len(msgs) == 0) {
        json_free(root);
        return NULL;
    }

    /* Normalize to Telegram-like update format */
    json_node_t *updates = json_new_array();
    size_t n = json_len(msgs);

    for (size_t i = 0; i < n; i++) {
        json_node_t *msg = json_get(msgs, n - 1 - i); /* oldest first */
        double ts = atof(json_get_str(msg, "ts", "0"));

        if (ts <= last_ts) continue;
        if (ts > last_ts) last_ts = ts;

        /* Skip bot's own messages */
        const char *subtype = json_get_str(msg, "subtype", "");
        bool is_bot = json_get_bool(msg, "bot_id", false) ||
                       (strcmp(subtype, "bot_message") == 0);
        if (is_bot) continue;

        /* Build update wrapper */
        json_node_t *update = json_new_object();
        json_set(update, "update_id", json_number(ts));

        json_node_t *message = json_new_object();
        json_set(message, "message_id", json_number(ts));

        json_node_t *chat = json_new_object();
        json_set(chat, "id", json_string(channel_id));
        json_set(message, "chat", chat);

        json_set(message, "text", json_string(json_get_str(msg, "text", "")));
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
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s", channel_id);
    return buf;
}

const char *slack_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}

#pragma GCC diagnostic pop
