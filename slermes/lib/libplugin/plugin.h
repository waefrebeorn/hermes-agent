#ifndef LIBPLUGIN_H
#define LIBPLUGIN_H

/*
 * libplugin.h — Dynamic plugin loader for C (dlopen-based).
 * Replaces Python's dynamic plugin discovery + importlib.
 *
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   plugin_t *p = plugin_load("path/to/plugin.so");
 *   if (p) {
 *       const char *name = plugin_name(p);
 *       plugin_init_fn init = plugin_symbol(p, "plugin_init");
 *       if (init) init();
 *       plugin_unload(p);
 *   }
 *
 *   // Auto-discover plugins in a directory:
 *   plugin_registry_t *reg = plugin_discover("/home/user/.slermes/plugins");
 *   for (int i = 0; i < reg->count; i++)
 *       plugin_init(reg->plugins[i]);
 *   plugin_registry_free(reg);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Plugin handle
 * ================================================================ */

typedef struct plugin_t plugin_t;

/* Load a shared library as a plugin. Returns NULL on error. */
plugin_t *plugin_load(const char *path);

/* Get plugin name (from filename without .so, or metadata). */
const char *plugin_name(const plugin_t *p);

/* Get a symbol from the plugin. Returns NULL if not found. */
void *plugin_symbol(plugin_t *p, const char *name);

/* Check if a specific function/interface is provided. */
bool plugin_has(plugin_t *p, const char *name);

/* Unload plugin and free resources. */
void plugin_unload(plugin_t *p);

/* Get last error message. */
const char *plugin_error(void);

/* ================================================================
 *  Plugin metadata/interface convention
 * ================================================================
 *
 * A plugin should export at minimum:
 *   const char *plugin_meta_name(void);    // Plugin name
 *   const char *plugin_meta_version(void); // Version string
 *   int plugin_init(void);                 // Initialize, return 0 on success
 *   int plugin_cleanup(void);              // Cleanup on unload
 *
 * Optional interfaces (checked via plugin_has):
 *   const char *plugin_get_type(void);     // "tool", "memory", "provider", etc.
 *   void *plugin_get_interface(void);      // Interface-specific struct
 */

/* ================================================================
 *  Plugin registry (auto-discovery)
 * ================================================================ */

typedef struct {
    plugin_t **plugins;
    int        count;
    int        capacity;
} plugin_registry_t;

/* Create empty registry. */
plugin_registry_t *plugin_registry_new(void);

/* Discover plugins in a directory (scans *.so files). */
int plugin_registry_discover(plugin_registry_t *reg, const char *dir);

/* Register an already-loaded plugin. */
bool plugin_registry_add(plugin_registry_t *reg, plugin_t *p);

/* Initialize all plugins in registry (calls plugin_init on each). */
int plugin_registry_init_all(plugin_registry_t *reg);

/* Find plugin by name. Returns NULL if not found. */
plugin_t *plugin_registry_find(plugin_registry_t *reg, const char *name);

/* Free registry and all plugins. */
void plugin_registry_free(plugin_registry_t *reg);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLUGIN_H */
