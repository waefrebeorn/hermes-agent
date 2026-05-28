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

/* ================================================================
 *  E54: File upload via two-step external upload API
 * ================================================================
 * Uses Slack's modern file upload:
 *   1. POST /api/files.getUploadURLExternal → gets upload_url + file_id
 *   2. PUT raw file content to upload_url
 *   3. POST /api/files.completeUploadExternal → attach to channel
 * No multipart needed. */

bool slack_upload_file(http_client_t *http, const char *file_path,
                        const char *filename, const char *channel) {
    if (!http || !file_path || !channel) return false;

    /* Read file from disk */
    FILE *f = fopen(file_path, "rb");
    if (!f) { fprintf(stderr, "[slack] cannot open file: %s\n", file_path); return false; }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);
    if (file_size <= 0) { fclose(f); return false; }

    char *file_data = (char *)malloc((size_t)file_size);
    if (!file_data) { fclose(f); return false; }
    size_t nread = fread(file_data, 1, (size_t)file_size, f);
    fclose(f);
    if ((long)nread != file_size) { free(file_data); return false; }

    const char *display_name = filename ? filename : strrchr(file_path, '/');
    display_name = display_name ? display_name : file_path;
    if (filename) display_name = filename;

    /* Step 1: getUploadURLExternal */
    char url[1024];
    snprintf(url, sizeof(url), "%s/files.getUploadURLExternal", SLACK_API);

    json_node_t *req1 = json_new_object();
    json_object_set(req1, "filename", json_new_string(display_name));
    json_object_set(req1, "length", json_number((double)file_size));
    json_object_set(req1, "alt_txt", json_new_string(display_name));

    char *req1_json = json_serialize(req1);
    json_free(req1);

    char headers[2048];
    slack_auth_header(headers, sizeof(headers));

    http_response_t *resp1 = http_request(http, HTTP_POST, url,
                                           headers, req1_json, strlen(req1_json));
    free(req1_json);

    if (!resp1 || resp1->status != 200) {
        fprintf(stderr, "[slack] getUploadURLExternal failed: %d\n",
                resp1 ? resp1->status : -1);
        if (resp1) http_response_free(resp1);
        free(file_data);
        return false;
    }

    char *err = NULL;
    json_node_t *j1 = json_parse(resp1->body, &err);
    http_response_free(resp1);
    if (!j1 || !json_get_bool(j1, "ok", false)) {
        free(err); json_free(j1); free(file_data);
        return false;
    }
    free(err);

    const char *upload_url = json_get_str(j1, "upload_url", NULL);
    const char *file_id = json_get_str(j1, "file_id", NULL);
    if (!upload_url || !file_id) {
        json_free(j1); free(file_data);
        return false;
    }

    /* Step 2: PUT file content to upload_url */
    const char *bin_headers = "Content-Type: application/octet-stream";
    http_response_t *resp2 = http_request(http, HTTP_PUT, upload_url,
                                           bin_headers, file_data, (size_t)nread);
    free(file_data);

    if (!resp2 || resp2->status != 200) {
        fprintf(stderr, "[slack] file upload PUT failed: %d\n",
                resp2 ? resp2->status : -1);
        if (resp2) http_response_free(resp2);
        json_free(j1);
        return false;
    }
    http_response_free(resp2);

    /* Step 3: completeUploadExternal */
    snprintf(url, sizeof(url), "%s/files.completeUploadExternal", SLACK_API);

    json_node_t *files_arr = json_new_array();
    json_node_t *file_entry = json_new_object();
    json_object_set(file_entry, "id", json_new_string(file_id));
    json_object_set(file_entry, "title", json_new_string(display_name));
    json_append(files_arr, file_entry);

    json_node_t *req3 = json_new_object();
    json_object_set(req3, "files", files_arr);
    json_object_set(req3, "channel_id", json_new_string(channel ? channel : channel_id));

    char *req3_json = json_serialize(req3);
    json_free(req3); /* frees files_arr + file_entry too */

    char headers3[2048];
    slack_auth_header(headers3, sizeof(headers3));

    http_response_t *resp3 = http_request(http, HTTP_POST, url,
                                           headers3, req3_json, strlen(req3_json));
    free(req3_json);
    json_free(j1);

    if (!resp3 || resp3->status != 200) {
        fprintf(stderr, "[slack] completeUploadExternal failed: %d\n",
                resp3 ? resp3->status : -1);
        if (resp3) http_response_free(resp3);
        return false;
    }

    char *err3 = NULL;
    json_node_t *j3 = json_parse(resp3->body, &err3);
    http_response_free(resp3);
    bool ok = j3 && json_get_bool(j3, "ok", false);
    json_free(j3); free(err3);
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
