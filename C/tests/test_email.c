/*
 * test_email.c — Email platform adapter test suite (S7 X01).
 *
 * Tests pure functions: email_set_from, email_get_chat_id,
 * email_get_text, email_get_html, email_get_subject,
 * email_get_attachments, email_get_thread_id, email_get_message_id,
 * email_get_references.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_email.c src/gateway/platforms/email.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_email
 *
 * Run:
 *   /tmp/hermes_test_email
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void email_set_from(const char *from);
const char *email_get_chat_id(json_node_t *update);
const char *email_get_text(json_node_t *update);
const char *email_get_html(json_node_t *update);
const char *email_get_subject(json_node_t *update);
json_node_t *email_get_attachments(json_node_t *update);
const char *email_get_thread_id(json_node_t *update);
const char *email_get_message_id(json_node_t *update);
const char *email_get_references(json_node_t *update);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* ================================================================
 *  1. Setter: email_set_from
 * ================================================================ */
static void test_set_from(void) {
    printf("\n--- email_set_from ---\n");

    /* Normal email */
    email_set_from("hermes@example.com");
    printf("  SET from=normal (no crash)\n");
    passed++;

    /* Empty string */
    email_set_from("");
    printf("  SET from=empty (no crash)\n");
    passed++;

    /* NULL */
    email_set_from(NULL);
    printf("  SET from=NULL (no crash)\n");
    passed++;

    /* Long email */
    email_set_from("very.long.email.address.with.many.parts@really-long-domain-name.example.com");
    printf("  SET from=long (no crash)\n");
    passed++;

    /* Special characters */
    email_set_from("user+tag@example.co.uk");
    printf("  SET from=special chars (no crash)\n");
    passed++;
}

/* ================================================================
 *  2. Extractors: get_chat_id
 * ================================================================ */
static void test_get_chat_id(void) {
    printf("\n--- email_get_chat_id ---\n");

    /* Normal update with chat_id */
    json_node_t *u1 = json_new_object();
    json_set(u1, "chat_id", json_string("user@example.com"));
    TEST_STR_EQ("get_chat_id normal", email_get_chat_id(u1), "user@example.com");
    json_free(u1);

    /* Different chat_id */
    json_node_t *u2 = json_new_object();
    json_set(u2, "chat_id", json_string("other@domain.org"));
    TEST_STR_EQ("get_chat_id different", email_get_chat_id(u2), "other@domain.org");
    json_free(u2);

    /* Missing chat_id — should return "" */
    json_node_t *u3 = json_new_object();
    json_set(u3, "subject", json_string("Re: hello"));
    TEST_STR_EQ("get_chat_id missing", email_get_chat_id(u3), "");
    json_free(u3);

    /* Empty update */
    json_node_t *u4 = json_new_object();
    TEST_STR_EQ("get_chat_id empty update", email_get_chat_id(u4), "");
    json_free(u4);

    /* NULL update */
    TEST_STR_EQ("get_chat_id NULL", email_get_chat_id(NULL), "");
}

/* ================================================================
 *  3. Extractors: get_text
 * ================================================================ */
static void test_get_text(void) {
    printf("\n--- email_get_text ---\n");

    /* Normal text */
    json_node_t *u1 = json_new_object();
    json_set(u1, "text", json_string("Hello, this is an email body"));
    TEST_STR_EQ("get_text normal", email_get_text(u1), "Hello, this is an email body");
    json_free(u1);

    /* Empty text */
    json_node_t *u2 = json_new_object();
    json_set(u2, "text", json_string(""));
    TEST_STR_EQ("get_text empty", email_get_text(u2), "");
    json_free(u2);

    /* Missing text */
    json_node_t *u3 = json_new_object();
    json_set(u3, "chat_id", json_string("user@example.com"));
    TEST_STR_EQ("get_text missing", email_get_text(u3), "");
    json_free(u3);

    /* Unicode text */
    json_node_t *u4 = json_new_object();
    json_set(u4, "text", json_string("邮件正文 你好世界"));
    TEST_STR_EQ("get_text unicode", email_get_text(u4), "邮件正文 你好世界");
    json_free(u4);

    /* Multi-line text */
    json_node_t *u5 = json_new_object();
    json_set(u5, "text", json_string("line1\nline2\nline3"));
    TEST_STR_EQ("get_text multi-line", email_get_text(u5), "line1\nline2\nline3");
    json_free(u5);

    /* Text with special characters */
    json_node_t *u6 = json_new_object();
    json_set(u6, "text", json_string("Price: $19.99 & Tax <overhead>"));
    TEST_STR_EQ("get_text special chars", email_get_text(u6), "Price: $19.99 & Tax <overhead>");
    json_free(u6);

    /* NULL update */
    TEST_STR_EQ("get_text NULL", email_get_text(NULL), "");
}

/* ================================================================
 *  4. Extractors: get_html
 * ================================================================ */
static void test_get_html(void) {
    printf("\n--- email_get_html ---\n");

    /* Normal HTML */
    json_node_t *u1 = json_new_object();
    json_set(u1, "html", json_string("<p>Hello</p>"));
    TEST_STR_EQ("get_html normal", email_get_html(u1), "<p>Hello</p>");
    json_free(u1);

    /* Empty HTML */
    json_node_t *u2 = json_new_object();
    json_set(u2, "html", json_string(""));
    TEST_STR_EQ("get_html empty", email_get_html(u2), "");
    json_free(u2);

    /* Missing HTML */
    json_node_t *u3 = json_new_object();
    json_set(u3, "text", json_string("plain text"));
    TEST_STR_EQ("get_html missing", email_get_html(u3), "");
    json_free(u3);

    /* Large-ish HTML */
    json_node_t *u4 = json_new_object();
    json_set(u4, "html", json_string("<html><body><div><p>Styled email content</p></div></body></html>"));
    TEST_STR_EQ("get_html complex", email_get_html(u4), "<html><body><div><p>Styled email content</p></div></body></html>");
    json_free(u4);

    /* NULL update */
    TEST_STR_EQ("get_html NULL", email_get_html(NULL), "");
}

/* ================================================================
 *  5. Extractors: get_subject
 * ================================================================ */
static void test_get_subject(void) {
    printf("\n--- email_get_subject ---\n");

    /* Normal subject */
    json_node_t *u1 = json_new_object();
    json_set(u1, "subject", json_string("Meeting Tomorrow"));
    TEST_STR_EQ("get_subject normal", email_get_subject(u1), "Meeting Tomorrow");
    json_free(u1);

    /* Empty subject */
    json_node_t *u2 = json_new_object();
    json_set(u2, "subject", json_string(""));
    TEST_STR_EQ("get_subject empty", email_get_subject(u2), "");
    json_free(u2);

    /* Missing subject */
    json_node_t *u3 = json_new_object();
    json_set(u3, "text", json_string("body only"));
    TEST_STR_EQ("get_subject missing", email_get_subject(u3), "");
    json_free(u3);

    /* Subject with special chars */
    json_node_t *u4 = json_new_object();
    json_set(u4, "subject", json_string("Re: [Project] Q3 report (updated)"));
    TEST_STR_EQ("get_subject special", email_get_subject(u4), "Re: [Project] Q3 report (updated)");
    json_free(u4);

    /* Unicode subject */
    json_node_t *u5 = json_new_object();
    json_set(u5, "subject", json_string("项目更新 2024年Q3"));
    TEST_STR_EQ("get_subject unicode", email_get_subject(u5), "项目更新 2024年Q3");
    json_free(u5);

    /* NULL update */
    TEST_STR_EQ("get_subject NULL", email_get_subject(NULL), "");
}

/* ================================================================
 *  6. Extractors: get_attachments
 * ================================================================ */
static void test_get_attachments(void) {
    printf("\n--- email_get_attachments ---\n");

    /* Normal attachments array */
    json_node_t *u1 = json_new_object();
    json_node_t *att1 = json_new_array();
    json_node_t *a1 = json_new_object();
    json_set(a1, "filename", json_string("report.pdf"));
    json_set(a1, "mime_type", json_string("application/pdf"));
    json_append(att1, a1);
    json_node_t *a2 = json_new_object();
    json_set(a2, "filename", json_string("photo.jpg"));
    json_set(a2, "mime_type", json_string("image/jpeg"));
    json_append(att1, a2);
    json_set(u1, "attachments", att1);
    json_node_t *got1 = email_get_attachments(u1);
    TEST("get_attachments normal: not null", got1 != NULL);
    TEST("get_attachments normal: array length 2", got1 && json_len(got1) == 2);
    json_free(u1);

    /* Empty attachments array */
    json_node_t *u2 = json_new_object();
    json_set(u2, "attachments", json_new_array());
    json_node_t *got2 = email_get_attachments(u2);
    TEST("get_attachments empty array: not null", got2 != NULL);
    TEST("get_attachments empty array: length 0", got2 && json_len(got2) == 0);
    json_free(u2);

    /* Missing attachments field */
    json_node_t *u3 = json_new_object();
    json_set(u3, "text", json_string("no attachments"));
    json_node_t *got3 = email_get_attachments(u3);
    TEST_NULL("get_attachments missing field", got3);
    json_free(u3);

    /* Empty update */
    json_node_t *u4 = json_new_object();
    json_node_t *got4 = email_get_attachments(u4);
    TEST_NULL("get_attachments empty update", got4);
    json_free(u4);

    /* NULL update */
    json_node_t *got5 = email_get_attachments(NULL);
    TEST_NULL("get_attachments NULL update", got5);
}

/* ================================================================
 *  7. Extractors: get_thread_id
 * ================================================================ */
static void test_get_thread_id(void) {
    printf("\n--- email_get_thread_id ---\n");

    /* Normal in_reply_to */
    json_node_t *u1 = json_new_object();
    json_set(u1, "in_reply_to", json_string("<abc123@mail.example.com>"));
    TEST_STR_EQ("get_thread_id normal", email_get_thread_id(u1), "<abc123@mail.example.com>");
    json_free(u1);

    /* Empty in_reply_to */
    json_node_t *u2 = json_new_object();
    json_set(u2, "in_reply_to", json_string(""));
    TEST_STR_EQ("get_thread_id empty", email_get_thread_id(u2), "");
    json_free(u2);

    /* Missing in_reply_to */
    json_node_t *u3 = json_new_object();
    json_set(u3, "message_id", json_string("<xyz@mail.com>"));
    TEST_STR_EQ("get_thread_id missing", email_get_thread_id(u3), "");
    json_free(u3);

    /* Different thread ID format */
    json_node_t *u4 = json_new_object();
    json_set(u4, "in_reply_to", json_string("<thread-9876@list.example.org>"));
    TEST_STR_EQ("get_thread_id different", email_get_thread_id(u4), "<thread-9876@list.example.org>");
    json_free(u4);

    /* NULL update */
    TEST_STR_EQ("get_thread_id NULL", email_get_thread_id(NULL), "");
}

/* ================================================================
 *  8. Extractors: get_message_id
 * ================================================================ */
static void test_get_message_id(void) {
    printf("\n--- email_get_message_id ---\n");

    /* Normal message_id */
    json_node_t *u1 = json_new_object();
    json_set(u1, "message_id", json_string("<abc123@mail.example.com>"));
    TEST_STR_EQ("get_message_id normal", email_get_message_id(u1), "<abc123@mail.example.com>");
    json_free(u1);

    /* Empty message_id */
    json_node_t *u2 = json_new_object();
    json_set(u2, "message_id", json_string(""));
    TEST_STR_EQ("get_message_id empty", email_get_message_id(u2), "");
    json_free(u2);

    /* Missing message_id */
    json_node_t *u3 = json_new_object();
    json_set(u3, "subject", json_string("No ID"));
    TEST_STR_EQ("get_message_id missing", email_get_message_id(u3), "");
    json_free(u3);

    /* Different format */
    json_node_t *u4 = json_new_object();
    json_set(u4, "message_id", json_string("<20240530T123456Z@host.example.com>"));
    TEST_STR_EQ("get_message_id timestamp", email_get_message_id(u4), "<20240530T123456Z@host.example.com>");
    json_free(u4);

    /* NULL update */
    TEST_STR_EQ("get_message_id NULL", email_get_message_id(NULL), "");
}

/* ================================================================
 *  9. Extractors: get_references
 * ================================================================ */
static void test_get_references(void) {
    printf("\n--- email_get_references ---\n");

    /* Normal references */
    json_node_t *u1 = json_new_object();
    json_set(u1, "references", json_string("<parent@mail.com> <grandparent@mail.com>"));
    TEST_STR_EQ("get_references normal", email_get_references(u1), "<parent@mail.com> <grandparent@mail.com>");
    json_free(u1);

    /* Empty references */
    json_node_t *u2 = json_new_object();
    json_set(u2, "references", json_string(""));
    TEST_STR_EQ("get_references empty", email_get_references(u2), "");
    json_free(u2);

    /* Missing references */
    json_node_t *u3 = json_new_object();
    json_set(u3, "message_id", json_string("<only@mail.com>"));
    TEST_STR_EQ("get_references missing", email_get_references(u3), "");
    json_free(u3);

    /* Single reference */
    json_node_t *u4 = json_new_object();
    json_set(u4, "references", json_string("<single@mail.com>"));
    TEST_STR_EQ("get_references single", email_get_references(u4), "<single@mail.com>");
    json_free(u4);

    /* NULL update */
    TEST_STR_EQ("get_references NULL", email_get_references(NULL), "");
}

/* ================================================================
 *  10. Setter interaction (re-sets)
 * ================================================================ */
static void test_setter_interaction(void) {
    printf("\n--- Setter interaction / re-sets ---\n");

    email_set_from("first@example.com");
    email_set_from("second@example.com");
    email_set_from("third@example.com");
    printf("  from re-sets (no crash)\n");
    passed++;
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Email Platform Test Suite ===\n");

    test_set_from();
    test_get_chat_id();
    test_get_text();
    test_get_html();
    test_get_subject();
    test_get_attachments();
    test_get_thread_id();
    test_get_message_id();
    test_get_references();
    test_setter_interaction();

    printf("\n--- Results: %d passed, %d failed ---\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
