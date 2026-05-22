/*
 * matrix.c — Matrix client-server API platform adapter for Hermes C.
 * REST polling via /sync endpoint. Sends via /send/m.room.message.
 *
 * P109: Room discovery, event type filtering, read receipts, typing indicators.
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

/* P109: User ID for typing endpoints */
static char user_id[256] = "";

/* P109: Comma-separated list of event types to accept.
 * Default: only m.room.message. Filter out m.typing, m.receipt, etc. */
static char event_filter[256] = "m.room.message";

/* Fallback room array when sync has no next_batch yet;
   re-discovered on each poll if empty (see matrix_list_rooms usage). */

void matrix_set_homeserver(const char *hs) {
    snprintf(homeserver, sizeof(homeserver), "%s", hs);
}

void matrix_set_token(const char *token) {
    snprintf(access_token, sizeof(access_token), "%s", token);
}

void matrix_set_room(const char *id) {
    snprintf(room_id, sizeof(room_id), "%s", id);
}

/* P109: Set Matrix user ID (e.g. @user:matrix.org) for typing endpoints */
void matrix_set_user_id(const char *uid) {
    snprintf(user_id, sizeof(user_id), "%s", uid);
}

/* P109: Set event type filter — comma-separated list of event types to accept.
 * Pass empty string or NULL to accept all event types.
 * Example: "m.room.message,m.room.member" */
void matrix_set_event_filter(const char *types) {
    if (types && types[0]) {
        snprintf(event_filter, sizeof(event_filter), "%s", types);
    } else {
        event_filter[0] = '\0'; /* accept all */
    }
}

/* Build auth header */
static void matrix_auth_header(char *buf, size_t cap) {
    snprintf(buf, cap, "Authorization: Bearer %s", access_token);
}

/* ================================================================
 *  P109: Check if event type passes the current filter
 * ================================================================ */
static bool event_type_allowed(const char *type) {
    if (!type || !type[0]) return false;
    if (!event_filter[0]) return true; /* empty = accept all */

    /* Check if type appears in comma-separated filter list */
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", event_filter);
    char *save = NULL;
    char *tok = strtok_r(copy, ",", &save);
    while (tok) {
        /* Trim whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';

        if (strcmp(tok, type) == 0) return true;
        tok = strtok_r(NULL, ",", &save);
    }
    return false;
}

/* ================================================================
 *  Build full auth+content-type headers string
 * ================================================================ */
static void matrix_headers(char *buf, size_t cap) {
    snprintf(buf, cap,
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json",
             access_token);
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
    matrix_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_PUT, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P109: Room discovery — list joined rooms
 *  Returns JSON array of room_id strings, or NULL on error.
 *  Caller must json_free() the result.
 * ================================================================ */

json_node_t *matrix_list_rooms(http_client_t *http) {
    if (!http || !access_token[0]) return NULL;

    char url[2048];
    snprintf(url, sizeof(url), "%s/_matrix/client/v3/joined_rooms", homeserver);

    char headers[512];
    matrix_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_get_with_headers(http, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) {
            fprintf(stderr, "[matrix] list_rooms API error: %d\n", resp->status);
            http_response_free(resp);
        }
        return NULL;
    }

    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);
    free(err);
    if (!root) return NULL;

    /* Extract "joined_rooms" array */
    json_node_t *rooms = json_obj_get(root, "joined_rooms");
    if (!rooms) {
        json_free(root);
        return NULL;
    }

    /* Return a copy so caller owns it independently */
    json_node_t *result = json_copy(rooms);
    json_free(root);
    return result;
}

/* ================================================================
 *  P109: Read receipt — mark event as read
 *  POST /_matrix/client/v3/rooms/{roomId}/receipt/m.read/{eventId}
 * ================================================================ */

bool matrix_mark_read(http_client_t *http, const char *r_id, const char *event_id) {
    if (!http || !r_id || !event_id || !access_token[0]) return false;

    char url[3072];
    snprintf(url, sizeof(url), "%s/_matrix/client/v3/rooms/%s/receipt/m.read/%s",
             homeserver, r_id, event_id);

    /* Read receipt body is an empty JSON object */
    json_node_t *body = json_new_object();
    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    matrix_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && (resp->status == 200 || resp->status == 204);
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P109: Typing indicator — send typing notification
 *  PUT /_matrix/client/v3/rooms/{roomId}/typing/{userId}
 *  timeout_ms: how long to show typing (0 = cancel typing, >0 = show)
 * ================================================================ */

bool matrix_send_typing(http_client_t *http, const char *r_id, int timeout_ms) {
    if (!http || !r_id || !user_id[0] || !access_token[0]) return false;

    char url[3072];
    snprintf(url, sizeof(url), "%s/_matrix/client/v3/rooms/%s/typing/%s",
             homeserver, r_id, user_id);

    json_node_t *body = json_new_object();
    if (timeout_ms > 0) {
        json_set(body, "typing", json_bool(true));
        json_set(body, "timeout", json_number((double)timeout_ms));
    } else {
        json_set(body, "typing", json_bool(false));
    }

    char *payload = json_serialize(body);
    json_free(body);

    char headers[2048];
    matrix_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_PUT, url,
                                          headers, payload, strlen(payload));
    free(payload);

    /* 200 = success; 429 = rate-limited but not an error for our purposes */
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  Poll for new messages via /sync
 *  Returns JSON array of updates in Telegram-like format.
 *
 *  P109: Now iterates ALL joined rooms (not just room_id),
 *  uses event_filter to skip unwanted event types,
 *  and records room_id in each update.
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

    /* Extract events from joined rooms */
    json_node_t *rooms = json_obj_get(root, "rooms");
    if (!rooms) { json_free(root); return NULL; }

    json_node_t *join = json_obj_get(rooms, "join");
    if (!join) { json_free(root); return NULL; }

    json_node_t *updates = json_new_array();

    /* P109: Properly iterate ALL joined rooms using json_t internal struct.
     * Since json_t is a transparent struct (see json.h), we access
     * keys/items directly for object iteration. */
    size_t n_rooms = json_len(join);
    for (size_t ri = 0; ri < n_rooms; ri++) {
        const char *r_id = join->c.keys[ri];
        json_node_t *r_data = join->c.items[ri];

        json_node_t *timeline = json_obj_get(r_data, "timeline");
        if (!timeline) continue;

        json_node_t *events = json_obj_get(timeline, "events");
        if (!events) continue;

        size_t n_events = json_len(events);
        for (size_t ei = 0; ei < n_events; ei++) {
            json_node_t *ev = json_get(events, ei);

            /* P109: Use configurable event type filter */
            const char *type = json_get_str(ev, "type", "");
            if (!event_type_allowed(type)) continue;

            const char *event_id = json_get_str(ev, "event_id", "");
            const char *sender = json_get_str(ev, "sender", "");

            /* P109: Skip own messages if we have a user_id */
            if (user_id[0] && sender[0] && strcmp(sender, user_id) == 0) continue;

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
            json_set(message, "sender", json_string(sender));
            json_set(message, "type", json_string(type));
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
