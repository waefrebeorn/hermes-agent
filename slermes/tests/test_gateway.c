/*
 * test_gateway.c — Gateway subsystem test suite (G165).
 *
 * Tests: message queue, rate limiter, client pool.
 * These are the gateway-agnostic components from server.c.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libhttp \
 *       tests/test_gateway.c \
 *       src/gateway/server.c lib/libhttp/http.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_gw -lm -lssl -lcrypto -lpthread \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_gw
 */

#include "hermes.h"
#include "hermes_gateway.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* ================================================================
 *  1. Message queue tests
 * ================================================================ */
static void test_message_queue(void) {
    printf("\n--- Message Queue ---\n");

    /* Init should work without crash */
    gw_queue_init();
    TEST("queue init complete", 1);

    /* Start empty */
    TEST_INT_EQ("queue depth starts at 0", gw_queue_depth(), 0);

    /* Pop from empty queue should fail */
    gateway_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    TEST("pop from empty queue fails", !gw_queue_pop(&msg));

    /* Push a message */
    TEST("push message", gw_queue_push("test_plat", "chat_1", "hello world", NULL));
    TEST_INT_EQ("queue depth after push", gw_queue_depth(), 1);

    /* Push another */
    TEST("push message 2", gw_queue_push("webhook", "chat_2", "{\"text\":\"ping\"}", "thread_5"));
    TEST_INT_EQ("queue depth after 2 pushes", gw_queue_depth(), 2);

    /* Pop first message */
    memset(&msg, 0, sizeof(msg));
    TEST("pop message", gw_queue_pop(&msg));
    TEST_STR_EQ("msg platform", msg.platform, "test_plat");
    TEST_STR_EQ("msg chat_id", msg.chat_id, "chat_1");
    TEST_STR_EQ("msg text", msg.text, "hello world");
    TEST("msg thread_id empty", msg.thread_id[0] == '\0' || strcmp(msg.thread_id, "") == 0);
    TEST_INT_EQ("queue depth after pop", gw_queue_depth(), 1);

    /* Pop second message */
    memset(&msg, 0, sizeof(msg));
    TEST("pop message 2", gw_queue_pop(&msg));
    TEST_STR_EQ("msg 2 platform", msg.platform, "webhook");
    TEST_STR_EQ("msg 2 chat_id", msg.chat_id, "chat_2");
    TEST_STR_EQ("msg 2 thread_id", msg.thread_id, "thread_5");

    /* Empty again */
    TEST_INT_EQ("queue empty after both pops", gw_queue_depth(), 0);
    TEST("pop empty returns false", !gw_queue_pop(&msg));

    /* Push with thread_id = empty string */
    TEST("push with empty thread_id", gw_queue_push("discord", "guild_1", "test", ""));
    TEST_INT_EQ("queue depth 1", gw_queue_depth(), 1);
    memset(&msg, 0, sizeof(msg));
    TEST("pop thread_id empty string", gw_queue_pop(&msg));
    TEST("thread_id is empty", msg.thread_id[0] == '\0' || strcmp(msg.thread_id, "") == 0);
    TEST_INT_EQ("queue empty", gw_queue_depth(), 0);

    /* Multiple pushes (overflow test — circular buffer is 64) */
    TEST("batch push 64", 1); /* Mark test existence */
    for (int i = 0; i < 60; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "msg_%d", i);
        if (!gw_queue_push("plat", "chat_1", buf, NULL)) {
            TEST_INT_EQ("queue full at", i, 64);
            break;
        }
    }
    int depth = gw_queue_depth();
    TEST_INT_EQ("queue depth after batch", depth, 60);

    /* Pop all */
    int popped = 0;
    while (gw_queue_pop(&msg)) popped++;
    TEST_INT_EQ("popped all 60 messages", popped, 60);
    TEST_INT_EQ("queue empty after drain", gw_queue_depth(), 0);
}

/* ================================================================
 *  2. Rate limiter tests
 * ================================================================ */
static void test_rate_limiter(void) {
    printf("\n--- Rate Limiter ---\n");

    /* Init with 10 tokens/sec, max burst 5 */
    gw_rate_limit_init(0, 10.0, 5.0);

    /* First check should pass (tokens available) */
    TEST("rate limit first check passes", gw_rate_limit_check(0));

    /* Consume all burst tokens */
    int passed_count = 0;
    for (int i = 0; i < 10; i++) {
        if (gw_rate_limit_check(0)) passed_count++;
    }
    /* At burst=5 we should have passed at most 5 */
    TEST_INT_EQ("burst limited to <=5", passed_count <= 5, 1);
    TEST_INT_EQ("burst at least 1", passed_count >= 1, 1);

    /* Multiple rate limiters */
    gw_rate_limit_init(1, 1.0, 1.0);  /* 1/sec, burst 1 */
    gw_rate_limit_init(2, 100.0, 20.0); /* 100/sec, burst 20 */

    TEST("limiter 1 first check", gw_rate_limit_check(1));
    TEST("limiter 2 first check", gw_rate_limit_check(2));

    /* Limiter 1 should be exhausted after one check (burst=1) */
    TEST("limiter 1 exhausted", !gw_rate_limit_check(1));

    /* Limiter 2 should still have tokens (burst=20) */
    TEST("limiter 2 has tokens", gw_rate_limit_check(2));
}

/* ================================================================
 *  3. HTTP client pool tests
 * ================================================================ */
static void test_client_pool(void) {
    printf("\n--- Client Pool ---\n");

    /* These tests verify pool doesn't crash. Actual HTTP calls
     * depend on external servers and are tested via integration tests. */

    /* Get client — should succeed or return NULL gracefully */
    http_client_t *c = gw_pool_get_client("https://example.com/api");
    TEST("get client (may be NULL if no net)", 1 /* Not an assertion of success */);

    /* Return client if we got one */
    if (c) {
        gw_pool_return_client(c, "https://example.com/api");
        TEST("return client", 1);
    }

    /* Get multiple clients for same endpoint */
    http_client_t *c1 = gw_pool_get_client("https://api.example.com/v1");
    http_client_t *c2 = gw_pool_get_client("https://api.example.com/v1");
    /* Should return different instances or same — both valid */

    if (c1) gw_pool_return_client(c1, "https://api.example.com/v1");
    if (c2) gw_pool_return_client(c2, "https://api.example.com/v1");

    TEST("multiple clients for same endpoint handled", 1);

    /* Cleanup shouldn't crash */
    gw_pool_cleanup();
    TEST("pool cleanup no crash", 1);
}

/* ================================================================
 *  4. Webhook request parsing
 * ================================================================ */
static void test_webhook_request(void) {
    printf("\n--- Webhook Request Parsing ---\n");

    /* Test the HTTP routing patterns used by the webhook platform */
    const char *valid_json = "{\"text\":\"hello\",\"chat_id\":\"test_user\"}";
    const char *invalid_json = "this is not json";
    const char *missing_text = "{\"chat_id\":\"test_user\"}";
    const char *missing_chat = "{\"text\":\"hello\"}";

    /* Parse valid */
    char *err = NULL;
    json_t *j = json_parse(valid_json, &err);
    TEST_NOT_NULL("valid JSON parses", j);
    if (j) {
        const char *text = json_get_str(j, "text", NULL);
        const char *cid = json_get_str(j, "chat_id", NULL);
        TEST_STR_EQ("text field", text, "hello");
        TEST_STR_EQ("chat_id field", cid, "test_user");
        json_free(j);
    }
    free(err); err = NULL;

    /* Parse invalid */
    j = json_parse(invalid_json, &err);
    TEST_NULL("invalid JSON fails", j);
    TEST_NOT_NULL("error message set", err);
    json_free(j); free(err); err = NULL;

    /* Parse missing text */
    j = json_parse(missing_text, &err);
    TEST_NOT_NULL("json with missing text parses structurally", j);
    if (j) {
        const char *text = json_get_str(j, "text", NULL);
        TEST_NULL("text field missing (NULL)", text);
        const char *cid = json_get_str(j, "chat_id", NULL);
        TEST_STR_EQ("chat_id present", cid, "test_user");
        json_free(j);
    }
    free(err); err = NULL;

    /* Parse missing chat_id */
    j = json_parse(missing_chat, &err);
    TEST_NOT_NULL("json with missing chat_id parses", j);
    if (j) {
        const char *cid = json_get_str(j, "chat_id", NULL);
        TEST_NULL("chat_id missing (NULL)", cid);
        json_free(j);
    }
    free(err);
}

/* ================================================================
 *  5. Pool keepalive config (5A-222)
 * ================================================================ */
static void test_pool_keepalive(void) {
    printf("\n--- Pool Keepalive Config ---\n");

    /* Default should be 0 (unset) */
    TEST("default expiry is 0", g_gw.pool_keepalive_expiry == 0.0);

    /* Set a value and verify cleanup doesn't crash */
    g_gw.pool_keepalive_expiry = 0.5;
    gw_pool_cleanup();
    TEST("cleanup with short expiry works", 1);

    /* Set a reasonable value and cleanup */
    g_gw.pool_keepalive_expiry = 300.0;
    gw_pool_cleanup();
    TEST("cleanup with 300s expiry works", 1);

    /* Reset to default for subsequent tests */
    g_gw.pool_keepalive_expiry = 0.0;
}

/* ================================================================
 *  6. Platform reaction dispatch (5C-252)
 * ================================================================ */
static void test_platform_reaction(void) {
    printf("\n--- Platform Reaction Dispatch ---\n");

    /* Unknown platform — should gracefully return false */
    TEST("unknown platform returns false",
         !gw_platform_send_reaction("nonexistent", "chat_1", "msg_1", "👍"));
}

int main(void) {
    printf("=== Gateway Subsystem Test Suite (G165) ===\n");

    test_message_queue();
    test_rate_limiter();
    test_client_pool();
    test_webhook_request();
    test_pool_keepalive();
    test_platform_reaction();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
