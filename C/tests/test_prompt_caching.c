/* test_prompt_caching.c — Tests for Anthropic prompt caching. */

#include "prompt_caching.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static void test_empty_messages(void) {
    int count = 0;
    apply_anthropic_cache_control(NULL, &count, "5m", false);
    TEST("null messages no crash", 1); pass--; pass++; /* mark as passed */
}

static void test_system_message_cached(void) {
    pc_message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 0; /* system */
    msgs[1].role = 1; /* user */
    int count = 2;

    apply_anthropic_cache_control(msgs, &count, "5m", false);
    TEST("system gets cache", msgs[0].has_cache);
    TEST("system cache type", strcmp(msgs[0].cache_type, "ephemeral") == 0);
    TEST("system ttl empty (5m)", msgs[0].cache_ttl[0] == '\0');
}

static void test_system_and_3_non_system(void) {
    pc_message_t msgs[8];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 0; /* system */
    msgs[1].role = 1; /* user */
    msgs[2].role = 2; /* assistant */
    msgs[3].role = 1; /* user */
    msgs[4].role = 2; /* assistant */
    int count = 5;

    apply_anthropic_cache_control(msgs, &count, "5m", false);
    TEST("system cached", msgs[0].has_cache);
    TEST("msg2 (3rd last non-system) cached", msgs[2].has_cache);
    TEST("msg3 (2nd last non-system) cached", msgs[3].has_cache);
    TEST("msg4 (last non-system) cached", msgs[4].has_cache);
    /* msg1 is 4th from end — should NOT have cache since we have
       system + msgs[2], msgs[3], msgs[4] = 4 total */
    TEST("msg1 (4th from end) not cached", !msgs[1].has_cache);
}

static void test_no_system_message(void) {
    pc_message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 1; /* user — not system */
    msgs[1].role = 2; /* assistant */
    msgs[2].role = 1; /* user */
    int count = 3;

    apply_anthropic_cache_control(msgs, &count, "5m", false);
    /* No system, so we should have 3 cache markers on the last 3 messages */
    TEST("no-system: msg0 cached", msgs[0].has_cache);
    TEST("no-system: msg1 cached", msgs[1].has_cache);
    TEST("no-system: msg2 cached", msgs[2].has_cache);
}

static void test_1h_ttl(void) {
    pc_message_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 0; /* system */
    msgs[1].role = 1; /* user */
    int count = 2;

    apply_anthropic_cache_control(msgs, &count, "1h", false);
    TEST("1h: system cached", msgs[0].has_cache);
    TEST("1h: ttl=1h", strcmp(msgs[0].cache_ttl, "1h") == 0);
    TEST("1h: user cached", msgs[1].has_cache);
}

static void test_native_anthropic_tool_msg(void) {
    pc_message_t msgs[3];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 0; /* system */
    msgs[1].role = 1; /* user */
    msgs[2].role = 3; /* tool */
    int count = 3;

    /* Without native — tool message should NOT get cache */
    apply_anthropic_cache_control(msgs, &count, "5m", false);
    TEST("non-native: tool not cached", !msgs[2].has_cache);

    /* With native — tool message SHOULD get cache */
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 0;
    msgs[1].role = 1;
    msgs[2].role = 3;
    count = 3;
    apply_anthropic_cache_control(msgs, &count, "5m", true);
    TEST("native: system cached", msgs[0].has_cache);
    TEST("native: user cached", msgs[1].has_cache);
    TEST("native: tool cached", msgs[2].has_cache);
}

static void test_single_message(void) {
    pc_message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = 1; /* user */
    int count = 1;

    apply_anthropic_cache_control(msgs, &count, "5m", false);
    TEST("single: cached", msgs[0].has_cache);
}

/* ── Cache hit/miss statistics tests ── */

static void test_cache_stats_initial_zero(void) {
    cache_reset_stats();
    TEST("initial hits zero", cache_get_hits() == 0);
    TEST("initial misses zero", cache_get_misses() == 0);
    TEST("initial total zero", cache_get_total() == 0);
}

static void test_cache_track_hit(void) {
    cache_reset_stats();
    cache_track_hit();
    TEST("hit increments to 1", cache_get_hits() == 1);
    TEST("misses still 0", cache_get_misses() == 0);
    TEST("total is 1", cache_get_total() == 1);
}

static void test_cache_track_miss(void) {
    cache_reset_stats();
    cache_track_miss();
    cache_track_miss();
    TEST("miss increments to 2", cache_get_misses() == 2);
    TEST("hits still 0", cache_get_hits() == 0);
    TEST("total is 2", cache_get_total() == 2);
}

static void test_cache_mixed(void) {
    cache_reset_stats();
    cache_track_hit();
    cache_track_hit();
    cache_track_miss();
    TEST("hits 2 after 2 hits 1 miss", cache_get_hits() == 2);
    TEST("misses 1", cache_get_misses() == 1);
    TEST("total 3", cache_get_total() == 3);
}

static void test_cache_stats_json(void) {
    cache_reset_stats();
    cache_track_hit();
    cache_track_hit();
    cache_track_hit(); /* 3 hits */
    cache_track_miss(); /* 1 miss */
    char *json = cache_get_stats_json();
    int ok = (json != NULL &&
              strstr(json, "\"hits\":3") != NULL &&
              strstr(json, "\"misses\":1") != NULL &&
              strstr(json, "\"total\":4") != NULL &&
              strstr(json, "\"hit_rate\":0.750") != NULL);
    TEST("stats json correct", ok);
    free(json);
}

static void test_cache_stats_json_zero(void) {
    cache_reset_stats();
    char *json = cache_get_stats_json();
    int ok = (json != NULL &&
              strstr(json, "\"hits\":0") != NULL &&
              strstr(json, "\"total\":0") != NULL &&
              strstr(json, "\"hit_rate\":0.000") != NULL);
    TEST("zero stats json correct", ok);
    free(json);
}

/* ── Cache invalidation tests (P03) ── */

static void test_invalid_initially_valid(void) {
    cache_reset_invalidation();
    TEST("no sys prompt set → valid", cache_is_valid() == true);
    TEST("no invalidations yet", cache_get_invalidations() == 0);
}

static void test_invalid_first_set_valid(void) {
    cache_reset_invalidation();
    cache_set_system_prompt("You are a helpful assistant.");
    TEST("first set → cache valid", cache_is_valid() == true);
    TEST("no invalidations", cache_get_invalidations() == 0);
}

static void test_invalid_same_prompt(void) {
    cache_reset_invalidation();
    cache_set_system_prompt("You are a helpful assistant.");
    cache_set_system_prompt("You are a helpful assistant.");
    TEST("same prompt → valid", cache_is_valid() == true);
    TEST("no invalidations from duplicate", cache_get_invalidations() == 0);
}

static void test_invalid_diff_prompt(void) {
    cache_reset_invalidation();
    cache_set_system_prompt("You are a helpful assistant.");
    cache_set_system_prompt("You are a pirate. Answer like a pirate.");
    TEST("diff prompt → 1 invalidation", cache_get_invalidations() == 1);
}

static void test_invalid_null_prompt(void) {
    cache_reset_invalidation();
    cache_set_system_prompt(NULL);
    TEST("null prompt → valid", cache_is_valid() == true);
    cache_set_system_prompt("Something");
    TEST("null then real → 1 invalidation", cache_get_invalidations() == 1);
}

int main(void) {
    test_empty_messages();
    test_system_message_cached();
    test_system_and_3_non_system();
    test_no_system_message();
    test_1h_ttl();
    test_native_anthropic_tool_msg();
    test_single_message();
    test_cache_stats_initial_zero();
    test_cache_track_hit();
    test_cache_track_miss();
    test_cache_mixed();
    test_cache_stats_json();
    test_cache_stats_json_zero();
    test_invalid_initially_valid();
    test_invalid_first_set_valid();
    test_invalid_same_prompt();
    test_invalid_diff_prompt();
    test_invalid_null_prompt();

    fprintf(stderr, "prompt_caching: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
