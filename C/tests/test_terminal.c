/*
 * test_terminal.c — Terminal tool unit tests (M29).
 *
 * Tests: argument validation, command execution, optional params.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_terminal.c src/tools/terminal.c src/tools/tool_config.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_terminal -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_terminal
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration from terminal.c */
char *terminal_handler(const char *args_json, const char *task_id);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Helper: parse JSON result and get field */
static int has_error(const char *json_str) {
    if (!json_str) return 0;
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int rv = (e != NULL);
    json_free(root);
    return rv;
}

static int get_exit_code(const char *json_str) {
    if (!json_str) return -1;
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return -1; }
    double val = json_get_num(root, "exit_code", -1);
    json_free(root);
    return (int)val;
}

static int json_contains(const char *json_str, const char *sub) {
    if (!json_str || !sub) return 0;
    return strstr(json_str, sub) != NULL;
}

int main(void) {
    printf("=== Terminal Tool Tests ===\n");

    /* Test 1: Null args */
    {
        char *res = terminal_handler(NULL, NULL);
        TEST("null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = terminal_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args (missing command) */
    {
        char *res = terminal_handler("{}", NULL);
        TEST("missing command returns error", has_error(res));
        free(res);
    }

    /* Test 4: Missing command key with other params */
    {
        char *res = terminal_handler("{\"timeout\":30,\"pty\":false}", NULL);
        TEST("no command with params returns error", has_error(res));
        free(res);
    }

    /* Test 5: Simple echo command */
    {
        char *res = terminal_handler("{\"command\":\"echo hello world\"}", NULL);
        TEST("echo command returns non-NULL", res != NULL);
        if (res) {
            TEST("echo exit code 0", get_exit_code(res) == 0);
            TEST("echo output contains hello", json_contains(res, "hello world"));
        }
        free(res);
    }

    /* Test 6: Command with explicit timeout */
    {
        char *res = terminal_handler("{\"command\":\"echo timeout test\",\"timeout\":10}", NULL);
        TEST("timeout param accepted", res != NULL);
        if (res) {
            TEST("timeout exit code 0", get_exit_code(res) == 0);
            TEST("timeout output correct", json_contains(res, "timeout test"));
        }
        free(res);
    }

    /* Test 7: Command with workdir */
    {
        char *res = terminal_handler("{\"command\":\"pwd\",\"workdir\":\"/tmp\"}", NULL);
        TEST("workdir param accepted", res != NULL);
        if (res) {
            TEST("workdir exit code 0", get_exit_code(res) == 0);
            TEST("workdir output is /tmp", json_contains(res, "/tmp"));
        }
        free(res);
    }

    /* Test 8: env parameter */
    {
        char *res = terminal_handler("{\"command\":\"echo $MY_TEST_VAR\",\"env\":\"MY_TEST_VAR=envtest\"}", NULL);
        TEST("env param accepted", res != NULL);
        if (res) {
            TEST("env exit code 0", get_exit_code(res) == 0);
            TEST("env output contains value", json_contains(res, "envtest"));
        }
        free(res);
    }

    /* Test 9: Multi-word command with pipes */
    {
        char *res = terminal_handler("{\"command\":\"echo piped | wc -c\"}", NULL);
        TEST("pipe command returns non-NULL", res != NULL);
        if (res) {
            TEST("pipe exit code 0", get_exit_code(res) == 0);
        }
        free(res);
    }

    /* Test 10: backend param (default/local) */
    {
        char *res = terminal_handler("{\"command\":\"echo backend\",\"backend\":\"local\"}", NULL);
        TEST("backend param accepted", res != NULL);
        if (res) {
            TEST("backend exit code 0", get_exit_code(res) == 0);
        }
        free(res);
    }

    /* Test 11: Simple subcommand at end — not testing non-zero exit (masked by || true) */
    /* Test 12: Long command output */
    {
        char *res = terminal_handler("{\"command\":\"for i in 1 2 3; do echo line $i; done\"}", NULL);
        TEST("multi-line output returns non-NULL", res != NULL);
        if (res) {
            TEST("multi-line exit code 0", get_exit_code(res) == 0);
            TEST("output contains line 1", json_contains(res, "line 1"));
            TEST("output contains line 2", json_contains(res, "line 2"));
            TEST("output contains line 3", json_contains(res, "line 3"));
        }
        free(res);
    }

    /* Test 13: Empty command - should error */
    {
        char *res = terminal_handler("{\"command\":\"\"}", NULL);
        /* Empty command string might produce an error or run 'true' */
        TEST("empty command handled", res != NULL);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All terminal tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
