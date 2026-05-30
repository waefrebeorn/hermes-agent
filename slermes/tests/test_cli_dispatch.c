/*
 * test_cli_dispatch.c — T02: CLI command dispatch + handler tests.
 *
 * Tests commands_dispatch(), commands_get_all(), and basic handler
 * behavior for commands that don't require full agent state.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_cli_dispatch.c \
 *       src/cli/commands.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_cli_dispatch -lm
 *
 * Run:
 *   /tmp/hermes_test_cli_dispatch
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

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))

/* ================================================================
 *  1. commands_get_all
 * ================================================================ */
static void test_commands_get_all(void) {
    printf("\n--- commands_get_all ---\n");

    const command_def_t *all = commands_get_all();
    TEST_NOT_NULL("get_all returns non-NULL", all);
    if (all) {
        TEST("first command has name", all[0].name != NULL);
        TEST("first command has desc", all[0].description != NULL);
        TEST_STR_EQ("first command is /new", all[0].name, "/new");
        TEST("last entry is sentinel (name==NULL)", all[commands_count()].name == NULL);
    }
}

/* ================================================================
 *  2. commands_dispatch
 * ================================================================ */
static void test_commands_dispatch(void) {
    printf("\n--- commands_dispatch ---\n");

    agent_state_t dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.interrupted = false;

    /* Dispatch known command */
    TEST("dispatch /help returns true", commands_dispatch("/help", &dummy));

    /* Dispatch with args */
    TEST("dispatch /help new", commands_dispatch("/help /new", &dummy));

    /* Dispatch unknown command (should return false) */
    TEST("dispatch /nonexistent returns false",
         !commands_dispatch("/nonexistent", &dummy));

    /* Dispatch NULL */
    TEST("dispatch NULL returns false",
         !commands_dispatch(NULL, &dummy));

    /* Dispatch empty */
    TEST("dispatch empty returns false",
         !commands_dispatch("", &dummy));

    /* Dispatch without leading slash */
    TEST("dispatch 'help' (no slash) returns false",
         !commands_dispatch("help", &dummy));
}

/* ================================================================
 *  3. /exit handler
 * ================================================================ */
static void test_cmd_exit(void) {
    printf("\n--- /exit handler ---\n");

    agent_state_t state;
    memset(&state, 0, sizeof(state));
    state.interrupted = false;

    /* /exit should set interrupted */
    TEST("/exit returns true", commands_dispatch("/exit", &state));
    TEST("state.interrupted set to true", state.interrupted == true);

    /* Reset and test with alias */
    state.interrupted = false;
    TEST("/quit alias sets interrupted", commands_dispatch("/quit", &state));
    TEST("interrupted after /quit", state.interrupted == true);
}

/* ================================================================
 *  4. /help handler output
 * ================================================================ */
static void test_help_output(void) {
    printf("\n--- /help output ---\n");

    agent_state_t state;
    memset(&state, 0, sizeof(state));

    /* General help — should print without crash */
    TEST("general help no crash", commands_dispatch("/help", &state));

    /* Specific help — should print without crash */
    TEST("help /new no crash", commands_dispatch("/help /new", &state));
    TEST("help /config no crash", commands_dispatch("/help /config", &state));

    /* Help for unknown command */
    TEST("help /unknown no crash",
         commands_dispatch("/help /totally_not_a_command", &state));
}

/* ================================================================
 *  5. Command count invariant
 * ================================================================ */
static void test_count_invariant(void) {
    printf("\n--- Count invariants ---\n");

    int count = commands_count();

    /* Each entry should have a non-empty name */
    const command_def_t *all = commands_get_all();
    int named = 0;
    for (int i = 0; all && all[i].name; i++) {
        TEST_NOT_NULL("command name exists", all[i].name);
        if (all[i].name) named++;
    }
    TEST_INT_EQ("named entries == count", named, count);

    /* Each entry should have a non-NULL handler */
    int handlers = 0;
    for (int i = 0; all && all[i].name; i++) {
        if (all[i].handler) handlers++;
    }
    TEST_INT_EQ("all commands have handlers", handlers, count);

    /* Sentinel check */
    TEST_NULL("sentinel name is NULL", all[count].name);
    TEST_NULL("sentinel alias is NULL", all[count].alias);
    TEST_NULL("sentinel description is NULL", all[count].description);
    TEST_NULL("sentinel handler is NULL", all[count].handler);
}

/* ================================================================
 *  6. commands_list_json validity
 * ================================================================ */
static void test_list_json(void) {
    printf("\n--- commands_list_json ---\n");

    const char *json = commands_list_json();
    TEST_NOT_NULL("list_json returns non-NULL", json);

    if (json) {
        /* Starts with [ */
        TEST("JSON starts with [", json[0] == '[');

        /* Contains known commands */
        TEST("JSON contains /new", strstr(json, "/new") != NULL);
        TEST("JSON contains /help", strstr(json, "/help") != NULL);
        TEST("JSON contains /exit", strstr(json, "/exit") != NULL);
        TEST("JSON contains /config", strstr(json, "/config") != NULL);
        TEST("JSON contains /session-search", strstr(json, "/session-search") != NULL);

        /* Ends with ] */
        size_t len = strlen(json);
        TEST("JSON ends with ]", json[len - 1] == ']');

        /* Well-formed (rough check) */
        int quotes = 0;
        for (size_t i = 0; json[i]; i++)
            if (json[i] == '"') quotes++;
        TEST("even number of quotes", quotes % 2 == 0);
    }

    /* Free the returned string */
    free((void *)json);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== CLI Command Dispatch Test Suite (T02) ===\n");

    test_commands_get_all();
    test_commands_dispatch();
    test_cmd_exit();
    test_help_output();
    test_count_invariant();
    test_list_json();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
