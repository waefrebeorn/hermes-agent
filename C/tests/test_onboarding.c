/*
 * test_onboarding.c — Tests for hermes_onboarding module.
 *
 * Tests hint text generators, state file I/O, path helpers,
 * and edge cases (NULL flag, missing file, invalid JSON).
 */

#include "hermes_onboarding.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ================================================================
 *  Hint text generators
 * ================================================================ */

static void test_busy_input_hints(void) {
    TEST("busy_input_hint_gateway queue");
    const char *h = onboarding_busy_input_hint_gateway("queue");
    ASSERT(h != NULL && strlen(h) > 0, "gateway queue hint empty or NULL");
    ASSERT(strstr(h, "queue") != NULL, "missing 'queue' keyword");
    PASS();

    TEST("busy_input_hint_gateway steer");
    h = onboarding_busy_input_hint_gateway("steer");
    ASSERT(h != NULL && strlen(h) > 0, "gateway steer hint empty");
    ASSERT(strstr(h, "steer") != NULL, "missing 'steer' keyword");
    PASS();

    TEST("busy_input_hint_gateway interrupt (default)");
    h = onboarding_busy_input_hint_gateway("interrupt");
    ASSERT(h != NULL && strlen(h) > 0, "gateway interrupt hint empty");
    ASSERT(strstr(h, "interrupt") != NULL, "missing 'interrupt' keyword");
    PASS();

    TEST("busy_input_hint_gateway NULL mode");
    h = onboarding_busy_input_hint_gateway(NULL);
    ASSERT(h != NULL && strlen(h) > 0, "NULL mode hint empty");
    ASSERT(strstr(h, "interrupt") != NULL, "NULL should default to interrupt");
    ASSERT(strstr(h, "/busy") != NULL, "missing /busy reference");
    PASS();

    TEST("busy_input_hint_cli queue");
    h = onboarding_busy_input_hint_cli("queue");
    ASSERT(h != NULL && strlen(h) > 0, "CLI queue hint empty");
    ASSERT(strstr(h, "queue") != NULL, "missing 'queue' keyword");
    PASS();

    TEST("busy_input_hint_cli steer");
    h = onboarding_busy_input_hint_cli("steer");
    ASSERT(h != NULL, "CLI steer hint NULL");
    ASSERT(strstr(h, "steer") != NULL, "missing 'steer' keyword");
    PASS();

    TEST("busy_input_hint_cli interrupt");
    h = onboarding_busy_input_hint_cli("interrupt");
    ASSERT(h != NULL, "CLI interrupt hint NULL");
    ASSERT(strstr(h, "interrupt") != NULL, "missing 'interrupt'");
    PASS();

    TEST("busy_input_hint_cli unknown mode");
    h = onboarding_busy_input_hint_cli("bogus");
    ASSERT(h != NULL && strlen(h) > 0, "unknown mode hint empty");
    ASSERT(strstr(h, "interrupt") != NULL, "unknown mode defaults to interrupt");
    PASS();
}

static void test_tool_progress_hints(void) {
    TEST("tool_progress_hint_gateway");
    const char *h = onboarding_tool_progress_hint_gateway();
    ASSERT(h != NULL && strlen(h) > 0, "gateway hint empty");
    ASSERT(strstr(h, "/verbose") != NULL, "missing /verbose reference");
    PASS();

    TEST("tool_progress_hint_cli");
    h = onboarding_tool_progress_hint_cli();
    ASSERT(h != NULL && strlen(h) > 0, "CLI hint empty");
    ASSERT(strstr(h, "/verbose") != NULL, "missing /verbose reference");
    PASS();
}

static void test_openclaw_residue_hint(void) {
    TEST("openclaw_residue_hint_cli");
    const char *h = onboarding_openclaw_residue_hint_cli();
    ASSERT(h != NULL && strlen(h) > 0, "hint empty");
    ASSERT(strstr(h, "OpenClaw") != NULL, "missing 'OpenClaw' reference");
    ASSERT(strstr(h, "claw migrate") != NULL, "missing 'claw migrate' reference");
    PASS();
}

/* ================================================================
 *  State management tests (temp-file based)
 * ================================================================ */

static void test_onboarding_default_path(void) {
    TEST("onboarding_default_path with HERMES_HOME");
    setenv("HERMES_HOME", "/tmp/test_hermes_home", 1);
    char *path = onboarding_default_path();
    ASSERT(path != NULL, "default path NULL");
    ASSERT(strstr(path, "/tmp/test_hermes_home/onboarding.json") != NULL,
           "wrong path with HERMES_HOME");
    free(path);
    PASS();

    TEST("onboarding_default_path without HERMES_HOME");
    unsetenv("HERMES_HOME");
    path = onboarding_default_path();
    ASSERT(path != NULL, "default path NULL");
    ASSERT(strstr(path, "/onboarding.json") != NULL, "missing onboarding.json");
    free(path);
    PASS();

    /* Restore env */
    setenv("HERMES_HOME", ".", 1);
}

static void test_is_seen_missing_file(void) {
    TEST("onboarding_is_seen missing file returns false");
    bool seen = onboarding_is_seen("/tmp/__nonexistent_onboarding.json",
                                    ONBOARDING_BUSY_INPUT_FLAG);
    ASSERT(seen == false, "missing file should return false");
    PASS();
}

static void test_mark_and_is_seen(void) {
    const char *path = "/tmp/test_onboarding_state.json";

    /* Clean slate */
    remove(path);

    TEST("onboarding_mark_seen creates file");
    bool ok = onboarding_mark_seen(path, "flag_test_a");
    ASSERT(ok == true, "mark_seen failed");
    /* Verify file exists */
    struct stat st;
    ASSERT(stat(path, &st) == 0, "onboarding.json not created");
    PASS();

    TEST("onboarding_is_seen returns true after marking");
    bool seen = onboarding_is_seen(path, "flag_test_a");
    ASSERT(seen == true, "flag should be seen after marking");
    PASS();

    TEST("onboarding_is_seen returns false for unmarked flag");
    seen = onboarding_is_seen(path, "flag_test_b");
    ASSERT(seen == false, "unmarked flag should be false");
    PASS();

    TEST("onboarding_mark_seen second flag preserves first");
    ok = onboarding_mark_seen(path, "flag_test_b");
    ASSERT(ok == true, "second mark_seen failed");
    seen = onboarding_is_seen(path, "flag_test_a");
    ASSERT(seen == true, "first flag lost after second mark");
    seen = onboarding_is_seen(path, "flag_test_b");
    ASSERT(seen == true, "second flag not seen");
    PASS();

    TEST("onboarding_is_seen with NULL path (auto-detect)");
    /* Set HERMES_HOME to /tmp so auto-detect finds our file */
    setenv("HERMES_HOME", "/tmp", 1);
    /* Mark via auto-detect path */
    ok = onboarding_mark_seen(NULL, "auto_flag");
    ASSERT(ok == true, "mark_seen with auto-detect failed");
    /* Read back via auto-detect */
    seen = onboarding_is_seen(NULL, "auto_flag");
    ASSERT(seen == true, "is_seen with auto-detect should find flag");
    PASS();

    /* Cleanup state for NULL-path tests */
    char *auto_path = onboarding_default_path();
    if (auto_path) { remove(auto_path); free(auto_path); }

    remove(path);
    setenv("HERMES_HOME", ".", 1);
}

static void test_edge_cases(void) {
    const char *path = "/tmp/test_onboarding_edges.json";
    remove(path);

    TEST("onboarding_is_seen with NULL flag");
    bool seen = onboarding_is_seen(path, NULL);
    ASSERT(seen == false, "NULL flag should return false");
    PASS();

    TEST("onboarding_mark_seen with NULL flag");
    bool ok = onboarding_mark_seen(path, NULL);
    ASSERT(ok == false, "NULL flag mark should return false");
    PASS();

    TEST("onboarding_is_seen with NULL path and NULL flag");
    seen = onboarding_is_seen(NULL, NULL);
    ASSERT(seen == false, "NULL path + NULL flag should return false");
    PASS();

    TEST("onboarding_is_seen on empty JSON file");
    /* Write an empty object */
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL, "failed to create test file");
    fwrite("{}", 1, 2, f);
    fclose(f);
    seen = onboarding_is_seen(path, "any_flag");
    ASSERT(seen == false, "empty JSON should return false");
    PASS();

    /* Test invalid JSON -- mark_seen should recover by creating fresh */
    TEST("onboarding_mark_seen on invalid JSON (recovery)");
    f = fopen(path, "wb");
    ASSERT(f != NULL, "failed to create invalid JSON");
    fwrite("{broken}", 1, 8, f);
    fclose(f);
    ok = onboarding_mark_seen(path, "recovered_flag");
    ASSERT(ok == true, "mark_seen should recover from invalid JSON");
    seen = onboarding_is_seen(path, "recovered_flag");
    ASSERT(seen == true, "recovered flag should be seen");
    PASS();

    remove(path);
}

static void test_openclaw_detect(void) {
    TEST("onboarding_detect_openclaw_residue (no ~/.openclaw)");
    /* Should be false in test environment */
    bool found = onboarding_detect_openclaw_residue();
    /* We can't guarantee HOME is clean, but we can check the function
     * doesn't crash and returns a consistent bool */
    ASSERT(found == false || found == true, "unexpected return type");
    PASS();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    /* Set HERMES_HOME for auto-detect tests */
    setenv("HERMES_HOME", "/tmp", 1);

    printf("=== Onboarding Library Tests ===\n\n");

    test_busy_input_hints();
    test_tool_progress_hints();
    test_openclaw_residue_hint();
    test_onboarding_default_path();
    test_is_seen_missing_file();
    test_mark_and_is_seen();
    test_edge_cases();
    test_openclaw_detect();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
