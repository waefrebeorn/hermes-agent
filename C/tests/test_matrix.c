/*
 * test_matrix.c — Matrix platform adapter test suite (S7 X01).
 *
 * Tests pure functions: matrix_set_homeserver, set_token, set_room,
 * set_user_id, set_event_filter, matrix_get_chat_id, matrix_get_text.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_matrix.c src/gateway/platforms/matrix.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_matrix
 *
 * Run:
 *   /tmp/hermes_test_matrix
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void matrix_set_homeserver(const char *hs);
void matrix_set_token(const char *token);
void matrix_set_room(const char *id);
void matrix_set_user_id(const char *uid);
void matrix_set_event_filter(const char *types);
const char *matrix_get_chat_id(json_node_t *update);
const char *matrix_get_text(json_node_t *update);

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
static void test_setters(void) {
    printf("\n--- set_homeserver / set_token / set_room / set_user_id ---\n");

    matrix_set_homeserver("https://matrix.example.com");
    TEST("set_homeserver normal", 1);

    matrix_set_homeserver("");
    TEST("set_homeserver empty", 1);

    matrix_set_homeserver("https://localhost:8448");
    TEST("set_homeserver reset", 1);

    matrix_set_token("syt_token_MATRIX_EXAMPLE");
    TEST("set_token normal", 1);

    matrix_set_token("");
    TEST("set_token empty", 1);

    matrix_set_token("syt_dG9rZW4_ABC123");
    TEST("set_token reset", 1);

    matrix_set_room("!roomid:matrix.org");
    TEST("set_room normal", 1);

    matrix_set_room("");
    TEST("set_room empty", 1);

    matrix_set_room("!abc:example.com");
    TEST("set_room reset", 1);

    matrix_set_user_id("@user:matrix.org");
    TEST("set_user_id normal", 1);

    matrix_set_user_id("");
    TEST("set_user_id empty", 1);

    matrix_set_user_id("@bot:example.com");
    TEST("set_user_id reset", 1);
}

/* ================================================================
 *  2. matrix_set_event_filter — special empty/NULL logic
 * ================================================================ */
static void test_set_event_filter(void) {
    printf("\n--- matrix_set_event_filter ---\n");

    /* Default filter */
    matrix_set_event_filter("m.room.message");
    TEST("set_event_filter single type", 1);

    /* Multiple types */
    matrix_set_event_filter("m.room.message,m.room.member");
    TEST("set_event_filter two types", 1);

    /* Types with whitespace */
    matrix_set_event_filter("m.room.message, m.room.member, m.room.topic");
    TEST("set_event_filter with spaces", 1);

    /* Empty string → accept all (event_filter[0] = '\\0') */
    matrix_set_event_filter("");
    TEST("set_event_filter empty clears", 1);

    /* NULL → also accept all */
    matrix_set_event_filter(NULL);
    TEST("set_event_filter NULL clears", 1);

    /* Reset to specific value */
    matrix_set_event_filter("m.room.message");
    TEST("set_event_filter re-set", 1);
}

/* ================================================================
 *  3. matrix_get_chat_id — walks update→message→chat→id
 * ================================================================ */
static void test_get_chat_id(void) {
    printf("\n--- matrix_get_chat_id ---\n");

    /* Normal nested path */
    json_node_t *chat = json_new_object();
    json_set(chat, "id", json_string("!room123:matrix.org"));
    json_node_t *message = json_new_object();
    json_set(message, "chat", chat);
    json_node_t *update = json_new_object();
    json_set(update, "message", message);

    TEST_STR_EQ("get_chat_id normal", matrix_get_chat_id(update), "!room123:matrix.org");
    json_free(update);

    /* No message */
    update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    TEST_NULL("get_chat_id no message", matrix_get_chat_id(update));
    json_free(update);

    /* Message with no chat */
    message = json_new_object();
    json_set(message, "text", json_string("hello"));
    update = json_new_object();
    json_set(update, "message", message);
    TEST_NULL("get_chat_id message no chat", matrix_get_chat_id(update));
    json_free(update);

    /* Chat with no id */
    chat = json_new_object();
    json_set(chat, "other", json_string("value"));
    message = json_new_object();
    json_set(message, "chat", chat);
    update = json_new_object();
    json_set(update, "message", message);
    TEST_NULL("get_chat_id missing id field", matrix_get_chat_id(update));
    json_free(update);

    /* NULL update */
    TEST_NULL("get_chat_id NULL update", matrix_get_chat_id(NULL));
}

/* ================================================================
 *  4. matrix_get_text — walks update→message→text
 * ================================================================ */
static void test_get_text(void) {
    printf("\n--- matrix_get_text ---\n");

    /* Normal */
    json_node_t *message = json_new_object();
    json_set(message, "text", json_string("Hello from Matrix!"));
    json_node_t *update = json_new_object();
    json_set(update, "message", message);
    TEST_STR_EQ("get_text normal", matrix_get_text(update), "Hello from Matrix!");
    json_free(update);

    /* Empty text */
    message = json_new_object();
    json_set(message, "text", json_string(""));
    update = json_new_object();
    json_set(update, "message", message);
    TEST_STR_EQ("get_text empty", matrix_get_text(update), "");
    json_free(update);

    /* No text field */
    message = json_new_object();
    json_set(message, "body", json_string("alt field"));
    update = json_new_object();
    json_set(update, "message", message);
    TEST_NULL("get_text no text field", matrix_get_text(update));
    json_free(update);

    /* No message */
    update = json_new_object();
    json_set(update, "update_id", json_number(42.0));
    TEST_NULL("get_text no message", matrix_get_text(update));
    json_free(update);

    /* NULL update */
    TEST_NULL("get_text NULL update", matrix_get_text(NULL));

    /* Unicode */
    message = json_new_object();
    json_set(message, "text", json_string("Matrix says こんにちは世界!"));
    update = json_new_object();
    json_set(update, "message", message);
    TEST_STR_EQ("get_text unicode", matrix_get_text(update), "Matrix says こんにちは世界!");
    json_free(update);

    /* Multi-line */
    message = json_new_object();
    json_set(message, "text", json_string("Line 1\nLine 2\nLine 3"));
    update = json_new_object();
    json_set(update, "message", message);
    TEST_STR_EQ("get_text multi-line", matrix_get_text(update), "Line 1\nLine 2\nLine 3");
    json_free(update);
}

/* ================================================================
 *  5. Combined: setter interaction
 * ================================================================ */
static void test_setter_interaction(void) {
    printf("\n--- setter interaction ---\n");

    /* Set and re-set all values multiple times */
    matrix_set_homeserver("https://first.server");
    matrix_set_homeserver("https://second.server");
    matrix_set_homeserver("https://third.server");
    TEST("multiple set_homeserver", 1);

    matrix_set_token("token-a");
    matrix_set_token("token-b");
    matrix_set_token("token-c");
    TEST("multiple set_token", 1);

    matrix_set_room("!r1");
    matrix_set_room("!r2");
    TEST("multiple set_room", 1);

    matrix_set_user_id("@u1");
    matrix_set_user_id("@u2");
    matrix_set_user_id("@u3");
    TEST("multiple set_user_id", 1);

    matrix_set_event_filter("m.text");
    matrix_set_event_filter("m.notice");
    matrix_set_event_filter("");       /* empty = accept all */
    TEST("multiple set_event_filter resets", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_matrix.c ===\n");

    test_setters();
    test_set_event_filter();
    test_get_chat_id();
    test_get_text();
    test_setter_interaction();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
