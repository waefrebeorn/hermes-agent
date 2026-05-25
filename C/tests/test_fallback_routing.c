/* test_fallback_routing.c — Fallback routing: create, add, advance, cool-off, tick, stats. */
#include "fallback_routing.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Fallback Routing Tests ===\n\n");

    /* Test 1: Create chain */
    printf("--- Create & basic properties ---\n");
    fallback_chain_t *c = fallback_chain_create("gpt-4o", "openai");
    TEST("chain non-NULL", c != NULL);
    TEST("primary model", strcmp(c->primary_model, "gpt-4o") == 0);
    TEST("primary provider", strcmp(c->primary_provider, "openai") == 0);
    TEST("entry count 0", c->entry_count == 0);
    TEST("current index -1 (primary)", c->current_index == -1);
    TEST("not using fallback", !c->using_fallback);

    /* Test 2: NULL/empty model create (does not return NULL — uses defaults) */
    printf("\n--- Create with NULL/defaults ---\n");
    fallback_chain_t *c_null = fallback_chain_create(NULL, NULL);
    TEST("create(NULL,NULL) non-NULL", c_null != NULL);
    TEST("NULL model defaults to unknown", strcmp(c_null->primary_model, "unknown") == 0);
    TEST("NULL provider defaults to openai", strcmp(c_null->primary_provider, "openai") == 0);
    fallback_chain_free(c_null);
    c_null = fallback_chain_create("m", NULL);
    TEST("create(m,NULL) non-NULL", c_null != NULL);
    TEST("provider defaults to openai", strcmp(c_null->primary_provider, "openai") == 0);
    fallback_chain_free(c_null);

    /* Test 3: Add fallbacks */
    printf("\n--- Add fallbacks ---\n");
    int i1 = fallback_chain_add(c, "claude-sonnet-4", "anthropic", NULL, NULL);
    TEST("first add returns 0", i1 == 0);
    TEST("entry count 1", c->entry_count == 1);
    TEST("model set", strcmp(c->entries[0].model, "claude-sonnet-4") == 0);
    TEST("provider set", strcmp(c->entries[0].provider, "anthropic") == 0);
    TEST("cooloff_until 0", c->entries[0].cooloff_until == 0);
    TEST("consecutive failures 0", c->entries[0].consecutive_failures == 0);
    TEST("max failures default 3", c->entries[0].max_failures_before_cooloff == FALLBACK_DEFAULT_MAX_FAILURES);
    TEST("cooloff seconds default 120", c->entries[0].cooloff_seconds == FALLBACK_DEFAULT_COOLOFF);

    int i2 = fallback_chain_add(c, "deepseek-chat", "deepseek", "https://custom.url", NULL);
    TEST("second add returns 1", i2 == 1);
    TEST("entry count 2", c->entry_count == 2);
    TEST("custom base_url set", strcmp(c->entries[1].base_url, "https://custom.url") == 0);

    /* Test 4: Fill chain to max and beyond */
    printf("\n--- Chain capacity ---\n");
    for (int i = c->entry_count; i < FALLBACK_MAX_CHAIN; i++) {
        char model[64], prov[64];
        snprintf(model, sizeof(model), "model-%d", i);
        snprintf(prov, sizeof(prov), "prov-%d", i);
        int idx = fallback_chain_add(c, model, prov, NULL, NULL);
        TEST("add up to max returns valid idx", idx == i);
    }
    TEST("chain full at max", c->entry_count == FALLBACK_MAX_CHAIN);
    int overflow = fallback_chain_add(c, "overflow", "test", NULL, NULL);
    TEST("overflow returns -1", overflow == -1);
    TEST("entry count unchanged", c->entry_count == FALLBACK_MAX_CHAIN);

    /* Test 5: Current on primary (no failures yet) */
    printf("\n--- Current target (primary) ---\n");
    char model[128] = {0}, prov[64] = {0}, url[512] = {0};
    const char *desc = fallback_chain_current(c, model, sizeof(model), prov, sizeof(prov), url, sizeof(url), NULL);
    TEST("desc non-NULL", desc != NULL);
    TEST("desc contains primary model", strstr(desc, "gpt-4o") != NULL);
    TEST("out_model is primary", strcmp(model, "gpt-4o") == 0);
    TEST("out_provider is primary", strcmp(prov, "openai") == 0);
    TEST("url empty on primary", url[0] == '\0');

    /* Test 6: Report failure → advance to first fallback */
    printf("\n--- Failure → advance to first fallback ---\n");
    bool should_retry = fallback_chain_report(c, false, 503);
    TEST("should retry (fallback available)", should_retry);
    TEST("now using fallback", c->using_fallback);
    TEST("current_index 0 (first fallback)", c->current_index == 0);

    desc = fallback_chain_current(c, model, sizeof(model), prov, sizeof(prov), url, sizeof(url), NULL);
    TEST("current fallback model", strstr(desc, "claude-sonnet-4") != NULL);
    TEST("out_model is fallback", strcmp(model, "claude-sonnet-4") == 0);
    TEST("out_provider is fallback", strcmp(prov, "anthropic") == 0);

    /* Test 7: Success resets state */
    printf("\n--- Success resets ---\n");
    fallback_chain_reset(c);
    TEST("reset → current_index -1", c->current_index == -1);
    TEST("reset → using_fallback false", !c->using_fallback);
    TEST("reset → has_available (primary)", fallback_chain_has_available(c));

    /* Advance then report success */
    fallback_chain_report(c, false, 502);
    TEST("after advance, using_fallback true", c->using_fallback);
    should_retry = fallback_chain_report(c, true, 200);
    TEST("success → no retry", !should_retry);
    TEST("success → current_index -1", c->current_index == -1);
    TEST("success → using_fallback false", !c->using_fallback);
    TEST("success → all cooloffs cleared", c->entries[0].cooloff_until == 0);

    /* Test 8: Cool-off triggers after max_failures (single entry chain) */
    printf("\n--- Cool-off after max failures ---\n");
    fallback_chain_t *co = fallback_chain_create("p", "o");
    fallback_chain_add(co, "f", "a", NULL, NULL);
    fallback_chain_report(co, false, 500); /* advance to entry 0 (transition, no increment) */
    TEST("co: consecutive still 0 (transition doesn't count)", co->entries[0].consecutive_failures == 0);
    TEST("co: no cooloff yet", co->entries[0].cooloff_until == 0);
    /* Entry 0 fails again (now INCREMENTED to 1) */
    fallback_chain_report(co, false, 500); /* consecutive=1 */
    TEST("co: consecutive=1", co->entries[0].consecutive_failures == 1);
    fallback_chain_report(co, false, 500); /* consecutive=2 */
    TEST("co: consecutive=2", co->entries[0].consecutive_failures == 2);
    fallback_chain_report(co, false, 500); /* consecutive=3 → cooloff */
    time_t t_now = time(NULL);
    TEST("co: consecutive=3", co->entries[0].consecutive_failures == 3);
    TEST("co: cooloff triggered", co->entries[0].cooloff_until > t_now + 100);
    fallback_chain_free(co);

    /* Test 9: has_available when fallback cooling off */
    printf("\n--- has_available ---\n");
    fallback_chain_reset(c);
    /* Reset also cleared all cooloffs, so everything is fresh */
    fallback_chain_report(c, false, 500); /* advance to 0 */
    TEST("has_available (entry 0 fresh, no cooloff)", fallback_chain_has_available(c));
    /* Simulate all entries cooled off by setting them directly */
    for (int i = 0; i < c->entry_count; i++) {
        c->entries[i].cooloff_until = time(NULL) + 3600;
        c->entries[i].consecutive_failures = 3;
    }
    c->using_fallback = true;
    TEST("all cooled off → not available", !fallback_chain_has_available(c));
    /* But has_available returns true if NOT using_fallback (primary available) */
    c->using_fallback = false;
    TEST("primary always available even with cooled off entries",
         fallback_chain_has_available(c));

    /* Test 10: Tick resurrects cooled-off entries */
    printf("\n--- Tick ---\n");
    fallback_chain_reset(c);
    /* Cool off all entries */
    for (int i = 0; i < c->entry_count; i++) {
        c->entries[i].consecutive_failures = 3;
        c->entries[i].cooloff_until = time(NULL) - 1; /* 1 second ago = expired */
    }
    int resurrected = fallback_chain_tick(c);
    TEST("all cooled-off entries resurrected (expired)", resurrected == c->entry_count);
    for (int i = 0; i < c->entry_count; i++) {
        char test_name[128];
        snprintf(test_name, sizeof(test_name), "entry %d resurrected", i);
        TEST(test_name, c->entries[i].cooloff_until == 0 && c->entries[i].consecutive_failures == 0);
    }

    /* Test 11: Tick doesn't resurrect active cool-offs */
    int active_count = 3;
    for (int i = 0; i < active_count; i++) {
        c->entries[i].consecutive_failures = 3;
        c->entries[i].cooloff_until = time(NULL) + 3600;
    }
    resurrected = fallback_chain_tick(c);
    TEST("active cool-offs not resurrected", resurrected == 0);

    /* Test 12: Stats JSON */
    printf("\n--- Stats JSON ---\n");
    fallback_chain_reset(c);
    char *stats = fallback_chain_stats_json(c);
    TEST("stats non-NULL", stats != NULL);
    TEST("stats contains primary model", strstr(stats, "gpt-4o") != NULL);
    TEST("stats contains fallbacks array", strstr(stats, "fallbacks") != NULL);
    TEST("stats contains primary_provider", strstr(stats, "openai") != NULL);
    free(stats);

    /* Test 13: NULL safety */
    printf("\n--- NULL safety ---\n");
    TEST("add to NULL returns -1", fallback_chain_add(NULL, "m", "p", NULL, NULL) == -1);
    TEST("current from NULL returns NULL", fallback_chain_current(NULL, NULL, 0, NULL, 0, NULL, 0, NULL) == NULL);
    TEST("report(NULL,...) returns false", !fallback_chain_report(NULL, false, 500));
    TEST("reset(NULL) no crash", (fallback_chain_reset(NULL), 1));
    TEST("has_available(NULL) false", !fallback_chain_has_available(NULL));
    TEST("tick(NULL) returns 0", fallback_chain_tick(NULL) == 0);
    TEST("free(NULL) no crash", (fallback_chain_free(NULL), 1));
    TEST("stats_json(NULL) returns NULL", fallback_chain_stats_json(NULL) == NULL);

    /* Test 14: Empty chain behavior */
    printf("\n--- Empty chain ---\n");
    fallback_chain_t *empty = fallback_chain_create("primary", "test");
    TEST("empty chain created", empty != NULL);
    TEST("empty has 0 entries", empty->entry_count == 0);
    desc = fallback_chain_current(empty, model, sizeof(model), prov, sizeof(prov), url, sizeof(url), NULL);
    TEST("empty chain current returns primary", desc != NULL && strstr(desc, "primary") != NULL);
    bool retry = fallback_chain_report(empty, false, 503);
    TEST("empty chain failure → no retry", !retry);
    fallback_chain_free(empty);

    /* Test 15: Multiple failures exhaust chain via cooloff */
    printf("\n--- Full chain exhaustion via cooloff ---\n");
    fallback_chain_t *ex = fallback_chain_create("gpt-4o", "openai");
    fallback_chain_add(ex, "claude", "anthropic", NULL, NULL);
    fallback_chain_add(ex, "deepseek", "deepseek", NULL, NULL);
    /* Reduce cooloff threshold to 1 failure per entry */
    ex->entries[0].max_failures_before_cooloff = 1;
    ex->entries[1].max_failures_before_cooloff = 1;

    /* First failure → advance to entry 0 */
    should_retry = fallback_chain_report(ex, false, 500);
    TEST("exhaust: retry after primary fail", should_retry);

    /* Now on entry 0: fail → consecutive=1 >= max_failures=1 → cooloff triggered. Advance to 1. */
    should_retry = fallback_chain_report(ex, false, 500);
    TEST("exhaust: retry on entry 0 fail", should_retry);
    TEST("exhaust: entry 0 cooled off", ex->entries[0].cooloff_until > 0);

    /* Now on entry 1: fail → consecutive=1 >= max_failures=1 → cooloff triggered. All cooled off. */
    should_retry = fallback_chain_report(ex, false, 500);
    TEST("exhaust: no retry (all cooled off)", !should_retry);
    TEST("exhaust: entry 1 cooled off", ex->entries[1].cooloff_until > 0);
    TEST("exhaust: !available", !fallback_chain_has_available(ex));

    fallback_chain_free(ex);

    /* Test 16: Auth failure (401/403) → longer cooloff */
    printf("\n--- Auth failure special handling ---\n");
    fallback_chain_t *auth = fallback_chain_create("gpt-4o", "openai");
    fallback_chain_add(auth, "claude", "anthropic", NULL, NULL);
    auth->entries[0].max_failures_before_cooloff = 1;
    auth->entries[0].cooloff_seconds = 120;

    fallback_chain_report(auth, false, 500); /* advance to 0 */
    fallback_chain_report(auth, false, 401); /* auth failure on entry 0 */
    /* Auth cooloff is cooloff_seconds * 6 = 720s */
    time_t t = time(NULL);
    TEST("auth failure longer cooloff", auth->entries[0].cooloff_until >= t + 700);
    fallback_chain_free(auth);

    /* Test 17: Rate limit (429) → 60s cooloff */
    printf("\n--- Rate limit special handling ---\n");
    fallback_chain_t *rl = fallback_chain_create("gpt-4o", "openai");
    fallback_chain_add(rl, "claude", "anthropic", NULL, NULL);
    rl->entries[0].max_failures_before_cooloff = 1;

    fallback_chain_report(rl, false, 500); /* advance to 0 */
    fallback_chain_report(rl, false, 429); /* rate limit on entry 0 */
    t = time(NULL);
    TEST("rate limit cooloff ~60s", rl->entries[0].cooloff_until >= t + 55 && rl->entries[0].cooloff_until <= t + 65);
    fallback_chain_free(rl);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
