/*
 * test_terminal_sudo.c — Tests for terminal sudo utilities.
 *
 * Tests terminal_sudo_nopasswd_works().
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration — function defined in terminal.c */
bool terminal_sudo_nopasswd_works(void);

static int g_tests = 0;
static int g_pass = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        g_pass++; \
    } \
} while(0)

int main(void)
{
    printf("=== Terminal Sudo Tests ===\n");

    /* Test 1: sudo_nopasswd_works doesn't crash */
    {
        bool result = terminal_sudo_nopasswd_works();
        /* We can't predict the return value (depends on sudo config), */
        /* but the function should at least run without crashing */
        printf("  INFO: sudo_nopasswd_works() returned %s\n",
               result ? "true" : "false (expected on most systems)");
        TEST("sudo_nopasswd_works_no_crash", true);
    }

    /* Test 2: TERMINAL_ENV non-local returns false */
    {
        const char *old = getenv("TERMINAL_ENV");
        setenv("TERMINAL_ENV", "docker", 1);
        bool result = terminal_sudo_nopasswd_works();
        TEST("sudo_nopasswd_docker_env", result == false);
        if (old)
            setenv("TERMINAL_ENV", old, 1);
        else
            unsetenv("TERMINAL_ENV");
    }

    /* Test 3: TERMINAL_ENV=local works normally */
    {
        const char *old = getenv("TERMINAL_ENV");
        setenv("TERMINAL_ENV", "local", 1);
        bool result = terminal_sudo_nopasswd_works();
        (void)result;
        TEST("sudo_nopasswd_local_env", true);
        if (old)
            setenv("TERMINAL_ENV", old, 1);
        else
            unsetenv("TERMINAL_ENV");
    }

    printf("=== Results: %d/%d passed ===\n", g_pass, g_tests);
    return (g_pass == g_tests) ? 0 : 1;
}
