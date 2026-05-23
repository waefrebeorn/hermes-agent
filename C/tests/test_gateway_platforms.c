/*
 * test_gateway_platforms.c — T01: Gateway per-platform integration tests.
 *
 * Tests platform-specific functions WITHOUT requiring network access.
 * Telegram: JSON parsing (is_mentioned, is_group, get_chat_id, get_text,
 *            get_update_type) and state management (set_username/get_username).
 * Discord:  State setters (set_token, set_channel, set_application_id).
 * Webhook:  HMAC verification, subscription management table operations.
 *
 * Build (telegram only):
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_gateway_platforms.c \
 *       src/gateway/platforms/telegram.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_gw_platforms -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Build (webhook only):
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libcrypto \
 *       tests/test_gateway_platforms.c \
 *       src/gateway/platforms/webhook.c lib/libcrypto/crypto.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_gw_platforms -lm -lssl -lcrypto
 *
 * Run:
 *   /tmp/hermes_test_gw_platforms
 */

#include "hermes.h"
#include "hermes_gateway.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))

/* Helper: build Telegram message update JSON */
static json_node_t *tg_update(const char *chat_type, const char *text,
                               bool has_entities, const char *mention_type)
{
    json_node_t *chat = json_new_object();
    json_object_set(chat, "id", json_new_number(12345));
    if (chat_type)
        json_object_set(chat, "type", json_new_string(chat_type));

    json_node_t *msg = json_new_object();
    json_object_set(msg, "chat", chat);
    json_object_set(msg, "text", json_new_string(text ? text : ""));

    if (has_entities && mention_type) {
        json_node_t *ent = json_new_object();
        json_object_set(ent, "type", json_new_string(mention_type));
        json_object_set(ent, "offset", json_new_number(0));
        json_object_set(ent, "length", json_new_number((double)(text ? strlen(text) : 0)));
        json_node_t *entities = json_new_array();
        json_array_append(entities, ent);
        json_object_set(msg, "entities", entities);
    }

    json_node_t *update = json_new_object();
    json_object_set(update, "update_id", json_new_number(1001));
    json_object_set(update, "message", msg);
    return update;
}

/* ================================================================
 *  Telegram: Username management
 * ================================================================ */
static void test_telegram_username(void) {
    printf("\n--- Telegram: username management ---\n");

    /* Default: empty */
    const char *u = telegram_get_username();
    TEST("default username is empty", u && u[0] == '\0');

    /* Set and get */
    telegram_set_username("TestBot");
    u = telegram_get_username();
    TEST_STR_EQ("username after set", u, "TestBot");

    /* Re-set */
    telegram_set_username("NewBot");
    u = telegram_get_username();
    TEST_STR_EQ("username re-set", u, "NewBot");

    /* Set NULL (function is no-op on NULL — keeps previous value) */
    telegram_set_username(NULL);
    u = telegram_get_username();
    TEST_STR_EQ("username after NULL set unchanged", u, "NewBot");

    /* Reset for tests */
    telegram_set_username("MentionBot");
}

/* ================================================================
 *  Telegram: is_mentioned
 * ================================================================ */
static void test_telegram_is_mentioned(void) {
    printf("\n--- Telegram: is_mentioned ---\n");

    telegram_set_username("MentionBot");

    /* Entity mention matching @MentionBot */
    json_node_t *upd = tg_update("private", "@MentionBot hello", true, "mention");
    TEST("entity mention matches bot", telegram_is_mentioned(upd));
    json_free(upd);

    /* Text mention matching @MentionBot (no entities) */
    upd = tg_update("private", "hello @MentionBot", false, NULL);
    TEST("text @mention matches bot (no entities)", telegram_is_mentioned(upd));
    json_free(upd);

    /* Different mention — should not match if entities check is correct */
    /* Note: with no entities and text doesn't contain @MentionBot,
     * but bob_username IS set, so is_mentioned returns false only
     * if no entity match AND no text match. */
    telegram_set_username("MentionBot");
    upd = tg_update("private", "hello @DifferentBot", false, NULL);
    /* @DifferentBot != @MentionBot in text, no entities */
    TEST("different @mention does not match", !telegram_is_mentioned(upd));
    json_free(upd);

    /* No username set = always mentioned (DM behaves as mentioned) */
    telegram_set_username("");
    upd = tg_update("private", "hello", false, NULL);
    TEST("empty username = always mentioned", telegram_is_mentioned(upd));
    json_free(upd);

    /* NULL update should return true (non-message updates always processed) */
    TEST("NULL update = mentioned", telegram_is_mentioned(NULL));

    /* callback_query without message should return true */
    json_node_t *cb = json_new_object();
    json_object_set(cb, "callback_query", json_new_object());
    json_object_set(cb, "update_id", json_new_number(1002));
    telegram_set_username("MentionBot");
    TEST("callback query = mentioned", telegram_is_mentioned(cb));
    json_free(cb);

    telegram_set_username("MentionBot");
}

/* ================================================================
 *  Telegram: is_group
 * ================================================================ */
static void test_telegram_is_group(void) {
    printf("\n--- Telegram: is_group ---\n");

    /* Private chat */
    json_node_t *upd = tg_update("private", "hello", false, NULL);
    TEST("private chat is NOT group", !telegram_is_group(upd));
    json_free(upd);

    /* Group chat */
    upd = tg_update("group", "hello", false, NULL);
    TEST("group chat IS group", telegram_is_group(upd));
    json_free(upd);

    /* Supergroup chat */
    upd = tg_update("supergroup", "hello", false, NULL);
    TEST("supergroup IS group", telegram_is_group(upd));
    json_free(upd);

    /* Channel */
    upd = tg_update("channel", "announcement", false, NULL);
    TEST("channel is NOT group (func only checks group/supergroup)", !telegram_is_group(upd));
    json_free(upd);

    /* NULL update */
    TEST("NULL update not group", !telegram_is_group(NULL));

    /* edited_message */
    json_node_t *chat = json_new_object();
    json_object_set(chat, "id", json_new_number(54321));
    json_object_set(chat, "type", json_new_string("supergroup"));
    json_node_t *msg = json_new_object();
    json_object_set(msg, "chat", chat);
    json_object_set(msg, "text", json_new_string("edited"));
    json_node_t *upd2 = json_new_object();
    json_object_set(upd2, "update_id", json_new_number(1003));
    json_object_set(upd2, "edited_message", msg);
    TEST("edited_message in group IS group", telegram_is_group(upd2));
    json_free(upd2);
}

/* ================================================================
 *  Telegram: get_chat_id
 * ================================================================ */
static void test_telegram_get_chat_id(void) {
    printf("\n--- Telegram: get_chat_id ---\n");

    /* Regular message */
    json_node_t *upd = tg_update("private", "hello", false, NULL);
    const char *cid = telegram_get_chat_id(upd);
    TEST_NOT_NULL("chat_id from regular message", cid);
    if (cid) TEST_STR_EQ("chat_id is 12345", cid, "12345");
    json_free(upd);

    /* NULL update */
    TEST_NULL("NULL update returns NULL", telegram_get_chat_id(NULL));

    /* edited_message */
    json_node_t *chat = json_new_object();
    json_object_set(chat, "id", json_new_number(99999));
    json_node_t *msg = json_new_object();
    json_object_set(msg, "chat", chat);
    json_object_set(msg, "text", json_new_string("edited"));
    json_node_t *upd2 = json_new_object();
    json_object_set(upd2, "update_id", json_new_number(1004));
    json_object_set(upd2, "edited_message", msg);
    cid = telegram_get_chat_id(upd2);
    TEST_NOT_NULL("chat_id from edited_message", cid);
    if (cid) TEST_STR_EQ("chat_id from edited is 99999", cid, "99999");
    json_free(upd2);

    /* No message or edited_message */
    json_node_t *empty = json_new_object();
    json_object_set(empty, "update_id", json_new_number(1005));
    TEST_NULL("no message returns NULL", telegram_get_chat_id(empty));
    json_free(empty);
}

/* ================================================================
 *  Telegram: get_text
 * ================================================================ */
static void test_telegram_get_text(void) {
    printf("\n--- Telegram: get_text ---\n");

    /* Regular text message */
    json_node_t *upd = tg_update("private", "hello world", false, NULL);
    const char *text = telegram_get_text(upd);
    TEST_STR_EQ("get_text from regular message", text, "hello world");
    json_free(upd);

    /* NULL update */
    TEST_NULL("NULL update returns NULL", telegram_get_text(NULL));

    /* inline_query */
    json_node_t *iq = json_new_object();
    json_object_set(iq, "inline_query", json_new_object());
    json_object_set(json_obj_get(iq, "inline_query"), "query",
                    json_new_string("inline search"));
    json_object_set(iq, "update_id", json_new_number(1006));
    text = telegram_get_text(iq);
    TEST_STR_EQ("get_text from inline_query", text, "inline search");
    json_free(iq);

    /* callback_query with data */
    json_node_t *cb = json_new_object();
    json_node_t *cb_data = json_new_object();
    json_object_set(cb_data, "data", json_new_string("btn_click"));
    json_object_set(cb, "callback_query", cb_data);
    json_object_set(cb, "update_id", json_new_number(1007));
    text = telegram_get_text(cb);
    TEST_STR_EQ("get_text from callback data", text, "btn_click");
    json_free(cb);

    /* Sticker */
    json_node_t *chat = json_new_object();
    json_object_set(chat, "id", json_new_number(12345));
    json_object_set(chat, "type", json_new_string("private"));
    json_node_t *sticker = json_new_object();
    json_object_set(sticker, "emoji", json_new_string("😀"));
    json_object_set(sticker, "set_name", json_new_string("TestSet"));
    json_node_t *msg = json_new_object();
    json_object_set(msg, "chat", chat);
    json_object_set(msg, "sticker", sticker);
    json_node_t *upd2 = json_new_object();
    json_object_set(upd2, "update_id", json_new_number(1008));
    json_object_set(upd2, "message", msg);
    text = telegram_get_text(upd2);
    TEST_NOT_NULL("sticker returns text", text);
    if (text) TEST("sticker contains 'Sticker'", strstr(text, "Sticker") != NULL);
    if (text) TEST("sticker contains emoji", strstr(text, "😀") != NULL);
    json_free(upd2);
}

/* ================================================================
 *  Telegram: get_update_type
 * ================================================================ */
static void test_telegram_get_update_type(void) {
    printf("\n--- Telegram: get_update_type ---\n");

    /* Text message */
    json_node_t *upd = tg_update("private", "hello", false, NULL);
    const char *t = telegram_get_update_type(upd);
    TEST_STR_EQ("text message type", t, "text");
    json_free(upd);

    /* NULL */
    TEST_NULL("NULL update returns NULL", telegram_get_update_type(NULL));

    /* inline_query */
    json_node_t *iq = json_new_object();
    json_object_set(iq, "inline_query", json_new_object());
    json_object_set(iq, "update_id", json_new_number(1009));
    t = telegram_get_update_type(iq);
    TEST_STR_EQ("inline_query type", t, "inline_query");
    json_free(iq);

    /* callback_query */
    json_node_t *cb = json_new_object();
    json_object_set(cb, "callback_query", json_new_object());
    json_object_set(cb, "update_id", json_new_number(1010));
    t = telegram_get_update_type(cb);
    TEST_STR_EQ("callback_query type", t, "callback_query");
    json_free(cb);

    /* Empty update (no recognized fields) */
    json_node_t *empty = json_new_object();
    json_object_set(empty, "update_id", json_new_number(1011));
    TEST_NULL("empty update returns NULL", telegram_get_update_type(empty));
    json_free(empty);
}

/* ================================================================
 *  Discord: Setters (no crash tests)
 * ================================================================ */
static void test_discord_setters(void) {
    printf("\n--- Discord: state setters ---\n");

    /* These only test that setters don't crash.
     * The static vars are not directly readable. */
    discord_set_token("test_token_12345");
    TEST("discord_set_token no crash", 1);

    discord_set_channel("987654321");
    TEST("discord_set_channel no crash", 1);

    discord_set_application_id("app_12345");
    TEST("discord_set_application_id no crash", 1);

    /* Add multiple channels */
    for (int i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "channel_%d", i);
        discord_add_channel(buf);
    }
    TEST("discord_add_channel x5 no crash", 1);

    /* Re-set with valid values */
    discord_set_token("reset_token");
    discord_set_channel("reset_channel");
    discord_set_application_id("reset_app");
    TEST("discord setters re-set no crash", 1);
}

#ifdef WEBHOOK_TESTS
/* ================================================================
 *  Webhook: HMAC verification
 * ================================================================ */
static void test_webhook_hmac(void) {
    printf("\n--- Webhook: HMAC signature verification ---\n");

    /* Set secret */
    webhook_set_verify_secret("test_secret_key");

    /* Invalid signature format — missing sha256= prefix */
    TEST("invalid prefix fails",
         !webhook_verify_hmac("md5=abc123", (const unsigned char *)"hello", 5));

    /* Empty/null */
    TEST("NULL signature fails",
         !webhook_verify_hmac(NULL, (const unsigned char *)"hello", 5));

    TEST("NULL body fails",
         !webhook_verify_hmac("sha256=abc", NULL, 5));

    /* Wrong length (not 64 hex chars) */
    TEST("short signature fails",
         !webhook_verify_hmac("sha256=abc", (const unsigned char *)"hello", 5));

    /* Disable verification */
    webhook_set_verify_secret(NULL);
    /* When disabled, verify_hmac still checks the signature format
     * and computes HMAC against the empty secret. The function
     * checks signature format first, then computes expected HMAC
     * using g_verify_secret (empty). */
    TEST("disabled verification still validates format",
         !webhook_verify_hmac("sha256=invalid_format", (const unsigned char *)"x", 1));

    /* Re-enable for remaining tests */
    webhook_set_verify_secret("test_key");
}

/* ================================================================
 *  Webhook: Subscription management
 * ================================================================ */
static void test_webhook_subscriptions(void) {
    printf("\n--- Webhook: subscription management ---\n");

    /* Start fresh */
    TEST_INT_EQ("fresh subscription count = 0", webhook_subscription_count(), 0);

    /* Add a subscription */
    int idx = webhook_subscription_add("https://example.com/hook",
                                        "secret123", 3, 1000);
    TEST("add returns valid index", idx >= 0);
    TEST_INT_EQ("count after add = 1", webhook_subscription_count(), 1);

    /* Add another */
    int idx2 = webhook_subscription_add("https://other.com/endpoint",
                                         NULL, 0, 500);
    TEST("second add returns valid index", idx2 >= 0);
    TEST_INT_EQ("count after 2 adds", webhook_subscription_count(), 2);

    /* NULL endpoint should fail */
    int bad = webhook_subscription_add(NULL, "secret", 1, 100);
    TEST_INT_EQ("NULL endpoint returns -1", bad, -1);

    /* Empty endpoint should fail */
    bad = webhook_subscription_add("", "secret", 1, 100);
    TEST_INT_EQ("empty endpoint returns -1", bad, -1);

    /* Remove first subscription */
    TEST("remove first succeeds", webhook_subscription_remove(idx));
    TEST_INT_EQ("count after remove = 1", webhook_subscription_count(), 1);

    /* Remove non-existent subscription */
    TEST("remove invalid index fails",
         !webhook_subscription_remove(999));

    /* Add headers to remaining subscription */
    TEST("add header succeeds",
         webhook_subscription_add_header(idx2, "X-Custom", "value1"));
    TEST("add second header",
         webhook_subscription_add_header(idx2, "Authorization", "Bearer tok"));

    /* Remove remaining */
    TEST("remove second succeeds", webhook_subscription_remove(idx2));
    TEST_INT_EQ("count after all removes = 0", webhook_subscription_count(), 0);

    /* List subscriptions */
    int idx3 = webhook_subscription_add("https://list.test/hook",
                                         "list_secret", 2, 200);
    int idx4 = webhook_subscription_add("https://list.test/hook2",
                                         "sec2", 5, 300);
    TEST("two subscriptions added", idx3 >= 0 && idx4 >= 0);
    TEST_INT_EQ("count = 2", webhook_subscription_count(), 2);

    webhook_subscription_t subs[5];
    int n = webhook_subscription_list(subs, 5);
    TEST_INT_EQ("list returns 2", n, 2);
    TEST("first endpoint non-empty", subs[0].endpoint[0] != '\0');
    TEST("second endpoint non-empty", subs[1].endpoint[0] != '\0');

    /* Cleanup */
    webhook_subscription_remove(idx3);
    webhook_subscription_remove(idx4);
    TEST_INT_EQ("final count = 0", webhook_subscription_count(), 0);
}
#endif /* WEBHOOK_TESTS */

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Gateway Per-Platform Integration Tests (T01) ===\n");

#ifndef WEBHOOK_TESTS
    test_telegram_username();
    test_telegram_is_mentioned();
    test_telegram_is_group();
    test_telegram_get_chat_id();
    test_telegram_get_text();
    test_telegram_get_update_type();
    test_discord_setters();
#endif

#ifdef WEBHOOK_TESTS
    test_webhook_hmac();
    test_webhook_subscriptions();
#endif

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
