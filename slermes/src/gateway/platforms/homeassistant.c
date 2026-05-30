/*
 * homeassistant.c — HomeAssistant gateway for Hermes C.
 * Sends notifications via HomeAssistant REST API.
 *
 * Env vars:
 *   HA_URL        — HomeAssistant URL (e.g., "http://homeassistant.local:8123")
 *   HA_TOKEN      — Long-lived access token
 *   HA_NOTIFY_ENTITY — Notification entity (default: "notify.mobile_app_hermes")
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_ha_url[512] = "";
static char g_ha_token[256] = "";
static char g_ha_notify_entity[128] = "notify.persistent_notification";

void ha_set_url(const char *url) {
    if (url) snprintf(g_ha_url, sizeof(g_ha_url), "%s", url);
}

void ha_set_token(const char *token) {
    if (token) snprintf(g_ha_token, sizeof(g_ha_token), "%s", token);
}

void ha_set_notify_entity(const char *entity) {
    if (entity) snprintf(g_ha_notify_entity, sizeof(g_ha_notify_entity), "%s", entity);
}

bool ha_send_notification(const char *title, const char *message) {
    if (!g_ha_url[0] || !g_ha_token[0]) return false;

    http_client_t *http = http_client_new(10);
    if (!http) return false;

    /* Build API URL */
    char url[1024];
    snprintf(url, sizeof(url), "%s/api/services/notify/%s",
             g_ha_url, g_ha_notify_entity);

    /* Build JSON body */
    json_node_t *body = json_new_object();
    json_node_t *data = json_new_object();
    if (title) json_set(data, "title", json_string(title));
    json_set(data, "message", json_string(message ? message : ""));
    json_set(body, "data", data);

    char *body_str = json_serialize(body);
    json_free(body);

    /* Build headers */
    char headers[1024];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json\r\n",
             g_ha_token);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, body_str, strlen(body_str));
    free(body_str);

    bool ok = resp && resp->status >= 200 && resp->status < 300;
    if (resp) http_response_free(resp);
    http_client_free(http);
    return ok;
}

/* Poll for HA events via REST API (simple: check a sensor or event) */
json_node_t *ha_poll_messages(http_client_t *http) {
    (void)http;
    if (!g_ha_url[0] || !g_ha_token[0]) return NULL;

    /* Build a second HTTP client for polling */
    http_client_t *poll_http = http_client_new(10);
    if (!poll_http) return NULL;

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/states/input_text.hermes_message",
             g_ha_url);

    char headers[512];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\nContent-Type: application/json\r\n",
             g_ha_token);

    http_response_t *resp = http_request(poll_http, HTTP_GET, url,
                                          headers, NULL, 0);
    if (!resp || resp->status != 200) {
        if (resp) http_response_free(resp);
        http_client_free(poll_http);
        return NULL;
    }

    /* Parse state response */
    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    http_response_free(resp);
    http_client_free(poll_http);

    if (!root) { free(err); return NULL; }

    const char *state = json_get_str(root, "state", "");
    if (!state || !state[0] || strcmp(state, "unknown") == 0 ||
        strcmp(state, "unavailable") == 0) {
        json_free(root);
        return NULL;
    }

    /* Create update from the state */
    json_node_t *results = json_array();
    json_node_t *msg = json_object();
    json_set(msg, "chat_id", json_string("homeassistant"));
    json_set(msg, "text", json_string(state));
    json_append(results, msg);

    /* Reset the input_text to clear it */
    {
        http_client_t *reset_http = http_client_new(5);
        if (reset_http) {
            char reset_url[1024];
            snprintf(reset_url, sizeof(reset_url), "%s/api/services/input_text/set_value",
                     g_ha_url);
            json_node_t *reset_body = json_new_object();
            json_set(reset_body, "entity_id", json_string("input_text.hermes_message"));
            json_set(reset_body, "value", json_string(""));
            char *reset_body_str = json_serialize(reset_body);
            json_free(reset_body);
            if (reset_body_str) {
                http_response_t *reset_resp = http_request(reset_http, HTTP_POST, reset_url,
                    headers, reset_body_str, strlen(reset_body_str));
                free(reset_body_str);
                if (reset_resp) http_response_free(reset_resp);
            }
            http_client_free(reset_http);
        }
    }
    json_free(root);
    return json_len(results) > 0 ? results : NULL;
}

const char *ha_get_chat_id(json_node_t *update) {
    return json_get_str(update, "chat_id", "homeassistant");
}

const char *ha_get_text(json_node_t *update) {
    return json_get_str(update, "text", "");
}
