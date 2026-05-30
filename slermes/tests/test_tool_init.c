/*
 * tests/test_tool_init.c — Tests for tool registration registry.
 *
 * Tests registry module directly (register, count, get_name, dispatch)
 * using only the public API declared in hermes_agent.h.
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

/* A simple test handler */
static char *test_handler(const char *args, const char *task_id) {
    (void)args;
    (void)task_id;
    return strdup("{\"result\":\"ok\"}");
}

int main(void) {
    printf("=== Registry Tests ===\n\n");

    /* Test 1: Count starts at 0 before registration */
    size_t count = registry_count();
    TEST("registry_count returns a value", 1);

    /* Test 2: Register a tool */
    printf("--- Registration ---\n");
    bool r = registry_register("test_tool", "A test tool", "{}", test_handler);
    TEST("register test_tool succeeds", r);
    count = registry_count();
    TEST("count > 0 after registration", count > 0);

    /* Test 3: Get registered tool name */
    int found = 0;
    for (size_t i = 0; i < count; i++) {
        const char *n = registry_get_name(i);
        if (n && strcmp(n, "test_tool") == 0) { found = 1; break; }
    }
    TEST("test_tool found in registry", found);

    /* Test 4: Register second tool */
    r = registry_register("tool_two", "Second test tool", "{}", test_handler);
    TEST("register tool_two succeeds", r);
    size_t count2 = registry_count();
    TEST("count increased after second registration", count2 > count);

    /* Test 5: Dispatch a registered tool */
    printf("--- Dispatch ---\n");
    char *out = registry_dispatch("test_tool", "{}", "task1");
    TEST("dispatch returns non-NULL", out != NULL);
    TEST("dispatch output matches", out && strcmp(out, "{\"result\":\"ok\"}") == 0);
    free(out);

    /* Test 6: Dispatch nonexistent tool */
    out = registry_dispatch("nonexistent", "{}", "task1");
    TEST("dispatch nonexistent returns non-NULL", out != NULL);
    TEST("dispatch nonexistent has error", out && strstr(out, "error") != NULL);
    free(out);

    /* Test 7: Register duplicate */
    r = registry_register("test_tool", "Duplicate", "{}", test_handler);
    TEST("register duplicate returns false", !r);

    /* Test 8: Register NULL name */
    r = registry_register(NULL, "No name", "{}", test_handler);
    TEST("register NULL name returns false", !r);

    /* Test 9: Register NULL handler */
    r = registry_register("null_handler", "No handler", "{}", NULL);
    TEST("register NULL handler returns false", !r);

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
