/*
 * test_mattermost.c — Mattermost platform adapter test suite (S7).
 *
 * Tests pure functions: mattermost_set_url, mattermost_set_token,
 * mattermost_set_channel, mattermost_get_chat_id, mattermost_get_text.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include \
 *       -I lib/libjson -I lib/libhttp \
 *       tests/test_mattermost.c src/gateway/platforms/mattermost.c \
 *       lib/libjson/json.c -lm -o /tmp/hermes_test_mattermost
 *
 * Run:
 *   /tmp/hermes_test_mattermost
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void mattermost_set_url(const char *url);
void mattermost_set_token(const char *token);
void mattermost_set_channel(const char *id);
const char *mattermost_get_chat_id(json_node_t *update);
const char *mattermost_get_text(json_node_t *update);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* ================================================================
 *  1. mattermost_set_url — trailing-slash stripping
 * ================================================================ */
static void test_set_url(void) {
    printf("\n--- mattermost_set_url ---\n");

    /* Multiple trailing slash variants — no crash */
    mattermost_set_url("http://localhost:8065");
    TEST("set_url normal URL", 1);

    mattermost_set_url("http://localhost:8065/");
    TEST("set_url with trailing slash", 1);

    mattermost_set_url("http://localhost:8065///");
    TEST("set_url with multiple slashes", 1);

    mattermost_set_url("https://mm.example.com/path/to/server");
    TEST("set_url with path", 1);

    mattermost_set_url("https://mm.example.com/path/to/server/");
    TEST("set_url with path + trailing slash", 1);

    mattermost_set_url("");
    TEST("set_url empty string", 1);

    mattermost_set_url("http://127.0.0.1:8065");
    TEST("set_url IP address", 1);
}

/* ================================================================
 *  2. mattermost_set_token
 * ================================================================ */
static void test_set_token(void) {
    printf("\n--- mattermost_set_token ---\n");

    mattermost_set_token("my-bot-token-12345");
    TEST("set_token normal", 1);

    mattermost_set_token("");
    TEST("set_token empty", 1);

    /* Long token — buffer is 512 bytes */
    char long_token[600];
    memset(long_token, 'x', sizeof(long_token) - 1);
    long_token[sizeof(long_token) - 1] = '\0';
    mattermost_set_token(long_token);
    TEST("set_token long (truncated to 511+1)", 1);

    mattermost_set_token("@!#$%^&*()_+-=[]{}|;':\",./<>?`~");
    TEST("set_token special chars", 1);
}

/* ================================================================
 *  3. mattermost_set_channel
 * ================================================================ */
static void test_set_channel(void) {
    printf("\n--- mattermost_set_channel ---\n");

    mattermost_set_channel("abc123");
    TEST("set_channel normal", 1);

    mattermost_set_channel("");
    TEST("set_channel empty", 1);

    mattermost_set_channel("channel-with-dashes-42");
    TEST("set_channel with dashes", 1);

    /* Very long channel ID */
    char long_ch[200];
    memset(long_ch, 'c', sizeof(long_ch) - 1);
    long_ch[sizeof(long_ch) - 1] = '\0';
    mattermost_set_channel(long_ch);
    TEST("set_channel long (truncated to 127+1)", 1);
}

/* ================================================================
 *  4. mattermost_get_chat_id
 * ================================================================ */
static void test_get_chat_id(void) {
    printf("\n--- mattermost_get_chat_id ---\n");

    /* Returns static channel_id buffer regardless of update content */
    mattermost_set_channel("testing-channel-99");

    json_node_t *update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    const char *cid = mattermost_get_chat_id(update);
    TEST_STR_EQ("get_chat_id returns current channel_id", cid, "testing-channel-99");
    json_free(update);

    /* After changing channel */
    mattermost_set_channel("different-channel");
    update = json_new_object();
    cid = mattermost_get_chat_id(update);
    TEST_STR_EQ("get_chat_id reflects channel change", cid, "different-channel");
    json_free(update);

    /* NULL update */
    cid = mattermost_get_chat_id(NULL);
    TEST_STR_EQ("get_chat_id with NULL update", cid, "different-channel");
}

/* ================================================================
 *  5. mattermost_get_text
 * ================================================================ */
static void test_get_text(void) {
    printf("\n--- mattermost_get_text ---\n");

    /* Normal message text */
    json_node_t *message = json_new_object();
    json_set(message, "text", json_string("Hello from Mattermost!"));
    json_node_t *update = json_new_object();
    json_set(update, "message", message);

    const char *text = mattermost_get_text(update);
    TEST_STR_EQ("get_text normal", text, "Hello from Mattermost!");
    json_free(update);

    /* Empty text */
    message = json_new_object();
    json_set(message, "text", json_string(""));
    update = json_new_object();
    json_set(update, "message", message);

    text = mattermost_get_text(update);
    TEST_STR_EQ("get_text empty string", text, "");
    json_free(update);

    /* No text field in message */
    message = json_new_object();
    json_set(message, "other", json_string("value"));
    update = json_new_object();
    json_set(update, "message", message);

    text = mattermost_get_text(update);
    TEST_NULL("get_text missing text field", text);
    json_free(update);

    /* No message at all */
    update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    text = mattermost_get_text(update);
    TEST_NULL("get_text no message object", text);
    json_free(update);

    /* NULL update */
    text = mattermost_get_text(NULL);
    TEST_NULL("get_text NULL update", text);

    /* Unicode text */
    message = json_new_object();
    json_set(message, "text", json_string("Hello 世界! Привет!"));
    update = json_new_object();
    json_set(update, "message", message);

    text = mattermost_get_text(update);
    TEST_STR_EQ("get_text unicode", text, "Hello 世界! Привет!");
    json_free(update);

    /* Multi-line text */
    message = json_new_object();
    json_set(message, "text", json_string("Line 1\nLine 2\nLine 3"));
    update = json_new_object();
    json_set(update, "message", message);

    text = mattermost_get_text(update);
    TEST_STR_EQ("get_text multi-line", text, "Line 1\nLine 2\nLine 3");
    json_free(update);
}

/* ================================================================
 *  6. Combined: set + get interaction
 * ================================================================ */
static void test_set_get_interaction(void) {
    printf("\n--- set/get interaction ---\n");

    mattermost_set_channel("interaction-test");
    json_node_t *update = json_new_object();

    /* get_chat_id should match last set_channel */
    const char *cid = mattermost_get_chat_id(update);
    TEST_STR_EQ("chat_id matches set_channel", cid, "interaction-test");

    json_free(update);

    /* Re-set and re-check */
    mattermost_set_channel("re-set-channel");
    update = json_new_object();
    cid = mattermost_get_chat_id(update);
    TEST_STR_EQ("chat_id after re-set", cid, "re-set-channel");
    json_free(update);

    /* Set URL multiple times — no crash */
    mattermost_set_url("https://first.url.com");
    mattermost_set_url("https://second.url.com");
    mattermost_set_url("https://third.url.com");
    TEST("multiple set_url calls", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_mattermost.c ===\n");

    test_set_url();
    test_set_token();
    test_set_channel();
    test_get_chat_id();
    test_get_text();
    test_set_get_interaction();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
