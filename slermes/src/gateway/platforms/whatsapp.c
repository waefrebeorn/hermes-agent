/*
 * whatsapp.c — WhatsApp Cloud API gateway for Hermes C.
 * Webhook-based receiver (uses existing webhook server infrastructure)
 * + REST API sender via graph.facebook.com.
 *
 * Env vars:
 *   WHATSAPP_TOKEN        — WhatsApp Cloud API permanent/temporary token
 *   WHATSAPP_PHONE_NUMBER_ID — Phone number ID from Meta Business
 *   WHATSAPP_VERIFY_TOKEN — Verify token for webhook verification
 *   WHATSAPP_API_VERSION  — Optional: Facebook Graph API version (default: v22.0)
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "slermes.h"
#include "slermes_json.h"
#include "slermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WHATSAPP_GRAPH_API "https://graph.facebook.com"
#define DEFAULT_API_VERSION "v22.0"

static char wa_token[1024] = "";
static char wa_phone_id[256] = "";
static char wa_verify_token[256] = "";
static char wa_api_version[32] = DEFAULT_API_VERSION;

void whatsapp_set_token(const char *token) {
    snprintf(wa_token, sizeof(wa_token), "%s", token);
}

void whatsapp_set_phone_id(const char *id) {
    snprintf(wa_phone_id, sizeof(wa_phone_id), "%s", id);
}

void whatsapp_set_verify_token(const char *token) {
    snprintf(wa_verify_token, sizeof(wa_verify_token), "%s", token);
}

/* ================================================================
 *  Send message via WhatsApp Cloud API
 * ================================================================ */

bool whatsapp_send_message(http_client_t *http, const char *to,
                            const char *text) {
    if (!http || !to || !text || !wa_token[0] || !wa_phone_id[0])
        return false;

    char url[2048];
    snprintf(url, sizeof(url), "%s/%s/%s/messages",
             WHATSAPP_GRAPH_API, wa_api_version, wa_phone_id);

    json_node_t *body = json_new_object();
    json_set(body, "messaging_product", json_string("whatsapp"));
    json_set(body, "to", json_string(to));

    json_node_t *text_obj = json_new_object();
    json_set(text_obj, "body", json_string(text));
    json_set(body, "text", text_obj);

    char *payload = json_serialize(body);
    json_free(body);

    char headers[4096];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json",
             wa_token);

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);

    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  WhatsApp webhook verification handler
 *  Returns challenge string on success, NULL on failure.
 * ================================================================ */

const char *whatsapp_verify_webhook(const char *query_string) {
    if (!query_string || !wa_verify_token[0]) return NULL;

    /* Parse query params — simple manual parse */
    const char *mode_str = NULL;
    const char *token_str = NULL;
    const char *challenge_str = NULL;

    /* Split by & */
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", query_string);

    char *saveptr;
    char *token = strtok_r(buf, "&", &saveptr);
    while (token) {
        char *eq = strchr(token, '=');
        if (!eq) { token = strtok_r(NULL, "&", &saveptr); continue; }
        *eq = '\0';
        char *key = token;
        char *val = eq + 1;

        if (strcmp(key, "hub.mode") == 0) mode_str = val;
        else if (strcmp(key, "hub.verify_token") == 0) token_str = val;
        else if (strcmp(key, "hub.challenge") == 0) challenge_str = val;

        token = strtok_r(NULL, "&", &saveptr);
    }

    if (!mode_str || !token_str || !challenge_str) return NULL;
    if (strcmp(mode_str, "subscribe") != 0) return NULL;
    if (strcmp(token_str, wa_verify_token) != 0) return NULL;

    /* Return challenge as static string (caller must not free) */
    static char challenge[128];
    snprintf(challenge, sizeof(challenge), "%s", challenge_str);
    return challenge;
}

/* ================================================================
 *  Parse WhatsApp webhook payload — extract messages array
 *  Returns JSON array of {from, text, message_id} objects.
 * ================================================================ */

json_node_t *whatsapp_parse_webhook(const char *body) {
    if (!body) return NULL;

    char *err = NULL;
    json_node_t *root = json_parse(body, &err);
    free(err);
    if (!root) return NULL;

    json_node_t *entry = json_obj_get(root, "entry");
    if (!entry || json_len(entry) == 0) {
        json_free(root);
        return NULL;
    }

    json_node_t *result = json_new_array();

    size_t n_entries = json_len(entry);
    for (size_t ei = 0; ei < n_entries; ei++) {
        json_node_t *e = json_get(entry, ei);

        json_node_t *changes = json_obj_get(e, "changes");
        if (!changes) continue;

        size_t n_changes = json_len(changes);
        for (size_t ci = 0; ci < n_changes; ci++) {
            json_node_t *c = json_get(changes, ci);
            json_node_t *value = json_obj_get(c, "value");
            if (!value) continue;

            json_node_t *messages = json_obj_get(value, "messages");
            if (!messages) continue;

            size_t n_msgs = json_len(messages);
            for (size_t mi = 0; mi < n_msgs; mi++) {
                json_node_t *msg = json_get(messages, mi);

                const char *msg_type = json_get_str(msg, "type", "");
                const char *from = json_get_str(msg, "from", "");
                const char *msg_id = json_get_str(msg, "id", "");

                if (!*from || !*msg_id) continue;

                /* Extract text */
                const char *text = "";
                if (strcmp(msg_type, "text") == 0) {
                    json_node_t *text_obj = json_obj_get(msg, "text");
                    if (text_obj)
                        text = json_get_str(text_obj, "body", "");
                }

                /* Build update wrapper */
                json_node_t *update = json_new_object();
                json_set(update, "update_id",
                         json_number((double)((unsigned long)time(NULL) * 1000 + mi)));
                json_set(update, "from", json_string(from));
                json_set(update, "message_id", json_string(msg_id));
                json_set(update, "text", json_string(text));
                json_set(update, "type", json_string(msg_type));

                json_append(result, update);
            }
        }
    }

    json_free(root);
    return result;
}

const char *whatsapp_get_chat_id(json_node_t *update) {
    return json_get_str(update, "from", NULL);
}

const char *whatsapp_get_text(json_node_t *update) {
    return json_get_str(update, "text", NULL);
}

#pragma GCC diagnostic pop
