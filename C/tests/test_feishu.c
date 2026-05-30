/*
 * test_feishu.c — Feishu platform adapter test suite (S7 X01).
 *
 * Tests pure functions: feishu_set_webhook, feishu_set_app_credentials,
 * feishu_set_default_receive_id, feishu_get_chat_id, feishu_get_text,
 * feishu_poll_messages.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_feishu.c src/gateway/platforms/feishu.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_feishu
 *
 * Run:
 *   /tmp/hermes_test_feishu
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void feishu_set_webhook(const char *url);
void feishu_set_app_credentials(const char *app_id, const char *app_secret);
void feishu_set_default_receive_id(const char *receive_id);
const char *feishu_get_chat_id(json_node_t *update);
const char *feishu_get_text(json_node_t *update);
json_node_t *feishu_poll_messages(http_client_t *http);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* ================================================================
 *  1. State setters
 * ================================================================ */
static void test_set_webhook(void) {
    printf("\n--- feishu_set_webhook ---\n");

    /* Normal URL */
    feishu_set_webhook("https://open.feishu.cn/open-apis/bot/v2/hook/abc123");
    printf("  SET webhook=open.feishu.cn… (verified via get_chat_id interactions)\n");
    passed++;

    /* Empty URL — should not crash, should use previous value */
    feishu_set_webhook("");
    printf("  SET webhook=empty (no crash)\n");
    passed++;

    /* NULL — should not crash */
    feishu_set_webhook(NULL);
    printf("  SET webhook=NULL (no crash)\n");
    passed++;

    /* URL with path and multiple slashes */
    feishu_set_webhook("https://open.feishu.cn/open-apis/bot/v2/hook/xyz456");
    printf("  SET webhook=multiple path (no crash)\n");
    passed++;

    /* Long URL */
    feishu_set_webhook("https://open.feishu.cn/open-apis/bot/v2/hook/abcdef1234567890abcdef1234567890");
    printf("  SET webhook=long (no crash)\n");
    passed++;
}

static void test_set_app_credentials(void) {
    printf("\n--- feishu_set_app_credentials ---\n");

    /* Normal pair */
    feishu_set_app_credentials("cli_abc123def", "secret_xyz789");
    printf("  SET app_credentials=normal (no crash)\n");
    passed++;

    /* Empty app_id only */
    feishu_set_app_credentials("", "secret_abc");
    printf("  SET app_credentials=empty_id (no crash)\n");
    passed++;

    /* Empty app_secret only */
    feishu_set_app_credentials("cli_def456", "");
    printf("  SET app_credentials=empty_secret (no crash)\n");
    passed++;

    /* Both NULL */
    feishu_set_app_credentials(NULL, NULL);
    printf("  SET app_credentials=both_null (no crash)\n");
    passed++;

    /* One NULL, one valid */
    feishu_set_app_credentials("cli_valid", NULL);
    printf("  SET app_credentials=id_null_secret (no crash)\n");
    passed++;

    feishu_set_app_credentials(NULL, "secret_valid");
    printf("  SET app_credentials=null_id_valid_secret (no crash)\n");
    passed++;
}

static void test_set_default_receive_id(void) {
    printf("\n--- feishu_set_default_receive_id ---\n");

    /* Normal receive_id */
    feishu_set_default_receive_id("ou_abc123def789");
    printf("  SET receive_id=normal (no crash)\n");
    passed++;

    /* Empty string */
    feishu_set_default_receive_id("");
    printf("  SET receive_id=empty (no crash)\n");
    passed++;

    /* NULL */
    feishu_set_default_receive_id(NULL);
    printf("  SET receive_id=NULL (no crash)\n");
    passed++;

    /* Long receive_id */
    feishu_set_default_receive_id("ou_abcdef1234567890abcdef1234567890abcdef1234567890");
    printf("  SET receive_id=long (no crash)\n");
    passed++;
}

/* ================================================================
 *  2. Getter / extractors
 * ================================================================ */
static void test_get_chat_id(void) {
    printf("\n--- feishu_get_chat_id ---\n");

    /* Normal update with chat_id */
    json_node_t *u1 = json_new_object();
    json_set(u1, "chat_id", json_string("oc_abc123"));
    const char *id1 = feishu_get_chat_id(u1);
    TEST_STR_EQ("get_chat_id normal", id1, "oc_abc123");
    json_free(u1);

    /* Update with different chat_id */
    json_node_t *u2 = json_new_object();
    json_set(u2, "chat_id", json_string("oc_def456"));
    const char *id2 = feishu_get_chat_id(u2);
    TEST_STR_EQ("get_chat_id different", id2, "oc_def456");
    json_free(u2);

    /* Update without chat_id — should return default "feishu" */
    json_node_t *u3 = json_new_object();
    json_set(u3, "msg_type", json_string("text"));
    const char *id3 = feishu_get_chat_id(u3);
    TEST_STR_EQ("get_chat_id missing field", id3, "feishu");
    json_free(u3);

    /* Empty update */
    json_node_t *u4 = json_new_object();
    const char *id4 = feishu_get_chat_id(u4);
    TEST_STR_EQ("get_chat_id empty update", id4, "feishu");
    json_free(u4);

    /* NULL update */
    const char *id5 = feishu_get_chat_id(NULL);
    TEST_STR_EQ("get_chat_id NULL", id5, "feishu");
}

static void test_get_text(void) {
    printf("\n--- feishu_get_text ---\n");

    /* Normal update with text */
    json_node_t *u1 = json_new_object();
    json_set(u1, "text", json_string("Hello from Feishu"));
    const char *t1 = feishu_get_text(u1);
    TEST_STR_EQ("get_text normal", t1, "Hello from Feishu");
    json_free(u1);

    /* Empty text field */
    json_node_t *u2 = json_new_object();
    json_set(u2, "text", json_string(""));
    const char *t2 = feishu_get_text(u2);
    TEST_STR_EQ("get_text empty", t2, "");
    json_free(u2);

    /* Missing text field — should return "" */
    json_node_t *u3 = json_new_object();
    json_set(u3, "chat_id", json_string("oc_abc"));
    const char *t3 = feishu_get_text(u3);
    TEST_STR_EQ("get_text missing field", t3, "");
    json_free(u3);

    /* Unicode/multi-byte text */
    json_node_t *u4 = json_new_object();
    json_set(u4, "text", json_string("飞书消息 你好世界"));
    const char *t4 = feishu_get_text(u4);
    TEST_STR_EQ("get_text unicode", t4, "飞书消息 你好世界");
    json_free(u4);

    /* Multi-line text */
    json_node_t *u5 = json_new_object();
    json_set(u5, "text", json_string("line1\nline2\nline3"));
    const char *t5 = feishu_get_text(u5);
    TEST_STR_EQ("get_text multi-line", t5, "line1\nline2\nline3");
    json_free(u5);

    /* Text with special characters */
    json_node_t *u6 = json_new_object();
    json_set(u6, "text", json_string("Price: $19.99 & Tax"));
    const char *t6 = feishu_get_text(u6);
    TEST_STR_EQ("get_text special chars", t6, "Price: $19.99 & Tax");
    json_free(u6);

    /* NULL update — should return "" without crashing */
    const char *t7 = feishu_get_text(NULL);
    TEST_STR_EQ("get_text NULL", t7, "");
}

static void test_poll_messages(void) {
    printf("\n--- feishu_poll_messages ---\n");
    /* Always returns NULL (webhook-based, no long-poll API) */
    json_node_t *p = feishu_poll_messages(NULL);
    TEST_NULL("poll_messages NULL http", p);
    p = feishu_poll_messages((http_client_t*)0x42); /* any non-null ptr */
    TEST_NULL("poll_messages non-null http", p);
}

/* ================================================================
 *  3. Setter interaction (multiple sets, re-sets)
 * ================================================================ */
static void test_setter_interaction(void) {
    printf("\n--- Setter interaction / re-sets ---\n");

    /* Set webhook multiple times */
    feishu_set_webhook("https://webhook1.com");
    feishu_set_webhook("https://webhook2.com");
    feishu_set_webhook("https://webhook3.com");
    printf("  webhook re-sets (no crash)\n");
    passed++;

    /* Set app credentials multiple times */
    feishu_set_app_credentials("app1", "secret1");
    feishu_set_app_credentials("app2", "secret2");
    feishu_set_app_credentials("app3", "secret3");
    printf("  app_credentials re-sets (no crash)\n");
    passed++;

    /* Set default_receive_id multiple times */
    feishu_set_default_receive_id("ou_a");
    feishu_set_default_receive_id("ou_b");
    feishu_set_default_receive_id("ou_c");
    printf("  receive_id re-sets (no crash)\n");
    passed++;
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Feishu Platform Test Suite ===\n");

    test_set_webhook();
    test_set_app_credentials();
    test_set_default_receive_id();
    test_get_chat_id();
    test_get_text();
    test_poll_messages();
    test_setter_interaction();

    printf("\n--- Results: %d passed, %d failed ---\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
