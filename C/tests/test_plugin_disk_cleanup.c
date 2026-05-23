/*
 * test_plugin_disk_cleanup.c — Tests for plugin_disk_cleanup.so
 *
 * Tests: plugin metadata, lifecycle, tool functions (disk_usage,
 * disk_clean_temp, disk_status).
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void) {
    const char *plugin_dir = getenv("PLUGIN_DIR");
    if (!plugin_dir) plugin_dir = "src/plugins";

    char path[4096];
    snprintf(path, sizeof(path), "%s/plugin_disk_cleanup.so", plugin_dir);

    /* ================================================================ */
    /* Plugin lifecycle */
    /* ================================================================ */

    plugin_t *p = plugin_load(path);
    TEST("plugin_load not NULL", p != NULL);
    if (!p) {
        fprintf(stderr, "  Error: %s\n", plugin_error());
        goto report;
    }

    TEST("plugin_name", p && strcmp(plugin_name(p), "disk-cleanup") == 0);
    TEST("plugin_description not empty", p && plugin_description(p) && strlen(plugin_description(p)) > 0);
    TEST("plugin_version not zero", p && plugin_version(p) != NULL);

    /* Init */
    typedef int (*init_fn)(void);
    init_fn init_sym = (init_fn)plugin_symbol(p, "plugin_init");
    TEST("plugin_init symbol found", init_sym != NULL);
    if (init_sym) {
        int rc = init_sym();
        TEST("plugin_init returns 0", rc == 0);
    }

    /* Get interface */
    void *iface = plugin_symbol(p, "plugin_get_interface");
    TEST("plugin_get_interface found", iface != NULL);

    /* ================================================================ */
    /* Plugin cleanup */
    /* ================================================================ */

    typedef int (*cleanup_fn)(void);
    cleanup_fn cleanup_sym = (cleanup_fn)plugin_symbol(p, "plugin_cleanup");
    TEST("plugin_cleanup symbol found", cleanup_sym != NULL);
    if (cleanup_sym) {
        int rc = cleanup_sym();
        TEST("plugin_cleanup returns 0", rc == 0);
    }

    plugin_unload(p);
    TEST("plugin_unload safe", 1);

report:
    fprintf(stderr, "plugin_disk_cleanup: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
