/*
 * qqbot.c — QQ Bot gateway for Hermes C.
 * Uses QQ Bot webhook API for sending messages.
 *
 * Env vars:
 *   QQ_BOT_WEBHOOK_URL — QQ Bot webhook URL
 *   QQ_BOT_TOKEN       — QQ Bot auth token
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_qqbot_webhook[1024] = "";
static char g_qqbot_token[256] = "";

void qqbot_set_webhook(const char *url) {
    if (url) snprintf(g_qqbot_webhook, sizeof(g_qqbot_webhook), "%s", url);
}

void qqbot_set_token(const char *token) {
    if (token) snprintf(g_qqbot_token, sizeof(g_qqbot_token), "%s", token);
}

bool qqbot_send_message(http_client_t *http, const char *text) {
    if (!g_qqbot_webhook[0] || !text) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    json_node_t *root = json_new_object();
    json_set(root, "content", json_string(text));
    char *body = json_serialize(root);
    json_free(root);

    char headers[512];
    if (g_qqbot_token[0])
        snprintf(headers, sizeof(headers),
                 "Authorization: Bearer %s\r\nContent-Type: application/json\r\n",
                 g_qqbot_token);
    else
        snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_qqbot_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

json_node_t *qqbot_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *qqbot_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "qqbot");
}
const char *qqbot_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
