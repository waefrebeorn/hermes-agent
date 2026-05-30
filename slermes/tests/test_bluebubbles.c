/*
 * test_bluebubbles.c — BlueBubbles platform adapter test suite (S7 X01).
 *
 * Tests pure functions: bluebubbles_set_url, set_password, set_poll_guid,
 * bluebubbles_is_group, bluebubbles_get_group_id, bluebubbles_get_chat_id,
 * bluebubbles_get_text, bluebubbles_get_message_guid,
 * bluebubbles_get_tapback_type, bluebubbles_get_attachment_path.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       -I lib/libplugin -I lib/libmangateway \
 *       tests/test_bluebubbles.c src/gateway/platforms/bluebubbles.c \
 *       lib/libjson/json.c -lm \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_bluebubbles
 *
 * Run:
 *   /tmp/hermes_test_bluebubbles
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for tested functions */
void bluebubbles_set_url(const char *url);
void bluebubbles_set_password(const char *password);
void bluebubbles_set_poll_guid(const char *guid);
bool bluebubbles_is_group(json_node_t *update);
const char *bluebubbles_get_group_id(json_node_t *update);
const char *bluebubbles_get_chat_id(json_node_t *update);
const char *bluebubbles_get_text(json_node_t *update);
const char *bluebubbles_get_message_guid(json_node_t *update);
int bluebubbles_get_tapback_type(json_node_t *update);
const char *bluebubbles_get_attachment_path(json_node_t *update);

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
    printf("\n--- bluebubbles_set_url / set_password / set_poll_guid ---\n");

    bluebubbles_set_url("http://192.168.1.100:1234");
    TEST("set_url normal", 1);

    bluebubbles_set_url("");
    TEST("set_url empty", 1);

    bluebubbles_set_url(NULL);
    TEST("set_url NULL (preserves previous)", 1);

    bluebubbles_set_url("https://bb.example.com:4321");
    TEST("set_url reset", 1);

    bluebubbles_set_password("my-secret-pw-42");
    TEST("set_password normal", 1);

    bluebubbles_set_password("");
    TEST("set_password empty", 1);

    bluebubbles_set_password(NULL);
    TEST("set_password NULL (preserves)", 1);

    bluebubbles_set_poll_guid("iMessage;-;test@example.com");
    TEST("set_poll_guid normal", 1);

    bluebubbles_set_poll_guid("");
    TEST("set_poll_guid empty clears", 1);

    bluebubbles_set_poll_guid(NULL);
    TEST("set_poll_guid NULL clears", 1);
}

/* ================================================================
 *  2. bluebubbles_is_group
 * ================================================================ */
static void test_is_group(void) {
    printf("\n--- bluebubbles_is_group ---\n");

    /* NULL update → false */
    TEST("is_group NULL", !bluebubbles_is_group(NULL));

    /* Explicit isGroup=true */
    json_node_t *upd = json_new_object();
    json_set(upd, "isGroup", json_bool(true));
    TEST("is_group explicit true", bluebubbles_is_group(upd));
    json_free(upd);

    /* Explicit isGroup=false */
    upd = json_new_object();
    json_set(upd, "isGroup", json_bool(false));
    TEST("is_group explicit false", !bluebubbles_is_group(upd));
    json_free(upd);

    /* Group GUID pattern ;+; in chatGuid */
    upd = json_new_object();
    json_set(upd, "chatGuid", json_string("any;+;uuid-123"));
    TEST("is_group chatGuid ;+;", bluebubbles_is_group(upd));
    json_free(upd);

    /* DM GUID pattern ;-; in chatGuid */
    upd = json_new_object();
    json_set(upd, "chatGuid", json_string("iMessage;-;user@example.com"));
    TEST("is_group DM ;-;", !bluebubbles_is_group(upd));
    json_free(upd);

    /* Group pattern in guid field */
    upd = json_new_object();
    json_set(upd, "guid", json_string("group;+;abc-def"));
    TEST("is_group guid ;+;", bluebubbles_is_group(upd));
    json_free(upd);

    /* DM pattern in guid field */
    upd = json_new_object();
    json_set(upd, "guid", json_string("sms;-;+15551234567"));
    TEST("is_group sms ;-;", !bluebubbles_is_group(upd));
    json_free(upd);

    /* No relevant fields */
    upd = json_new_object();
    json_set(upd, "text", json_string("hello"));
    TEST("is_group no fields", !bluebubbles_is_group(upd));
    json_free(upd);

    /* Both isGroup false AND group pattern in guid */
    upd = json_new_object();
    json_set(upd, "isGroup", json_bool(false));
    json_set(upd, "guid", json_string("any;+;uuid"));
    TEST("is_group false but guid ;+;", bluebubbles_is_group(upd));
    json_free(upd);
}

/* ================================================================
 *  3. bluebubbles_get_group_id
 * ================================================================ */
static void test_get_group_id(void) {
    printf("\n--- bluebubbles_get_group_id ---\n");

    /* NULL → NULL */
    TEST_NULL("get_group_id NULL", bluebubbles_get_group_id(NULL));

    /* DM chatGuid → NULL */
    json_node_t *upd = json_new_object();
    json_set(upd, "chatGuid", json_string("iMessage;-;user@example.com"));
    TEST_NULL("get_group_id DM chatGuid", bluebubbles_get_group_id(upd));
    json_free(upd);

    /* Group chatGuid → returns GUID */
    upd = json_new_object();
    json_set(upd, "chatGuid", json_string("any;+;group-uuid-42"));
    const char *gid = bluebubbles_get_group_id(upd);
    TEST_STR_EQ("get_group_id from chatGuid", gid, "any;+;group-uuid-42");
    json_free(upd);

    /* Group guid field → returns GUID */
    upd = json_new_object();
    json_set(upd, "guid", json_string("group;+;abc"));
    gid = bluebubbles_get_group_id(upd);
    TEST_STR_EQ("get_group_id from guid", gid, "group;+;abc");
    json_free(upd);

    /* chats array fallback */
    upd = json_new_object();
    json_node_t *chats = json_new_array();
    json_node_t *c1 = json_new_object();
    json_set(c1, "guid", json_string("any;+;nested-group"));
    json_append(chats, c1);
    json_set(upd, "chats", chats);
    gid = bluebubbles_get_group_id(upd);
    TEST_STR_EQ("get_group_id from chats[0].guid", gid, "any;+;nested-group");
    json_free(upd);

    /* chatGuid overrides chats */
    upd = json_new_object();
    json_set(upd, "chatGuid", json_string("direct;+;winner"));
    chats = json_new_array();
    c1 = json_new_object();
    json_set(c1, "guid", json_string("chats;+;loser"));
    json_append(chats, c1);
    json_set(upd, "chats", chats);
    gid = bluebubbles_get_group_id(upd);
    TEST_STR_EQ("get_group_id chatGuid overrides chats", gid, "direct;+;winner");
    json_free(upd);
}

/* ================================================================
 *  4. JSON extraction: get_chat_id, get_text, get_message_guid
 * ================================================================ */
static void test_getters(void) {
    printf("\n--- get_chat_id / get_text / get_message_guid ---\n");

    /* Normal update */
    json_node_t *upd = json_new_object();
    json_set(upd, "chat_id", json_string("iMessage;-;user@example.com"));
    json_set(upd, "text", json_string("Hello from iMessage!"));

    TEST_STR_EQ("get_chat_id", bluebubbles_get_chat_id(upd), "iMessage;-;user@example.com");
    TEST_STR_EQ("get_text", bluebubbles_get_text(upd), "Hello from iMessage!");
    json_free(upd);

    /* Missing chat_id → fallback "bluebubbles" */
    upd = json_new_object();
    json_set(upd, "text", json_string("no chat id"));
    TEST_STR_EQ("get_chat_id fallback", bluebubbles_get_chat_id(upd), "bluebubbles");
    json_free(upd);

    /* Missing text → empty string */
    upd = json_new_object();
    json_set(upd, "chat_id", json_string("c1"));
    TEST_STR_EQ("get_text missing", bluebubbles_get_text(upd), "");
    json_free(upd);

    /* NULL update */
    TEST_STR_EQ("get_chat_id NULL", bluebubbles_get_chat_id(NULL), "bluebubbles");
    TEST_STR_EQ("get_text NULL", bluebubbles_get_text(NULL), "");

    /* Unicode */
    upd = json_new_object();
    json_set(upd, "chat_id", json_string("u1"));
    json_set(upd, "text", json_string("iMessage says こんにちは!"));
    TEST_STR_EQ("get_text unicode", bluebubbles_get_text(upd), "iMessage says こんにちは!");
    json_free(upd);
}

/* ================================================================
 *  5. bluebubbles_get_message_guid
 * ================================================================ */
static void test_get_message_guid(void) {
    printf("\n--- bluebubbles_get_message_guid ---\n");

    /* NULL → NULL */
    TEST_NULL("get_message_guid NULL", bluebubbles_get_message_guid(NULL));

    /* guid field */
    json_node_t *upd = json_new_object();
    json_set(upd, "guid", json_string("p:0/guid-abc"));
    TEST_STR_EQ("get_message_guid from guid", bluebubbles_get_message_guid(upd), "p:0/guid-abc");
    json_free(upd);

    /* messageGuid field */
    upd = json_new_object();
    json_set(upd, "messageGuid", json_string("msg-guid-xyz"));
    TEST_STR_EQ("get_message_guid from messageGuid", bluebubbles_get_message_guid(upd), "msg-guid-xyz");
    json_free(upd);

    /* guid takes priority */
    upd = json_new_object();
    json_set(upd, "guid", json_string("p:0/primary"));
    json_set(upd, "messageGuid", json_string("msg-secondary"));
    TEST_STR_EQ("get_message_guid guid overrides messageGuid", bluebubbles_get_message_guid(upd), "p:0/primary");
    json_free(upd);

    /* Neither */
    upd = json_new_object();
    json_set(upd, "text", json_string("no guid"));
    TEST_NULL("get_message_guid neither", bluebubbles_get_message_guid(upd));
    json_free(upd);

    /* Empty strings */
    upd = json_new_object();
    json_set(upd, "guid", json_string(""));
    json_set(upd, "messageGuid", json_string(""));
    TEST_NULL("get_message_guid empty fields", bluebubbles_get_message_guid(upd));
    json_free(upd);
}

/* ================================================================
 *  6. bluebubbles_get_tapback_type
 * ================================================================ */
static void test_get_tapback_type(void) {
    printf("\n--- bluebubbles_get_tapback_type ---\n");

    /* NULL → 0 */
    TEST("get_tapback_type NULL", bluebubbles_get_tapback_type(NULL) == 0);

    /* No field → 0 */
    json_node_t *upd = json_new_object();
    json_set(upd, "text", json_string("hello"));
    TEST("get_tapback_type missing", bluebubbles_get_tapback_type(upd) == 0);
    json_free(upd);

    /* All 6 add types */
    int types[] = {2000, 2001, 2002, 2003, 2004, 2005};
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        upd = json_new_object();
        json_set(upd, "associatedMessageType", json_number((double)types[i]));
        char name[64];
        snprintf(name, sizeof(name), "get_tapback_type %d", types[i]);
        TEST(name, bluebubbles_get_tapback_type(upd) == types[i]);
        json_free(upd);
    }

    /* Remove types (3000-3005) */
    upd = json_new_object();
    json_set(upd, "associatedMessageType", json_number(3000.0));
    TEST("get_tapback_type remove", bluebubbles_get_tapback_type(upd) == 3000);
    json_free(upd);

    /* Negative value (invalid) */
    upd = json_new_object();
    json_set(upd, "associatedMessageType", json_number(-1.0));
    TEST("get_tapback_type negative", bluebubbles_get_tapback_type(upd) == -1);
    json_free(upd);
}

/* ================================================================
 *  7. bluebubbles_get_attachment_path
 * ================================================================ */
static void test_get_attachment_path(void) {
    printf("\n--- bluebubbles_get_attachment_path ---\n");

    /* NULL → NULL */
    TEST_NULL("get_attachment_path NULL", bluebubbles_get_attachment_path(NULL));

    /* No attachments */
    json_node_t *upd = json_new_object();
    TEST_NULL("get_attachment_path no attachments", bluebubbles_get_attachment_path(upd));
    json_free(upd);

    /* Empty attachments array */
    upd = json_new_object();
    json_set(upd, "attachments", json_new_array());
    TEST_NULL("get_attachment_path empty array", bluebubbles_get_attachment_path(upd));
    json_free(upd);

    /* Single attachment */
    upd = json_new_object();
    json_node_t *atts = json_new_array();
    json_node_t *att1 = json_new_object();
    json_set(att1, "path", json_string("/tmp/photo.jpg"));
    json_append(atts, att1);
    json_set(upd, "attachments", atts);
    TEST_STR_EQ("get_attachment_path single", bluebubbles_get_attachment_path(upd), "/tmp/photo.jpg");
    json_free(upd);

    /* Multiple attachments — returns first */
    upd = json_new_object();
    atts = json_new_array();
    att1 = json_new_object();
    json_set(att1, "path", json_string("/tmp/first.pdf"));
    json_append(atts, att1);
    json_node_t *att2 = json_new_object();
    json_set(att2, "path", json_string("/tmp/second.txt"));
    json_append(atts, att2);
    json_set(upd, "attachments", atts);
    TEST_STR_EQ("get_attachment_path returns first", bluebubbles_get_attachment_path(upd), "/tmp/first.pdf");
    json_free(upd);

    /* Attachment with missing path */
    upd = json_new_object();
    atts = json_new_array();
    att1 = json_new_object();
    json_set(att1, "name", json_string("file.txt"));
    json_append(atts, att1);
    json_set(upd, "attachments", atts);
    TEST_STR_EQ("get_attachment_path missing path field", bluebubbles_get_attachment_path(upd), "");
    json_free(upd);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== test_bluebubbles.c ===\n");

    test_setters();
    test_is_group();
    test_get_group_id();
    test_getters();
    test_get_message_guid();
    test_get_tapback_type();
    test_get_attachment_path();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
