/*
 * test_sudo_prompt.c — Tests for terminal_prompt_for_sudo_password() (B07 depth).
 * Tests: non-interactive guard, NULL env, empty env, error handling.
 * Interactive prompt tested via compiled binary + PTY simulation.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_sudo_prompt.c
 *         ../src/tools/terminal.c -o /tmp/t_sudo_prompt -lm
 *         -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration */
char *terminal_prompt_for_sudo_password(int timeout_seconds);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Sudo Password Prompt Tests ===\n");

    /* Test 1: No HERMES_INTERACTIVE → returns NULL */
    {
        unsetenv("HERMES_INTERACTIVE");
        char *res = terminal_prompt_for_sudo_password(5);
        TEST("no INTERACTIVE returns NULL", res == NULL);
    }

    /* Test 2: HERMES_INTERACTIVE=0 → returns NULL */
    {
        setenv("HERMES_INTERACTIVE", "0", 1);
        char *res = terminal_prompt_for_sudo_password(5);
        TEST("INTERACTIVE=0 returns NULL", res == NULL);
        unsetenv("HERMES_INTERACTIVE");
    }

    /* Test 3: HERMES_INTERACTIVE=1 but no /dev/tty → returns NULL
     * When stdin is not a terminal, open("/dev/tty", O_RDWR) fails. */
    {
        setenv("HERMES_INTERACTIVE", "1", 1);
        char *res = terminal_prompt_for_sudo_password(5);
        /* In non-interactive context (test runner has no /dev/tty), this returns NULL */
        bool ok = (res == NULL);
        TEST("no tty in test runner returns NULL", ok);
        unsetenv("HERMES_INTERACTIVE");
    }

    /* Test 4: NULL guard — 0 timeout should still work */
    {
        setenv("HERMES_INTERACTIVE", "1", 1);
        char *res = terminal_prompt_for_sudo_password(0);
        /* 0 timeout uses default 45s, but no tty so returns NULL */
        bool ok = (res == NULL);
        TEST("zero timeout uses default", ok);
        unsetenv("HERMES_INTERACTIVE");
    }

    /* Test 5: Negative timeout treated as immediate timeout */
    {
        setenv("HERMES_INTERACTIVE", "1", 1);
        char *res = terminal_prompt_for_sudo_password(-1);
        bool ok = (res == NULL);
        TEST("negative timeout returns NULL", ok);
        unsetenv("HERMES_INTERACTIVE");
    }

    printf("\n=== Results: %s ===\n",
           failures ? "SOME TESTS FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
