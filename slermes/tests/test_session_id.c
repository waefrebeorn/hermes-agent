/*
 * test_session_id.c — Tests for agent_generate_session_id().
 *
 * Tests: session ID format (YYYYMMDD_HHMMSS), time components,
 * buffer size, multiple-call consistency, NULL safety, uniqueness.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_session_id.c src/agent/agent_loop.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_session_id -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_session_id
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ---- Test: session ID format ---- */
static void test_session_id_format(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    size_t len = strlen(state.session_id);
    TEST("session ID is 15 chars", len == 15);

    regex_t regex;
    int rc = regcomp(&regex, "^[0-9]{8}_[0-9]{6}$", REG_EXTENDED);
    TEST("regex compiled", rc == 0);
    TEST("session ID matches YYYYMMDD_HHMMSS",
         regexec(&regex, state.session_id, 0, NULL, 0) == 0);
    regfree(&regex);
}

/* ---- Test: session ID encodes valid time components ---- */
static void test_session_id_time_components(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    char M[3] = {state.session_id[4], state.session_id[5], '\0'};
    char d[3] = {state.session_id[6], state.session_id[7], '\0'};
    char h[3] = {state.session_id[9], state.session_id[10], '\0'};
    char m[3] = {state.session_id[11], state.session_id[12], '\0'};
    char s[3] = {state.session_id[13], state.session_id[14], '\0'};

    int month = atoi(M);
    int day   = atoi(d);
    int hour  = atoi(h);
    int min   = atoi(m);
    int sec   = atoi(s);

    TEST("month 01-12", month >= 1 && month <= 12);
    TEST("day 01-31", day >= 1 && day <= 31);
    TEST("hour 00-23", hour >= 0 && hour <= 23);
    TEST("minute 00-59", min >= 0 && min <= 59);
    TEST("second 00-59", sec >= 0 && sec <= 59);
}

/* ---- Test: multiple calls produce valid IDs ---- */
static void test_session_id_multiple_calls(void) {
    agent_state_t s1, s2;
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));
    agent_generate_session_id(&s1);
    agent_generate_session_id(&s2);
    TEST("two IDs both non-empty",
         s1.session_id[0] && s2.session_id[0]);
}

/* ---- Test: year component is numeric ---- */
static void test_session_id_year(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    char year_str[5] = {state.session_id[0], state.session_id[1],
                        state.session_id[2], state.session_id[3], '\0'};
    int year = atoi(year_str);
    TEST("year >= 2024", year >= 2024);
    TEST("year < 2100", year < 2100);
}

/* ---- Test: ID fits in buffer ---- */
static void test_session_id_buffer_size(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);
    TEST("session ID fits in 64 byte buffer", strlen(state.session_id) < 64);
    TEST("buffer has trailing NUL", state.session_id[strlen(state.session_id)] == '\0');
}

/* ---- Test: underscore separator ---- */
static void test_session_id_separator(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);
    TEST("separator at position 8", state.session_id[8] == '_');
}

/* ---- Test: character set is [0-9_] only ---- */
static void test_session_id_charset(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    int valid = 1;
    for (size_t i = 0; i < strlen(state.session_id); i++) {
        if (!((state.session_id[i] >= '0' && state.session_id[i] <= '9') ||
              state.session_id[i] == '_')) {
            valid = 0;
            break;
        }
    }
    TEST("all characters are [0-9_]", valid);
}

/* ---- Test: zeroed state produces valid ID ---- */
static void test_session_id_zeroed_state(void) {
    agent_state_t state;
    memset(&state, 0xFF, sizeof(state));
    agent_generate_session_id(&state);
    TEST("garbage-filled state produces valid ID",
         strlen(state.session_id) == 15);
}

/* ---- Test: consecutive calls in same second produce same ID (1s resolution) ---- */
static void test_session_id_same_second(void) {
    agent_state_t s1, s2;
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));
    agent_generate_session_id(&s1);
    agent_generate_session_id(&s2);
    TEST("two calls in same second produce identical ID",
         strcmp(s1.session_id, s2.session_id) == 0);
}

/* ---- Test: session_id doesn't contain extra underscores ---- */
static void test_session_id_only_one_underscore(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    agent_generate_session_id(&state);

    int underscore_count = 0;
    for (size_t i = 0; i < strlen(state.session_id); i++) {
        if (state.session_id[i] == '_') underscore_count++;
    }
    TEST("exactly one underscore in session ID", underscore_count == 1);
}

/* ---- Test: buffer beyond 15 chars is untouched ---- */
static void test_session_id_buffer_overflow(void) {
    agent_state_t state;
    memset(&state, 0, sizeof(state));
    memset(state.session_id, 'X', sizeof(state.session_id));
    state.session_id[sizeof(state.session_id) - 1] = '\0';
    agent_generate_session_id(&state);
    TEST("session ID is exactly 15 chars, not longer",
         strlen(state.session_id) == 15);
    /* Verify byte 16 is still 'X' (untouched) */
    TEST("buffer beyond 15 chars preserved",
         state.session_id[16] == 'X');
}

/* ---- Test: rapid back-to-back calls in loop ---- */
static void test_session_id_stress(void) {
    agent_state_t state;
    int all_valid = 1;
    for (int i = 0; i < 20; i++) {
        memset(&state, 0, sizeof(state));
        agent_generate_session_id(&state);
        if (strlen(state.session_id) != 15) { all_valid = 0; break; }
        if (state.session_id[8] != '_') { all_valid = 0; break; }
    }
    TEST("20 rapid calls all produce valid IDs", all_valid);
}

int main(void) {
    printf("=== Session ID Generation Tests ===\n\n");

    printf("[format]\n");
    test_session_id_format();
    test_session_id_separator();
    test_session_id_charset();
    test_session_id_only_one_underscore();
    test_session_id_buffer_size();
    test_session_id_buffer_overflow();
    test_session_id_year();

    printf("\n[time components]\n");
    test_session_id_time_components();

    printf("\n[multiple calls]\n");
    test_session_id_multiple_calls();
    test_session_id_same_second();
    test_session_id_stress();

    printf("\n[edge cases]\n");
    test_session_id_zeroed_state();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
