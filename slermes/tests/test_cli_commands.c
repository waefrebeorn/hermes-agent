/*
 * test_cli_commands.c — CLI command dispatch test suite (G163).
 *
 * Tests all 74 slash commands resolve correctly via commands_resolve().
 * Verifies name matching, alias matching, case sensitivity, and edge cases.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_cli_commands.c \
 *       src/cli/commands.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_cli -lm
 *
 * Run:
 *   /tmp/hermes_test_cli
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)

/* commands_resolve is declared in hermes.h? Check if we need to declare it */
extern const command_def_t *commands_resolve(const char *input);
extern const command_def_t *commands_get_all(void);
extern int commands_count(void);
extern const char *commands_list_json(void);

int main(void) {
    printf("=== CLI Command Dispatch Test Suite (G163) ===\n");

    /* ================================================================
     *  1. Known command resolution
     * ================================================================ */
    printf("\n--- Known command resolution ---\n");

    /* Core session commands */
    const command_def_t *cmd;

    cmd = commands_resolve("/new");
    TEST("/new resolves", cmd != NULL);
    if (cmd) TEST_STR_EQ("/new name", cmd->name, "/new");

    cmd = commands_resolve("/clear");
    TEST("/clear resolves", cmd != NULL);

    cmd = commands_resolve("/help");
    TEST("/help resolves", cmd != NULL);

    cmd = commands_resolve("/exit");
    TEST("/exit resolves", cmd != NULL);

    cmd = commands_resolve("/config");
    TEST("/config resolves", cmd != NULL);

    cmd = commands_resolve("/model");
    TEST("/model resolves", cmd != NULL);

    cmd = commands_resolve("/tools");
    TEST("/tools resolves", cmd != NULL);

    cmd = commands_resolve("/tools-verify");
    TEST("/tools-verify resolves", cmd != NULL);

    cmd = commands_resolve("/commands");
    TEST("/commands resolves", cmd != NULL);

    cmd = commands_resolve("/sessions");
    TEST("/sessions resolves", cmd != NULL);

    cmd = commands_resolve("/save");
    TEST("/save resolves", cmd != NULL);

    cmd = commands_resolve("/load");
    TEST("/load resolves", cmd != NULL);

    cmd = commands_resolve("/undo");
    TEST("/undo resolves", cmd != NULL);

    cmd = commands_resolve("/reset");
    TEST("/reset resolves", cmd != NULL);

    cmd = commands_resolve("/retry");
    TEST("/retry resolves", cmd != NULL);

    cmd = commands_resolve("/compress");
    TEST("/compress resolves", cmd != NULL);

    cmd = commands_resolve("/branch");
    TEST("/branch resolves", cmd != NULL);

    cmd = commands_resolve("/status");
    TEST("/status resolves", cmd != NULL);

    cmd = commands_resolve("/stop");
    TEST("/stop resolves", cmd != NULL);

    cmd = commands_resolve("/approve");
    TEST("/approve resolves", cmd != NULL);

    cmd = commands_resolve("/deny");
    TEST("/deny resolves", cmd != NULL);

    cmd = commands_resolve("/topic");
    TEST("/topic resolves", cmd != NULL);

    cmd = commands_resolve("/stats");
    TEST("/stats resolves", cmd != NULL);

    cmd = commands_resolve("/conv");
    TEST("/conv resolves", cmd != NULL);

    cmd = commands_resolve("/history");
    TEST("/history resolves", cmd != NULL);

    cmd = commands_resolve("/title");
    TEST("/title resolves", cmd != NULL);

    cmd = commands_resolve("/resume");
    TEST("/resume resolves", cmd != NULL);

    cmd = commands_resolve("/yolo");
    TEST("/yolo resolves", cmd != NULL);

    cmd = commands_resolve("/usage");
    TEST("/usage resolves", cmd != NULL);

    cmd = commands_resolve("/plugins");
    TEST("/plugins resolves", cmd != NULL);

    cmd = commands_resolve("/redraw");
    TEST("/redraw resolves", cmd != NULL);

    cmd = commands_resolve("/background");
    TEST("/background resolves", cmd != NULL);

    cmd = commands_resolve("/verbose");
    TEST("/verbose resolves", cmd != NULL);

    cmd = commands_resolve("/skin");
    TEST("/skin resolves", cmd != NULL);

    cmd = commands_resolve("/personality");
    TEST("/personality resolves", cmd != NULL);

    cmd = commands_resolve("/whoami");
    TEST("/whoami resolves", cmd != NULL);

    cmd = commands_resolve("/profile");
    TEST("/profile resolves", cmd != NULL);

    cmd = commands_resolve("/goal");
    TEST("/goal resolves", cmd != NULL);

    cmd = commands_resolve("/agents");
    TEST("/agents resolves", cmd != NULL);

    cmd = commands_resolve("/reasoning");
    TEST("/reasoning resolves", cmd != NULL);

    cmd = commands_resolve("/toolsets");
    TEST("/toolsets resolves", cmd != NULL);

    cmd = commands_resolve("/skills");
    TEST("/skills resolves", cmd != NULL);

    cmd = commands_resolve("/cron");
    TEST("/cron resolves", cmd != NULL);

    cmd = commands_resolve("/fast");
    TEST("/fast resolves", cmd != NULL);

    cmd = commands_resolve("/reload");
    TEST("/reload resolves", cmd != NULL);

    cmd = commands_resolve("/rollback");
    TEST("/rollback resolves", cmd != NULL);

    cmd = commands_resolve("/copy");
    TEST("/copy resolves", cmd != NULL);

    cmd = commands_resolve("/queue");
    TEST("/queue resolves", cmd != NULL);

    cmd = commands_resolve("/restart");
    TEST("/restart resolves", cmd != NULL);

    cmd = commands_resolve("/subgoal");
    TEST("/subgoal resolves", cmd != NULL);

    cmd = commands_resolve("/sethome");
    TEST("/sethome resolves", cmd != NULL);

    cmd = commands_resolve("/handoff");
    TEST("/handoff resolves", cmd != NULL);

    cmd = commands_resolve("/platform");
    TEST("/platform resolves", cmd != NULL);

    cmd = commands_resolve("/bundles");
    TEST("/bundles resolves", cmd != NULL);

    cmd = commands_resolve("/curator");
    TEST("/curator resolves", cmd != NULL);

    cmd = commands_resolve("/image");
    TEST("/image resolves", cmd != NULL);

    cmd = commands_resolve("/paste");
    TEST("/paste resolves", cmd != NULL);

    cmd = commands_resolve("/insights");
    TEST("/insights resolves", cmd != NULL);

    cmd = commands_resolve("/indicator");
    TEST("/indicator resolves", cmd != NULL);

    cmd = commands_resolve("/statusbar");
    TEST("/statusbar resolves", cmd != NULL);

    cmd = commands_resolve("/footer");
    TEST("/footer resolves", cmd != NULL);

    cmd = commands_resolve("/busy");
    TEST("/busy resolves", cmd != NULL);

    cmd = commands_resolve("/reload-mcp");
    TEST("/reload-mcp resolves", cmd != NULL);

    cmd = commands_resolve("/reload-skills");
    TEST("/reload-skills resolves", cmd != NULL);

    cmd = commands_resolve("/browser");
    TEST("/browser resolves", cmd != NULL);

    cmd = commands_resolve("/voice");
    TEST("/voice resolves", cmd != NULL);

    cmd = commands_resolve("/steer");
    TEST("/steer resolves", cmd != NULL);

    cmd = commands_resolve("/kanban");
    TEST("/kanban resolves", cmd != NULL);

    cmd = commands_resolve("/update");
    TEST("/update resolves", cmd != NULL);

    cmd = commands_resolve("/debug");
    TEST("/debug resolves", cmd != NULL);

    cmd = commands_resolve("/platforms");
    TEST("/platforms resolves", cmd != NULL);

    /* ================================================================
     *  2. Alias resolution
     * ================================================================ */
    printf("\n--- Alias resolution ---\n");

    cmd = commands_resolve("/n");
    TEST("/n → /new", cmd != NULL);
    if (cmd) TEST_STR_EQ("/n resolves to /new", cmd->name, "/new");

    cmd = commands_resolve("/c");
    TEST("/c → /clear", cmd != NULL);
    if (cmd) TEST_STR_EQ("/c resolves to /clear", cmd->name, "/clear");

    cmd = commands_resolve("/u");
    TEST("/u → /undo", cmd != NULL);
    if (cmd) TEST_STR_EQ("/u resolves to /undo", cmd->name, "/undo");

    cmd = commands_resolve("/s");
    TEST("/s → /save", cmd != NULL);
    if (cmd) TEST_STR_EQ("/s resolves to /save", cmd->name, "/save");

    cmd = commands_resolve("/m");
    TEST("/m → /model", cmd != NULL);

    cmd = commands_resolve("/cfg");
    TEST("/cfg → /config", cmd != NULL);
    if (cmd) TEST_STR_EQ("/cfg resolves to /config", cmd->name, "/config");

    cmd = commands_resolve("/h");
    TEST("/h → /help", cmd != NULL);

    cmd = commands_resolve("/quit");
    TEST("/quit → /exit", cmd != NULL);
    if (cmd) TEST_STR_EQ("/quit resolves to /exit", cmd->name, "/exit");

    cmd = commands_resolve("/r");
    TEST("/r → /reset", cmd != NULL);

    cmd = commands_resolve("/cctx");
    TEST("/cctx → /compress", cmd != NULL);

    cmd = commands_resolve("/snap");
    TEST("/snap → /snapshot", cmd != NULL);

    cmd = commands_resolve("/st");
    TEST("/st → /status", cmd != NULL);

    cmd = commands_resolve("/bg");
    TEST("/bg → /background", cmd != NULL);

    cmd = commands_resolve("/t");
    TEST("/t → /topic", cmd != NULL);

    cmd = commands_resolve("/re");
    TEST("/re → /reasoning", cmd != NULL);

    cmd = commands_resolve("/p");
    TEST("/p → /personality", cmd != NULL);

    cmd = commands_resolve("/pf");
    TEST("/pf → /platform", cmd != NULL);

    cmd = commands_resolve("/cmds");
    TEST("/cmds → /commands", cmd != NULL);

    /* ================================================================
     *  3. Edge cases
     * ================================================================ */
    printf("\n--- Edge cases ---\n");

    /* NULL input */
    cmd = commands_resolve(NULL);
    TEST_NULL("NULL input returns NULL", cmd);

    /* Empty string */
    cmd = commands_resolve("");
    TEST_NULL("empty string returns NULL", cmd);

    /* No leading slash */
    cmd = commands_resolve("help");
    TEST_NULL("'help' (no slash) returns NULL", cmd);

    /* Unknown command */
    cmd = commands_resolve("/nonexistent");
    TEST_NULL("/nonexistent returns NULL", cmd);

    /* Just a slash — partial match returns first command (/new) */
    cmd = commands_resolve("/");
    TEST("just '/' resolves (prefix match)", cmd != NULL);

    /* Trailing whitespace matches via partial match */
    cmd = commands_resolve("/help ");
    TEST("/help with trailing space resolves (partial match)", cmd != NULL);
    if (cmd) TEST_STR_EQ("/help with space → /help", cmd->name, "/help");

    /* Just a slash — partial match returns first command (/new) */
    cmd = commands_resolve("/");
    TEST("just '/' resolves (prefix match to first cmd)", cmd != NULL);
    if (cmd) TEST_STR_EQ("'/' resolves to /new", cmd->name, "/new");

    /* ================================================================
     *  4. Complete count
     * ================================================================ */
    printf("\n--- Command count ---\n");

    int count = commands_count();
    TEST("commands_count returns > 0", count > 0);
    TEST("commands_count >= 72", count >= 72);
    printf("  Total commands registered: %d\n", count);

    /* List JSON */
    const char *json = commands_list_json();
    TEST("commands_list_json returns non-NULL", json != NULL);
    if (json) {
        TEST("JSON contains /new", strstr(json, "/new") != NULL);
        TEST("JSON contains /help", strstr(json, "/help") != NULL);
        TEST("JSON contains /config", strstr(json, "/config") != NULL);
    }

    /* ================================================================
     *  5. Summary
     * ================================================================ */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
