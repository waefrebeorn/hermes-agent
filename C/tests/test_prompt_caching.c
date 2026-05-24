/* test_prompt_caching.c — Tests for Anthropic prompt caching. */

#include "prompt_caching.h"
#include <stdio.h>
#include <string.h>

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

int main(void) {
    test_empty_messages();
    test_system_message_cached();
    test_system_and_3_non_system();
    test_no_system_message();
    test_1h_ttl();
    test_native_anthropic_tool_msg();
    test_single_message();

    fprintf(stderr, "prompt_caching: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
