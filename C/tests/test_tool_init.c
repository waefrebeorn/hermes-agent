/*
 * test_tool_init.c — Tests for tool initialization (tool_init.c).
 *
 * Verifies that tools_init_all() runs without crashing and that
 * tools are registered after initialization, matching basic structural
 * behavior of the Python Hermes tool registration system.
 *
 * Build:
 *   gcc -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_tool_init.c \
 *       src/tools/tool_init.c \
 *       src/tools/registry.c \
 *       src/sandbox_escape.c \
 *       lib/libjson/json.c \
 *       -o /tmp/hermes_test_tool_init -lm
 *
 * Run:
 *   /tmp/hermes_test_tool_init
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Stub sandbox init — sandbox_init is in file.c (too heavy to link),
 * sandbox_escape_init is in sandbox_escape.c. */
void sandbox_init(void) {}
void sandbox_escape_init(void) {}

/* Extern declarations needed */
extern void tools_init_all(void);
extern size_t register_count(void);
extern const char *registry_get_name(size_t i);

int main(void) {
    printf("=== Tool Init Tests ===\n\n");

    /* Test 1: Init should not crash */
    printf("--- Basic Init ---\n");
    tools_init_all();
    TEST("tools_init_all runs without crash", 1);

    /* Test 2: Tools registered after init */
    size_t count = registry_count();
    TEST("tools registered after init (> 0)", count > 0);

    /* Test 3: Verify core tools exist */
    printf("--- Core Tool Registration (count=%zu) ---\n", count);
    int found_terminal = 0, found_file = 0, found_patch = 0;
    int found_web = 0, found_skills = 0, found_exec = 0;
    for (size_t i = 0; i < count; i++) {
        const char *name = registry_get_name(i);
        if (!name) continue;
        if (strcmp(name, "terminal") == 0) found_terminal = 1;
        if (strcmp(name, "read_file") == 0) found_file = 1;
        if (strcmp(name, "patch") == 0) found_patch = 1;
        if (strcmp(name, "web_search") == 0) found_web = 1;
        if (strcmp(name, "skills_list") == 0) found_skills = 1;
        if (strcmp(name, "execute_code") == 0) found_exec = 1;
    }
    TEST("terminal tool registered", found_terminal);
    TEST("read_file tool registered", found_file);
    TEST("patch tool registered", found_patch);
    TEST("web_search tool registered", found_web);
    TEST("skills_list tool registered", found_skills);
    TEST("execute_code tool registered", found_exec);

    /* Summation */
    printf("\n---\n");
    printf("  Results: %d passed, %d failed\n", passed, failed);
    if (failed > 0) {
        printf("  Some tests FAILED.\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
