/*
 * test_hook_registry.c — Tests for hook registration and invocation.
 *
 * Tests: register, unregister, invoke, event_count, has_callbacks,
 * reset_all, and hook_parse_result edge cases.
 */

#include "hermes_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

/* A simple counting callback */
static char *count_cb(const char *event, const char *payload, void *userdata) {
    (void)event;
    (void)payload;
    if (userdata) {
        int *counter = (int *)userdata;
        (*counter)++;
    }
    return NULL;
}

/* A callback that returns a JSON result */
static char *result_cb(const char *event, const char *payload, void *userdata) {
    (void)event;
    (void)payload;
    (void)userdata;
    return strdup("{\"status\":\"ok\"}");
}

/* A callback that inspects payload */
static char *payload_cb(const char *event, const char *payload, void *userdata) {
    (void)event;
    (void)userdata;
    /* Return payload back as proof we got it */
    if (payload)
        return strdup(payload);
    return strdup("{\"got\":\"null\"}");
}

static void test_register_invoke(void) {
    printf("\n--- Register & invoke ---\n");
    int counter = 0;

    /* Hook_reset_all ensures clean slate */
    hook_reset_all();
    TEST("initial event_count == 0", hook_event_count() == 0);

    TEST("register with NULL event fails", !hook_register(NULL, count_cb, &counter));
    TEST("register with NULL cb fails", !hook_register("test_event", NULL, &counter));
    TEST("register valid", hook_register("test_event", count_cb, &counter));
    TEST("event_count == 1", hook_event_count() == 1);
    TEST("has_callbacks true", hook_has_callbacks("test_event"));

    /* Invoke should call the callback once */
    char *res = hook_invoke("test_event", "{}");
    TEST("callback was called", counter == 1);
    TEST("no results (returns NULL)", res == NULL);
    free(res);

    /* Invoking non-existent event returns NULL */
    char *res2 = hook_invoke("nonexistent", "{}");
    TEST("nonexistent event returns NULL", res2 == NULL);
    free(res2);

    hook_reset_all();
}

static void test_multiple_callbacks(void) {
    printf("\n--- Multiple callbacks per event ---\n");
    int c1 = 0, c2 = 0;

    hook_reset_all();
    hook_register("multi", count_cb, &c1);
    hook_register("multi", count_cb, &c2);
    TEST("event_count == 1", hook_event_count() == 1);

    /* Both callbacks fire */
    char *res = hook_invoke("multi", "{}");
    TEST("c1 incremented", c1 == 1);
    TEST("c2 incremented", c2 == 1);
    TEST("both return NULL (no results)", res == NULL);
    free(res);

    hook_reset_all();
}

static void test_register_duplicate_idempotent(void) {
    printf("\n--- Duplicate registration is idempotent ---\n");
    int c = 0;

    hook_reset_all();
    TEST("first register", hook_register("dup", count_cb, &c));
    TEST("second register (same cb+ud) returns true (idempotent)",
         hook_register("dup", count_cb, &c));

    /* Invoke should call once, not twice */
    hook_invoke("dup", "{}");
    TEST("called only once", c == 1);

    hook_reset_all();
}

static void test_unregister(void) {
    printf("\n--- Unregister ---\n");
    int c1 = 0, c2 = 0;

    hook_reset_all();
    hook_register("evt", count_cb, &c1);
    hook_register("evt", count_cb, &c2);

    /* Unregister c1 */
    TEST("unregister c1", hook_unregister("evt", count_cb, &c1));
    hook_invoke("evt", "{}");
    TEST("c1 unchanged (unregistered)", c1 == 0);
    TEST("c2 incremented", c2 == 1);

    /* Unregister c2 */
    TEST("unregister c2", hook_unregister("evt", count_cb, &c2));
    TEST("no callbacks left", !hook_has_callbacks("evt"));

    /* Unregister from non-existent event */
    TEST("unregister non-existent", !hook_unregister("no_such_event", count_cb, &c1));

    /* Unregister with NULL params */
    TEST("unregister NULL event", !hook_unregister(NULL, count_cb, &c1));
    TEST("unregister NULL cb", !hook_unregister("evt", NULL, &c1));

    hook_reset_all();
}

static void test_collects_results(void) {
    printf("\n--- Collect results from callbacks ---\n");

    hook_reset_all();
    hook_register("results", result_cb, (void*)(intptr_t)1);
    hook_register("results", result_cb, (void*)(intptr_t)2);

    char *res = hook_invoke("results", "{}");
    TEST("got results", res != NULL);
    /* Results are comma-separated JSON objects wrapped in [...] */
    TEST("results start with [", res && res[0] == '[');
    TEST("results contain status:ok", res && strstr(res, "status") != NULL);
    TEST("results contain two items",
         res && strstr(res, ",") != NULL && strstr(res, "},") != NULL);
    free(res);

    hook_reset_all();
}

static void test_mixed_null_and_result_callbacks(void) {
    printf("\n--- Mixed NULL and result callbacks ---\n");
    int c1 = 0, c2 = 0;

    hook_reset_all();
    hook_register("mixed", count_cb, &c1);         /* returns NULL */
    hook_register("mixed", result_cb, NULL);        /* returns JSON */
    hook_register("mixed", count_cb, &c2);           /* returns NULL */

    char *res = hook_invoke("mixed", "{}");
    TEST("c1 incremented", c1 == 1);
    TEST("c2 incremented", c2 == 1);
    TEST("got collected results", res != NULL);
    TEST_STR_EQ("only non-NULL results collected", res, "[{\"status\":\"ok\"}]");
    free(res);

    hook_reset_all();
}

static void test_invoke_with_payload(void) {
    printf("\n--- Payload delivery ---\n");

    hook_reset_all();
    hook_register("payload", payload_cb, NULL);

    char *res = hook_invoke("payload", "{\"key\":\"value\"}");
    TEST("got payload back", res != NULL);
    TEST_STR_EQ("payload echoed back", res, "[{\"key\":\"value\"}]");
    free(res);

    /* Null payload should get default */
    char *res2 = hook_invoke("payload", NULL);
    TEST("NULL payload default", res2 != NULL);
    TEST_STR_EQ("default {} payload", res2, "[{}]");
    free(res2);

    hook_reset_all();
}

static void test_parse_result(void) {
    printf("\n--- hook_parse_result ---\n");

    /* Allow (default) */
    hook_result_t r = hook_parse_result(NULL);
    TEST("NULL -> ALLOW", r.decision == HOOK_DECISION_ALLOW);

    r = hook_parse_result("");
    TEST("empty -> ALLOW", r.decision == HOOK_DECISION_ALLOW);

    r = hook_parse_result("{\"irrelevant\":true}");
    TEST("irrelevant -> ALLOW", r.decision == HOOK_DECISION_ALLOW);

    /* Block by decision */
    r = hook_parse_result("{\"decision\":\"block\",\"reason\":\"dangerous tool\"}");
    TEST("decision:block -> BLOCK", r.decision == HOOK_DECISION_BLOCK);
    TEST("reason extracted", strstr(r.message, "dangerous") != NULL);

    /* Block by action */
    r = hook_parse_result("{\"action\":\"block\",\"message\":\"bad thing\"}");
    TEST("action:block -> BLOCK", r.decision == HOOK_DECISION_BLOCK);
    TEST("message extracted", strstr(r.message, "bad thing") != NULL);

    /* Context */
    r = hook_parse_result("{\"context\":\"add context here\"}");
    TEST("context -> CONTEXT", r.decision == HOOK_DECISION_CONTEXT);

    /* Block with action takes precedence over context */
    r = hook_parse_result("{\"action\":\"block\",\"context\":\"also present\"}");
    TEST("block+context -> BLOCK", r.decision == HOOK_DECISION_BLOCK);
}

static void test_event_max(void) {
    printf("\n--- Event limits ---\n");
    int dummy[HOOK_MAX_CBS + 2] = {0};

    hook_reset_all();

    /* Register HOOK_MAX_EVENTS events */
    for (int i = 0; i < HOOK_MAX_EVENTS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "event_%d", i);
        TEST("register event", hook_register(name, count_cb, &dummy[0]));
    }
    TEST("max events reachable", hook_event_count() == HOOK_MAX_EVENTS);

    /* Next event should fail */
    TEST("extra event fails",
         !hook_register("extra_event", count_cb, &dummy[0]));

    /* Callback per-event limit */
    hook_reset_all();
    for (int i = 0; i < HOOK_MAX_CBS; i++) {
        char expected[16];
        snprintf(expected, sizeof(expected), "cb %d", i);
        TEST(expected, hook_register("full", count_cb, &dummy[i]));
    }
    TEST("max cbs + 1 fails",
         !hook_register("full", result_cb, NULL));

    hook_reset_all();
}

int main(void) {
    printf("=== Hook Registry Tests ===\n");

    test_register_invoke();
    test_multiple_callbacks();
    test_register_duplicate_idempotent();
    test_unregister();
    test_collects_results();
    test_mixed_null_and_result_callbacks();
    test_invoke_with_payload();
    test_parse_result();
    test_event_max();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
