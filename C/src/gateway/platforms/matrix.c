/*
 * matrix.c — Matrix client-server API platform adapter for Hermes C.
 * REST polling via /sync endpoint. Sends via /send/m.room.message.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char homeserver[512] = "https://matrix.org";
static char access_token[512] = "";
static char room_id[256] = "";
static char next_batch[128] = "";  /* sync token for incremental sync */

void matrix_set_homeserver(const char *hs) {
    snprintf(homeserver, sizeof(homeserver), "%s", hs);
}

void matrix_set_token(const char *token) {
    snprintf(access_token, sizeof(access_token), "%s", token);
}

void matrix_set_room(const char *id) {
    snprintf(room_id, sizeof(room_id), "%s", id);
}

/* Build auth header */
static void matrix_auth_header(char *buf, size_t cap) {
    snprintf(buf, cap, "Authorization: Bearer %s", access_token);
}

/* ================================================================
 *  Send message to room
 * ================================================================ */

bool matrix_send_message(http_client_t *http, const char *text) {
    if (!http || !text || !room_id[0] || !access_token[0]) return false;

    char url[2048];
    /* Use a random transaction ID to avoid idempotency conflicts */
    snprintf(url, sizeof(url), "%s/_matrix/client/v3/rooms/%s/send/m.room.message/%ld",
             homeserver, room_id, (long)time(NULL));

    json_node_t *body = json_new_object();
    json_set(body, "msgtype", json_string("m.text"));
    json_set(body, "body", json_string(text));

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json",
             access_token);

    http_response_t *resp = http_request(http, HTTP_PUT, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Poll for new messages via /sync
 *  Returns JSON array of updates in Telegram-like format.
 * ================================================================ */

json_node_t *matrix_poll_messages(http_client_t *http) {
    if (!http || !access_token[0]) return NULL;

    char url[4096];
    if (next_batch[0]) {
        snprintf(url, sizeof(url), "%s/_matrix/client/v3/sync?since=%s&timeout=30000&set_presence=offline",
                 homeserver, next_batch);
    } else {
        /* First sync: get initial state without timeline */
        snprintf(url, sizeof(url), "%s/_matrix/client/v3/sync?timeout=30000&set_presence=offline",
                 homeserver);
    }

    char headers[512];
    matrix_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_get_with_headers(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) {
            fprintf(stderr, "[matrix] API error: %d\n", resp->status);
            http_response_free(resp);
        }
        return NULL;
    }

    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);
    free(err);
    if (!root) return NULL;

    /* Store next_batch for incremental sync */
    const char *nb = json_get_str(root, "next_batch", "");
    if (nb[0]) snprintf(next_batch, sizeof(next_batch), "%s", nb);

    /* If no room filter, extract events from all rooms */
    json_node_t *rooms = json_obj_get(root, "rooms");
    if (!rooms) { json_free(root); return NULL; }

    json_node_t *join = json_obj_get(rooms, "join");
    if (!join) { json_free(root); return NULL; }

    json_node_t *updates = json_new_array();

    /* Walk joined rooms */
    size_t n_rooms = json_len(join);
    for (size_t ri = 0; ri < n_rooms; ri++) {
        const char *r_id = NULL;
        json_node_t *r_data = json_get(join, ri);
        /* rooms is an object, not array — get by key */
        /* Actually json_len on object returns count; iterate obj_get by index */
        /* For simplicity, check if room_id is set and only use that room */
        if (room_id[0]) {
            r_data = json_obj_get(join, room_id);
            if (!r_data) continue;
            r_id = room_id;
        } else {
            /* Use first room */
            if (ri > 0) break;
            continue; /* skip non-room_id filtering for now */
        }

        json_node_t *timeline = json_obj_get(r_data, "timeline");
        if (!timeline) continue;

        json_node_t *events = json_obj_get(timeline, "events");
        if (!events) continue;

        size_t n_events = json_len(events);
        for (size_t ei = 0; ei < n_events; ei++) {
            json_node_t *ev = json_get(events, ei);

            /* Only process m.room.message events */
            const char *type = json_get_str(ev, "type", "");
            if (strcmp(type, "m.room.message") != 0) continue;

            /* Skip own messages */
            /* Can't easily skip own messages in Matrix without user_id */
            /* We'll just process all and let agent handle duplicates */

            const char *event_id = json_get_str(ev, "event_id", "");
            const char *content_text = "";
            json_node_t *content = json_obj_get(ev, "content");
            if (content) {
                content_text = json_get_str(content, "body", "");
            }

            if (!*content_text) continue;

            /* Build update wrapper */
            json_node_t *update = json_new_object();
            /* Use hash of event_id as update_id */
            unsigned long hash = 0;
            for (const char *p = event_id; *p; p++)
                hash = hash * 31 + (unsigned char)*p;
            json_set(update, "update_id", json_number((double)hash));

            json_node_t *message = json_new_object();
            json_set(message, "message_id", json_string(event_id));

            json_node_t *chat = json_new_object();
            json_set(chat, "id", json_string(r_id ? r_id : ""));
            json_set(message, "chat", chat);

            json_set(message, "text", json_string(content_text));
            json_set(update, "message", message);

            json_append(updates, update);
        }
    }

    json_free(root);
    return updates;
}

/* ================================================================
 *  Extract info from update
 * ================================================================ */

const char *matrix_get_chat_id(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    json_node_t *chat = json_obj_get(msg, "chat");
    if (!chat) return NULL;
    return json_get_str(chat, "id", NULL);
}

const char *matrix_get_text(json_node_t *update) {
    json_node_t *msg = json_obj_get(update, "message");
    if (!msg) return NULL;
    return json_get_str(msg, "text", NULL);
}

#pragma GCC diagnostic pop
