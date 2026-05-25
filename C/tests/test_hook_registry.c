/*
 * test_hook_registry.c — Hook registry unit tests.
 *
 * Tests hook_register, hook_unregister, hook_invoke, hook_has_callbacks,
 * hook_event_count, hook_reset_all, and hook_parse_result.
 *
 * Build (from C/):
 *   gcc -O2 -g -Wall -Werror -I include \
 *       tests/test_hook_registry.c src/agent/hook_registry.c \
 *       -o /tmp/hermes_test_hook_registry -lpthread
 *
 * Run:
 *   /tmp/hermes_test_hook_registry
 */

#include "hermes_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ── Test callbacks ─────────────────────────────────────────────── */

/* Simple callback: returns static JSON result */
static char *cb_allow(const char *event, const char *payload, void *ud) {
    (void)event; (void)payload; (void)ud;
    return strdup("{\"decision\":\"allow\"}");
}

static char *cb_block(const char *event, const char *payload, void *ud) {
    (void)event; (void)payload; (void)ud;
    return strdup("{\"decision\":\"block\",\"reason\":\"test block\"}");
}

/* Returns NULL — silent callback */
static char *cb_null(const char *event, const char *payload, void *ud) {
    (void)event; (void)payload; (void)ud;
    return NULL;
}

/* Track how many times called */
struct cb_counter {
    int count;
    char last_event[64];
    char last_payload[128];
};
static char *cb_counter_fn(const char *event, const char *payload, void *ud) {
    struct cb_counter *c = (struct cb_counter *)ud;
    c->count++;
    snprintf(c->last_event, sizeof(c->last_event), "%s", event);
    snprintf(c->last_payload, sizeof(c->last_payload), "%s", payload);
    return strdup("{\"count\":\"ok\"}");
}

/* ── Trackers for ordering/reentrant tests ──────────────────────── */

static int g_reentrant_called = 0;
static int g_call_order[16];
static int g_call_idx = 0;

static char *cb_reentrant(const char *event, const char *payload, void *ud) {
    (void)event; (void)payload; (void)ud;
    g_reentrant_called++;
    hook_register("reentrant_other", cb_allow, NULL);
    return strdup("{\"reentrant\":\"ok\"}");
}

static char *cb_order_a(const char *e, const char *p, void *ud) {
    (void)e; (void)p; (void)ud; g_call_order[g_call_idx++] = 1; return NULL;
}
static char *cb_order_b(const char *e, const char *p, void *ud) {
    (void)e; (void)p; (void)ud; g_call_order[g_call_idx++] = 2; return NULL;
}
static char *cb_order_c(const char *e, const char *p, void *ud) {
    (void)e; (void)p; (void)ud; g_call_order[g_call_idx++] = 3; return NULL;
}

/* ── Setup / teardown ───────────────────────────────────────────── */


int main(void)
{
    hook_reset_all();

    /* ================================================================== */
    /* 1. Basic register + invoke                                          */
    /* ================================================================== */

    TEST("register: basic", hook_register("test_event", cb_allow, NULL));
    TEST("has_callbacks: registered event",
         hook_has_callbacks("test_event"));
    TEST("event_count: 1", hook_event_count() == 1);

    char *result = hook_invoke("test_event", "{}");
    TEST("invoke: returns result", result != NULL);
    TEST("invoke: contains decision",
         result && strstr(result, "allow") != NULL);
    free(result);

    /* ================================================================== */
    /* 2. Multiple callbacks per event                                     */
    /* ================================================================== */

    hook_reset_all();
    TEST("register: cb_allow", hook_register("multi", cb_allow, NULL));
    TEST("register: cb_block", hook_register("multi", cb_block, NULL));
    TEST("register: cb_null",  hook_register("multi", cb_null, NULL));
    TEST("event_count: 1 event, 3 cbs", hook_event_count() == 1);

    result = hook_invoke("multi", "{}");
    TEST("multi invoke: has result", result != NULL);
    /* should have both allow and block results, cb_null skipped */
    TEST("multi invoke: has allow",
         result && strstr(result, "allow") != NULL);
    TEST("multi invoke: has block",
         result && strstr(result, "block") != NULL);
    /* JSON array: should start with [ */
    TEST("multi invoke: json array start",
         result && result[0] == '[');
    free(result);

    /* ================================================================== */
    /* 3. Multiple events                                                  */
    /* ================================================================== */

    hook_reset_all();
    TEST("register ev1", hook_register("event_one", cb_allow, NULL));
    TEST("register ev2", hook_register("event_two", cb_block, NULL));
    TEST("event_count: 2", hook_event_count() == 2);

    TEST("has_callbacks: ev1", hook_has_callbacks("event_one"));
    TEST("has_callbacks: ev2", hook_has_callbacks("event_two"));
    TEST("has_callbacks: unregistered ev",
         !hook_has_callbacks("nonexistent"));

    /* Invoke each separately */
    result = hook_invoke("event_one", "{}");
    TEST("ev1 invoke", result && strstr(result, "allow") != NULL);
    free(result);

    result = hook_invoke("event_two", "{}");
    TEST("ev2 invoke", result && strstr(result, "block") != NULL);
    free(result);

    /* ================================================================== */
    /* 4. Unregister                                                       */
    /* ================================================================== */

    hook_reset_all();
    hook_register("unreg", cb_allow, NULL);
    hook_register("unreg", cb_block, NULL);
    TEST("unreg: count 1 before", hook_event_count() == 1);

    /* Unregister should remove only the matching cb+userdata */
    TEST("unregister: cb_allow", hook_unregister("unreg", cb_allow, NULL));
    TEST("unregister: still has callbacks", hook_has_callbacks("unreg"));
    TEST("unregister: event_count still 1", hook_event_count() == 1);

    result = hook_invoke("unreg", "{}");
    TEST("unreg invoke: only block remains",
         result && strstr(result, "block") != NULL);
    TEST("unreg invoke: allow gone",
         result && strstr(result, "allow") == NULL);
    free(result);

    /* Unregister the other one */
    TEST("unregister: cb_block", hook_unregister("unreg", cb_block, NULL));
    TEST("unregister: no callbacks left",
         !hook_has_callbacks("unreg"));

    result = hook_invoke("unreg", "{}");
    TEST("unregister: no results after all removed",
         result == NULL);

    /* ================================================================== */
    /* 5. Duplicate registration (idempotent)                              */
    /* ================================================================== */

    hook_reset_all();
    TEST("dup: first register", hook_register("dup", cb_allow, NULL));
    TEST("dup: second register (same)", hook_register("dup", cb_allow, NULL));
    TEST("dup: event_count 1", hook_event_count() == 1);

    result = hook_invoke("dup", "{}");
    /* strstr count of "allow" in result should be 1, not 2 */
    if (result) {
        int count = 0;
        const char *p = result;
        while ((p = strstr(p, "allow")) != NULL) { count++; p++; }
        TEST("dup: only one result", count == 1);
        free(result);
    }

    /* ================================================================== */
    /* 6. NULL / invalid inputs                                            */
    /* ================================================================== */

    hook_reset_all();
    TEST("register: NULL event false",
         !hook_register(NULL, cb_allow, NULL));
    TEST("register: NULL cb false",
         !hook_register("ev", NULL, NULL));
    TEST("unregister: NULL event false",
         !hook_unregister(NULL, cb_allow, NULL));
    TEST("unregister: NULL cb false",
         !hook_unregister("ev", NULL, NULL));

    result = hook_invoke(NULL, "{}");
    TEST("invoke: NULL event returns NULL",
         result == NULL);

    TEST("has_callbacks: NULL event false",
         !hook_has_callbacks(NULL));

    /* ================================================================== */
    /* 7. Invoke with non-existent event                                   */
    /* ================================================================== */

    hook_reset_all();
    result = hook_invoke("no_such_event", "{}");
    TEST("invoke: nonexistent event returns NULL",
         result == NULL);

    /* ================================================================== */
    /* 8. hook_event_count after reset                                     */
    /* ================================================================== */

    hook_reset_all();
    TEST("event_count: 0 after reset", hook_event_count() == 0);
    hook_register("a", cb_allow, NULL);
    hook_register("b", cb_block, NULL);
    TEST("event_count: 2", hook_event_count() == 2);
    hook_reset_all();
    TEST("event_count: 0 after second reset", hook_event_count() == 0);

    /* ================================================================== */
    /* 9. Callback receives event and payload correctly                    */
    /* ================================================================== */

    hook_reset_all();
    struct cb_counter counter;
    memset(&counter, 0, sizeof(counter));
    TEST("register: counter cb",
         hook_register("counter_ev", cb_counter_fn, &counter));

    result = hook_invoke("counter_ev", "{\"key\":\"value\"}");
    TEST("counter: called once", counter.count == 1);
    TEST("counter: event name", strcmp(counter.last_event, "counter_ev") == 0);
    TEST("counter: payload",
         strstr(counter.last_payload, "key") != NULL);
    free(result);

    /* Second invoke increments */
    result = hook_invoke("counter_ev", "{}");
    TEST("counter: called twice", counter.count == 2);
    free(result);

    /* ================================================================== */
    /* 10. All-NULL callbacks (all return NULL)                            */
    /* ================================================================== */

    hook_reset_all();
    hook_register("nulls", cb_null, NULL);
    hook_register("nulls", cb_null, NULL);
    result = hook_invoke("nulls", "{}");
    TEST("all null: returns NULL when all return NULL",
         result == NULL);

    /* ================================================================== */
    /* 11. Payload passthrough to callback                                 */
    /* ================================================================== */

    hook_reset_all();
    memset(&counter, 0, sizeof(counter));
    hook_register("payload_test", cb_counter_fn, &counter);
    result = hook_invoke("payload_test", "{\"msg\":\"hello\"}");
    TEST("payload: received msg",
         strstr(counter.last_payload, "hello") != NULL);
    free(result);

    /* ================================================================== */
    /* 12. hook_parse_result                                               */
    /* ================================================================== */

    /* Allow (empty) */
    hook_result_t hr = hook_parse_result("");
    TEST("parse: empty -> allow",
         hr.decision == HOOK_DECISION_ALLOW);

    /* Allow (non-matching JSON) */
    hr = hook_parse_result("{\"foo\":\"bar\"}");
    TEST("parse: non-matching -> allow",
         hr.decision == HOOK_DECISION_ALLOW);

    /* Block - decision format (Claude-Code style) */
    hr = hook_parse_result("{\"decision\":\"block\",\"reason\":\"policy violation\"}");
    TEST("parse: decision/block",
         hr.decision == HOOK_DECISION_BLOCK);
    TEST("parse: reason extracted",
         strcmp(hr.message, "policy violation") == 0);

    /* Block - action format (Hermes canonical) */
    hr = hook_parse_result("{\"action\":\"block\",\"message\":\"denied\"}");
    TEST("parse: action/block",
         hr.decision == HOOK_DECISION_BLOCK);
    TEST("parse: message extracted",
         strcmp(hr.message, "denied") == 0);

    /* Block with both reason and message — message wins */
    hr = hook_parse_result(
        "{\"action\":\"block\",\"message\":\"msg text\",\"reason\":\"reason text\"}");
    TEST("parse: both message and reason -> block",
         hr.decision == HOOK_DECISION_BLOCK);
    TEST("parse: message preferred",
         strcmp(hr.message, "msg text") == 0);

    /* Block without reason — fallback message */
    hr = hook_parse_result("{\"decision\":\"block\"}");
    TEST("parse: block no reason -> fallback msg",
         hr.decision == HOOK_DECISION_BLOCK);
    TEST("parse: fallback message",
         strcmp(hr.message, "Blocked by shell hook.") == 0);

    /* Context */
    hr = hook_parse_result("{\"context\":\"additional info\"}");
    TEST("parse: context decision",
         hr.decision == HOOK_DECISION_CONTEXT);

    /* Context with block — context wins (last matching pattern) */
    hr = hook_parse_result(
        "{\"decision\":\"block\",\"context\":\"extra\"}");
    TEST("parse: context+block -> context wins (code order)",
         hr.decision == HOOK_DECISION_CONTEXT);

    /* NULL input */
    hr = hook_parse_result(NULL);
    TEST("parse: NULL -> allow",
         hr.decision == HOOK_DECISION_ALLOW);

    /* Invalid JSON — strstr can still match partial */
    hr = hook_parse_result("{\"decision\":\"block\" garbage");
    TEST("parse: partial JSON block",
         hr.decision == HOOK_DECISION_BLOCK);

    /* ================================================================== */
    /* 13. Re-entrant safety: callback that registers during invoke        */
    /* ================================================================== */

    hook_reset_all();
    g_reentrant_called = 0;

    hook_register("reentrant_main", cb_reentrant, NULL);
    result = hook_invoke("reentrant_main", "{}");
    TEST("reentrant: called once", g_reentrant_called == 1);
    TEST("reentrant: got result", result && strstr(result, "reentrant") != NULL);
    free(result);
    TEST("reentrant: new event registered",
         hook_has_callbacks("reentrant_other"));

    /* ================================================================== */
    /* 14. Verify callback ordering                                        */
    /* ================================================================== */

    hook_reset_all();
    g_call_idx = 0;

    hook_register("order_test", cb_order_a, NULL);
    hook_register("order_test", cb_order_b, NULL);
    hook_register("order_test", cb_order_c, NULL);
    result = hook_invoke("order_test", "{}");
    free(result);
    TEST("ordering: first a(1)", g_call_idx == 3 && g_call_order[0] == 1);
    TEST("ordering: then b(2)", g_call_idx == 3 && g_call_order[1] == 2);
    TEST("ordering: then c(3)", g_call_idx == 3 && g_call_order[2] == 3);

    /* After unregistering middle callback, ordering preserved */
    hook_unregister("order_test", cb_order_b, NULL);
    g_call_idx = 0;
    result = hook_invoke("order_test", "{}");
    free(result);
    TEST("ordering after unreg: a then c",
         g_call_idx == 2 && g_call_order[0] == 1 && g_call_order[1] == 3);

    /* ================================================================== */
    /* Summary                                                             */
    /* ================================================================== */

    printf("\n");
    printf("  Results: %d passed, %d failed, 0 skipped\n", passed, tests - passed);

    return (passed == tests) ? 0 : 1;
}
