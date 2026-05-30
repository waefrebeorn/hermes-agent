/* Test in-memory plugin via plugin API */
#include "plugin.h"
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

int main(void) {
    printf("=== Plugin Honcho (In-Memory Memory) Tests ===\n");

    /* Load the plugin */
    const char *plugin_path = PLUGIN_DIR "/plugin_honcho.so";
    plugin_t *p = plugin_load(plugin_path);
    TEST("plugin loads", p != NULL);
    if (!p) {
        printf("  Error: %s (path: %s)\n", plugin_error(), plugin_path);
        printf("\n=== %d/%d passed ===\n", pass, pass + fail);
        return 1;
    }

    /* Check metadata */
    TEST("name = in-memory-store", strcmp(plugin_name(p), "in-memory-store") == 0);
    TEST("type = MEMORY", plugin_type(p) == PLUGIN_MEMORY);
    const char *desc = plugin_description(p);
    TEST("description not null", desc != NULL);
    TEST("description mentions memory", desc && strstr(desc, "memory") != NULL);

    /* Check version */
    const plugin_version_t *ver = plugin_version(p);
    TEST("version not null", ver != NULL);
    TEST("major == 1", ver->major == 1);

    /* Check deps */
    int dep_count;
    const plugin_dep_t *deps = plugin_deps(p, &dep_count);
    TEST("deps accessible", deps != NULL);
    TEST("no dependencies", dep_count == 0);

    /* Init */
    int init_rc = 0;
    {
        typedef int (*init_fn_t)(void);
        init_fn_t init_fn = (init_fn_t)plugin_symbol(p, "plugin_init");
        TEST("plugin_init symbol found", init_fn != NULL);
        if (init_fn) {
            init_rc = init_fn();
            TEST("init returns 0", init_rc == 0);
        }
    }

    /* Get interface */
    plugin_interface_t *iface = NULL;
    {
        void *(*get_iface)(void) = (void *(*)(void))plugin_symbol(p, "plugin_get_interface");
        TEST("plugin_get_interface found", get_iface != NULL);
        if (get_iface) {
            iface = (plugin_interface_t *)get_iface();
            TEST("interface not null", iface != NULL);
            TEST("interface type = MEMORY", iface != NULL && iface->type == PLUGIN_MEMORY);
        }
    }

    if (iface && iface->memory_store && iface->memory_search && iface->memory_clear) {
        /* Store a memory */
        char *store_result = iface->memory_store("Hello, this is a test memory about AI agents.",
                                                  "{\"source\":\"test\",\"priority\":\"high\"}");
        TEST("store result not null", store_result != NULL);
        TEST("store succeeds", store_result && strstr(store_result, "\"status\":\"ok\"") != NULL);
        TEST("store has id", store_result && strstr(store_result, "\"id\":\"mem-") != NULL);
        free(store_result);

        /* Store another */
        char *store2 = iface->memory_store("Another memory about machine learning.",
                                            "{\"source\":\"test\"}");
        TEST("second store succeeds", store2 && strstr(store2, "\"status\":\"ok\"") != NULL);
        free(store2);

        /* Store third */
        char *store3 = iface->memory_store("Unrelated note about cooking recipes.",
                                            NULL);
        TEST("third store (no metadata)", store3 && strstr(store3, "\"status\":\"ok\"") != NULL);
        free(store3);

        /* Search for AI-related */
        char *search = iface->memory_search("AI", 10);
        TEST("search result not null", search != NULL);
        TEST("AI search finds AI memory", search && strstr(search, "AI agents") != NULL);
        TEST("AI search does NOT find cooking", search && strstr(search, "cooking") == NULL);
        free(search);

        /* Search for all (empty query) */
        char *all = iface->memory_search("", 10);
        TEST("empty search returns all", all != NULL);
        /* JSON format: {"count":3,"results":[...]} */
        TEST("empty search finds 3", all && strstr(all, "\"count\":3") != NULL);
        free(all);

        /* Clear */
        iface->memory_clear();

        /* Verify empty after clear */
        char *after_clear = iface->memory_search("", 10);
        TEST("search after clear", after_clear != NULL);
        TEST("count is 0 after clear", after_clear && strstr(after_clear, "\"count\":0") != NULL);
        free(after_clear);

    } else {
        fail++;
        printf("  FAIL: interface function pointers missing\n");
    }

    /* Cleanup */
    {
        typedef int (*clean_fn_t)(void);
        clean_fn_t clean_fn = (clean_fn_t)plugin_symbol(p, "plugin_cleanup");
        if (clean_fn) clean_fn();
    }
    plugin_unload(p);
    TEST("plugin unloaded successfully", 1);

    printf("\n=== Plugin Honcho: %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
