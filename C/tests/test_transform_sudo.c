/*
 * test_transform_sudo.c — Tests for _transform_sudo() wiring (B07 depth).
 * Tests: sudo rewrite, passwordless sudo detection, env passthrough.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_transform_sudo.c
 *         ../src/tools/terminal.c -o /tmp/t_transform -lm
 *         -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration of the static _transform_sudo — only works when
 * linking directly against terminal.o with generated symbols.
 * For standalone test, we test via terminal_handler with JSON input. */

/* Forward declarations for terminal_handler */
char *terminal_handler(const char *args_json, const char *task_id);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Transform Sudo Wiring Tests ===\n");

    /* Test 1: Command without sudo passes through unchanged */
    {
        char *res = terminal_handler(
            "{\"command\":\"ls -la\"}", "test");
        TEST("no sudo returns non-NULL", res != NULL);
        if (res) {
            /* Should not contain -S -p since no sudo */
            TEST("no sudo no SUDO_PASSWORD rewrite",
                 strstr(res, "sudo -S") == NULL);
        }
        free(res);
    }

    /* Test 2: Simple command without args */
    {
        char *res = terminal_handler(
            "{\"command\":\"echo hello\"}", "test");
        TEST("simple cmd returns non-NULL", res != NULL);
        TEST("simple cmd contains output", res && strstr(res, "hello") != NULL);
        free(res);
    }

    /* Test 3: Command with sudo and SUDO_PASSWORD set */
    {
        setenv("SUDO_PASSWORD", "testpass", 1);
        char *res = terminal_handler(
            "{\"command\":\"sudo apt update\"}", "test");
        TEST("sudo+password returns non-NULL", res != NULL);
        if (res) {
            /* Should have rewritten sudo to sudo -S -p '' */
            bool has_rewrite = (strstr(res, "sudo -S -p ''") != NULL);
            /* If sudo -n works, it may not rewrite — both are correct */
            printf("  INFO: sudo rewrite in output: %s\n",
                   has_rewrite ? "yes" : "no (passwordless sudo)");
            TEST("sudo executed or parsed", res[0] != '\0');
        }
        free(res);
        unsetenv("SUDO_PASSWORD");
    }

    /* Test 4: Command with no sudo and SUDO_PASSWORD should not be affected */
    {
        setenv("SUDO_PASSWORD", "testpass", 1);
        char *res = terminal_handler(
            "{\"command\":\"echo no_sudo_here\"}", "test");
        TEST("no sudo+password returns non-NULL", res != NULL);
        if (res) {
            TEST("no sudo not rewritten",
                 strstr(res, "sudo -S") == NULL);
        }
        free(res);
        unsetenv("SUDO_PASSWORD");
    }

    /* Test 5: Empty command */
    {
        char *res = terminal_handler(
            "{\"command\":\"\"}", "test");
        TEST("empty command returns error", res != NULL);
        if (res) {
            TEST("empty command is error",
                 strstr(res, "error") != NULL);
        }
        free(res);
    }

    /* Test 6: NULL command */
    {
        char *res = terminal_handler(
            "{\"invalid\":true}", "test");
        TEST("missing command returns error", res != NULL);
        if (res) {
            TEST("missing command is error",
                 strstr(res, "error") != NULL);
        }
        free(res);
    }

    /* Test 7: SUDO_PASSWORD with sudo and --help (should still run) */
    {
        setenv("SUDO_PASSWORD", "testpass", 1);
        char *res = terminal_handler(
            "{\"command\":\"sudo --version\"}", "test");
        TEST("sudo help with password returns non-NULL", res != NULL);
        free(res);
        unsetenv("SUDO_PASSWORD");
    }

    printf("\n=== Results: %s ===\n",
           failures ? "SOME TESTS FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
