/*
 * bluebubbles.c — BlueBubbles (iMessage) gateway for Hermes C.
 * Uses BlueBubbles server REST API for sending iMessages.
 *
 * Env vars:
 *   BLUEBUBBLES_URL   — BlueBubbles server URL (e.g., "http://192.168.1.100:1234")
 *   BLUEBUBBLES_PASSWORD — BlueBubbles server password
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_bb_url[512] = "";
static char g_bb_password[256] = "";

void bluebubbles_set_url(const char *url) {
    if (url) snprintf(g_bb_url, sizeof(g_bb_url), "%s", url);
}

void bluebubbles_set_password(const char *password) {
    if (password) snprintf(g_bb_password, sizeof(g_bb_password), "%s", password);
}

bool bluebubbles_send_message(http_client_t *http, const char *to, const char *text) {
    if (!g_bb_url[0] || !to || !text) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/v1/chat/send", g_bb_url);

    json_node_t *root = json_new_object();
    json_set(root, "chatGuid", json_string(to));
    json_set(root, "text", json_string(text));
    char *body = json_serialize(root);
    json_free(root);

    char headers[512];
    snprintf(headers, sizeof(headers),
             "Password: %s\r\nContent-Type: application/json\r\n",
             g_bb_password);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

json_node_t *bluebubbles_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *bluebubbles_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "bluebubbles");
}
const char *bluebubbles_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
