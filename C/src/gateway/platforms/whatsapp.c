/*
 * whatsapp.c — WhatsApp Cloud API gateway for Hermes C.
 * P107: Business API, message templates, interactive messages, onboarding.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
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

/* Build auth header */
static void wa_auth_header(char *buf, size_t cap) {
    snprintf(buf, cap,
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json",
             wa_token);
}

/* Helper: POST to WhatsApp API */
static http_response_t *wa_post(http_client_t *http, json_node_t *body) {
    char url[2048];
    snprintf(url, sizeof(url), "%s/%s/%s/messages",
             WHATSAPP_GRAPH_API, wa_api_version, wa_phone_id);

    char *payload = json_serialize(body);
    json_free(body);

    char headers[4096];
    wa_auth_header(headers, sizeof(headers));

    http_response_t *resp = http_request(http, HTTP_POST, url,
                                          headers, payload, strlen(payload));
    free(payload);
    return resp;
}

/* ================================================================
 *  Send text message
 * ================================================================ */

bool whatsapp_send_message(http_client_t *http, const char *to,
                            const char *text) {
    if (!http || !to || !text || !wa_token[0] || !wa_phone_id[0])
        return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "messaging_product", json_new_string("whatsapp"));
    json_object_set(body, "to", json_new_string(to));

    json_node_t *text_obj = json_new_object();
    json_object_set(text_obj, "body", json_new_string(text));
    json_object_set(body, "text", text_obj);

    http_response_t *resp = wa_post(http, body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P107: Send message template (for onboarding/notifications)
 * ================================================================ */

bool whatsapp_send_template(http_client_t *http, const char *to,
                             const char *template_name,
                             const char *language_code,
                             json_node_t *components)
{
    if (!http || !to || !template_name || !wa_token[0]) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "messaging_product", json_new_string("whatsapp"));
    json_object_set(body, "to", json_new_string(to));
    json_object_set(body, "recipient_type", json_new_string("individual"));

    json_node_t *tmpl = json_new_object();
    json_object_set(tmpl, "name", json_new_string(template_name));

    json_node_t *lang = json_new_object();
    json_object_set(lang, "code", json_new_string(language_code ? language_code : "en_US"));
    json_object_set(tmpl, "language", lang);

    if (components)
        json_object_set(tmpl, "components", json_copy(components));

    json_object_set(body, "template", tmpl);

    http_response_t *resp = wa_post(http, body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P107: Send interactive message (buttons)
 * ================================================================ */

bool whatsapp_send_interactive_buttons(http_client_t *http, const char *to,
                                        const char *header_text,
                                        const char *body_text,
                                        const char *footer_text,
                                        json_node_t *buttons)
{
    if (!http || !to || !body_text || !buttons) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "messaging_product", json_new_string("whatsapp"));
    json_object_set(body, "to", json_new_string(to));

    json_node_t *interactive = json_new_object();
    json_object_set(interactive, "type", json_new_string("button"));

    if (header_text) {
        json_node_t *header = json_new_object();
        json_object_set(header, "type", json_new_string("text"));
        json_object_set(header, "text", json_new_string(header_text));
        json_object_set(interactive, "header", header);
    }

    json_node_t *b = json_new_object();
    json_object_set(b, "text", json_new_string(body_text));
    json_object_set(interactive, "body", b);

    if (footer_text) {
        json_node_t *footer = json_new_object();
        json_object_set(footer, "text", json_new_string(footer_text));
        json_object_set(interactive, "footer", footer);
    }

    json_object_set(interactive, "action", json_copy(buttons));
    json_object_set(body, "interactive", interactive);

    http_response_t *resp = wa_post(http, body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P107: Send interactive list message
 * ================================================================ */

bool whatsapp_send_interactive_list(http_client_t *http, const char *to,
                                     const char *body_text,
                                     const char *button_text,
                                     const char *section_title,
                                     json_node_t *rows)
{
    if (!http || !to || !body_text || !button_text) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "messaging_product", json_new_string("whatsapp"));
    json_object_set(body, "to", json_new_string(to));

    json_node_t *interactive = json_new_object();
    json_object_set(interactive, "type", json_new_string("list"));

    json_node_t *b = json_new_object();
    json_object_set(b, "text", json_new_string(body_text));
    json_object_set(interactive, "body", b);

    json_node_t *action = json_new_object();
    json_object_set(action, "button", json_new_string(button_text));

    json_node_t *section = json_new_object();
    if (section_title)
        json_object_set(section, "title", json_new_string(section_title));
    if (rows)
        json_object_set(section, "rows", json_copy(rows));
    json_node_t *sections = json_new_array();
    json_array_append(sections, section);
    json_object_set(action, "sections", sections);

    json_object_set(interactive, "action", action);
    json_object_set(body, "interactive", interactive);

    http_response_t *resp = wa_post(http, body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  P107: Mark message as read
 * ================================================================ */

bool whatsapp_mark_read(http_client_t *http, const char *message_id) {
    if (!http || !message_id || !wa_token[0]) return false;

    json_node_t *body = json_new_object();
    json_object_set(body, "messaging_product", json_new_string("whatsapp"));
    json_object_set(body, "status", json_new_string("read"));
    json_object_set(body, "message_id", json_new_string(message_id));

    http_response_t *resp = wa_post(http, body);
    bool ok = resp && resp->status == 200;
    if (resp) http_response_free(resp);
    return ok;
}

/* ================================================================
 *  WhatsApp webhook verification handler
 * ================================================================ */

const char *whatsapp_verify_webhook(const char *query_string) {
    if (!query_string || !wa_verify_token[0]) return NULL;

    /* Parse query params — simple manual parse */
    const char *mode_str = NULL;
    const char *token_str = NULL;
    const char *challenge_str = NULL;

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

    static char challenge[128];
    snprintf(challenge, sizeof(challenge), "%s", challenge_str);
    return challenge;
}

/* ================================================================
 *  Parse WhatsApp webhook payload
 * ================================================================ */

json_node_t *whatsapp_parse_webhook(const char *body) {
    if (!body) return NULL;

    char *err = NULL;
    json_node_t *root = json_parse(body, &err);
    free(err);
    if (!root) return NULL;

    json_node_t *entry = json_object_get(root, "entry");
    if (!entry || json_len(entry) == 0) {
        json_free(root);
        return NULL;
    }

    json_node_t *result = json_new_array();

    size_t n_entries = json_len(entry);
    for (size_t ei = 0; ei < n_entries; ei++) {
        json_node_t *e = json_get(entry, ei);

        json_node_t *changes = json_object_get(e, "changes");
        if (!changes) continue;

        size_t n_changes = json_len(changes);
        for (size_t ci = 0; ci < n_changes; ci++) {
            json_node_t *c = json_get(changes, ci);
            json_node_t *value = json_object_get(c, "value");
            if (!value) continue;

            json_node_t *messages = json_object_get(value, "messages");
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
                    json_node_t *text_obj = json_object_get(msg, "text");
                    if (text_obj)
                        text = json_get_str(text_obj, "body", "");
                }

                /* P107: Extract interactive response */
                if (strcmp(msg_type, "interactive") == 0) {
                    json_node_t *interactive = json_object_get(msg, "button_reply");
                    if (interactive)
                        text = json_get_str(interactive, "title", "");
                }

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
