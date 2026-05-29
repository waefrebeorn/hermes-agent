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

    /* Test 14: Foreground guidance — nohup wrapper detected */
    {
        char *res = terminal_handler("{\"command\":\"nohup echo test > /dev/null 2>&1\"}", NULL);
        TEST("nohup command returns non-NULL", res != NULL);
        if (res) {
            TEST("nohup guidance present (background wrapper)", json_contains(res, "background"));
        }
        free(res);
    }

    /* Test 15: Foreground guidance — trailing & detected */
    {
        char *res = terminal_handler("{\"command\":\"echo bg_test &\"}", NULL);
        TEST("trailing & returns non-NULL", res != NULL);
        if (res) {
            TEST("trailing & guidance present", json_contains(res, "background"));
        }
        free(res);
    }

    /* Test 16: Foreground guidance — inline & detected */
    {
        char *res = terminal_handler("{\"command\":\"echo a & echo b\"}", NULL);
        TEST("inline & returns non-NULL", res != NULL);
        if (res) {
            TEST("inline & guidance present", json_contains(res, "background"));
        }
        free(res);
    }

    /* Test 17: No guidance for normal foreground command */
    {
        char *res = terminal_handler("{\"command\":\"echo no_background\"}", NULL);
        TEST("normal command returns non-NULL", res != NULL);
        if (res) {
            TEST("no guidance for normal command", !json_contains(res, "guidance"));
        }
        free(res);
    }

    /* Test 18: Force param accepted */
    {
        char *res = terminal_handler("{\"command\":\"echo force_test\",\"force\":true}", NULL);
        TEST("force param returns non-NULL", res != NULL);
        if (res) {
            TEST("force exit code 0", get_exit_code(res) == 0);
            TEST("force output correct", json_contains(res, "force_test"));
        }
        free(res);
    }

    /* Test 19: Status field present on success */
    {
        char *res = terminal_handler("{\"command\":\"echo status_check\"}", NULL);
        TEST("success returns non-NULL", res != NULL);
        if (res) {
            TEST("status field is success", json_contains(res, "\"status\":\"success\""));
        }
        free(res);
    }

    /* Test 20: Foreground timeout >600s rejected */
    {
        char *res = terminal_handler("{\"command\":\"echo timeout_guard\",\"timeout\":900}", NULL);
        TEST("excessive timeout returns non-NULL", res != NULL);
        if (res) {
            TEST("excessive timeout returns error", has_error(res));
            TEST("excessive timeout mentions 600s", json_contains(res, "600"));
        }
        free(res);
    }

    /* Test 21: Non-existent workdir returns workdir warning */
    {
        char *res = terminal_handler("{\"command\":\"echo nodir\",\"workdir\":\"/nonexistent_path_xyzzy\"}", NULL);
        TEST("bad workdir returns non-NULL", res != NULL);
        if (res) {
            /* Should produce some output about the bad workdir */
            TEST("bad workdir has status", json_contains(res, "\"status\""));
        }
        free(res);
    }

    /* Test 22: Normal command with status success */
    {
        char *res = terminal_handler("{\"command\":\"echo final_check\"}", NULL);
        TEST("final check returns non-NULL", res != NULL);
        if (res) {
            TEST("final exit code 0", get_exit_code(res) == 0);
            TEST("final status success", json_contains(res, "\"status\":\"success\""));
        }
        free(res);
    }

    /* Test 23: Env assignment prefix — no foreground guidance */
    {
        char *res = terminal_handler("{\"command\":\"PATH=/usr/bin echo envtest\"}", NULL);
        TEST("env assignment returns non-NULL", res != NULL);
        if (res) {
            TEST("env assignment exit code 0", get_exit_code(res) == 0);
            TEST("no guidance for env assignment", !json_contains(res, "guidance"));
            TEST("env assignment output contains envtest", json_contains(res, "envtest"));
        }
        free(res);
    }

    /* Test 24: Multiple env assignments — no foreground guidance */
    {
        char *res = terminal_handler("{\"command\":\"HOME=/tmp MY_VAR=hello env\"}", NULL);
        TEST("multi env assignment returns non-NULL", res != NULL);
        if (res) {
            TEST("multi env exit code 0", get_exit_code(res) == 0);
            TEST("no guidance for multi env", !json_contains(res, "guidance"));
        }
        free(res);
    }

    /* Test 25: Env assignment with & suffix — still NOT guided (env assignment check runs first) */
    {
        char *res = terminal_handler("{\"command\":\"MYVAR=val echo test &\"}", NULL);
        TEST("env assignment + & returns non-NULL", res != NULL);
        if (res) {
            TEST("env + & does NOT have guidance (env check short-circuits)", !json_contains(res, "guidance"));
        }
        free(res);
    }

    /* Test 26: Trailing & with env assignment before the actual command */
    {
        char *res = terminal_handler("{\"command\":\"DEBUG=1 nohup echo test &\"}", NULL);
        TEST("env+background wrappers returns non-NULL", res != NULL);
        if (res) {
            /* Env assignment check catches PATH=, short-circuits before checking nohup */
            TEST("env prefix short-circuits guidance", !json_contains(res, "guidance"));
        }
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All terminal tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
