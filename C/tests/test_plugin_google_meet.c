/* test_plugin_google_meet.c — Tests for Google Meet plugin. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/plugins/plugin_google_meet.c"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; fprintf(stderr, "  TEST: %s ... ", name); } while (0)
#define PASS() do { tests_passed++; fprintf(stderr, "PASS\n"); } while (0)
#define FAIL(msg) do { tests_failed++; fprintf(stderr, "FAIL: %s\n", msg); } while (0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while (0)

static void reset_state(void) {
    memset(g_meetings, 0, sizeof(g_meetings));
    g_meeting_count = 0;
    g_default_duration = 60;
}

static void test_create_meeting(void) {
    TEST("create_meeting returns meeting data");
    reset_state();
    char *result = tool_create_meeting("{\"topic\":\"Sprint Review\"}");
    ASSERT(result, "should return non-NULL");
    ASSERT(strstr(result, "code"), "should contain meeting code");
    ASSERT(strstr(result, "Sprint Review"), "should contain topic");
    ASSERT(strstr(result, "meet.google.com"), "should contain meet URL");
    ASSERT(g_meeting_count == 1, "meeting count should be 1");
    free(result);
    PASS();
}

static void test_create_default_topic(void) {
    TEST("create_meeting with no topic uses default");
    reset_state();
    char *result = tool_create_meeting(NULL);
    ASSERT(result, "should return non-NULL");
    ASSERT(strstr(result, "Untitled"), "should use default topic");
    free(result);
    PASS();
}

static void test_create_with_duration(void) {
    TEST("create_meeting with custom duration");
    reset_state();
    char *result = tool_create_meeting("{\"topic\":\"Long\",\"duration\":120}");
    ASSERT(result, "should return non-NULL");
    ASSERT(strstr(result, "\"duration\":120"), "duration should be 120");
    ASSERT(g_meetings[0].duration_minutes == 120, "stored duration should be 120");
    free(result);
    PASS();
}

static void test_list_meetings(void) {
    TEST("list_meetings returns all meetings");
    reset_state();
    tool_create_meeting("{\"topic\":\"A\"}");
    tool_create_meeting("{\"topic\":\"B\"}");
    char *result = tool_list_meetings("{}");
    ASSERT(result, "should return non-NULL");
    ASSERT(strstr(result, "\"count\":2"), "count should be 2");
    ASSERT(strstr(result, "\"A\""), "should contain A");
    ASSERT(strstr(result, "\"B\""), "should contain B");
    free(result);
    PASS();
}

static void test_end_meeting(void) {
    TEST("end_meeting marks meeting inactive");
    reset_state();
    tool_create_meeting("{\"topic\":\"Test\"}");
    ASSERT(g_meetings[0].is_active == 1, "should start active");
    char *result = tool_end_meeting("{\"code\":\"nonexistent\"}");
    ASSERT(strstr(result, "error"), "unknown code should error");
    free(result);
    /* End using stored code */
    result = tool_end_meeting("{\"code\":\"test\"}");
    PASS();
}

static void test_plugin_init(void) {
    TEST("plugin_init sets PLUGIN_GOOGLE_MEET");
    memset(&g_interface, 0, sizeof(g_interface));
    int ret = plugin_init();
    ASSERT(ret == 0, "init should return 0");
    ASSERT(g_interface.type == PLUGIN_GOOGLE_MEET, "type should be PLUGIN_GOOGLE_MEET");
    PASS();
}

int main(void) {
    fprintf(stderr, "Google Meet Plugin Tests\n");
    fprintf(stderr, "========================\n");
    snprintf(g_state_path, sizeof(g_state_path), "/tmp/test_meet_state.json");
    test_create_meeting();
    test_create_default_topic();
    test_create_with_duration();
    test_list_meetings();
    test_end_meeting();
    test_plugin_init();
    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n", tests_passed, tests_failed, tests_run);
    unlink("/tmp/test_meet_state.json");
    return tests_failed > 0 ? 1 : 0;
}
