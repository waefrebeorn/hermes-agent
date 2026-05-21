/*
 * mattermost.c — Mattermost Bot API platform adapter for Hermes C.
 * REST-only polling. Polls channel posts via /api/v4/channels/{id}/posts.
 * Sends via /api/v4/posts.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "slermes.h"
#include "slermes_json.h"
#include "slermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MATTERMOST_API "/api/v4"

static char server_url[1024] = "http://localhost:8065";
static char bot_token[512] = "";
static char channel_id[128] = "";
static char last_post_id[128] = "";  /* track last seen post ID */

void mattermost_set_url(const char *url) {
    snprintf(server_url, sizeof(server_url), "%s", url);
    /* Strip trailing slash */
    size_t len = strlen(server_url);
    while (len > 0 && server_url[len-1] == '/') server_url[--len] = '\0';
}

void mattermost_set_token(const char *token) {
    snprintf(bot_token, sizeof(bot_token), "%s", token);
}

void mattermost_set_channel(const char *id) {
    snprintf(channel_id, sizeof(channel_id), "%s", id);
}

/* ================================================================
 *  Send message to channel
 * ================================================================ */

bool mattermost_send_message(http_client_t *http, const char *text) {
    if (!http || !text || !channel_id[0] || !bot_token[0]) return false;

    char url[2048];
    snprintf(url, sizeof(url), "%s%s/posts", server_url, MATTERMOST_API);

    json_node_t *body = json_new_object();
    json_set(body, "channel_id", json_string(channel_id));
    json_set(body, "message", json_string(text));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json", bot_token);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 201;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Poll for new messages via /api/v4/channels/{id}/posts
 *  Returns JSON array of updates in Telegram-like format.
 * ================================================================ */

json_node_t *mattermost_poll_messages(http_client_t *http) {
    if (!http || !channel_id[0] || !bot_token[0]) return NULL;

    char url[2048];
    if (last_post_id[0]) {
        /* Fetch posts since last seen — API has ?since=unix_millis */
        /* We use the most direct approach: fetch latest, dedup by ID */
        snprintf(url, sizeof(url), "%s%s/channels/%s/posts?per_page=50",
                 server_url, MATTERMOST_API, channel_id);
    } else {
        /* First poll — get latest post for initial state */
        snprintf(url, sizeof(url), "%s%s/channels/%s/posts?per_page=1",
                 server_url, MATTERMOST_API, channel_id);
    }

    char headers[512];
    snprintf(headers, sizeof(headers), "Authorization: Bearer %s", bot_token);

    http_response_t *resp = http_get_with_headers(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) {
            fprintf(stderr, "[mattermost] API error: %d %s\n", resp->status,
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

    json_node_t *posts_obj = json_obj_get(root, "posts");
    if (!posts_obj || json_len(posts_obj) == 0) {
        json_free(root);
        return NULL;
    }

    /* posts is an object keyed by post ID. Get order array */
    json_node_t *order = json_obj_get(root, "order");
    if (!order || json_len(order) == 0) {
        json_free(root);
        return NULL;
    }

    json_node_t *updates = json_new_array();
    size_t n = json_len(order);

    for (size_t i = 0; i < n; i++) {
        const char *pid = json_get_str(json_get(order, i), NULL, "");
        if (!pid[0]) continue;

        json_node_t *post = json_obj_get(posts_obj, pid);
        if (!post) continue;

        /* Skip if we've already seen it */
        if (last_post_id[0] && strcmp(pid, last_post_id) <= 0)
            continue;

        /* Update last seen */
        if (!last_post_id[0] || strcmp(pid, last_post_id) > 0)
            snprintf(last_post_id, sizeof(last_post_id), "%s", pid);

        /* Skip own messages (bot messages have a "props.from_webhook" or
         * check user_id against bot's user ID — for simplicity skip if
         * "pending_post_id" not set or user_id is "bot") */
        /* We can't easily skip bot messages without knowing bot user ID,
         * so we'll filter by the message not being from the bot user.
         * For now, accept all and let agent handle duplicates. */

        /* Build update wrapper */
        json_node_t *update = json_new_object();
        /* Use string hash as update_id */
        unsigned long hash = 0;
        for (const char *p = pid; *p; p++)
            hash = hash * 31 + (unsigned char)*p;
        json_set(update, "update_id", json_number((double)hash));

        json_node_t *message = json_new_object();
        json_set(message, "message_id", json_string(pid));

        json_node_t *chat = json_new_object();
        json_set(chat, "id", json_string(channel_id));
        json_set(message, "chat", chat);

        const char *msg_text = json_get_str(post, "message", "");
        json_set(message, "text", json_string(msg_text));
        json_set(update, "message", message);

        json_append(updates, update);
    }

    json_free(root);
    return updates;
}

/* ================================================================
 *  Extract info from update
 * ================================================================ */

const char *mattermost_get_chat_id(json_node_t *update) {
    (void)update;
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s", channel_id);
    return buf;
}

const char *mattermost_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}

#pragma GCC diagnostic pop
