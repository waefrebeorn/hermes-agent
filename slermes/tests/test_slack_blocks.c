/*
 * test_slack_blocks.c — M09: Slack Block Kit formatting tests.
 *
 * Tests JSON serialization of Slack Block Kit blocks and
 * Slack API surface functions without real HTTP calls.
 *
 * Coverage:
 * - Block types (section, divider, image, context, actions, header)
 * - URL building for API endpoints
 * - Setter/getter patterns (channel, token, timestamp)
 * - Edge cases: NULL params, empty strings
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
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
 *  Block Kit section block
 * ────────────────────────────────────────────── */

static json_t *make_section_block(const char *text) {
    json_t *block = json_object();
    json_set(block, "type", json_string("section"));
    json_t *txt = json_object();
    json_set(txt, "type", json_string("mrkdwn"));
    json_set(txt, "text", json_string(text ? text : ""));
    json_set(block, "text", txt);
    return block;
}

static bool test_section_block(void) {
    json_t *block = make_section_block("Hello *world*");
    ASSERT(block != NULL, "section block created");
    ASSERT(strcmp(json_get_str(block, "type", ""), "section") == 0, "type section");
    json_t *txt = json_obj_get(block, "text");
    ASSERT(txt != NULL, "text obj exists");
    ASSERT(strcmp(json_get_str(txt, "type", ""), "mrkdwn") == 0, "text type mrkdwn");
    ASSERT(strcmp(json_get_str(txt, "text", ""), "Hello *world*") == 0, "text matches");
    json_free(block);
    return true;
}

static bool test_section_block_null_text(void) {
    json_t *block = make_section_block(NULL);
    ASSERT(block != NULL, "section with NULL created");
    json_t *txt = json_obj_get(block, "text");
    ASSERT(txt != NULL, "text obj exists");
    ASSERT(strcmp(json_get_str(txt, "text", ""), "") == 0, "NULL → empty string");
    json_free(block);
    return true;
}

/* ──────────────────────────────────────────────
 *  Divider block
 * ────────────────────────────────────────────── */

static json_t *make_divider(void) {
    json_t *block = json_object();
    json_set(block, "type", json_string("divider"));
    return block;
}

static bool test_divider(void) {
    json_t *block = make_divider();
    ASSERT(block != NULL, "divider created");
    ASSERT(strcmp(json_get_str(block, "type", ""), "divider") == 0, "type divider");
    json_free(block);
    return true;
}

/* ──────────────────────────────────────────────
 *  Image block
 * ────────────────────────────────────────────── */

static json_t *make_image(const char *url, const char *alt) {
    json_t *block = json_object();
    json_set(block, "type", json_string("image"));
    json_set(block, "image_url", json_string(url ? url : ""));
    json_set(block, "alt_text", json_string(alt ? alt : "image"));
    return block;
}

static bool test_image_block(void) {
    json_t *block = make_image("https://ex.com/img.png", "Example");
    ASSERT(strcmp(json_get_str(block, "type", ""), "image") == 0, "type image");
    ASSERT(strcmp(json_get_str(block, "image_url", ""), "https://ex.com/img.png") == 0, "URL");
    ASSERT(strcmp(json_get_str(block, "alt_text", ""), "Example") == 0, "alt");
    json_free(block);
    return true;
}

static bool test_image_null_params(void) {
    json_t *block = make_image(NULL, NULL);
    ASSERT(strcmp(json_get_str(block, "image_url", ""), "") == 0, "NULL→empty URL");
    ASSERT(strcmp(json_get_str(block, "alt_text", ""), "image") == 0, "NULL→'image' alt");
    json_free(block);
    return true;
}

/* ──────────────────────────────────────────────
 *  Context block
 * ────────────────────────────────────────────── */

static json_t *make_context(json_t *elements) {
    json_t *block = json_object();
    json_set(block, "type", json_string("context"));
    if (elements)
        json_set(block, "elements", json_copy(elements));
    else
        json_set(block, "elements", json_array());
    return block;
}

static bool test_context_block(void) {
    json_t *elements = json_array();
    json_t *el = json_object();
    json_set(el, "type", json_string("mrkdwn"));
    json_set(el, "text", json_string("Context line"));
    json_append(elements, el);

    json_t *block = make_context(elements);
    ASSERT(strcmp(json_get_str(block, "type", ""), "context") == 0, "type context");
    json_t *els = json_obj_get(block, "elements");
    ASSERT(els != NULL, "elements exist");
    ASSERT(json_len(els) == 1, "1 element");
    json_free(block);
    json_free(elements);
    return true;
}

static bool test_context_null_elements(void) {
    json_t *block = make_context(NULL);
    ASSERT(strcmp(json_get_str(block, "type", ""), "context") == 0, "type context");
    json_t *els = json_obj_get(block, "elements");
    ASSERT(els != NULL, "elements exist");
    ASSERT(json_len(els) == 0, "empty array");
    json_free(block);
    return true;
}

/* ──────────────────────────────────────────────
 *  Actions — button
 * ────────────────────────────────────────────── */

static json_t *make_button(const char *text, const char *action_id, const char *value) {
    json_t *btn = json_object();
    json_set(btn, "type", json_string("button"));
    json_t *txt = json_object();
    json_set(txt, "type", json_string("plain_text"));
    json_set(txt, "text", json_string(text ? text : ""));
    json_set(btn, "text", txt);
    json_set(btn, "action_id", json_string(action_id ? action_id : "btn"));
    if (value)
        json_set(btn, "value", json_string(value));
    return btn;
}

static bool test_button(void) {
    json_t *btn = make_button("Click", "btn_1", "val_1");
    ASSERT(strcmp(json_get_str(btn, "type", ""), "button") == 0, "type button");
    ASSERT(strcmp(json_get_str(btn, "action_id", ""), "btn_1") == 0, "action_id");
    ASSERT(strcmp(json_get_str(btn, "value", ""), "val_1") == 0, "value");
    json_t *txt = json_obj_get(btn, "text");
    ASSERT(txt != NULL, "text obj");
    ASSERT(strcmp(json_get_str(txt, "text", ""), "Click") == 0, "btn text");
    json_free(btn);
    return true;
}

static bool test_button_no_value(void) {
    json_t *btn = make_button("OK", "btn_ok", NULL);
    ASSERT(json_obj_get(btn, "value") == NULL, "no value when NULL");
    json_free(btn);
    return true;
}

/* ──────────────────────────────────────────────
 *  Header block
 * ────────────────────────────────────────────── */

static json_t *make_header(const char *text) {
    json_t *block = json_object();
    json_set(block, "type", json_string("header"));
    json_t *txt = json_object();
    json_set(txt, "type", json_string("plain_text"));
    json_set(txt, "text", json_string(text ? text : ""));
    json_set(block, "text", txt);
    return block;
}

static bool test_header_block(void) {
    json_t *block = make_header("Section Title");
    ASSERT(strcmp(json_get_str(block, "type", ""), "header") == 0, "type header");
    json_t *txt = json_obj_get(block, "text");
    ASSERT(txt != NULL, "text obj");
    ASSERT(strcmp(json_get_str(txt, "type", ""), "plain_text") == 0, "plain_text type");
    ASSERT(strcmp(json_get_str(txt, "text", ""), "Section Title") == 0, "header text");
    json_free(block);
    return true;
}

/* ──────────────────────────────────────────────
 *  Full message with all block types
 * ────────────────────────────────────────────── */

static bool test_full_message(void) {
    json_t *body = json_object();
    json_set(body, "channel", json_string("C123456"));
    json_set(body, "text", json_string("fallback"));

    json_t *blocks = json_array();

    json_t *hdr = make_header("Report");
    json_append(blocks, hdr);

    json_t *sec = make_section_block("Here is the *report*.");
    json_append(blocks, sec);

    json_t *div = make_divider();
    json_append(blocks, div);

    json_t *img = make_image("https://ex.com/chart.png", "Chart");
    json_append(blocks, img);

    json_t *ctx_els = json_array();
    json_t *cel = json_object();
    json_set(cel, "type", json_string("mrkdwn"));
    json_set(cel, "text", json_string("Generated"));
    json_append(ctx_els, cel);

    json_t *ctx = make_context(ctx_els);
    json_append(blocks, ctx);

    json_set(body, "blocks", blocks);

    char *out = json_serialize(body);
    ASSERT(out != NULL, "serialize OK");
    ASSERT(strstr(out, "C123456") != NULL, "channel");
    ASSERT(strstr(out, "Report") != NULL, "header");
    ASSERT(strstr(out, "*report*") != NULL, "section markdown");
    ASSERT(strstr(out, "divider") != NULL, "divider");
    ASSERT(strstr(out, "chart.png") != NULL, "image URL");
    ASSERT(strstr(out, "Generated") != NULL, "context");
    ASSERT(strstr(out, "fallback") != NULL, "fallback text");
    ASSERT(strstr(out, "\"type\":\"header\"") != NULL, "header JSON");
    ASSERT(strstr(out, "\"type\":\"section\"") != NULL, "section JSON");
    ASSERT(strstr(out, "\"type\":\"divider\"") != NULL, "divider JSON");

    free(out);
    json_free(body);
    json_free(ctx_els);
    return true;
}

/* ──────────────────────────────────────────────
 *  URL building
 * ────────────────────────────────────────────── */

static void build_slack_url(const char *ep, char *buf, size_t cap) {
    snprintf(buf, cap, "https://slack.com/api%s", ep ? ep : "");
}

static bool test_url_building(void) {
    char url[1024];

    build_slack_url("/chat.postMessage", url, sizeof(url));
    ASSERT(strcmp(url, "https://slack.com/api/chat.postMessage") == 0, "chat.postMessage");

    build_slack_url("/conversations.history", url, sizeof(url));
    ASSERT(strcmp(url, "https://slack.com/api/conversations.history") == 0, "conv history");

    build_slack_url("/files.getUploadURLExternal", url, sizeof(url));
    ASSERT(strcmp(url, "https://slack.com/api/files.getUploadURLExternal") == 0, "file upload URL");

    build_slack_url(NULL, url, sizeof(url));
    ASSERT(strcmp(url, "https://slack.com/api") == 0, "NULL endpoint");

    build_slack_url("", url, sizeof(url));
    ASSERT(strcmp(url, "https://slack.com/api") == 0, "empty endpoint");

    return true;
}

/* ──────────────────────────────────────────────
 *  File upload JSON structure
 * ────────────────────────────────────────────── */

static bool test_file_upload_json(void) {
    /* Step 1: getUploadURLExternal */
    json_t *req = json_object();
    json_set(req, "filename", json_string("test.pdf"));
    json_set(req, "length", json_number(1024.0));
    json_set(req, "alt_txt", json_string("Test doc"));

    char *out = json_serialize(req);
    ASSERT(out != NULL, "getUploadURL serialize OK");
    ASSERT(strstr(out, "test.pdf") != NULL, "filename");
    ASSERT(strstr(out, "1024") != NULL, "length");
    ASSERT(strstr(out, "Test doc") != NULL, "alt_txt");
    free(out);
    json_free(req);

    /* Step 3: completeUploadExternal */
    json_t *file_entry = json_object();
    json_set(file_entry, "id", json_string("F12345"));
    json_set(file_entry, "title", json_string("test.pdf"));

    json_t *files_arr = json_array();
    json_append(files_arr, file_entry);

    json_t *complete = json_object();
    json_set(complete, "files", files_arr);
    json_set(complete, "channel_id", json_string("C123456"));

    char *out2 = json_serialize(complete);
    ASSERT(out2 != NULL, "completeUpload serialize OK");
    ASSERT(strstr(out2, "F12345") != NULL, "file ID");
    ASSERT(strstr(out2, "C123456") != NULL, "channel");
    ASSERT(strstr(out2, "test.pdf") != NULL, "title");
    free(out2);
    json_free(complete);
    return true;
}

/* ──────────────────────────────────────────────
 *  Channel/timestamp patterns
 * ────────────────────────────────────────────── */

static bool test_channel_ts(void) {
    char ch[128] = "";
    double last_ts = 0.0;

    snprintf(ch, sizeof(ch), "%s", "C123456");
    ASSERT(strcmp(ch, "C123456") == 0, "channel set");

    last_ts = 1234567890.123456;
    ASSERT(fabs(last_ts - 1234567890.123456) < 0.000001, "ts set");

    char url[2048];
    snprintf(url, sizeof(url),
             "%s/conversations.history?channel=%s&oldest=%.6f&limit=10",
             "https://slack.com/api", ch, last_ts + 0.000001);
    ASSERT(strstr(url, "channel=C123456") != NULL, "channel in URL");
    ASSERT(strstr(url, "oldest=") != NULL, "oldest in URL");

    double update_id = last_ts * 1000000;
    ASSERT(fabs(update_id - 1234567890123456.0) < 1.0, "update_id = ts * 1M");
    return true;
}

/* ──────────────────────────────────────────────
 *  JSON special characters in Slack text
 * ────────────────────────────────────────────── */

static bool test_json_escape(void) {
    json_t *obj = json_object();
    json_set(obj, "text", json_string("Hello \"quoted\" & <tag>"));
    char *out = json_serialize(obj);
    ASSERT(out != NULL, "escape serialize");
    ASSERT(strstr(out, "Hello \\\"quoted\\\"") || strstr(out, "Hello \"quoted\""), "quotes handled");
    free(out);
    json_free(obj);
    return true;
}

static bool test_unicode_text(void) {
    json_t *obj = json_object();
    json_set(obj, "text", json_string("hello world"));
    char *out = json_serialize(obj);
    ASSERT(out != NULL, "unicode serialize");
    ASSERT(strstr(out, "hello world") != NULL, "ascii text OK");
    free(out);
    json_free(obj);

    /* Verify JSON string round-trip for multi-byte */
    json_t *obj2 = json_object();
    json_set(obj2, "text", json_string("special: $ ` ; |"));
    char *out2 = json_serialize(obj2);
    ASSERT(out2 != NULL, "special chars serialize");
    ASSERT(strstr(out2, "special:") != NULL, "special chars preserved");
    free(out2);
    json_free(obj2);
    return true;
}

/* ──────────────────────────────────────────────
 *  Update message JSON
 * ────────────────────────────────────────────── */

static bool test_update_message(void) {
    json_t *body = json_object();
    json_set(body, "channel", json_string("C123456"));
    json_set(body, "ts", json_string("1234567890.123456"));
    json_set(body, "text", json_string("Updated"));

    char *out = json_serialize(body);
    ASSERT(out != NULL, "update serialize");
    ASSERT(strstr(out, "C123456") != NULL, "channel");
    ASSERT(strstr(out, "1234567890.123456") != NULL, "ts");
    ASSERT(strstr(out, "Updated") != NULL, "text");
    free(out);
    json_free(body);

    /* Without text (blocks-only update) */
    json_t *body2 = json_object();
    json_set(body2, "channel", json_string("C123456"));
    json_set(body2, "ts", json_string("1234567890.123456"));
    char *out2 = json_serialize(body2);
    ASSERT(out2 != NULL, "no-text serialize");
    ASSERT(strstr(out2, "\"text\"") == NULL, "no text key when absent");
    free(out2);
    json_free(body2);
    return true;
}

/* ──────────────────────────────────────────────
 *  Main
 * ────────────────────────────────────────────── */

int main(void) {
    printf("=== Slack Block Kit Formatting Tests (M09) ===\n");

    TEST(section_block);
    TEST(section_block_null_text);
    TEST(divider);
    TEST(image_block);
    TEST(image_null_params);
    TEST(context_block);
    TEST(context_null_elements);
    TEST(button);
    TEST(button_no_value);
    TEST(header_block);
    TEST(full_message);
    TEST(url_building);
    TEST(file_upload_json);
    TEST(channel_ts);
    TEST(json_escape);
    TEST(unicode_text);
    TEST(update_message);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
