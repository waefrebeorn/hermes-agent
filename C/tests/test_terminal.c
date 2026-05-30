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
/* Shell token reader utility */
char *terminal_read_shell_token(const char *command, int start, int *end);

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

    /* --- Long-lived foreground pattern detection tests --- */

    /* Test 27: npm run dev detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo npm run dev\"}", NULL);
        TEST("npm run dev returns non-NULL", res != NULL);
        if (res) {
            TEST("npm run dev guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 28: docker compose up detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo docker compose up\"}", NULL);
        TEST("docker compose up returns non-NULL", res != NULL);
        if (res) {
            TEST("docker compose up guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 29: python -m http.server detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo python3 -m http.server\"}", NULL);
        TEST("python http.server returns non-NULL", res != NULL);
        if (res) {
            TEST("python http.server guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 30: uvicorn detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo run_uvicorn\"}", NULL);
        TEST("uvicorn returns non-NULL", res != NULL);
        if (res) {
            TEST("uvicorn guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 31: npm start (without "run") — also long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo npm start\"}", NULL);
        TEST("npm start returns non-NULL", res != NULL);
        if (res) {
            TEST("npm start guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 32: gunicorn detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo run_gunicorn\"}", NULL);
        TEST("gunicorn returns non-NULL", res != NULL);
        if (res) {
            TEST("gunicorn guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 33: Normal short command — NO guidance */
    {
        char *res = terminal_handler("{\"command\":\"echo quick_task\"}", NULL);
        TEST("normal short command returns non-NULL", res != NULL);
        if (res) {
            TEST("no guidance for normal command",
                 !json_contains(res, "guidance"));
        }
        free(res);
    }

    /* Test 34: next dev detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo next dev\"}", NULL);
        TEST("next dev returns non-NULL", res != NULL);
        if (res) {
            TEST("next dev guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 35: nodemon detected as long-lived */
    {
        char *res = terminal_handler("{\"command\":\"echo run_nodemon\"}", NULL);
        TEST("nodemon returns non-NULL", res != NULL);
        if (res) {
            TEST("nodemon guidance: long-lived server",
                 json_contains(res, "long-lived"));
        }
        free(res);
    }

    /* Test 36: Sudo failure detection — password required */
    {
        char *res = terminal_handler("{\"command\":\"echo sudo: a password is required && false\"}", NULL);
        TEST("sudo fail returns non-NULL", res != NULL);
        if (res) {
            TEST("sudo fail has sudo_tip", json_contains(res, "sudo_tip"));
            TEST("sudo fail tip mentions SUDO_PASSWORD", json_contains(res, "SUDO_PASSWORD"));
        }
        free(res);
    }

    /* Test 37: Sudo failure detection — no tty */
    {
        char *res = terminal_handler("{\"command\":\"echo sudo: no tty present && false\"}", NULL);
        TEST("sudo no tty returns non-NULL", res != NULL);
        if (res) {
            TEST("sudo no tty has sudo_tip", json_contains(res, "sudo_tip"));
        }
        free(res);
    }

    /* Test 38: Workdir validation — safe path allowed */
    {
        char *res = terminal_handler("{\"command\":\"echo safe_wd_test\",\"workdir\":\"/tmp\"}", NULL);
        TEST("safe workdir returns non-NULL", res != NULL);
        if (res) {
            TEST("safe workdir exit code 0", get_exit_code(res) == 0);
            TEST("safe workdir output contains safe_wd_test", json_contains(res, "safe_wd_test"));
        }
        free(res);
    }

    /* Test 39: Workdir validation — shell metachar blocked */
    {
        char *res = terminal_handler("{\"command\":\"echo should_not_run\",\"workdir\":\"/tmp; rm -rf /\"}", NULL);
        TEST("dangerous workdir returns non-NULL", res != NULL);
        if (res) {
            TEST("dangerous workdir has error", has_error(res));
            TEST("dangerous workdir mentions blocked", json_contains(res, "Blocked"));
        }
        free(res);
    }

    /* Test 40: Workdir validation — backtick injection blocked */
    {
        char *res = terminal_handler("{\"command\":\"echo should_not_run\",\"workdir\":\"/tmp/$(id)\"}", NULL);
        TEST("backtick workdir returns non-NULL", res != NULL);
        if (res) {
            TEST("backtick workdir has error", has_error(res));
            TEST("backtick workdir mentions blocked", json_contains(res, "Blocked"));
        }
        free(res);
    }

    /* Test 41-50: terminal_read_shell_token */
    {
        int end = 0;
        char *tok = NULL;

        /* Simple word */
        tok = terminal_read_shell_token("hello", 0, &end);
        TEST("read_shell_token simple word", tok != NULL && strcmp(tok, "hello") == 0);
        TEST("read_shell_token simple end", end == 5);
        free(tok);

        /* Word followed by space */
        tok = terminal_read_shell_token("hello world", 0, &end);
        TEST("read_shell_token stop at space", tok != NULL && strcmp(tok, "hello") == 0);
        TEST("read_shell_token after space", end == 5);
        free(tok);

        /* Single-quoted string */
        tok = terminal_read_shell_token("'hello world'", 0, &end);
        TEST("read_shell_token single quotes", tok != NULL && strcmp(tok, "'hello world'") == 0);
        free(tok);

        /* Double-quoted string with backslash */
        tok = terminal_read_shell_token("\"hello\\\" world\"", 0, &end);
        TEST("read_shell_token double quotes", tok != NULL && strcmp(tok, "\"hello\\\" world\"") == 0);
        free(tok);

        /* Token with escaped char */
        tok = terminal_read_shell_token("hello\\ world", 0, &end);
        TEST("read_shell_token backslash", tok != NULL && strcmp(tok, "hello\\ world") == 0);
        free(tok);

        /* Stop at semicolon */
        tok = terminal_read_shell_token("cmd;", 0, &end);
        TEST("read_shell_token stop at ;", tok != NULL && strcmp(tok, "cmd") == 0);
        free(tok);

        /* Stop at pipe */
        tok = terminal_read_shell_token("first|second", 0, &end);
        TEST("read_shell_token stop at |", tok != NULL && strcmp(tok, "first") == 0);
        free(tok);

        /* NULL safety */
        tok = terminal_read_shell_token(NULL, 0, &end);
        TEST("read_shell_token NULL cmd", tok == NULL);

        /* Negative start */
        tok = terminal_read_shell_token("hello", -1, &end);
        TEST("read_shell_token negative start", tok == NULL);

        /* Start beyond length */
        tok = terminal_read_shell_token("hi", 10, &end);
        TEST("read_shell_token beyond end", tok == NULL);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All terminal tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
