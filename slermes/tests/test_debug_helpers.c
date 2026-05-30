/*
 * test_debug_helpers.c — Debug session test suite.
 *
 * Tests: init, active, log, get_info, save, NULL safety.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libdebug \
 *       tests/test_debug_helpers.c lib/libdebug/debug_helpers.c \
 *       -o /tmp/hermes_test_debug -lm
 *
 * Run:
 *   /tmp/hermes_test_debug
 */

#define _GNU_SOURCE
#include "debug_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))

/* ================================================================
 *  1. Init without env var → disabled
 * ================================================================ */
static void test_init_no_env(void) {
    printf("\n--- Init: No Env Var ---\n");

    /* Unset any lingering env var */
    unsetenv("DEBUG_TEST_ACTIVE");

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");
    TEST_FALSE("disabled when env var not set", session.enabled);
    TEST_TRUE("session_id empty when disabled",
              session.session_id[0] == '\0');
    TEST("call_count starts at 0", session.call_count == 0);
}

/* ================================================================
 *  2. Init with "true" env var → enabled
 * ================================================================ */
static void test_init_true(void) {
    printf("\n--- Init: Env Var 'true' ---\n");

    setenv("DEBUG_TEST_ACTIVE", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");
    TEST_TRUE("enabled when env var is 'true'", session.enabled);
    TEST_TRUE("has session_id when enabled",
              session.session_id[0] != '\0');
    TEST("tool_name set",
         strcmp(session.tool_name, "test_tool") == 0);
    TEST("has log_dir", session.log_dir[0] != '\0');
    TEST("call_count starts at 0", session.call_count == 0);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  3. Init with "1" env var → enabled
 * ================================================================ */
static void test_init_one(void) {
    printf("\n--- Init: Env Var '1' ---\n");

    setenv("DEBUG_TEST_ACTIVE", "1", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");
    TEST_TRUE("enabled when env var is '1'", session.enabled);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  4. Init with "false" env var → disabled
 * ================================================================ */
static void test_init_false(void) {
    printf("\n--- Init: Env Var 'false' ---\n");

    setenv("DEBUG_TEST_ACTIVE", "false", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");
    TEST_FALSE("disabled when env var is 'false'", session.enabled);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  5. NULL pointer safety
 * ================================================================ */
static void test_null_safety(void) {
    printf("\n--- NULL Safety ---\n");

    /* NULL session pointer */
    debug_session_init(NULL, "tool", "ENV", "/tmp");
    TEST("NULL init doesn't crash", 1);

    debug_session_active(NULL);
    TEST("NULL active doesn't crash", 1);

    debug_session_log_call(NULL, "call", "data");
    TEST("NULL log_call doesn't crash", 1);

    debug_session_save(NULL, "/tmp");
    TEST("NULL save doesn't crash", 1);

    char *info = debug_session_get_info(NULL);
    TEST_TRUE("NULL get_info returns NULL", info == NULL);

    /* NULL tool_name */
    debug_session_t session;
    debug_session_init(&session, NULL, "DEBUG_TEST_ACTIVE", "/tmp");
    TEST("NULL tool_name doesn't crash", 1);

    /* NULL hermes_home */
    debug_session_init(&session, "tool", "DEBUG_TEST_ACTIVE", NULL);
    TEST("NULL hermes_home doesn't crash", 1);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  6. Log calls when disabled → no-op
 * ================================================================ */
static void test_log_disabled(void) {
    printf("\n--- Log: Disabled ---\n");

    unsetenv("DEBUG_TEST_ACTIVE");

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");

    debug_session_log_call(&session, "search", "{\"q\":\"hello\"}");
    TEST("log call count 0 when disabled", session.call_count == 0);

    char *info = debug_session_get_info(&session);
    TEST_TRUE("get_info NULL when disabled", info == NULL);
}

/* ================================================================
 *  7. Log calls when enabled
 * ================================================================ */
static void test_log_enabled(void) {
    printf("\n--- Log: Enabled ---\n");

    setenv("DEBUG_TEST_ACTIVE", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");

    TEST_TRUE("session active", debug_session_active(&session));

    /* Log a call */
    debug_session_log_call(&session, "search", "{\"q\":\"hello\"}");
    TEST("call_count incremented", session.call_count == 1);
    TEST("call_data stored",
         strcmp(session.calls[0].call_data, "{\"q\":\"hello\"}") == 0);
    TEST("call tool_name matches",
         strcmp(session.calls[0].tool_name, "search") == 0);
    TEST_TRUE("timestamp not empty", session.calls[0].timestamp[0] != '\0');

    /* Log another */
    debug_session_log_call(&session, "fetch", "https://example.com");
    TEST("second call logged", session.call_count == 2);
    TEST("confirmed second call tool_name",
         strcmp(session.calls[1].tool_name, "fetch") == 0);

    /* NULL call_data */
    debug_session_log_call(&session, "nulldata", NULL);
    TEST("NULL call_data logged", session.call_count == 3);
    TEST("NULL call_data stays empty",
         session.calls[2].call_data[0] == '\0');

    /* NULL call_name */
    debug_session_log_call(&session, NULL, "data");
    TEST("NULL call_name logged", session.call_count == 4);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  8. get_info returns JSON
 * ================================================================ */
static void test_get_info(void) {
    printf("\n--- get_info JSON ---\n");

    setenv("DEBUG_TEST_ACTIVE", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");

    debug_session_log_call(&session, "call1", "data1");
    debug_session_log_call(&session, "call2", "data2");

    char *info = debug_session_get_info(&session);
    TEST_TRUE("get_info returns non-NULL", info != NULL);
    if (info) {
        TEST_TRUE("contains 'enabled'", strstr(info, "\"enabled\":true") != NULL);
        TEST_TRUE("contains session_id", strstr(info, "\"session_id\"") != NULL);
        TEST_TRUE("contains total_calls", strstr(info, "\"total_calls\":2") != NULL);
        free(info);
    }

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  9. Save to file
 * ================================================================ */
static void test_save(void) {
    printf("\n--- Save to File ---\n");

    setenv("DEBUG_TEST_ACTIVE", "true", 1);

    debug_session_t session;
    char hermes_home[] = "/tmp/debug_test_XXXXXX";
    char *tmpdir = mkdtemp(hermes_home);
    TEST_TRUE("tmpdir created", tmpdir != NULL);

    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", tmpdir);
    TEST_TRUE("session enabled", session.enabled);

    debug_session_log_call(&session, "call1", "{\"val\":1}");

    /* Save should create the file */
    debug_session_save(&session, tmpdir);

    /* Check the file exists */
    char path[1024];
    snprintf(path, sizeof(path), "%s/logs/%s_debug_%s.json",
             tmpdir, session.tool_name, session.session_id);
    FILE *f = fopen(path, "r");
    TEST_TRUE("save file exists", f != NULL);
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        TEST("save file non-empty", sz > 0);
        fclose(f);
    }

    /* Cleanup */
    unlink(path);
    rmdir(tmpdir);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  10. Max calls limit
 * ================================================================ */
static void test_max_calls(void) {
    printf("\n--- Max Calls Limit ---\n");

    setenv("DEBUG_TEST_ACTIVE", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "test_tool", "DEBUG_TEST_ACTIVE", "/tmp");

    /* Log DEBUG_MAX_CALLS+1 calls */
    for (int i = 0; i < DEBUG_MAX_CALLS + 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "call_%d", i);
        debug_session_log_call(&session, buf, NULL);
    }

    TEST("call_count capped at MAX", session.call_count == DEBUG_MAX_CALLS);

    unsetenv("DEBUG_TEST_ACTIVE");
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Debug Helpers Test Suite ===\n");

    test_init_no_env();
    test_init_true();
    test_init_one();
    test_init_false();
    test_null_safety();
    test_log_disabled();
    test_log_enabled();
    test_get_info();
    test_save();
    test_max_calls();

    printf("\n--- Results: %d passed, %d failed ---\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
