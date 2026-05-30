/*
 * test_tool_registry.c — Tool registry API test suite (G164).
 *
 * Tests the registry API directly: register, find, dispatch, count,
 * timeout, and edge cases. Does NOT call tools_init_all() to avoid
 * external symbol dependencies.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_tool_registry.c \
 *       src/tools/registry.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_tools -lm
 *
 * Run:
 *   /tmp/hermes_test_tools
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* Test handlers */
static char *dummy_handler(const char *args, const char *task_id) {
    (void)args; (void)task_id;
    char *r = malloc(64);
    if (r) snprintf(r, 64, "{\"result\":\"ok\"}");
    return r;
}

static char *echo_handler(const char *args, const char *task_id) {
    (void)task_id;
    char *r = malloc(strlen(args) + 32);
    if (r) snprintf(r, strlen(args) + 32, "{\"echo\":%s}", args);
    return r;
}

int main(void) {
    printf("=== Tool Registry API Test Suite (G164) ===\n");

    /* ================================================================
     *  1. Registration
     * ================================================================ */
    printf("\n--- Registration ---\n");

    size_t count = registry_count();
    TEST_INT_EQ("registry starts empty", count, 0);

    /* Register valid tools */
    TEST("register terminal", registry_register("terminal",
        "Execute shell commands", "{\"type\":\"object\",\"properties\":{}}",
        dummy_handler));
    TEST("register read_file", registry_register("read_file",
        "Read file contents", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}",
        dummy_handler));
    TEST("register write_file", registry_register("write_file",
        "Write content to file", "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}",
        echo_handler));

    count = registry_count();
    TEST_INT_EQ("count is 3", count, 3);

    /* Error: NULL name */
    TEST("NULL name rejected", !registry_register(NULL, "desc", "{}", dummy_handler));

    /* Error: NULL handler */
    TEST("NULL handler rejected", !registry_register("bad", "desc", "{}", NULL));

    /* Error: duplicate name */
    TEST("duplicate name rejected", !registry_register("terminal", "dup", "{}", dummy_handler));
    TEST_INT_EQ("count unchanged", registry_count(), 3);

    /* ================================================================
     *  2. Find
     * ================================================================ */
    printf("\n--- Find ---\n");

    tool_t *t = registry_find("terminal");
    TEST_NOT_NULL("find terminal", t);
    if (t) {
        TEST_STR_EQ("terminal name", t->name, "terminal");
        TEST("terminal description non-empty", t->description[0] != '\0');
        TEST_NOT_NULL("terminal handler", t->handler);
        TEST("terminal available", t->available);
        TEST("terminal schema non-empty", t->schema_json[0] != '\0');
    }

    t = registry_find("read_file");
    TEST_NOT_NULL("find read_file", t);
    if (t) TEST_STR_EQ("read_file name", t->name, "read_file");

    t = registry_find("_nonexistent");
    TEST_NULL("find nonexistent", t);

    t = registry_find(NULL);
    TEST_NULL("find NULL", t);

    /* ================================================================
     *  3. Get by index
     * ================================================================ */
    printf("\n--- Index access ---\n");

    const char *name0 = registry_get_name(0);
    TEST_STR_EQ("index 0 is terminal", name0, "terminal");

    const char *name1 = registry_get_name(1);
    TEST_STR_EQ("index 1 is read_file", name1, "read_file");

    const char *name2 = registry_get_name(2);
    TEST_STR_EQ("index 2 is write_file", name2, "write_file");

    const char *name3 = registry_get_name(3);
    TEST_NULL("index 3 is NULL (out of range)", name3);

    const char *name_neg = registry_get_name(999);
    TEST_NULL("index 999 is NULL", name_neg);

    /* ================================================================
     *  4. Dispatch
     * ================================================================ */
    printf("\n--- Dispatch ---\n");

    char *result;

    /* Dispatch known tool */
    result = registry_dispatch("terminal", "{\"cmd\":\"ls\"}", "test123");
    TEST_NOT_NULL("dispatch terminal", result);
    if (result) TEST("result contains ok", strstr(result, "ok") != NULL);
    free(result);

    /* Dispatch echo tool */
    result = registry_dispatch("write_file", "{\"path\":\"/tmp/test\"}", "test456");
    TEST_NOT_NULL("dispatch write_file", result);
    if (result) TEST("result contains echo", strstr(result, "echo") != NULL);
    free(result);

    /* Dispatch NULL name */
    result = registry_dispatch(NULL, "{}", "test");
    TEST_NOT_NULL("dispatch NULL (returns error)", result);
    if (result) TEST("error for NULL", strstr(result, "error") != NULL || strstr(result, "Error") != NULL);
    free(result);

    /* Dispatch unknown tool */
    result = registry_dispatch("_nonexistent", "{}", "test");
    TEST_NOT_NULL("dispatch unknown", result);
    if (result) TEST("error for unknown", strstr(result, "error") != NULL || strstr(result, "Error") != NULL);
    free(result);

    /* Dispatch with NULL args */
    result = registry_dispatch("terminal", NULL, "test");
    TEST_NOT_NULL("dispatch NULL args", result);
    free(result);

    /* Dispatch with NULL task_id */
    result = registry_dispatch("terminal", "{}", NULL);
    TEST_NOT_NULL("dispatch NULL task_id", result);
    free(result);

    /* ================================================================
     *  5. Schema validation
     * ================================================================ */
    printf("\n--- Schema ---\n");

    t = registry_find("read_file");
    if (t) {
        TEST("read_file schema has type=object",
            strstr(t->schema_json, "\"type\":\"object\"") != NULL);
        TEST("read_file schema has properties.path",
            strstr(t->schema_json, "properties") != NULL &&
            strstr(t->schema_json, "path") != NULL);
    }

    /* ================================================================
     *  6. Timeout API
     * ================================================================ */
    printf("\n--- Timeout ---\n");

    /* Default timeout is 0 (inherit from global default) */
    TEST_INT_EQ("default timeout", registry_get_timeout("terminal"), 0);

    /* Set and get */
    registry_set_timeout("terminal", 300);
    TEST_INT_EQ("set timeout 300", registry_get_timeout("terminal"), 300);

    /* Set different value */
    registry_set_timeout("read_file", 60);
    TEST_INT_EQ("read_file timeout 60", registry_get_timeout("read_file"), 60);
    TEST_INT_EQ("terminal unchanged", registry_get_timeout("terminal"), 300);

    /* Reset to 0 */
    registry_set_timeout("terminal", 0);
    TEST_INT_EQ("terminal reset", registry_get_timeout("terminal"), 0);

    /* Unknown tool */
    TEST_INT_EQ("unknown timeout", registry_get_timeout("_no_such_tool"), 0);

    /* ================================================================
     *  7. Registry integrity after edge cases
     * ================================================================ */
    printf("\n--- Registry integrity ---\n");

    /* Count should still be 3 after all operations */
    TEST_INT_EQ("registry count still 3", registry_count(), 3);

    /* Names should still be correct */
    TEST_STR_EQ("terminal still at 0", registry_get_name(0), "terminal");
    TEST_STR_EQ("read_file still at 1", registry_get_name(1), "read_file");
    TEST_STR_EQ("write_file still at 2", registry_get_name(2), "write_file");

    /* ================================================================
     *  Summary
     * ================================================================ */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
