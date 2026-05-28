/*
 * test_debug_helpers.c — Tests for debug session logging.
 * Port of Python tools/debug_helpers.py tests.
 */

#include "debug_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int tests = 0;
static int passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ─── Tests ─────────────────────────────────────────────── */

static void test_session_init_disabled(void)
{
    debug_session_t session;
    /* No env var set — should be disabled */
    debug_session_init(&session, "test_tool", "TEST_DEBUG_NONEXISTENT", "/tmp");
    TEST("disabled by default", !session.enabled);
    TEST("tool name set", strcmp(session.tool_name, "test_tool") == 0);
    TEST("call count starts at 0", session.call_count == 0);
    TEST("not active", !debug_session_active(&session));
}

static void test_session_init_enabled(void)
{
    setenv("TEST_DEBUG_ENABLE", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "my_tool", "TEST_DEBUG_ENABLE", "/tmp");
    TEST("enabled when env var is true", session.enabled);
    TEST("session id generated", strlen(session.session_id) > 0);
    TEST("start time set", strlen(session.start_time) > 0);
    TEST("log dir set", strlen(session.log_dir) > 0);
    TEST("active check", debug_session_active(&session));

    unsetenv("TEST_DEBUG_ENABLE");
}

static void test_session_init_env_1(void)
{
    setenv("TEST_DEBUG_ONE", "1", 1);

    debug_session_t session;
    debug_session_init(&session, "tool", "TEST_DEBUG_ONE", "/tmp");
    TEST("enabled with 1", session.enabled);

    unsetenv("TEST_DEBUG_ONE");
}

static void test_log_calls(void)
{
    setenv("TEST_DEBUG_LOG", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "log_tool", "TEST_DEBUG_LOG", "/tmp");
    TEST("enabled for log test", session.enabled);

    debug_session_log_call(&session, "search", "{\"query\": \"hello\"}");
    TEST("call count incremented", session.call_count == 1);
    TEST("call tool name", strcmp(session.calls[0].tool_name, "search") == 0);

    debug_session_log_call(&session, "crawl", "{\"url\": \"example.com\"}");
    TEST("second call", session.call_count == 2);

    unsetenv("TEST_DEBUG_LOG");
}

static void test_log_disabled_noop(void)
{
    debug_session_t session;
    debug_session_init(&session, "noop", "TEST_DEBUG_NONEXISTENT", "/tmp");
    TEST("disabled", !session.enabled);

    debug_session_log_call(&session, "any", "data");
    TEST("calls ignored when disabled", session.call_count == 0);
}

static void test_save_json(void)
{
    setenv("TEST_DEBUG_SAVE", "true", 1);
    const char *tmp_log = "/tmp";

    debug_session_t session;
    debug_session_init(&session, "save_tool", "TEST_DEBUG_SAVE", tmp_log);
    TEST("enabled", session.enabled);

    debug_session_log_call(&session, "search", "{\"query\":\"test\"}");
    debug_session_log_call(&session, "extract", "{\"url\":\"x.com\"}");

    debug_session_save(&session, tmp_log);

    /* Check the file was created */
    char expected_path[1024];
    snprintf(expected_path, sizeof(expected_path),
             "%s/logs/save_tool_debug_%s.json",
             tmp_log, session.session_id);
    TEST("log file exists", access(expected_path, F_OK) == 0);

    /* Clean up */
    unlink(expected_path);
    unsetenv("TEST_DEBUG_SAVE");
}

static void test_get_info(void)
{
    setenv("TEST_DEBUG_INFO", "true", 1);

    debug_session_t session;
    debug_session_init(&session, "info_tool", "TEST_DEBUG_INFO", "/tmp");
    TEST("enabled", session.enabled);

    debug_session_log_call(&session, "test_call", "data");

    char *info = debug_session_get_info(&session);
    TEST("get_info returns non-null", info != NULL);
    if (info) {
        TEST("info contains enabled", strstr(info, "\"enabled\":true") != NULL);
        TEST("info contains session_id", strstr(info, "\"session_id\"") != NULL);
        TEST("info contains total_calls", strstr(info, "\"total_calls\":1") != NULL);
        free(info);
    }

    unsetenv("TEST_DEBUG_INFO");
}

static void test_get_info_disabled(void)
{
    debug_session_t session;
    debug_session_init(&session, "off", "TEST_DEBUG_NONEXISTENT", "/tmp");
    char *info = debug_session_get_info(&session);
    TEST("get_info returns NULL when disabled", info == NULL);
    free(info);
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_session_init_disabled();
    test_session_init_enabled();
    test_session_init_env_1();
    test_log_calls();
    test_log_disabled_noop();
    test_save_json();
    test_get_info();
    test_get_info_disabled();

    printf("debug_helpers: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
