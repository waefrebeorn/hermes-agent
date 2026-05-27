/*
 * test_registry.c — Tests for tool registry.
 * Tests: registration, lookup, dispatch, availability, toolset filtering, name matching.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Types (matching include/hermes.h layout) */
typedef struct tool_t {
    char   name[64];
    char   description[512];
    char   schema_json[4096];
    char *(*handler)(const char *args_json, const char *task_id);
    bool   available;
    int    timeout_sec;
    char   toolset[32];
} tool_t;

typedef struct {
    tool_t *tools;
    size_t count;
    size_t capacity;
} tool_registry_t;

/* Declare the functions we need */
bool registry_register(const char *name, const char *description,
                       const char *schema_json,
                       char *(*handler)(const char *args_json, const char *task_id));
bool registry_register_ex(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id));
tool_t *registry_find(const char *name);
char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id);
size_t registry_count(void);
size_t registry_get_count(void);
const char *registry_get_name(size_t i);
void registry_set_available(const char *name, bool available);
void registry_set_available_pattern(const char *pattern, bool available);
bool registry_name_matches(const char *name, const char *pattern);
void registry_set_timeout(const char *name, int seconds);
int registry_get_timeout(const char *name);
void registry_set_toolset(const char *name, const char *toolset);
const char *registry_get_toolset(const char *name);
tool_registry_t *registry_get(void);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

static char *test_handler(const char *args, const char *task_id)
{
    (void)task_id;
    char *r = malloc(128);
    snprintf(r, 128, "{\"result\":\"handled %s\"}", args ? args : "null");
    return r;
}

int main(void)
{
    printf("=== Registry Tests ===\n\n");

    /* ── Basic registration ── */
    printf("-- Registration --\n");
    {
        size_t count = registry_count();
        size_t initial = count;

        bool ok = registry_register("test_tool", "A test tool", "{}", test_handler);
        TEST("register returns true", ok);
        TEST("count increased", registry_count() == initial + 1);

        ok = registry_register("test_tool", "duplicate", "{}", test_handler);
        TEST("duplicate registration returns false", !ok);
        TEST("count unchanged", registry_count() == initial + 1);
    }

    /* ── Find ── */
    printf("\n-- Find --\n");
    {
        tool_t *t = registry_find("test_tool");
        TEST("find returns non-NULL", t != NULL);
        if (t) {
            TEST("name matches", strcmp(t->name, "test_tool") == 0);
            TEST("description matches", strcmp(t->description, "A test tool") == 0);
            TEST("is available", t->available);
        }

        t = registry_find("nonexistent");
        TEST("find nonexistent returns NULL", t == NULL);
    }

    /* ── Dispatch ── */
    printf("\n-- Dispatch --\n");
    {
        char *result = registry_dispatch("test_tool", "{\"key\":\"val\"}", "task1");
        TEST("dispatch returns result", result != NULL);
        if (result) {
            TEST("result contains handled",
                 strstr(result, "handled") != NULL);
            free(result);
        }

        result = registry_dispatch("nonexistent", "{}", "task");
        TEST("dispatch nonexistent returns error", result != NULL);
        if (result) {
            TEST("error mentions not found",
                 strstr(result, "not found") != NULL);
            free(result);
        }
    }

    /* ── Available flag ── */
    printf("\n-- Available flag --\n");
    {
        /* Register a disabled tool */
        registry_register("disabled_tool", "test", "{}", test_handler);
        tool_t *t = registry_find("disabled_tool");
        TEST("disabled_tool found before disable", t != NULL);

        registry_set_available("disabled_tool", false);
        t = registry_find("disabled_tool");
        TEST("disabled_tool NOT findable after disable", t == NULL);

        registry_set_available("disabled_tool", true);
        t = registry_find("disabled_tool");
        TEST("disabled_tool findable after re-enable", t != NULL);
    }

    /* ── Timeout ── */
    printf("\n-- Timeout --\n");
    {
        registry_set_timeout("test_tool", 30);
        int to = registry_get_timeout("test_tool");
        TEST("timeout set to 30", to == 30);

        to = registry_get_timeout("nonexistent");
        TEST("timeout for nonexistent returns 0", to == 0);
    }

    /* ── Toolset ── */
    printf("\n-- Toolset --\n");
    {
        registry_set_toolset("test_tool", "core");
        const char *ts = registry_get_toolset("test_tool");
        TEST("toolset set to core", ts && strcmp(ts, "core") == 0);

        ts = registry_get_toolset("nonexistent");
        TEST("toolset for nonexistent returns empty", ts && *ts == '\0');
    }

    /* ── Name matching ── */
    printf("\n-- Name matching --\n");
    {
        TEST("exact match", registry_name_matches("hello", "hello"));
        TEST("exact match fail", !registry_name_matches("hello", "world"));
        TEST("prefix match", registry_name_matches("hello_world", "hello*"));
        TEST("suffix match", registry_name_matches("foo_bar", "*bar"));
        TEST("prefix+suffix match", registry_name_matches("ab_cd_ef", "ab*ef"));
        TEST("prefix+suffix fail", !registry_name_matches("ab_cd_ef", "ab*xy"));
        TEST("null name returns false", !registry_name_matches(NULL, "test"));
        TEST("null pattern returns false", !registry_name_matches("test", NULL));
    }

    /* ── Available pattern ── */
    printf("\n-- Available pattern --\n");
    {
        registry_register("regex_enabled", "test", "{}", test_handler);
        registry_register("regex_disabled", "test", "{}", test_handler);
        TEST("regex_enabled findable before pattern",
             registry_find("regex_enabled") != NULL);
        TEST("regex_disabled findable before pattern",
             registry_find("regex_disabled") != NULL);

        registry_set_available_pattern("regex_*", false);
        TEST("regex_enabled NOT findable after pattern",
             registry_find("regex_enabled") == NULL);
        TEST("regex_disabled NOT findable after pattern",
             registry_find("regex_disabled") == NULL);

        registry_set_available_pattern("regex_*", true);
        TEST("regex_enabled findable after re-enable",
             registry_find("regex_enabled") != NULL);
        TEST("regex_disabled findable after re-enable",
             registry_find("regex_disabled") != NULL);
    }

    /* ── Get count and names ── */
    printf("\n-- Accessors --\n");
    {
        size_t c = registry_get_count();
        TEST("get_count returns positive", c > 0);

        const char *name = registry_get_name(0);
        TEST("get_name returns non-NULL", name != NULL);
        TEST("get_name returns string", name && *name);

        name = registry_get_name(9999);
        TEST("get_name out of range returns NULL", name == NULL);
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
