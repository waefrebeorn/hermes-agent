/*
 * test_registry.c — Tool registry unit tests.
 *
 * Build:
 *   gcc -O2 -g -Wall -I include -I lib/libplugin -I lib/libjson tests/test_registry.c \
 *       src/tools/registry.c lib/libjson/json.c -o /tmp/hermes_test_registry \
 *       -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Simple handler for testing */
static char *test_handler(const char *args, const char *task_id) {
    (void)args; (void)task_id;
    return strdup("{\"result\":\"ok\"}");
}

int main(void) {
    printf("=== Registry Tests ===\n");

    /* 1. Register and lookup */
    TEST("register returns true",
         registry_register("test_tool", "A test tool", "{}", test_handler));
    TEST("register duplicate returns false",
         !registry_register("test_tool", "dup", "{}", test_handler));
    TEST("register NULL name returns false",
         !registry_register(NULL, "desc", "{}", test_handler));
    TEST("register NULL handler returns false",
         !registry_register("null_test", "desc", "{}", NULL));
    TEST("count after register is non-zero",
         registry_get_count() > 0);
    TEST("get_name returns registered name",
         strcmp(registry_get_name(0), "test_tool") == 0);

    /* 2. Dispatch */
    char *result = registry_dispatch("test_tool", "{}", NULL);
    TEST("dispatch returns result", result != NULL);
    TEST("dispatch result contains ok",
         result && strstr(result, "\"ok\"") != NULL);
    free(result);

    /* 3. Dispatch unknown */
    result = registry_dispatch("nonexistent", "{}", NULL);
    TEST("dispatch unknown returns error", result != NULL);
    TEST("dispatch unknown has tool not found",
         result && strstr(result, "not found") != NULL);
    free(result);

    /* 4. Name matching */
    TEST("name_matches exact", registry_name_matches("test_tool", "test_tool"));
    TEST("name_matches wildcard", registry_name_matches("test_tool", "test_*"));
    TEST("name_matches prefix wildcard", registry_name_matches("test_tool", "*tool"));
    TEST("name_matches no match", !registry_name_matches("test_tool", "other"));
    TEST("name_matches NULL name", !registry_name_matches(NULL, "test_*"));

    /* 5. Set unavailable — dispatch returns error */
    registry_set_available("test_tool", false);
    result = registry_dispatch("test_tool", "{}", NULL);
    TEST("dispatch unavailable returns not found",
         result && strstr(result, "not found") != NULL);
    free(result);
    registry_set_available("test_tool", true);
    result = registry_dispatch("test_tool", "{}", NULL);
    TEST("dispatch re-enabled returns ok",
         result && strstr(result, "\"ok\"") != NULL);
    free(result);

    /* 6. Timeout */
    registry_set_timeout("test_tool", 60);
    TEST("get_timeout returns set value",
         registry_get_timeout("test_tool") == 60);
    TEST("get_timeout unknown returns 0 (not -1)",
         registry_get_timeout("nonexistent") == 0);

    /* 7. Toolset + JSON serialization */
    registry_set_toolset("test_tool", "testing");
    json_node_t *json = registry_to_json();
    TEST("to_json returns non-null", json != NULL);
    if (json) {
        const char *ser = json_serialize(json);
        TEST("serialized JSON contains test_tool",
             ser && strstr(ser, "test_tool") != NULL);
        TEST("serialized JSON type is function",
             ser && strstr(ser, "\"function\"") != NULL);
        free((void*)ser);
        json_free(json);
    }

    /* 8. Registry register_ex with toolset */
    TEST("register_ex with toolset",
         registry_register_ex("toolset_test", "ts desc", "{}", "testing", test_handler));
    json = registry_to_json();
    if (json) {
        const char *ser = json_serialize(json);
        TEST("JSON contains toolset_test",
             ser && strstr(ser, "toolset_test") != NULL);
        free((void*)ser);
        json_free(json);
    }

    /* 9. Registry filter by toolset */
    tool_registry_t filtered;
    memcpy(&filtered, registry_get(), sizeof(filtered));
    registry_filter_by_toolset(&filtered, "testing", NULL);
    TEST("filter by toolset finds tools",
         filtered.count > 0);
    /* Note: unlabeled tools (empty toolset) pass all filters per toolset_in_list.
     * The filter only restricts tools with a non-empty toolset that doesn't match. */

    /* 10. Pattern available */
    registry_set_available_pattern("test_*", false);
    result = registry_dispatch("test_tool", "{}", NULL);
    TEST("dispatch pattern-set-unavailable returns not found",
         result && strstr(result, "not found") != NULL);
    free(result);
    registry_set_available_pattern("test_*", true);
    result = registry_dispatch("test_tool", "{}", NULL);
    TEST("dispatch pattern-re-enabled returns ok",
         result && strstr(result, "\"ok\"") != NULL);
    free(result);

    /* 11. registry_get — returns non-NULL pointer */
    tool_registry_t *reg = registry_get();
    TEST("registry_get returns non-NULL", reg != NULL);
    TEST("registry_get count matches", reg && reg->count == registry_get_count());
    TEST("registry_count non-zero", registry_count() > 0);

    /* 12. Tool name repair — exact match returns copy */
    {
        char *r = registry_repair_tool_name("test_tool");
        TEST("repair exact name match", r && strcmp(r, "test_tool") == 0);
        free(r);
    }

    /* 13. Repair with hyphens -> underscores */
    {
        char *r = registry_repair_tool_name("test-tool");
        TEST("repair hyphens to underscores", r && strcmp(r, "test_tool") == 0);
        free(r);
    }

    /* 14. Repair with spaces -> underscores */
    {
        char *r = registry_repair_tool_name("test tool");
        TEST("repair spaces to underscores", r && strcmp(r, "test_tool") == 0);
        free(r);
    }

    /* 15. Repair CamelCase -> snake_case */
    registry_register("camel_case_tool", "camel test", "{}", test_handler);
    {
        char *r = registry_repair_tool_name("CamelCaseTool");
        TEST("repair CamelCase to snake_case", r && strcmp(r, "camel_case_tool") == 0);
        free(r);
    }

    /* 16. Repair _tool suffix strip */
    {
        char *r = registry_repair_tool_name("camel_case_tool_tool");
        TEST("repair _tool suffix strip (double)", r && strcmp(r, "camel_case_tool") == 0);
        free(r);
    }

    /* 17. Repair -tool suffix strip */
    {
        char *r = registry_repair_tool_name("test_tool-tool");
        TEST("repair -tool suffix strip", r && strcmp(r, "test_tool") == 0);
        free(r);
    }

    /* 18. Classic #14784: BrowserClick_tool -> browser_click */
    registry_register("browser_click", "Click browser", "{}", test_handler);
    {
        char *r = registry_repair_tool_name("BrowserClick_tool");
        TEST("repair BrowserClick_tool -> browser_click (gh#14784)",
             r && strcmp(r, "browser_click") == 0);
        free(r);
    }

    /* 19. NULL name returns NULL */
    TEST("repair NULL returns NULL", registry_repair_tool_name(NULL) == NULL);

    /* 20. Empty name returns NULL */
    TEST("repair empty returns NULL", registry_repair_tool_name("") == NULL);

    /* 21. Dispatch with repairable name dispatches successfully */
    {
        char *result = registry_dispatch("TEST-TOOL", "{}", NULL);
        TEST("dispatch repairable name (TEST-TOOL -> test_tool)",
             result && strstr(result, "\"ok\"") != NULL);
        free(result);
    }

    /* 22. Dispatch with CamelCase name dispatches successfully */
    {
        char *result = registry_dispatch("TestTool", "{}", NULL);
        TEST("dispatch CamelCase (TestTool -> test_tool)",
             result && strstr(result, "\"ok\"") != NULL);
        free(result);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
