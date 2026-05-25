/*
 * test_env_passthrough.c — Tests for env passthrough registry.
 */

#include "env_passthrough.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

static void setup(void)
{
    env_passthrough_clear();
}

static void test_blocked_list(void)
{
    TEST("ANTHROPIC_API_KEY blocked",
         env_passthrough_is_blocked("ANTHROPIC_API_KEY"));
    TEST("OPENAI_API_KEY blocked",
         env_passthrough_is_blocked("OPENAI_API_KEY"));
    TEST("OPENROUTER_API_KEY blocked",
         env_passthrough_is_blocked("OPENROUTER_API_KEY"));
    TEST("HERMES_HOME blocked",
         env_passthrough_is_blocked("HERMES_HOME"));
    TEST("AWS_ACCESS_KEY_ID blocked",
         env_passthrough_is_blocked("AWS_ACCESS_KEY_ID"));
    TEST("TENOR_API_KEY NOT blocked",
         !env_passthrough_is_blocked("TENOR_API_KEY"));
    TEST("NOTION_TOKEN NOT blocked",
         !env_passthrough_is_blocked("NOTION_TOKEN"));
    TEST("empty string not blocked",
         !env_passthrough_is_blocked(""));
    TEST("NULL not blocked",
         !env_passthrough_is_blocked(NULL));
}

static void test_register_basic(void)
{
    TEST("register new var", env_passthrough_register("TENOR_API_KEY"));
    TEST("is_allowed true", env_passthrough_is_allowed("TENOR_API_KEY"));

    /* Duplicate should succeed (already registered) */
    TEST("register duplicate", env_passthrough_register("TENOR_API_KEY"));
}

static void test_register_blocked(void)
{
    TEST("register blocked returns false",
         !env_passthrough_register("ANTHROPIC_API_KEY"));
    TEST("blocked var not in allowlist",
         !env_passthrough_is_allowed("ANTHROPIC_API_KEY"));
}

static void test_is_allowed_unregistered(void)
{
    TEST("unregistered not allowed",
         !env_passthrough_is_allowed("UNREGISTERED_VAR"));
    TEST("empty not allowed",
         !env_passthrough_is_allowed(""));
    TEST("NULL not allowed",
         !env_passthrough_is_allowed(NULL));
}

static void test_register_batch(void)
{
    const char *vars[] = {"TOOL_VAR_A", "TOOL_VAR_B", "ANTHROPIC_API_KEY", "TOOL_VAR_C"};
    int registered = env_passthrough_register_batch(vars, 4);
    TEST("batch registered 3 of 4 (1 blocked)", registered == 3);
    TEST("batch: TOOL_VAR_A allowed", env_passthrough_is_allowed("TOOL_VAR_A"));
    TEST("batch: TOOL_VAR_B allowed", env_passthrough_is_allowed("TOOL_VAR_B"));
    TEST("batch: TOOL_VAR_C allowed", env_passthrough_is_allowed("TOOL_VAR_C"));
    TEST("batch: ANTHROPIC_API_KEY NOT allowed",
         !env_passthrough_is_allowed("ANTHROPIC_API_KEY"));
}

static void test_clear(void)
{
    env_passthrough_register("MY_VAR");
    TEST("before clear: allowed", env_passthrough_is_allowed("MY_VAR"));
    env_passthrough_clear();
    TEST("after clear: not allowed", !env_passthrough_is_allowed("MY_VAR"));
}

static void test_get_all(void)
{
    env_passthrough_clear();
    env_passthrough_register("VAR_X");
    env_passthrough_register("VAR_Y");

    char **list = NULL;
    int count = 0;
    env_passthrough_get_all(&list, &count);

    TEST("get_all count", count == 2);
    if (count == 2) {
        TEST("get_all VAR_X", strcmp(list[0], "VAR_X") == 0);
        TEST("get_all VAR_Y", strcmp(list[1], "VAR_Y") == 0);
    }
    env_passthrough_free_list(list, count);
}

static void test_get_all_empty(void)
{
    env_passthrough_clear();
    char **list = (char **)0x1; /* poison */
    int count = -1;
    env_passthrough_get_all(&list, &count);
    TEST("get_all empty count", count == 0);
    TEST("get_all empty list", list == NULL);
}

static void test_null_edge_cases(void)
{
    /* Should not crash */
    env_passthrough_register_batch(NULL, 0);
    env_passthrough_get_all(NULL, NULL);
    env_passthrough_free_list(NULL, 0);
    env_passthrough_is_allowed(NULL);
    env_passthrough_is_blocked(NULL);
    TEST("null edge cases don't crash", 1);
}

int main(void)
{
    printf("=== Env Passthrough Library Tests ===\n");

    setup();
    test_blocked_list();
    setup();
    test_register_basic();
    setup();
    test_register_blocked();
    setup();
    test_is_allowed_unregistered();
    setup();
    test_register_batch();
    setup();
    test_clear();
    setup();
    test_get_all();
    setup();
    test_get_all_empty();
    setup();
    test_null_edge_cases();

    printf("\nResults: %d passed, %d failed, %d total\n",
           passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
