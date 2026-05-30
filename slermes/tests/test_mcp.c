/*
 * test_mcp.c — MCP tool unit tests (M44).
 *
 * Tests the MCP tool integration: registration, handler validation,
 * and state management. Does NOT require running MCP servers.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_mcp.c src/tools/mcp_tool.c lib/libmcp/mcp.c \
 *       src/tools/registry.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_mcp -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_mcp
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from registry.c and mcp_tool.c */
extern void registry_init_mcp(void);
extern tool_registry_t *registry_get(void);
extern size_t registry_count(void);
extern tool_t *registry_find(const char *name);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ================================================================
 *  Tests
 * ================================================================ */

static void test_registration(void) {
    printf("\n--- Registration Tests ---\n");

    registry_init_mcp();

    size_t count = registry_count();
    TEST("registry has tools after init", count > 0);

    /* Check specific tool names exist */
    bool has_status   = registry_find("mcp_status") != NULL;
    bool has_call     = registry_find("mcp_tool_call") != NULL;
    bool has_auth     = registry_find("mcp_auth") != NULL;
    bool has_rlist    = registry_find("mcp_resource_list") != NULL;
    bool has_rread    = registry_find("mcp_resource_read") != NULL;
    bool has_plist    = registry_find("mcp_prompt_list") != NULL;
    bool has_pget     = registry_find("mcp_prompt_get") != NULL;
    TEST("mcp_status registered", has_status);
    TEST("mcp_tool_call registered", has_call);
    TEST("mcp_auth registered", has_auth);
    TEST("mcp_resource_list registered", has_rlist);
    TEST("mcp_resource_read registered", has_rread);
    TEST("mcp_prompt_list registered", has_plist);
    TEST("mcp_prompt_get registered", has_pget);

    /* Double init is safe */
    registry_init_mcp();
    TEST("double registry init doesn't crash", 1);
}

static void test_handler_null_args(void) {
    printf("\n--- Handler Validation Tests ---\n");

    /* Call handlers with valid args to test they don't crash */
    tool_t *tool;

    tool = registry_find("mcp_status");
    if (tool) {
        char *result = tool->handler("{}", NULL);
        TEST("mcp_status with {} args returns non-null", result != NULL);
        free(result);
    }

    tool = registry_find("mcp_auth");
    if (tool) {
        char *result = tool->handler("{\"action\":\"status\"}", NULL);
        TEST("mcp_auth with status action returns non-null", result != NULL);
        free(result);
    }

    tool = registry_find("mcp_resource_list");
    if (tool) {
        char *result = tool->handler("{}", NULL);
        TEST("mcp_resource_list returns non-null", result != NULL);
        free(result);
    }

    tool = registry_find("mcp_resource_read");
    if (tool) {
        char *result = tool->handler("{\"server\":\"test\",\"uri\":\"test://x\"}", NULL);
        TEST("mcp_resource_read with args returns non-null", result != NULL);
        free(result);
    }

    tool = registry_find("mcp_prompt_list");
    if (tool) {
        char *result = tool->handler("{}", NULL);
        TEST("mcp_prompt_list returns non-null", result != NULL);
        free(result);
    }

    tool = registry_find("mcp_prompt_get");
    if (tool) {
        char *result = tool->handler("{\"server\":\"test\",\"name\":\"test\"}", NULL);
        TEST("mcp_prompt_get with args returns non-null", result != NULL);
        free(result);
    }
}

static void test_schema_validation(void) {
    printf("\n--- Schema Validation Tests ---\n");

    /* Verify each tool has a non-null schema */
    static const char *expected[] = {
        "mcp_status", "mcp_tool_call", "mcp_auth",
        "mcp_resource_list", "mcp_resource_read",
        "mcp_prompt_list", "mcp_prompt_get", NULL
    };

    for (int i = 0; expected[i]; i++) {
        tool_t *tool = registry_find(expected[i]);
        if (tool) {
            TEST("tool has schema", tool->schema_json != NULL);
        }
    }

    /* Test mcp_tool_call timeout */
    tool_t *tool = registry_find("mcp_tool_call");
    if (tool) {
        TEST("mcp_tool_call has 180s timeout", tool->timeout_sec == 180);
    }

    tool = registry_find("mcp_resource_read");
    if (tool) {
        TEST("mcp_resource_read has 60s timeout", tool->timeout_sec == 60);
    }
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("MCP Tool Tests (M44)\n");
    printf("====================\n");

    test_registration();
    test_handler_null_args();
    test_schema_validation();

    printf("\n====================\n");
    printf("  %d passed, %d failed\n", passed, failed);
    printf("====================\n");

    return failed > 0 ? 1 : 0;
}
