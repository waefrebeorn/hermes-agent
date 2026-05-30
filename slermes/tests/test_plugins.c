/*
 * test_plugins.c — Quick verification test for plugin system (P126-P129).
 *
 * Build:
 *   gcc -O2 -g -I include -I lib/libplugin test_plugins.c lib/libplugin/plugin.o -o /tmp/test_plugins -ldl -lm
 *
 * Run:
 *   /tmp/test_plugins
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plugin.h"

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        printf("  ✓ %s\n", name); \
        passed++; \
    } else { \
        printf("  ✗ %s (FAILED)\n", name); \
        failed++; \
    } \
} while(0)

int main(void) {
    printf("=== Plugin System Tests (P126-P129) ===\n\n");

    /* --- Test 1: Version utilities --- */
    printf("[P126] Version parsing & comparison:\n");

    plugin_version_t v1, v2;
    TEST("parse 1.0.0", plugin_version_parse("1.0.0", &v1) && v1.major == 1 && v1.minor == 0 && v1.patch == 0);
    TEST("parse 0.3.0", plugin_version_parse("0.3.0", &v2) && v2.major == 0 && v2.minor == 3 && v2.patch == 0);
    TEST("cmp 1.0.0 > 0.3.0", plugin_version_cmp(&v1, &v2) > 0);
    TEST("cmp 0.3.0 < 1.0.0", plugin_version_cmp(&v2, &v1) < 0);
    TEST("cmp equal", plugin_version_cmp(&v1, &v1) == 0);

    char buf[32];
    TEST("version str", strcmp(plugin_version_str(&v1, buf, sizeof(buf)), "1.0.0") == 0);

    /* --- Test 2: Type enum --- */
    printf("\n[P126] Plugin type enum:\n");

    TEST("PLUGIN_TOOL str", strcmp(plugin_type_str(PLUGIN_TOOL), "tool") == 0);
    TEST("PLUGIN_MEMORY str", strcmp(plugin_type_str(PLUGIN_MEMORY), "memory") == 0);
    TEST("PLUGIN_KANBAN str", strcmp(plugin_type_str(PLUGIN_KANBAN), "kanban") == 0);
    TEST("PLUGIN_SPOTIFY str", strcmp(plugin_type_str(PLUGIN_SPOTIFY), "spotify") == 0);

    TEST("from_str 'memory'", plugin_type_from_str("memory") == PLUGIN_MEMORY);
    TEST("from_str 'MEMORY'", plugin_type_from_str("MEMORY") == PLUGIN_MEMORY);
    TEST("from_str 'kanban'", plugin_type_from_str("kanban") == PLUGIN_KANBAN);
    TEST("from_str 'unknown'", plugin_type_from_str("bogus") == PLUGIN_UNKNOWN);

    /* --- Test 3: Registry creation --- */
    printf("\n[P127] Registry operations:\n");

    plugin_registry_t *reg = plugin_registry_new();
    TEST("registry created", reg != NULL);
    TEST("registry empty", reg->count == 0);

    /* --- Test 4: Load example plugins from ~/.hermes/plugins/ --- */
    printf("\n[P127] Plugin discovery & loading:\n");

    const char *home = getenv("HOME");
    char plugin_dir[1024];
    snprintf(plugin_dir, sizeof(plugin_dir), "%s/.hermes/plugins", home ? home : "/tmp");

    int n = plugin_registry_discover(reg, plugin_dir);
    TEST("discovered plugins", n > 0);
    printf("  Found %d plugin(s) in %s\n", n, plugin_dir);

    /* --- Test 5: Plugin metadata inspection --- */
    printf("\n[P126] Plugin metadata:\n");

    for (int i = 0; i < reg->count; i++) {
        plugin_t *p = reg->plugins[i];
        const char *name = plugin_name(p);
        plugin_type_t ptype = plugin_type(p);
        const plugin_version_t *ver = plugin_version(p);
        char ver_str[32];
        plugin_version_str(ver, ver_str, sizeof(ver_str));
        const char *desc = plugin_description(p);

        printf("  Plugin %d:\n", i);
        printf("    name:    %s\n", name ? name : "(null)");
        printf("    type:    %s (%d)\n", plugin_type_str(ptype), ptype);
        printf("    version: %s\n", ver_str);
        printf("    desc:    %s\n", desc ? desc : "(none)");
        printf("    path:    %s\n", plugin_path(p));

        TEST("name non-null", name != NULL);

        int ndeps;
        const plugin_dep_t *deps = plugin_deps(p, &ndeps);
        printf("    deps:    %d\n", ndeps);
        for (int d = 0; d < ndeps; d++) {
            printf("      - %s (min v%d.%d.%d, %s)\n",
                   deps[d].name,
                   deps[d].min_version.major,
                   deps[d].min_version.minor,
                   deps[d].min_version.patch,
                   deps[d].optional ? "optional" : "required");
        }

        /* Test exported symbols */
        TEST("has plugin_init", plugin_has(p, "plugin_init"));
        TEST("has plugin_cleanup", plugin_has(p, "plugin_cleanup"));

        /* Test interface pointer */
        void *iface_ptr = plugin_symbol(p, "plugin_get_interface");
        TEST("has plugin_get_interface", iface_ptr != NULL);

        if (iface_ptr) {
            /* plugin_get_interface returns plugin_interface_t* directly */
            plugin_interface_t *(*get_iface)(void) = (plugin_interface_t *(*)(void))iface_ptr;
            plugin_interface_t *pi = get_iface();
            TEST("interface type matches plugin type", pi != NULL && pi->type == ptype);
            printf("    iface type: %s\n", plugin_type_str(pi->type));
        }
    }

    /* --- Test 6: Dependency resolution --- */
    printf("\n[P128] Dependency resolution:\n");

    int sorted_count = 0;
    int *order = plugin_registry_resolve_deps(reg, &sorted_count);
    TEST("dependency resolution completed", order != NULL);
    if (order) {
        TEST("all plugins sorted", sorted_count == reg->count);
        printf("  Order: ");
        for (int i = 0; i < sorted_count; i++) {
            printf("%s%s", i > 0 ? " → " : "", plugin_name(reg->plugins[order[i]]));
        }
        printf("\n");
        free(order);
    }

    /* --- Test 7: Lifecycle --- */
    printf("\n[P128] Lifecycle:\n");

    int inited = plugin_registry_init_ordered(reg);
    TEST("init_ordered succeeded", inited > 0);
    printf("  Initialized: %d/%d\n", inited, reg->count);

    /* Verify each plugin is marked as initialized */
    int all_inited = 1;
    for (int i = 0; i < reg->count; i++) {
        if (!plugin_is_initialized(reg->plugins[i])) {
            all_inited = 0;
            break;
        }
    }
    TEST("all plugins marked initialized", all_inited);

    int shut = plugin_registry_shutdown(reg);
    TEST("shutdown succeeded", shut > 0);
    printf("  Shut down: %d/%d\n", shut, reg->count);

    /* Verify plugins are marked as not initialized after shutdown */
    int all_shut = 1;
    for (int i = 0; i < reg->count; i++) {
        if (plugin_is_initialized(reg->plugins[i])) {
            all_shut = 0;
            break;
        }
    }
    TEST("all plugins marked uninitialized after shutdown", all_shut);

    /* --- Summary --- */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);

    plugin_registry_free(reg);
    return failed > 0 ? 1 : 0;
}
