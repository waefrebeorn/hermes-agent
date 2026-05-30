/*
 * test_plugin_file_memory.c — Tests for plugin_file_memory.so
 *
 * Tests: metadata, lifecycle, store/search/clear cycle.
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* Typedef for plugin_get_interface — returns void*, but is actually plugin_interface_t* */
typedef void *(*get_iface_fn)(void);

int main(void) {
    const char *plugin_dir = getenv("PLUGIN_DIR");
    if (!plugin_dir) plugin_dir = "src/plugins";

    char path[4096];
    snprintf(path, sizeof(path), "%s/plugin_file_memory.so", plugin_dir);

    plugin_t *p = plugin_load(path);
    TEST("plugin_load not NULL", p != NULL);
    if (!p) {
        fprintf(stderr, "  Error: %s\n", plugin_error());
        goto report;
    }

    TEST("plugin_name", strcmp(plugin_name(p), "file-memory") == 0);
    TEST("plugin_type memory", plugin_type(p) == PLUGIN_MEMORY);

    /* Init */
    typedef int (*init_fn)(void);
    init_fn init = (init_fn)plugin_symbol(p, "plugin_init");
    TEST("init found", init != NULL);
    if (init) TEST("init returns 0", init() == 0);

    /* Get interface — must CALL the function, not cast the symbol address! */
    get_iface_fn get_iface = (get_iface_fn)plugin_symbol(p, "plugin_get_interface");
    TEST("plugin_get_interface found", get_iface != NULL);
    if (get_iface) {
        plugin_interface_t *pi = (plugin_interface_t *)get_iface();
        TEST("interface type MEMORY", pi->type == PLUGIN_MEMORY);
        TEST("memory_store not NULL", pi->memory_store != NULL);
        TEST("memory_search not NULL", pi->memory_search != NULL);
        TEST("memory_clear not NULL", pi->memory_clear != NULL);

        if (pi->memory_store && pi->memory_search && pi->memory_clear) {
            /* Store a memory */
            char *r1 = pi->memory_store("hello world", "{\"source\":\"test\"}");
            TEST("store returns not null", r1 != NULL);
            TEST("store contains id", r1 && strstr(r1, "\"id\"") != NULL);
            free(r1);

            /* Store another */
            char *r2 = pi->memory_store("goodbye world", "{}");
            TEST("second store ok", r2 != NULL);
            free(r2);

            /* Search — should find "hello" */
            char *s1 = pi->memory_search("hello", 10);
            TEST("search hello not null", s1 != NULL);
            TEST("search hello finds result", s1 && strstr(s1, "hello world") != NULL);
            free(s1);

            /* Search — should NOT find "nonexistent" */
            char *s2 = pi->memory_search("nonexistent_keyword_xyz", 10);
            TEST("search nonexistent not null", s2 != NULL);
            TEST("search nonexistent count 0", s2 && strstr(s2, "\"count\":0") != NULL);
            free(s2);

            /* Clear */
            pi->memory_clear();

            /* Verify cleared */
            char *s3 = pi->memory_search("hello", 10);
            TEST("search after clear not null", s3 != NULL);
            TEST("search after clear count 0", s3 && strstr(s3, "\"count\":0") != NULL);
            free(s3);
        }
    }

    /* Cleanup */
    typedef int (*cleanup_fn)(void);
    cleanup_fn cleanup = (cleanup_fn)plugin_symbol(p, "plugin_cleanup");
    TEST("cleanup found", cleanup != NULL);
    if (cleanup) TEST("cleanup returns 0", cleanup() == 0);

    plugin_unload(p);
    TEST("unload safe", 1);

report:
    fprintf(stderr, "plugin_file_memory: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
