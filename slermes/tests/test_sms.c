/*
 * test_sms.c — SMS/MMS gateway platform unit tests.
 *
 * Tests: setters, webhook parsing, queue operations, update extractors,
 * webhook verification. These functions are pure logic with no Twilio
 * network dependency.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_sms.c src/gateway/platforms/sms.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_sms \
 *       -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_sms
 */
#include "hermes.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Functions under test */
void sms_set_twilio(const char *sid, const char *token, const char *from);
void sms_set_status_callback(const char *url);
void sms_set_webhook_url(const char *url);
json_node_t *sms_parse_webhook(const char *body);
const char *sms_verify_webhook(const char *query_string);
void sms_queue_message(const char *chat_id, const char *text, const char *sender_id);
json_node_t *sms_poll_messages(http_client_t *http);
const char *sms_get_chat_id(json_node_t *update);
const char *sms_get_text(json_node_t *update);
const char *sms_get_message_sid(json_node_t *update);
const char *sms_get_status(json_node_t *update);
const char *sms_get_media_url(json_node_t *update, size_t index);
size_t sms_get_num_media(json_node_t *update);
void sms_handle_webhook(const char *body);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== SMS Gateway Tests ===\n");

    /* Test 1: sms_verify_webhook returns "OK" for any input */
    {
        const char *r = sms_verify_webhook(NULL);
        TEST("verify_webhook(NULL) returns 'OK'", r && strcmp(r, "OK") == 0);
    }

    /* Test 2: sms_verify_webhook with non-NULL query string */
    {
        const char *r = sms_verify_webhook("From=%2B1234567890&To=%2B0987654321");
        TEST("verify_webhook(query) returns 'OK'", r && strcmp(r, "OK") == 0);
    }

    /* Test 3: sms_parse_webhook with NULL */
    {
        json_node_t *r = sms_parse_webhook(NULL);
        TEST("parse_webhook(NULL) returns NULL", r == NULL);
    }

    /* Test 4: sms_parse_webhook with empty body */
    {
        json_node_t *r = sms_parse_webhook("");
        TEST("parse_webhook('') returns NULL", r == NULL);
    }

    /* Test 5: sms_parse_webhook with inbound SMS body */
    {
        json_node_t *r = sms_parse_webhook(
            "From=%2B1234567890&To=%2B0987654321&Body=Hello+world&MessageSid=SM123"
            "&NumMedia=1&MediaUrl0=https://example.com/img.jpg"
            "&MediaContentType0=image/jpeg");
        TEST("parse_webhook(inbound) returns non-NULL", r != NULL);
        if (r) {
            size_t len = json_len(r);
            TEST("inbound has 1 update", len == 1);
            json_node_t *u = json_get(r, 0);
            TEST("update not NULL", u != NULL);
            if (u) {
                const char *from = sms_get_chat_id(u);
                TEST("chat_id is +1234567890", from && strcmp(from, "+1234567890") == 0);
                const char *text = sms_get_text(u);
                TEST("text is 'Hello world'", text && strcmp(text, "Hello world") == 0);
                const char *sid = sms_get_message_sid(u);
                TEST("message_sid is SM123", sid && strcmp(sid, "SM123") == 0);
                size_t nm = sms_get_num_media(u);
                TEST("num_media is 1", nm == 1);
                const char *murl = sms_get_media_url(u, 0);
                TEST("media_url[0] is correct", murl && strcmp(murl, "https://example.com/img.jpg") == 0);
                const char *murl2 = sms_get_media_url(u, 99);
                TEST("media_url[99] returns NULL", murl2 == NULL);
            }
            json_free(r);
        }
    }

    /* Test 6: sms_parse_webhook with status callback body */
    {
        json_node_t *r = sms_parse_webhook(
            "MessageSid=SM999&MessageStatus=delivered&To=%2B0987654321"
            "&From=%2B1234567890&SmsStatus=delivered&AccountSid=AC123");
        TEST("parse_webhook(status) returns non-NULL", r != NULL);
        if (r) {
            size_t len = json_len(r);
            TEST("status has 1 update", len == 1);
            json_node_t *u = json_get(r, 0);
            const char *status = sms_get_status(u);
            TEST("status is 'delivered'", status && strcmp(status, "delivered") == 0);
            const char *sid = sms_get_message_sid(u);
            TEST("message_sid is SM999", sid && strcmp(sid, "SM999") == 0);
            const char *from = sms_get_chat_id(u);
            TEST("from is +1234567890", from && strcmp(from, "+1234567890") == 0);
            json_free(r);
        }
    }

    /* Test 7: sms_parse_webhook with status error body */
    {
        json_node_t *r = sms_parse_webhook(
            "MessageSid=SM555&MessageStatus=failed&ErrorCode=30007"
            "&ErrorMessage=Carrier+violation");
        TEST("parse_webhook(error) returns non-NULL", r != NULL);
        if (r) {
            json_node_t *u = json_get(r, 0);
            const char *status = sms_get_status(u);
            TEST("error status is 'failed'", status && strcmp(status, "failed") == 0);
            const char *ec = json_get_str(u, "error_code", "");
            TEST("error_code is 30007", strcmp(ec, "30007") == 0);
            const char *em = json_get_str(u, "error_message", "");
            TEST("error_message contains 'violation'", strstr(em, "violation") != NULL);
            json_free(r);
        }
    }

    /* Test 8: sms_parse_webhook with multiple MediaUrl entries */
    {
        json_node_t *r = sms_parse_webhook(
            "From=A&To=B&Body=C&NumMedia=2"
            "&MediaUrl0=https://a.com/1.jpg&MediaUrl1=https://a.com/2.jpg");
        TEST("parse_webhook(multi-media) returns non-NULL", r != NULL);
        if (r) {
            json_node_t *u = json_get(r, 0);
            size_t nm = sms_get_num_media(u);
            TEST("num_media is 2", nm == 2);
            const char *m0 = sms_get_media_url(u, 0);
            TEST("media_url[0] is 1.jpg", m0 && strstr(m0, "1.jpg") != NULL);
            const char *m1 = sms_get_media_url(u, 1);
            TEST("media_url[1] is 2.jpg", m1 && strstr(m1, "2.jpg") != NULL);
            json_free(r);
        }
    }

    /* Test 9: sms_parse_webhook with URL-encoded special chars */
    {
        json_node_t *r = sms_parse_webhook(
            "From=%2B123&To=%2B456&Body=AT%26T+test%3F%21"
            "&MessageSid=SM777");
        TEST("parse_webhook(encoded chars) returns non-NULL", r != NULL);
        if (r) {
            json_node_t *u = json_get(r, 0);
            const char *text = sms_get_text(u);
            /* Body was AT%26T+test%3F%21 → "AT&T test?!" */
            TEST("encoded body decoded correctly", text && strcmp(text, "AT&T test?!") == 0);
            json_free(r);
        }
    }

    /* Test 10: sms_handle_webhook + queue + poll cycle */
    {
        /* Queue a message directly */
        sms_queue_message("+1234567890", "direct queue message", "+0987654321");

        /* Also queue via handle_webhook */
        sms_handle_webhook(
            "From=%2B5551111&To=%2B5552222&Body=webhook+message&MessageSid=SM444");

        /* Poll the queue — should get both messages */
        json_node_t *msgs = sms_poll_messages(NULL);
        TEST("poll_messages returns non-NULL after queue", msgs != NULL);
        if (msgs) {
            size_t len = json_len(msgs);
            TEST("polled 2 messages", len == 2);
            if (len >= 1) {
                json_node_t *m0 = json_get(msgs, 0);
                const char *t0 = sms_get_text(m0);
                TEST("msg[0] text is 'direct queue message'", t0 && strcmp(t0, "direct queue message") == 0);
            }
            if (len >= 2) {
                json_node_t *m1 = json_get(msgs, 1);
                const char *t1 = sms_get_text(m1);
                TEST("msg[1] text is 'webhook message'", t1 && strcmp(t1, "webhook message") == 0);
            }
            json_free(msgs);
        }

        /* After poll, queue should be empty */
        json_node_t *empty = sms_poll_messages(NULL);
        TEST("queue empty after drain", empty == NULL);
    }

    /* Test 11: sms_queue_message with NULL values */
    {
        sms_queue_message(NULL, NULL, NULL);
        json_node_t *msgs = sms_poll_messages(NULL);
        TEST("queue accepts NULL params", msgs != NULL);
        if (msgs) {
            json_node_t *m = json_get(msgs, 0);
            const char *cid = sms_get_chat_id(m);
            const char *txt = sms_get_text(m);
            /* chat_id defaults to "sms" when NULL sender_id */
            TEST("NULL chat_id defaults to 'sms'", cid && strcmp(cid, "sms") == 0);
            TEST("NULL text defaults to empty", txt && txt[0] == '\0');
            json_free(msgs);
        }
    }

    /* Test 12: sms_get_media_url with NULL update */
    {
        const char *m = sms_get_media_url(NULL, 0);
        TEST("get_media_url(NULL, 0) returns NULL", m == NULL);
    }

    /* Test 13: sms_get_num_media with no media field */
    {
        json_node_t *r = sms_parse_webhook("From=%2B123&To=%2B456&Body=test&MessageSid=SM111");
        TEST("parse_webhook(no media) returns non-NULL", r != NULL);
        if (r) {
            json_node_t *u = json_get(r, 0);
            size_t nm = sms_get_num_media(u);
            TEST("num_media is 0 when absent", nm == 0);
            json_free(r);
        }
    }

    /* Test 14: sms_handle_webhook with NULL */
    {
        sms_handle_webhook(NULL);
        /* Should not crash */
        TEST("handle_webhook(NULL) no crash", 1);
    }

    /* Test 15: sms_parse_webhook with malformed input */
    {
        json_node_t *r = sms_parse_webhook("no equals sign");
        TEST("parse_webhook(malformed) returns non-NULL (graceful)", r != NULL);
        if (r) {
            /* Should have default values */
            json_node_t *u = json_get(r, 0);
            TEST("malformed has default from", u != NULL);
            json_free(r);
        }
    }

    printf("=== SMS Tests: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
