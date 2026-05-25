/*
 * test_approve.c — Approval security module unit tests
 * Tests pattern matching, cache management, and YOLO mode
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* External functions from approval.c */
extern void approval_set_yolo(bool enabled);
extern void approval_reset_session(void);
extern bool approval_is_terminal_dangerous(const char *command);
extern bool approval_is_path_dangerous(const char *path);
extern bool approval_is_path_traversal(const char *path);
extern bool approval_is_url_dangerous(const char *url);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Approval Security Tests ===\n");

    /* === Terminal dangerous patterns === */

    TEST("rm -rf is dangerous",
        approval_is_terminal_dangerous("rm -rf /"));

    TEST("rm -rf / is dangerous",
        approval_is_terminal_dangerous("rm -rf /"));

    TEST("dd if=/dev/zero is dangerous",
        approval_is_terminal_dangerous("dd if=/dev/zero of=/dev/sda"));

    TEST("mkfs is dangerous",
        approval_is_terminal_dangerous("mkfs.ext4 /dev/sda1"));

    TEST("sudo command is dangerous",
        approval_is_terminal_dangerous("sudo apt install"));

    TEST("reboot is dangerous",
        approval_is_terminal_dangerous("reboot"));

    TEST("safe command is not dangerous",
        !approval_is_terminal_dangerous("ls -la"));

    TEST("grep is not dangerous",
        !approval_is_terminal_dangerous("grep -r 'foo' ."));

    TEST("make is not dangerous",
        !approval_is_terminal_dangerous("make -j4"));

    TEST("null command is not dangerous",
        !approval_is_terminal_dangerous(NULL));

    TEST("empty command is not dangerous",
        !approval_is_terminal_dangerous(""));

    /* === Path dangerous patterns === */

    TEST("/etc/passwd is dangerous",
        approval_is_path_dangerous("/etc/passwd"));

    TEST("/boot/vmlinuz is dangerous",
        approval_is_path_dangerous("/boot/vmlinuz"));

    TEST("/dev/sda is dangerous",
        approval_is_path_dangerous("/dev/sda"));

    TEST("/usr/bin/gcc is dangerous",
        approval_is_path_dangerous("/usr/bin/gcc"));

    TEST("/home/user/file.txt is safe",
        !approval_is_path_dangerous("/home/user/file.txt"));

    TEST("/tmp/test.txt is safe",
        !approval_is_path_dangerous("/tmp/test.txt"));

    TEST("/var/log/syslog is safe",
        !approval_is_path_dangerous("/var/log/syslog"));

    TEST("null path is not dangerous",
        !approval_is_path_dangerous(NULL));

    /* === Path traversal patterns === */

    TEST("../ is traversal",
        approval_is_path_traversal("../etc/passwd"));

    TEST("..\\\\ is traversal",
        approval_is_path_traversal("..\\etc\\passwd"));

    TEST("%2e%2e is traversal",
        approval_is_path_traversal("%2e%2e/etc"));

    TEST("normal path is not traversal",
        !approval_is_path_traversal("/home/user/file.txt"));

    TEST("null path is not traversal",
        !approval_is_path_traversal(NULL));

    /* === YOLO mode (basic) === */

    approval_set_yolo(true);
    /* No crash means success for YOLO toggle */
    TEST("yolo enable does not crash",
        true);

    approval_set_yolo(false);
    TEST("yolo disable does not crash",
        true);

    /* === Session reset === */

    approval_reset_session();
    TEST("session reset does not crash",
        true);

    /* === URL patterns (quick string match only — no DNS) === */

    TEST("localhost url is dangerous",
        approval_is_url_dangerous("http://localhost:8080/admin"));

    TEST("127.0.0.1 url is dangerous",
        approval_is_url_dangerous("http://127.0.0.1:3000"));

    TEST("169.254.0.1 link-local is dangerous",
        approval_is_url_dangerous("http://169.254.0.1/config"));

    TEST("10.x internal is dangerous",
        approval_is_url_dangerous("http://10.0.0.1/admin"));

    TEST("192.168.x internal is dangerous",
        approval_is_url_dangerous("http://192.168.1.1/setup"));

    TEST("172.16.x internal is dangerous",
        approval_is_url_dangerous("http://172.16.0.1/secret"));

    TEST("null url is not dangerous",
        !approval_is_url_dangerous(NULL));

    /* === Summary === */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
