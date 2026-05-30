/*
 * test_terminal_sudo.c — Tests for terminal_rewrite_sudo() (B07 depth).
 * Tests: sudo replacement, env assignment preservation, comments, operators.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_terminal_sudo.c
 *         ../src/tools/terminal.c -o /tmp/t_sudo -lm
 *         -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration */
char *terminal_rewrite_sudo(const char *command, bool *found);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Terminal Sudo Rewrite Tests ===\n");

    /* Test 1: NULL command */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo(NULL, &found);
        TEST("NULL command returns NULL", res == NULL);
        TEST("NULL command found=false", found == false);
    }

    /* Test 2: Empty command */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("", &found);
        TEST("empty command returns empty", res && res[0] == '\0');
        TEST("empty command found=false", found == false);
        free(res);
    }

    /* Test 3: Simple sudo replacement */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("sudo apt update", &found);
        TEST("sudo apt update returns non-NULL", res != NULL);
        TEST("sudo apt update found=true", found == true);
        if (res) {
            TEST("sudo replaced with -S -p", strstr(res, "sudo -S -p ''") != NULL);
            TEST("apt preserved", strstr(res, "apt update") != NULL);
        }
        free(res);
    }

    /* Test 4: No sudo passthrough */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("ls -la", &found);
        TEST("no sudo returns non-NULL", res != NULL);
        TEST("no sudo found=false", found == false);
        if (res) {
            TEST("no sudo passthrough unchanged", strcmp(res, "ls -la") == 0);
        }
        free(res);
    }

    /* Test 5: sudo not at command start (not replaced) */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("echo sudo", &found);
        TEST("sudo not at start found=false", found == false);
        if (res) {
            TEST("sudo not at start passthrough", strcmp(res, "echo sudo") == 0);
        }
        free(res);
    }

    /* Test 6: Env assignment before sudo keeps command_start */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("PATH=/usr/bin sudo cmd", &found);
        TEST("env assignment + sudo found=true", found == true);
        if (res) {
            TEST("env assignment preserved", strstr(res, "PATH=/usr/bin") != NULL);
            TEST("sudo rewritten", strstr(res, "sudo -S -p ''") != NULL);
        }
        free(res);
    }

    /* Test 7: Multiple sudo in compound command */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("sudo cmd1 && sudo cmd2", &found);
        TEST("compound sudo found=true", found == true);
        if (res) {
            TEST("first sudo rewritten", strstr(res, "sudo -S -p ''") != NULL);
            /* Count replacements */
            int count = 0;
            const char *p = res;
            while ((p = strstr(p, "sudo -S -p ''")) != NULL) { count++; p++; }
            TEST("both sudo rewritten", count == 2);
        }
        free(res);
    }

    /* Test 8: Comment at start skips sudo detection */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("# sudo comment\nsudo cmd", &found);
        TEST("comment then sudo found=true", found == true);
        if (res) {
            TEST("comment preserved", strstr(res, "# sudo comment") != NULL);
            TEST("second sudo rewritten", strstr(res, "sudo -S -p ''") != NULL);
        }
        free(res);
    }

    /* Test 9: Pipe operator resets command_start */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("cmd | sudo tee", &found);
        TEST("pipe + sudo found=true", found == true);
        if (res) {
            TEST("sudo after pipe rewritten", strstr(res, "sudo -S -p ''") != NULL);
        }
        free(res);
    }

    /* Test 10: Semicolon resets command_start */
    {
        bool found = false;
        char *res = terminal_rewrite_sudo("cmd1; sudo cmd2", &found);
        TEST("semicolon + sudo found=true", found == true);
        if (res) {
            TEST("sudo after semicolon rewritten", strstr(res, "sudo -S -p ''") != NULL);
        }
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All terminal sudo rewrite tests PASSED");
    return failures ? 1 : 0;
}
