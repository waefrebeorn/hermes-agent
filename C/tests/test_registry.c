/* test_registry.c — Tests for core tool registry (register, find, dispatch). */
#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

/* Test handler that echoes args back */
static char *echo_handler(const char *args, const char *task_id) {
    (void)task_id;
    char *buf = (char *)malloc(256);
    if (!buf) return NULL;
    snprintf(buf, 256, "{\"echo\":\"%s\"}", args ? args : "");
    return buf;
}

/* Test handler that returns a fixed string */
static char *fixed_handler(const char *args, const char *task_id) {
    (void)args;
    (void)task_id;
    return strdup("{\"result\":42}");
}

int main(void) {
    size_t count_before = registry_count();
    printf("=== Registry Tests ===\n\n");

    /* --- registry_register --- */
    printf("--- registry_register ---\n");
    TEST("register echo tool", registry_register("echo_test", "Echo args", "{}", echo_handler));
    TEST("register fixed tool", registry_register("fixed_test", "Fixed result", "{}", fixed_handler));
    TEST("duplicate register returns false",
         !registry_register("echo_test", "", "{}", echo_handler));
    TEST("register NULL name returns false",
         !registry_register(NULL, "", "{}", echo_handler));
    TEST("register NULL handler returns false",
         !registry_register("no_handler", "", "{}", NULL));

    /* --- registry_find --- */
    printf("\n--- registry_find ---\n");
    tool_t *t = registry_find("echo_test");
    TEST("find echo_test returns non-NULL", t != NULL);
    if (t) {
        TEST("echo_test name matches", strcmp(t->name, "echo_test") == 0);
        TEST("echo_test description matches", strcmp(t->description, "Echo args") == 0);
        TEST("echo_test handler non-NULL", t->handler != NULL);
    }
    t = registry_find("fixed_test");
    TEST("find fixed_test returns non-NULL", t != NULL);
    t = registry_find("nonexistent");
    TEST("find nonexistent returns NULL", t == NULL);
    t = registry_find(NULL);
    TEST("find NULL returns NULL", t == NULL);

    /* --- registry_dispatch --- */
    printf("\n--- registry_dispatch ---\n");
    {
        char *r = registry_dispatch("echo_test", "hello", NULL);
        TEST("dispatch echo_test returns result", r != NULL);
        if (r) {
            TEST("dispatch echo_test contains echo field",
                 strstr(r, "\"echo\"") != NULL);
            free(r);
        }
    }
    {
        char *r = registry_dispatch("fixed_test", "{}", NULL);
        TEST("dispatch fixed_test returns result", r != NULL);
        if (r) {
            TEST("dispatch fixed_test returns 42",
                 strstr(r, "\"result\":42") != NULL);
            free(r);
        }
    }
    {
        char *r = registry_dispatch("nonexistent", "{}", NULL);
        TEST("dispatch nonexistent returns error", r != NULL);
        if (r) {
            TEST("dispatch nonexistent has error message",
                 strstr(r, "error") != NULL);
            free(r);
        }
    }

    /* --- registry_count / registry_get_count --- */
    printf("\n--- registry_count ---\n");
    TEST("registry_count includes pre-existing + our tools",
         registry_count() >= count_before + 2);
    TEST("registry_get_count matches",
         registry_get_count() == registry_count());

    /* --- registry_get_name / registry_get --- */
    printf("\n--- registry_get_name ---\n");
    {
        const char *n0 = registry_get_name(0);
        TEST("registry_get_name(0) non-NULL", n0 != NULL);
        if (n0) TEST("registry_get_name(0) non-empty", n0[0]);
    }
    {
        const char *nx = registry_get_name(9999);
        TEST("registry_get_name(out of bounds) returns NULL", nx == NULL);
    }
    {
        tool_registry_t *reg = registry_get();
        TEST("registry_get returns non-NULL", reg != NULL);
        TEST("registry_get count > 0", reg->count > 0);
    }

    /* --- registry_to_json --- */
    printf("\n--- registry_to_json ---\n");
    {
        json_node_t *tools = registry_to_json();
        TEST("registry_to_json returns non-NULL", tools != NULL);
        if (tools) {
            TEST("registry_to_json has elements", json_len(tools) > 0);
            json_free(tools);
        }
    }

    /* --- registry_set_available --- */
    printf("\n--- registry_set_available ---\n");
    TEST("echo_test available initially",
         registry_find("echo_test") != NULL);
    registry_set_available("echo_test", false);
    TEST("echo_test not available after set_available(false)",
         registry_find("echo_test") == NULL);
    registry_set_available("echo_test", true);
    TEST("echo_test available after set_available(true)",
         registry_find("echo_test") != NULL);
    registry_set_available("nonexistent", false);
    /* Should not crash — no assert needed */

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
