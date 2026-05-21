/*
 * wecom.c — WeCom (WeChat Work) gateway for Hermes C.
 * Sends messages via WeCom bot webhook API.
 *
 * Env vars:
 *   WECOM_WEBHOOK_URL — WeCom bot webhook URL
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_wecom_webhook[1024] = "";

void wecom_set_webhook(const char *url) {
    if (url) snprintf(g_wecom_webhook, sizeof(g_wecom_webhook), "%s", url);
}

bool wecom_send_message(http_client_t *http, const char *text) {
    if (!g_wecom_webhook[0] || !text) return false;
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

    http_response_t *resp = http_request(http, HTTP_POST, g_wecom_webhook,
                                          headers, body, strlen(body));
    free(body);
    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

json_node_t *wecom_poll_messages(http_client_t *http) {
    (void)http; return NULL;
}

const char *wecom_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "wecom");
}
const char *wecom_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
