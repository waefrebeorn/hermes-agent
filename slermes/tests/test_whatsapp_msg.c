/*
 * test_whatsapp_msg.c — M10: WhatsApp message format tests.
 *
 * Tests JSON serialization of WhatsApp Cloud API message bodies
 * without real HTTP calls. Covers: text, template, interactive
 * buttons, interactive list, URL building, edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (!test_##name()) { \
        printf("  FAIL: %s\n", #name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", #name); \
        tests_passed++; \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* ──────────────────────────────────────────────
 *  Text message body
 * ────────────────────────────────────────────── */

static json_t *make_text_msg(const char *to, const char *text) {
    json_t *body = json_object();
    json_set(body, "messaging_product", json_string("whatsapp"));
    json_set(body, "to", json_string(to ? to : ""));
    json_t *txt = json_object();
    json_set(txt, "body", json_string(text ? text : ""));
    json_set(body, "text", txt);
    return body;
}

static bool test_text_message(void) {
    json_t *body = make_text_msg("15551234567", "Hello from Hermes!");
    ASSERT(body != NULL, "text msg body created");

    char *out = json_serialize(body);
    ASSERT(out != NULL, "serialize OK");
    ASSERT(strstr(out, "\"messaging_product\":\"whatsapp\"") != NULL, "product field");
    ASSERT(strstr(out, "\"to\":\"15551234567\"") != NULL, "to field");
    ASSERT(strstr(out, "\"body\":\"Hello from Hermes!\"") != NULL, "text body");
    ASSERT(strstr(out, "\"text\"") != NULL, "text wrapper object");
    free(out);
    json_free(body);
    return true;
}

static bool test_text_message_null_params(void) {
    json_t *body = make_text_msg(NULL, NULL);
    char *out = json_serialize(body);
    ASSERT(out != NULL, "NULL params serialize");
    ASSERT(strstr(out, "\"to\":\"\"") != NULL, "empty to");
    ASSERT(strstr(out, "\"body\":\"\"") != NULL, "empty body");
    free(out);
    json_free(body);
    return true;
}

/* ──────────────────────────────────────────────
 *  Template message body
 * ────────────────────────────────────────────── */

static json_t *make_template_msg(const char *to, const char *name,
                                  const char *lang_code, json_t *components) {
    json_t *body = json_object();
    json_set(body, "messaging_product", json_string("whatsapp"));
    json_set(body, "to", json_string(to ? to : ""));
    json_set(body, "recipient_type", json_string("individual"));

    json_t *tmpl = json_object();
    json_set(tmpl, "name", json_string(name ? name : ""));
    json_t *lang = json_object();
    json_set(lang, "code", json_string(lang_code ? lang_code : "en_US"));
    json_set(tmpl, "language", lang);
    if (components)
        json_set(tmpl, "components", json_copy(components));
    json_set(body, "template", tmpl);
    return body;
}

static bool test_template_message(void) {
    /* Template with header + body components */
    json_t *components = json_array();
    json_t *header = json_object();
    json_set(header, "type", json_string("header"));
    json_t *hdr_params = json_array();
    json_t *hdr_param = json_object();
    json_set(hdr_param, "type", json_string("text"));
    json_set(hdr_param, "text", json_string("John"));
    json_append(hdr_params, hdr_param);
    json_set(header, "parameters", hdr_params);
    json_append(components, header);

    json_t *body_part = json_object();
    json_set(body_part, "type", json_string("body"));
    json_t *body_params = json_array();
    json_t *bp1 = json_object();
    json_set(bp1, "type", json_string("text"));
    json_set(bp1, "text", json_string("50.00"));
    json_append(body_params, bp1);
    json_set(body_part, "parameters", body_params);
    json_append(components, body_part);

    json_t *msg = make_template_msg("15551234567", "order_confirmed", "en", components);

    char *out = json_serialize(msg);
    ASSERT(out != NULL, "template serialize OK");
    ASSERT(strstr(out, "order_confirmed") != NULL, "template name");
    ASSERT(strstr(out, "\"code\":\"en\"") != NULL, "language code");
    ASSERT(strstr(out, "individual") != NULL, "recipient_type");
    ASSERT(strstr(out, "\"type\":\"header\"") != NULL, "header component");
    ASSERT(strstr(out, "\"type\":\"body\"") != NULL, "body component");
    ASSERT(strstr(out, "John") != NULL, "header param text");
    ASSERT(strstr(out, "50.00") != NULL, "body param text");
    free(out);

    json_free(msg);
    json_free(components); /* freed by copy inside, but original separate */
    return true;
}

static bool test_template_null_language(void) {
    json_t *msg = make_template_msg("15551234567", "welcome", NULL, NULL);
    char *out = json_serialize(msg);
    ASSERT(out != NULL, "template null lang serialize");
    ASSERT(strstr(out, "\"code\":\"en_US\"") != NULL, "default lang");
    free(out);
    json_free(msg);
    return true;
}

/* ──────────────────────────────────────────────
 *  Interactive buttons message
 * ────────────────────────────────────────────── */

static json_t *make_interactive_buttons(const char *to, const char *header,
                                         const char *body_text,
                                         const char *footer,
                                         json_t *buttons) {
    json_t *msg = json_object();
    json_set(msg, "messaging_product", json_string("whatsapp"));
    json_set(msg, "to", json_string(to ? to : ""));

    json_t *interactive = json_object();
    json_set(interactive, "type", json_string("button"));

    if (header) {
        json_t *hdr = json_object();
        json_set(hdr, "type", json_string("text"));
        json_set(hdr, "text", json_string(header));
        json_set(interactive, "header", hdr);
    }
    json_t *b = json_object();
    json_set(b, "text", json_string(body_text ? body_text : ""));
    json_set(interactive, "body", b);
    if (footer) {
        json_t *ft = json_object();
        json_set(ft, "text", json_string(footer));
        json_set(interactive, "footer", ft);
    }
    if (buttons)
        json_set(interactive, "action", json_copy(buttons));
    json_set(msg, "interactive", interactive);
    return msg;
}

static bool test_interactive_buttons(void) {
    json_t *buttons_arr = json_array();
    json_t *btn1 = json_object();
    json_set(btn1, "type", json_string("reply"));
    json_set(btn1, "reply", json_object());
    json_set(json_obj_get(btn1, "reply"), "id", json_string("btn_yes"));
    json_set(json_obj_get(btn1, "reply"), "title", json_string("Yes"));
    json_append(buttons_arr, btn1);

    json_t *btn2 = json_object();
    json_set(btn2, "type", json_string("reply"));
    json_set(btn2, "reply", json_object());
    json_set(json_obj_get(btn2, "reply"), "id", json_string("btn_no"));
    json_set(json_obj_get(btn2, "reply"), "title", json_string("No"));
    json_append(buttons_arr, btn2);

    json_t *msg = make_interactive_buttons("15551234567", "Confirm?",
                                            "Would you like to proceed?",
                                            "Tap a button", buttons_arr);
    char *out = json_serialize(msg);
    ASSERT(out != NULL, "interactive buttons serialize");
    ASSERT(strstr(out, "\"type\":\"button\"") != NULL, "interactive type");
    ASSERT(strstr(out, "Confirm?") != NULL, "header text");
    ASSERT(strstr(out, "Would you like to proceed?") != NULL, "body text");
    ASSERT(strstr(out, "Tap a button") != NULL, "footer text");
    ASSERT(strstr(out, "btn_yes") != NULL, "button 1 id");
    ASSERT(strstr(out, "\"title\":\"Yes\"") != NULL, "button 1 title");
    ASSERT(strstr(out, "btn_no") != NULL, "button 2 id");

    free(out);
    json_free(msg);
    json_free(buttons_arr);
    return true;
}

static bool test_interactive_no_header(void) {
    json_t *buttons = json_array();
    json_t *btn = json_object();
    json_set(btn, "type", json_string("reply"));
    json_set(btn, "reply", json_object());
    json_set(json_obj_get(btn, "reply"), "id", json_string("ok"));
    json_set(json_obj_get(btn, "reply"), "title", json_string("OK"));
    json_append(buttons, btn);

    json_t *msg = make_interactive_buttons("15551234567", NULL,
                                            "Just body", NULL, buttons);
    char *out = json_serialize(msg);
    ASSERT(out != NULL, "no header serialize");
    ASSERT(strstr(out, "\"header\"") == NULL, "no header key when NULL");
    ASSERT(strstr(out, "\"footer\"") == NULL, "no footer key when NULL");
    ASSERT(strstr(out, "Just body") != NULL, "body present");
    free(out);
    json_free(msg);
    json_free(buttons);
    return true;
}

/* ──────────────────────────────────────────────
 *  Interactive list message
 * ────────────────────────────────────────────── */

static json_t *make_list_row(const char *id, const char *title,
                              const char *desc) {
    json_t *row = json_object();
    json_set(row, "id", json_string(id ? id : ""));
    json_set(row, "title", json_string(title ? title : ""));
    if (desc)
        json_set(row, "description", json_string(desc));
    return row;
}

static json_t *make_list_msg(const char *to, const char *body_text,
                              const char *button_text,
                              const char *section_title,
                              json_t *rows) {
    json_t *msg = json_object();
    json_set(msg, "messaging_product", json_string("whatsapp"));
    json_set(msg, "to", json_string(to ? to : ""));

    json_t *interactive = json_object();
    json_set(interactive, "type", json_string("list"));

    json_t *b = json_object();
    json_set(b, "text", json_string(body_text ? body_text : ""));
    json_set(interactive, "body", b);

    json_t *action = json_object();
    json_set(action, "button", json_string(button_text ? button_text : ""));

    json_t *section = json_object();
    if (section_title)
        json_set(section, "title", json_string(section_title));
    if (rows)
        json_set(section, "rows", json_copy(rows));

    json_t *sections = json_array();
    json_append(sections, section);
    json_set(action, "sections", sections);
    json_set(interactive, "action", action);
    json_set(msg, "interactive", interactive);
    return msg;
}

static bool test_list_message(void) {
    json_t *rows = json_array();

    json_t *r1 = make_list_row("opt_1", "Option 1", "First choice");
    json_append(rows, r1);

    json_t *r2 = make_list_row("opt_2", "Option 2", NULL);
    json_append(rows, r2);

    json_t *msg = make_list_msg("15551234567",
                                 "Choose an option:",
                                 "View Options",
                                 "Available choices",
                                 rows);
    char *out = json_serialize(msg);
    ASSERT(out != NULL, "list msg serialize");
    ASSERT(strstr(out, "\"type\":\"list\"") != NULL, "type list");
    ASSERT(strstr(out, "Choose an option:") != NULL, "body text");
    ASSERT(strstr(out, "View Options") != NULL, "button text");
    ASSERT(strstr(out, "Available choices") != NULL, "section title");
    ASSERT(strstr(out, "opt_1") != NULL, "row 1 id");
    ASSERT(strstr(out, "Option 1") != NULL, "row 1 title");
    ASSERT(strstr(out, "First choice") != NULL, "row 1 desc");
    ASSERT(strstr(out, "opt_2") != NULL, "row 2 id");
    ASSERT(strstr(out, "\"description\"") != NULL, "row has desc");
    free(out);

    json_free(msg);
    json_free(rows);
    return true;
}

static bool test_list_no_section_title(void) {
    json_t *rows = json_array();
    json_t *r = make_list_row("opt_a", "Option A", NULL);
    json_append(rows, r);

    json_t *msg = make_list_msg("15551234567", "Pick", "Go", NULL, rows);
    char *out = json_serialize(msg);
    ASSERT(out != NULL, "no section title serialize");
    /* Parse and verify section has no title key */
    json_t *parsed = json_parse(out, NULL);
    ASSERT(parsed != NULL, "valid JSON");
    json_t *interactive = json_obj_get(parsed, "interactive");
    ASSERT(interactive != NULL, "interactive wrapper");
    json_t *action = json_obj_get(interactive, "action");
    ASSERT(action != NULL, "action wrapper");
    json_t *sections = json_obj_get(action, "sections");
    ASSERT(sections != NULL, "sections array");
    ASSERT(json_len(sections) >= 1, "at least 1 section");
    json_t *section = json_get(sections, 0);
    ASSERT(section != NULL, "section exists");
    ASSERT(json_obj_get(section, "title") == NULL, "section title absent when NULL");
    ASSERT(strstr(out, "Pick") != NULL, "body present");
    json_free(parsed);
    free(out);

    json_free(msg);
    json_free(rows);
    return true;
}

/* ──────────────────────────────────────────────
 *  URL building
 * ────────────────────────────────────────────── */

static bool test_url_building(void) {
    char url[2048];

    /* https://graph.facebook.com/v22.0/{phone_id}/messages */
    snprintf(url, sizeof(url), "%s/%s/%s/messages",
             "https://graph.facebook.com", "v22.0", "123456789");
    ASSERT(strstr(url, "graph.facebook.com") != NULL, "graph URL");
    ASSERT(strstr(url, "v22.0") != NULL, "API version");
    ASSERT(strstr(url, "123456789") != NULL, "phone ID");
    ASSERT(strstr(url, "/messages") != NULL, "messages endpoint");

    /* Custom API version */
    snprintf(url, sizeof(url), "%s/%s/%s/messages",
             "https://graph.facebook.com", "v21.0", "987654321");
    ASSERT(strstr(url, "v21.0") != NULL, "custom version");
    ASSERT(strstr(url, "987654321") != NULL, "other phone ID");

    return true;
}

/* ──────────────────────────────────────────────
 *  Webhook verification response
 * ────────────────────────────────────────────── */

static bool test_webhook_verify(void) {
    /* WhatsApp webhook verify challenge response */
    json_t *resp = json_object();
    json_set(resp, "hub.challenge", json_string("12345678"));

    char *out = json_serialize(resp);
    ASSERT(out != NULL, "webhook verify serialize");
    ASSERT(strstr(out, "12345678") != NULL, "challenge value");
    free(out);
    json_free(resp);

    /* Standard success response from messages API */
    json_t *success = json_object();
    json_set(success, "messaging_product", json_string("whatsapp"));
    json_set(success, "contacts", json_array());
    json_t *contact = json_object();
    json_set(contact, "input", json_string("15551234567"));
    json_set(contact, "wa_id", json_string("15551234567"));
    json_append(json_obj_get(success, "contacts"), contact);
    json_set(success, "messages", json_array());
    json_t *msg_resp = json_object();
    json_set(msg_resp, "id", json_string("wamid.test"));
    json_append(json_obj_get(success, "messages"), msg_resp);

    char *out2 = json_serialize(success);
    ASSERT(out2 != NULL, "success response serialize");
    ASSERT(strstr(out2, "wamid.test") != NULL, "message ID");
    ASSERT(strstr(out2, "15551234567") != NULL, "WA ID");
    free(out2);
    json_free(success);

    return true;
}

/* ──────────────────────────────────────────────
 *  Main
 * ────────────────────────────────────────────── */

int main(void) {
    printf("=== WhatsApp Message Format Tests (M10) ===\n");

    TEST(text_message);
    TEST(text_message_null_params);
    TEST(template_message);
    TEST(template_null_language);
    TEST(interactive_buttons);
    TEST(interactive_no_header);
    TEST(list_message);
    TEST(list_no_section_title);
    TEST(url_building);
    TEST(webhook_verify);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
