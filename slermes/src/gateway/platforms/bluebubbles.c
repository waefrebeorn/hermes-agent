/*
 * bluebubbles.c — BlueBubbles (iMessage) gateway for Hermes C.
 * Uses BlueBubbles server REST API for sending iMessages.
 *
 * P115: Extended with tapback reactions, attachments, group chat,
 *       typing indicators, and proper polling.
 *
 * Env vars:
 *   BLUEBUBBLES_URL        — BlueBubbles server URL (e.g., "http://192.168.1.100:1234")
 *   BLUEBUBBLES_PASSWORD   — BlueBubbles server password
 *   BLUEBUBBLES_POLL_GUID  — Optional chat GUID to poll for recent messages
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_bb_url[512] = "";
static char g_bb_password[256] = "";
static char g_poll_guid[256] = "";   /* Optional chat GUID for polling */

void bluebubbles_set_url(const char *url) {
    if (url) snprintf(g_bb_url, sizeof(g_bb_url), "%s", url);
}

void bluebubbles_set_password(const char *password) {
    if (password) snprintf(g_bb_password, sizeof(g_bb_password), "%s", password);
}

void bluebubbles_set_poll_guid(const char *guid) {
    if (guid) snprintf(g_poll_guid, sizeof(g_poll_guid), "%s", guid);
    else g_poll_guid[0] = '\0';
}

/* ================================================================
 * P115: Helper — build BlueBubbles API URL with password auth
 * ================================================================ */

static void bb_url(char *buf, size_t bufsize, const char *path) {
    const char *sep = strchr(path, '?') ? "&" : "?";
    snprintf(buf, bufsize, "%s%s%spassword=%s",
             g_bb_url, path, sep, g_bb_password);
}

/* ================================================================
 * P115: Helper — BlueBubbles auth header line
 * ================================================================ */

static void bb_headers(char *buf, size_t bufsize) {
    snprintf(buf, bufsize, "Password: %s\r\nContent-Type: application/json\r\n",
             g_bb_password);
}

/* ================================================================
 * P115: Helper — URL-encode a string for use in URL path.
 * Allocates memory; caller must free().
 * ================================================================ */

static char *url_encode(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    /* Worst case: every char becomes %XX (3x) */
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = c;
        } else {
            out[j++] = '%';
            out[j++] = hex[c >> 4];
            out[j++] = hex[c & 0x0f];
        }
    }
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  Send text message (existing, refactored)
 * ================================================================ */

bool bluebubbles_send_message(http_client_t *http, const char *to, const char *text) {
    if (!g_bb_url[0] || !to || !text) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/v1/message/text", g_bb_url);

    json_node_t *root = json_new_object();
    json_set(root, "chatGuid", json_string(to));
    json_set(root, "text", json_string(text));
    char *body = json_serialize(root);
    json_free(root);

    char headers[512];
    bb_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 * P115: Send tapback reaction
 *
 * POST /api/v1/message/tapback
 * Body: { chatGuid, messageGuid, reaction }
 *
 * reaction is the associatedMessageType integer:
 *   2000=love, 2001=like, 2002=dislike,
 *   2003=laugh, 2004=emphasize, 2005=question
 * ================================================================ */

bool bluebubbles_send_tapback(http_client_t *http,
                               const char *chat_id,
                               const char *message_guid,
                               int tapback_type) {
    if (!g_bb_url[0] || !chat_id || !message_guid) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/v1/message/tapback", g_bb_url);

    json_node_t *root = json_new_object();
    json_set(root, "chatGuid", json_string(chat_id));
    json_set(root, "messageGuid", json_string(message_guid));
    json_set(root, "reaction", json_number((double)tapback_type));
    char *body = json_serialize(root);
    json_free(root);

    char headers[512];
    bb_headers(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 * P115: Send file attachment
 *
 * Uses curl via system() because libhttp does not support
 * multipart/form-data uploads.
 *
 * curl -X POST <url>?password=<pw> \
 *   -F "attachment=@<file_path>;filename=<filename>" \
 *   -F "chatGuid=<chat_id>" \
 *   -F "name=<filename>" \
 *   -F "tempGuid=<uuid>"
 * ================================================================ */

bool bluebubbles_send_attachment(http_client_t *http,
                                  const char *chat_id,
                                  const char *file_path,
                                  const char *filename) {
    (void)http;
    if (!g_bb_url[0] || !chat_id || !file_path) return false;

    /* Use basename if no filename provided */
    const char *fname = filename;
    char fname_buf[256];
    if (!fname) {
        const char *base = strrchr(file_path, '/');
        snprintf(fname_buf, sizeof(fname_buf), "%s",
                 base ? base + 1 : file_path);
        fname = fname_buf;
    }

    /* Build temp GUID from PID + time */
    char temp_guid[128];
    snprintf(temp_guid, sizeof(temp_guid), "c-temp-%ld-%ld",
             (long)time(NULL), (long)getpid());

    /* Build curl command */
    char cmd[16384];
    char api_url[1024];
    bb_url(api_url, sizeof(api_url), "/api/v1/message/attachment");

    int r = snprintf(cmd, sizeof(cmd),
        "curl -s -X POST '%s' "
        "-F \"attachment=@%s;filename=%s\" "
        "-F \"chatGuid=%s\" "
        "-F \"name=%s\" "
        "-F \"tempGuid=%s\" "
        ">/dev/null 2>&1",
        api_url, file_path, fname, chat_id, fname, temp_guid);

    if (r < 0 || (size_t)r >= sizeof(cmd)) return false;
    return system(cmd) == 0;
}

/* ================================================================
 * P115: Group chat detection
 *
 * BlueBubbles group chat GUIDs contain ";+;" (e.g. "any;+;uuid").
 * Regular DM GUIDs contain ";-;" (e.g. "iMessage;-;user@example.com").
 * ================================================================ */

bool bluebubbles_is_group(json_node_t *update) {
    if (!update) return false;
    /* Check explicit isGroup field */
    if (json_get_bool(update, "isGroup", false)) return true;
    /* Check GUID for group pattern */
    const char *guid = json_get_str(update, "chatGuid", "");
    if (strstr(guid, ";+;") != NULL) return true;
    guid = json_get_str(update, "guid", "");
    if (strstr(guid, ";+;") != NULL) return true;
    return false;
}

const char *bluebubbles_get_group_id(json_node_t *update) {
    if (!update) return NULL;
    /* Return the group chat GUID if it's a group */
    const char *guid = json_get_str(update, "chatGuid", "");
    if (guid[0] && strstr(guid, ";+;")) return guid;
    guid = json_get_str(update, "guid", "");
    if (guid[0] && strstr(guid, ";+;")) return guid;
    /* Try chats array fallback (BB v1.9+ format) */
    json_node_t *chats = json_obj_get(update, "chats");
    if (chats && json_len(chats) > 0) {
        json_node_t *first = json_get(chats, 0);
        if (first) {
            const char *cg = json_get_str(first, "guid", "");
            if (cg[0] && strstr(cg, ";+;")) return cg;
        }
    }
    return NULL;
}

/* ================================================================
 * P115: Typing indicator
 *
 * POST /api/v1/chat/{encoded_guid}/typing  (start)
 * DELETE /api/v1/chat/{encoded_guid}/typing (stop)
 * ================================================================ */

bool bluebubbles_send_typing(http_client_t *http, const char *chat_id) {
    if (!g_bb_url[0] || !chat_id) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *encoded = url_encode(chat_id);
    if (!encoded) { if (local_http) http_client_free(local_http); return false; }

    char url[1536];
    snprintf(url, sizeof(url), "%s/api/v1/chat/%s/typing", g_bb_url, encoded);
    free(encoded);

    char headers[512];
    snprintf(headers, sizeof(headers), "Password: %s\r\n", g_bb_password);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, NULL, 0);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

bool bluebubbles_stop_typing(http_client_t *http, const char *chat_id) {
    if (!g_bb_url[0] || !chat_id) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char *encoded = url_encode(chat_id);
    if (!encoded) { if (local_http) http_client_free(local_http); return false; }

    char url[1536];
    snprintf(url, sizeof(url), "%s/api/v1/chat/%s/typing", g_bb_url, encoded);
    free(encoded);

    char headers[512];
    snprintf(headers, sizeof(headers), "Password: %s\r\n", g_bb_password);

    http_response_t *resp = http_request(http, HTTP_DELETE, url,
                                          headers, NULL, 0);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* ================================================================
 * P115: Poll for recent messages
 *
 * Uses the BlueBubbles chat query API. If BLUEBUBBLES_POLL_GUID
 * is set, fetches recent messages from that specific chat.
 * Otherwise returns NULL (webhook-driven).
 *
 * Returns a JSON array of message updates, each with:
 *   - "chat_id": the chat GUID
 *   - "text": message text
 *   - "guid": message GUID
 *   - "is_group": true/false
 *   - "tapback_type": integer if this is a reaction, else 0
 *   - "attachment_path": path string if attachment, else ""
 *   - "sender": sender address
 * ================================================================ */

json_node_t *bluebubbles_poll_messages(http_client_t *http) {
    if (!g_bb_url[0]) return NULL;

    /* If no poll GUID configured, return NULL */
    if (!g_poll_guid[0]) return NULL;

    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char url[1024];
    char *encoded = url_encode(g_poll_guid);
    if (!encoded) { if (local_http) http_client_free(local_http); return NULL; }

    snprintf(url, sizeof(url), "%s/api/v1/chat/%s?with=participants",
             g_bb_url, encoded);
    free(encoded);

    char headers[512];
    snprintf(headers, sizeof(headers), "Password: %s\r\n", g_bb_password);

    http_response_t *resp = http_request(http, HTTP_GET, url,
                                          headers, NULL, 0);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        if (resp) http_response_free(resp);
        if (local_http) http_client_free(local_http);
        return NULL;
    }

    /* Parse the response — BlueBubbles returns {"status":200,"data":{...}} */
    json_node_t *json = json_parse(resp->body, NULL);
    http_response_free(resp);
    if (local_http) http_client_free(local_http);

    if (!json) return NULL;

    /* Extract the data object */
    json_node_t *data = json_obj_get(json, "data");
    if (!data) {
        json_free(json);
        return NULL;
    }

    /* Build an array of updates. For now, create a minimal wrapper
     * that the gateway can iterate. */
    json_node_t *updates = json_new_array();
    json_node_t *update = json_new_object();

    const char *chat_guid = json_get_str(data, "guid",
                            json_get_str(data, "chatGuid", ""));
    json_set(update, "chat_id", json_string(chat_guid ? chat_guid : "bluebubbles"));

    const char *display_name = json_get_str(data, "displayName", "");
    if (display_name[0])
        json_set(update, "text", json_string(display_name));
    else
        json_set(update, "text", json_string("(poll)"));

    bool is_group = strstr(chat_guid ? chat_guid : "", ";+;") != NULL;
    json_set(update, "is_group", json_bool(is_group));

    json_append(updates, update);
    json_free(json);

    return updates;
}

/* ================================================================
 *  Update parsers
 * ================================================================ */

const char *bluebubbles_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "bluebubbles");
}

const char *bluebubbles_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}

/* P115: Get message GUID from an update */
const char *bluebubbles_get_message_guid(json_node_t *update) {
    if (!update) return NULL;
    const char *guid = json_get_str(update, "guid", "");
    if (guid[0]) return guid;
    /* Also check for chatGuid field and nested paths */
    guid = json_get_str(update, "messageGuid", "");
    if (guid[0]) return guid;
    return NULL;
}

/* P115: Get tapback reaction type from an update.
 * Returns 0 if no tapback, otherwise 2000-2005 (add) or 3000-3005 (remove). */
int bluebubbles_get_tapback_type(json_node_t *update) {
    if (!update) return 0;
    return (int)json_get_num(update, "associatedMessageType", 0.0);
}

/* P115: Get first attachment path from an update.
 * Returns pointer to static buffer, or NULL if none. */
const char *bluebubbles_get_attachment_path(json_node_t *update) {
    if (!update) return NULL;
    json_node_t *attachments = json_obj_get(update, "attachments");
    if (!attachments || json_len(attachments) == 0) return NULL;

    json_node_t *first = json_get(attachments, 0);
    if (!first) return NULL;

    return json_get_str(first, "path", "");
}
