/*
 * dingtalk.c — DingTalk gateway for Hermes C.
 * Sends messages via DingTalk bot webhook API.
 *
 * Env vars:
 *   DINGTALK_WEBHOOK_URL — DingTalk bot webhook URL (with access_token)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_dingtalk_webhook[1024] = "";

void dingtalk_set_webhook(const char *url) {
    if (url) snprintf(g_dingtalk_webhook, sizeof(g_dingtalk_webhook), "%s", url);
}

bool dingtalk_send_message(http_client_t *http, const char *text) {
    if (!g_dingtalk_webhook[0] || !text) return false;
    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    json_node_t *root = json_new_object();
    json_set(root, "msgtype", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "content", json_string(text));
    json_set(root, "text", content);

    char *body = json_serialize(root);
    json_free(root);

    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_dingtalk_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

json_node_t *dingtalk_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *dingtalk_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "dingtalk");
}
const char *dingtalk_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
