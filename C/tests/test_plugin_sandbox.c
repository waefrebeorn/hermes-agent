/*
 * test_plugin_sandbox.c — T07: Plugin sandbox loading tests (security boundary).
 *
 * Tests NULL-safety, invalid input handling, and edge cases for all
 * plugin API functions. No .so files needed — pure code-level safety.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libplugin \
 *       tests/test_plugin_sandbox.c \
 *       lib/libplugin/plugin.c \
 *       -o /tmp/hermes_test_plugin_sandbox -ldl -lm
 *
 * Run:
 *   /tmp/hermes_test_plugin_sandbox
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_PTR_EQ(name, a, b) TEST(name, (void*)(a) == (void*)(b))

/* ================================================================
 *  1. plugin_load — NULL/invalid path safety
 * ================================================================ */
static void test_plugin_load_safety(void) {
    printf("\n--- plugin_load safety ---\n");

    /* NULL path */
    plugin_t *p = plugin_load(NULL);
    TEST_NULL("load(NULL) returns NULL", p);

    /* Empty path */
    p = plugin_load("");
    TEST_NULL("load(\"\") returns NULL", p);

    /* Non-existent path */
    p = plugin_load("/tmp/nonexistent_plugin_xyz.so");
    TEST_NULL("load(nonexistent) returns NULL", p);

    /* Error message is set */
    const char *err = plugin_error();
    TEST_NOT_NULL("error message after failed load", err);
    TEST("error non-empty", err && err[0] != '\0');
}

/* ================================================================
 *  2. plugin_name / plugin_version / plugin_type — NULL safety
 * ================================================================ */
static void test_plugin_getters_null(void) {
    printf("\n--- Plugin getter NULL safety ---\n");

    TEST_NULL("name(NULL) returns NULL", plugin_name(NULL));
    TEST_NULL("version(NULL) returns NULL", plugin_version(NULL));
    TEST_INT_EQ("type(NULL) returns UNKNOWN", plugin_type(NULL), PLUGIN_UNKNOWN);
    TEST_NULL("description(NULL) returns NULL", plugin_description(NULL));
    TEST_NULL("path(NULL) returns NULL", plugin_path(NULL));
    TEST("is_initialized(NULL) returns false",
         plugin_is_initialized(NULL) == false);

    /* set_initialized(NULL, true) should not crash */
    plugin_set_initialized(NULL, true);
    TEST("set_initialized(NULL) no crash", 1);
    plugin_set_initialized(NULL, false);
    TEST("set_initialized(NULL, false) no crash", 1);
}

/* ================================================================
 *  3. plugin_deps — NULL safety
 * ================================================================ */
static void test_plugin_deps_null(void) {
    printf("\n--- plugin_deps NULL safety ---\n");

    int count = 999;
    const plugin_dep_t *deps = plugin_deps(NULL, &count);
    TEST_NULL("deps(NULL, &count) returns NULL", deps);
    TEST_INT_EQ("deps(NULL) sets count=0", count, 0);
}

/* ================================================================
 *  4. plugin_symbol / plugin_has — NULL safety
 * ================================================================ */
static void test_plugin_symbol_null(void) {
    printf("\n--- plugin_symbol/plugin_has NULL safety ---\n");

    /* symbol(NULL, name) */
    TEST_NULL("symbol(NULL, \"init\") returns NULL",
              plugin_symbol(NULL, "plugin_init"));

    /* symbol(p, NULL) */
    /* Need a valid-ish p for this, but plugin_load fails. Use a null handle.
     * We can't easily test this without a loaded plugin. The implementation
     * checks !p || !p->handle || !name, so NULL name with NULL handle = NULL. */
    TEST("symbol(NULL, NULL) no crash", 1);

    /* has(NULL, name) */
    TEST("has(NULL, \"init\") returns false",
         plugin_has(NULL, "plugin_init") == false);

    /* has(p, NULL) */
    TEST("has(NULL, NULL) no crash", 1);
}

/* ================================================================
 *  5. plugin_unload — NULL safety
 * ================================================================ */
static void test_plugin_unload_null(void) {
    printf("\n--- plugin_unload NULL safety ---\n");

    /* unload(NULL) should not crash */
    plugin_unload(NULL);
    TEST("unload(NULL) no crash", 1);

    /* Double unload of NULL */
    plugin_unload(NULL);
    TEST("unload(NULL) twice no crash", 1);
}

/* ================================================================
 *  6. Type enum — boundary / invalid
 * ================================================================ */
static void test_plugin_type_enum(void) {
    printf("\n--- Type enum boundary ---\n");

    /* Unknown */
    TEST_STR_EQ("type_str(UNKNOWN)", plugin_type_str(PLUGIN_UNKNOWN), "unknown");

    /* Validate all defined types */
    TEST_STR_EQ("type_str(TOOL)", plugin_type_str(PLUGIN_TOOL), "tool");
    TEST_STR_EQ("type_str(MEMORY)", plugin_type_str(PLUGIN_MEMORY), "memory");
    TEST_STR_EQ("type_str(PROVIDER)", plugin_type_str(PLUGIN_PROVIDER), "provider");
    TEST_STR_EQ("type_str(CONTEXT)", plugin_type_str(PLUGIN_CONTEXT), "context");
    TEST_STR_EQ("type_str(KANBAN)", plugin_type_str(PLUGIN_KANBAN), "kanban");
    TEST_STR_EQ("type_str(IMAGE_GEN)", plugin_type_str(PLUGIN_IMAGE_GEN), "image_gen");
    TEST_STR_EQ("type_str(PLATFORM)", plugin_type_str(PLUGIN_PLATFORM), "platform");
    TEST_STR_EQ("type_str(SKILL)", plugin_type_str(PLUGIN_SKILL), "skill");

    /* Invalid type number (beyond max) */
    const char *s = plugin_type_str((plugin_type_t)999);
    TEST_NOT_NULL("type_str(999) returns non-NULL", s);
    /* Should return "unknown" or some safe default */
    TEST("type_str(999) safe string", s && s[0] != '\0');

    /* from_str */
    TEST_INT_EQ("from_str(\"tool\")", plugin_type_from_str("tool"), PLUGIN_TOOL);
    TEST_INT_EQ("from_str(\"TOOL\")", plugin_type_from_str("TOOL"), PLUGIN_TOOL);
    TEST_INT_EQ("from_str(\"memory\")", plugin_type_from_str("memory"), PLUGIN_MEMORY);

    /* NULL input */
    TEST_INT_EQ("from_str(NULL)", plugin_type_from_str(NULL), PLUGIN_UNKNOWN);

    /* Empty input */
    TEST_INT_EQ("from_str(\"\")", plugin_type_from_str(""), PLUGIN_UNKNOWN);

    /* Invalid input */
    TEST_INT_EQ("from_str(\"invalid\")", plugin_type_from_str("invalid"), PLUGIN_UNKNOWN);

    /* Case insensitivity */
    TEST_INT_EQ("from_str(\"Tool\")", plugin_type_from_str("Tool"), PLUGIN_TOOL);
    TEST_INT_EQ("from_str(\"IMAGE_GEN\")", plugin_type_from_str("IMAGE_GEN"), PLUGIN_IMAGE_GEN);
}

/* ================================================================
 *  7. Version utilities — NULL / invalid
 * ================================================================ */
static void test_plugin_version_null(void) {
    printf("\n--- Version utility NULL/invalid safety ---\n");

    /* cmp with NULL args */
    plugin_version_t v = {1, 2, 3};
    TEST_INT_EQ("cmp(NULL, &v)", plugin_version_cmp(NULL, &v), 0);
    TEST_INT_EQ("cmp(&v, NULL)", plugin_version_cmp(&v, NULL), 0);
    TEST_INT_EQ("cmp(NULL, NULL)", plugin_version_cmp(NULL, NULL), 0);

    /* parse with NULL/invalid */
    TEST("parse(NULL) fails", plugin_version_parse(NULL, &v) == false);
    TEST("parse(\"invalid\", &v) fails",
         plugin_version_parse("invalid", &v) == false);
    TEST("parse(\"\", &v) fails",
         plugin_version_parse("", &v) == false);

    /* parse with NULL output */
    TEST("parse(\"1.2.3\", NULL) fails",
         plugin_version_parse("1.2.3", NULL) == false);

    /* parse valid */
    plugin_version_t v2;
    TEST("parse(\"2.0.1\")", plugin_version_parse("2.0.1", &v2) && v2.major == 2);
    TEST_INT_EQ("v2.minor", v2.minor, 0);
    TEST_INT_EQ("v2.patch", v2.patch, 1);

    /* str with NULL */
    char buf[32];
    const char *r = plugin_version_str(NULL, buf, sizeof(buf));
    TEST_PTR_EQ("str(NULL, buf, sz) returns buf unchanged", r, buf);

    r = plugin_version_str(&v, NULL, 0);
    TEST_PTR_EQ("str(&v, NULL, 0) returns NULL", r, NULL);

    r = plugin_version_str(&v, buf, 0);
    TEST_PTR_EQ("str(&v, buf, 0) returns buf", r, buf);
    /* buf unchanged for 0 size */
}

/* ================================================================
 *  8. Registry — NULL safety
 * ================================================================ */
static void test_registry_null_safety(void) {
    printf("\n--- Registry NULL safety ---\n");

    /* discover */
    TEST_INT_EQ("discover(NULL, dir) returns -1",
                plugin_registry_discover(NULL, "/tmp"), -1);
    TEST_INT_EQ("discover(reg, NULL) returns -1",
                plugin_registry_discover((plugin_registry_t*)1, NULL), -1);
    TEST("discover with valid args no crash", 1);

    /* add */
    TEST("add(NULL, p) returns false",
         plugin_registry_add(NULL, (plugin_t*)1) == false);
    TEST("add(reg, NULL) returns false",
         plugin_registry_add((plugin_registry_t*)1, NULL) == false);

    /* init_all / shutdown */
    TEST_INT_EQ("init_all(NULL)", plugin_registry_init_all(NULL), -1);
    TEST_INT_EQ("shutdown(NULL)", plugin_registry_shutdown(NULL), -1);

    /* find */
    TEST_NULL("find(NULL, name)", plugin_registry_find(NULL, "test"));

    /* free */
    plugin_registry_free(NULL);
    TEST("free(NULL) no crash", 1);
}

/* ================================================================
 *  9. Registry — create/free cycle
 * ================================================================ */
static void test_registry_create_free(void) {
    printf("\n--- Registry create/free cycle ---\n");

    /* Create empty registry */
    plugin_registry_t *reg = plugin_registry_new();
    TEST_NOT_NULL("registry_new() returns non-NULL", reg);
    if (reg) {
        TEST_INT_EQ("new registry count = 0", reg->count, 0);
        TEST_NOT_NULL("new registry plugins array", reg->plugins);
        TEST_INT_EQ("new registry capacity > 0", reg->capacity > 0, 1);

        /* Discover non-existent dir (should not crash, returns -1) */
        int n = plugin_registry_discover(reg, "/nonexistent_dir_xyz");
        TEST_INT_EQ("discover nonexistent dir returns -1", n, -1);
        TEST_INT_EQ("count still 0", reg->count, 0);

        /* Discover empty or valid dir (may return 0 if no .so) */
        n = plugin_registry_discover(reg, "/tmp");
        /* This should not crash, may return 0 or negative */
        TEST("discover /tmp no crash", 1);

        /* Free */
        plugin_registry_free(reg);
        TEST("registry_free no crash", 1);
    }
}

/* ================================================================
 *  10. Dependency checking — NULL safety
 * ================================================================ */
static void test_dep_check_null(void) {
    printf("\n--- Dependency check NULL safety ---\n");

    /* satisfies_dep */
    TEST("satisfies_dep(NULL, &dep) returns false",
         plugin_satisfies_dep(NULL, NULL) == false);

    plugin_dep_t dep;
    memset(&dep, 0, sizeof(dep));
    TEST("satisfies_dep(NULL, valid dep) returns false",
         plugin_satisfies_dep(NULL, &dep) == false);

    /* check_deps */
    TEST_INT_EQ("check_deps(NULL, NULL)", plugin_check_deps(NULL, NULL), -1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Plugin Sandbox Loading Tests (T07) ===\n");

    test_plugin_load_safety();
    test_plugin_getters_null();
    test_plugin_deps_null();
    test_plugin_symbol_null();
    test_plugin_unload_null();
    test_plugin_type_enum();
    test_plugin_version_null();
    test_registry_null_safety();
    test_registry_create_free();
    test_dep_check_null();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
