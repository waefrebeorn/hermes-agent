/*
 * feishu.c — Feishu (Lark) gateway for Hermes C.
 * Sends messages via Feishu/Lark bot webhook API.
 *
 * Env vars:
 *   FEISHU_WEBHOOK_URL  — Feishu bot webhook URL
 *   FEISHU_APP_ID       — Feishu app ID (for API mode, optional)
 *   FEISHU_APP_SECRET   — Feishu app secret (for API mode, optional)
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_feishu_webhook[1024] = "";

void feishu_set_webhook(const char *url) {
    if (url) snprintf(g_feishu_webhook, sizeof(g_feishu_webhook), "%s", url);
}

bool feishu_send_message(http_client_t *http, const char *text) {
    if (!g_feishu_webhook[0] || !text) return false;

    if (!http) http = http_client_new(10);
    http_client_t *local_http = NULL;
    if (!http) { local_http = http_client_new(10); http = local_http; }

    /* Build Feishu message JSON */
    json_node_t *root = json_new_object();
    json_set(root, "msg_type", json_string("text"));
    json_node_t *content = json_new_object();
    json_set(content, "text", json_string(text));
    json_set(root, "content", content);

    char *body = json_serialize(root);
    json_free(root);

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\n");

    http_response_t *resp = http_request(http, HTTP_POST, g_feishu_webhook,
                                          headers, body, strlen(body));
    free(body);

    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    if (local_http) http_client_free(local_http);
    return ok;
}

/* Feishu polls via webhook receiver (no long-polling API for bots) */
json_node_t *feishu_poll_messages(http_client_t *http) {
    (void)http;
    return NULL;
}

const char *feishu_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "feishu");
}

const char *feishu_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
