/* test_file_watch.c — expanded test suite for file_watch tool.
 * Tests: handler error paths, parameter handling, edge cases.
 *
 * Build:
 *   gcc -O2 -g -Wall -I include -I lib/libjson \
 *       tests/test_file_watch.c src/tools/file_watch.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_fwatch -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Forward declaration — handler from file_watch.c */
extern char *file_watch_handler(const char *args_json, const char *task_id);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %-50s ... ", name); fflush(stdout); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ── Handler error paths ── */

static void test_null_args(void) {
    TEST("handler(NULL args) returns error");
    char *result = file_watch_handler(NULL, "test");
    assert(result != NULL);
    assert(strstr(result, "No args") != NULL);
    free(result);
    PASS();
}

static void test_bad_json(void) {
    TEST("handler(bad JSON) returns error");
    char *result = file_watch_handler("{bad json}", "test");
    assert(result != NULL);
    assert(strstr(result, "JSON parse") != NULL);
    free(result);
    PASS();
}

static void test_missing_path(void) {
    TEST("handler(empty obj) returns error");
    char *result = file_watch_handler("{}", "test");
    assert(result != NULL);
    assert(strstr(result, "Missing") != NULL || strstr(result, "error") != NULL);
    free(result);
    PASS();
}

static void test_empty_path(void) {
    TEST("handler(empty path) returns error");
    char *result = file_watch_handler("{\"path\":\"\"}", "test");
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    free(result);
    PASS();
}

static void test_nonexistent_path(void) {
    TEST("handler(nonexistent path) returns error");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_path_xyz789\"}", "test");
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    free(result);
    PASS();
}

/* ── Timeout clamping (tested via nonexistent path → error, but no crash) ── */

static void test_negative_timeout_clamped(void) {
    TEST("handler(negative timeout) no crash");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_abc\",\"timeout\":-5}", "test");
    assert(result != NULL);
    free(result);
    PASS();
}

static void test_large_timeout_clamped(void) {
    TEST("handler(timeout > 60) no crash");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_abc\",\"timeout\":999}", "test");
    assert(result != NULL);
    free(result);
    PASS();
}

/* ── Event names via handler (check parameter → result shape) ── */

static void test_event_modify_default(void) {
    TEST("handler(default events=modify) with nonexistent path");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_xyz\"}", "test");
    assert(result != NULL);
    free(result);
    PASS();
}

static void test_event_all_explicit(void) {
    TEST("handler(events=all) nonexistent path — no crash");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_xyz\",\"events\":\"all\"}", "test");
    assert(result != NULL);
    free(result);
    PASS();
}

static void test_event_comma_separated(void) {
    TEST("handler(events=create,delete) no crash");
    char *result = file_watch_handler("{\"path\":\"/nonexistent_xyz\",\"events\":\"create,delete\"}", "test");
    assert(result != NULL);
    free(result);
    PASS();
}

/* ── Positive test: watch a temp file ── */

static void test_watch_temp_file_timeout(void) {
    printf("\n--- positive watch test ---\n");
    /* Create temp file to watch — should get timeout (no events expected) */
    char tmpfile[] = "/tmp/hermes_fwatch_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    assert(fd >= 0);
    close(fd);

    char args[1024];
    snprintf(args, sizeof(args), "{\"path\":\"%s\",\"timeout\":1}", tmpfile);

    TEST("watch temp file returns valid JSON with events");
    char *result = file_watch_handler(args, "test");
    assert(result != NULL);
    assert(result[0] == '{');
    assert(strstr(result, "events") != NULL);
    assert(strstr(result, "timeout") != NULL);
    free(result);
    PASS();

    TEST("watch with all events");
    snprintf(args, sizeof(args), "{\"path\":\"%s\",\"events\":\"all\",\"timeout\":1}", tmpfile);
    result = file_watch_handler(args, "test");
    assert(result != NULL);
    assert(strstr(result, "events") != NULL);
    free(result);
    PASS();

    unlink(tmpfile);
}

/* ── Recursive watch (directory) ── */

static void test_recursive_watch(void) {
    char tmpdir[] = "/tmp/hermes_fwatch_dir_XXXXXX";
    char *dir = mkdtemp(tmpdir);
    assert(dir != NULL);

    char args[1024];
    snprintf(args, sizeof(args), "{\"path\":\"%s\",\"recursive\":true,\"timeout\":1}", dir);

    TEST("recursive directory watch");
    char *result = file_watch_handler(args, "test");
    assert(result != NULL);
    assert(strstr(result, "events") != NULL);
    free(result);
    PASS();

    rmdir(dir);
}

int main(void) {
    printf("=== File Watch Tests ===\n\n");

    printf("-- Handler Error Paths --\n");
    test_null_args();
    test_bad_json();
    test_missing_path();
    test_empty_path();
    test_nonexistent_path();

    printf("\n-- Timeout Clamping --\n");
    test_negative_timeout_clamped();
    test_large_timeout_clamped();

    printf("\n-- Event Parameters --\n");
    test_event_modify_default();
    test_event_all_explicit();
    test_event_comma_separated();

    test_watch_temp_file_timeout();
    test_recursive_watch();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
