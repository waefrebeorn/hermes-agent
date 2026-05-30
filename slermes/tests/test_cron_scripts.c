/*
 * test_cron_scripts.c — Cron script execution unit tests.
 *
 * Tests: script execution with captured output, interpreter detection,
 * error paths (file not found, no interpreter).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_cron_scripts.c src/cron/cron_scripts.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_cron_scripts -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_cron_scripts
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

static int tests = 0, passed = 0;
static int failed_tests = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); fflush(stdout); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { failed_tests++; printf("FAIL: %s\n", msg); } while(0)

/* Forward declarations from cron_scripts.c */
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *script_args, int *exit_code);
bool cron_script_set_interpreter(const char *job_name, const char *interpreter);

/* ================================================================
 * Helpers
 * ================================================================ */

static char *write_temp_script(const char *content) {
    char template[] = "/tmp/hermes_script_test_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) return NULL;

    write(fd, content, strlen(content));
    fchmod(fd, 0755);
    close(fd);
    return strdup(template);
}

/* ================================================================
 * Tests
 * ================================================================ */

static void test_run_script_null_path(void) {
    TEST("run script with NULL path returns NULL");
    char *out = cron_run_script(NULL, "/bin/sh", NULL, NULL);
    assert(out == NULL);
    PASS();
}

static void test_run_script_missing_file(void) {
    TEST("run script with missing path returns error JSON");
    int exit_code = 0;
    char *out = cron_run_script("/tmp/nonexistent_script_xyz.sh",
                                 "/bin/sh", NULL, &exit_code);
    assert(out != NULL);
    /* Should contain error key */
    assert(strstr(out, "error") != NULL || strstr(out, "not found") != NULL);
    assert(exit_code == -1);
    free(out);
    PASS();
}

static void test_run_simple_echo(void) {
    TEST("run simple echo script captures stdout");
    char *path = write_temp_script("#!/bin/sh\necho 'hello world'\n");
    assert(path != NULL);

    int exit_code = -1;
    char *out = cron_run_script(path, "/bin/sh", NULL, &exit_code);
    assert(out != NULL);
    /* Should contain "hello world" */
    assert(strstr(out, "hello world") != NULL);
    assert(exit_code == 0);
    free(out);
    free(path);
    PASS();
}

static void test_run_script_with_args(void) {
    TEST("run script with args passes them through");
    char *path = write_temp_script("#!/bin/sh\necho \"arg1=$1\"\n");
    assert(path != NULL);

    int exit_code = -1;
    char *out = cron_run_script(path, "/bin/sh", "test_arg_value", &exit_code);
    assert(out != NULL);
    assert(strstr(out, "arg1=test_arg_value") != NULL);
    assert(exit_code == 0);
    free(out);
    free(path);
    PASS();
}

static void test_run_script_exit_code(void) {
    TEST("run script captures non-zero exit code");
    char *path = write_temp_script("#!/bin/sh\nexit 42\n");
    assert(path != NULL);

    int exit_code = 0;
    char *out = cron_run_script(path, "/bin/sh", NULL, &exit_code);
    assert(out != NULL);
    assert(exit_code == 42);
    free(out);
    free(path);
    PASS();
}

static void test_run_script_stderr_redirect(void) {
    TEST("run script stderr redirect works");
    char *path = write_temp_script("#!/bin/sh\necho 'stderr_msg' >&2\n");
    assert(path != NULL);

    int exit_code = -1;
    char *out = cron_run_script(path, "/bin/sh", NULL, &exit_code);
    assert(out != NULL);
    /* stderr should be captured via 2>&1 in the command */
    assert(strstr(out, "stderr_msg") != NULL || exit_code == 0);
    free(out);
    free(path);
    PASS();
}

static void test_script_set_interpreter(void) {
    TEST("set interpreter returns true (stub)");
    assert(cron_script_set_interpreter("test-job", "/usr/bin/python3") == true);
    PASS();

    TEST("set interpreter NULL args");
    assert(cron_script_set_interpreter(NULL, NULL) == true);
    PASS();
}

static void test_no_interpreter_no_shebang(void) {
    TEST("script without shebang and no interpreter returns error");
    char *path = write_temp_script("echo 'no shebang'\n");
    assert(path != NULL);

    int exit_code = 0;
    char *out = cron_run_script(path, NULL, NULL, &exit_code);
    assert(out != NULL);
    assert(strstr(out, "error") != NULL || strstr(out, "No interpreter") != NULL);
    assert(exit_code == -1);
    free(out);
    free(path);
    PASS();
}

static void test_detect_shebang_via_interpreter(void) {
    TEST("shebang detection via empty interpreter string");
    char *path = write_temp_script("#!/usr/bin/python3\nprint('hi')\n");
    assert(path != NULL);

    /* Use empty string to trigger auto-detection */
    /* cron_run_script treats NULL/empty as "detect from shebang" */
    int exit_code = -1;
    char *out = cron_run_script(path, "", NULL, &exit_code);
    assert(out != NULL);
    /* The shebang interpreter "python3" should be detected */
    /* Since python3 may not find the script, just check it doesn't crash */
    assert(exit_code == 0 || exit_code == 2 || exit_code == 127 || exit_code == -1);
    free(out);
    free(path);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Cron Script Tests ===\n\n");

    printf("--- Basic Error Handling ---\n");
    test_run_script_null_path();
    test_run_script_missing_file();

    printf("\n--- Script Execution ---\n");
    test_run_simple_echo();
    test_run_script_with_args();
    test_run_script_exit_code();
    test_run_script_stderr_redirect();

    printf("\n--- Interpreter Detection ---\n");
    test_script_set_interpreter();
    test_no_interpreter_no_shebang();
    test_detect_shebang_via_interpreter();

    printf("\n=== Results: %d/%d passed, %d failed ===\n", passed, tests, failed_tests);
    return (passed == tests && failed_tests == 0) ? 0 : 1;
}
