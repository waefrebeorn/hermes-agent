/* test_plugin_load.c — Test loading example plugins.
 * Compile: gcc -I../lib/libplugin test_plugin_load.c ../lib/libplugin/plugin.c -o /tmp/test_plugin -ldl
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

    /* Test 1: load hello plugin */
    plugin_t *p = plugin_load("out/hello_plugin.so");
    TEST("plugin_load", p != NULL);

    if (p) {
        TEST("plugin_name", plugin_name(p) != NULL);
        printf("       name: %s\n", plugin_name(p));

        /* Check symbols */
        TEST("plugin_symbol(plugin_init)", plugin_symbol(p, "plugin_init") != NULL);
        TEST("plugin_symbol(plugin_cleanup)", plugin_symbol(p, "plugin_cleanup") != NULL);
        TEST("plugin_symbol(plugin_meta_name)", plugin_symbol(p, "plugin_meta_name") != NULL);
        TEST("plugin_symbol(plugin_meta_version)", plugin_symbol(p, "plugin_meta_version") != NULL);

        /* Check interfaces */
        TEST("plugin_has(plugin_init)", plugin_has(p, "plugin_init"));
        TEST("plugin_has(plugin_meta_name)", plugin_has(p, "plugin_meta_name"));
        TEST("plugin_has(nonexistent) == false", !plugin_has(p, "nonexistent"));

        /* Call init */
        int (*init_fn)(void) = (int (*)(void))plugin_symbol(p, "plugin_init");
        TEST("plugin_init returns 0", init_fn() == 0);

        /* Unload */
        plugin_unload(p);
        TEST("plugin_unload ok", 1);
    }

    /* Test 2: discover plugins in directory */
    plugin_registry_t *reg = plugin_registry_new();
    TEST("plugin_registry_new", reg != NULL);

    if (reg) {
        int n = plugin_registry_discover(reg, "out");
        TEST("plugin_registry_discover found >= 1", n >= 1);
        printf("       discovered: %d\n", n);

        if (n > 0) {
            TEST("registry->count > 0", reg->count > 0);
            printf("       first: %s\n", plugin_name(reg->plugins[0]));

            /* Init all */
            int ok = plugin_registry_init_all(reg);
            TEST("plugin_registry_init_all runs", ok > 0);

            /* Find by name */
            plugin_t *found = plugin_registry_find(reg, "hello_plugin");
            TEST("plugin_registry_find(hello_plugin)", found != NULL);
        }

        plugin_registry_free(reg);
        TEST("plugin_registry_free ok", 1);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All plugin tests PASSED");
    return failures ? 1 : 0;
}
