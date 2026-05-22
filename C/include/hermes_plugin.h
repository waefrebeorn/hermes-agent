/**
 * @defgroup hermes_plugin Plugin System
 * @brief Dynamic .so plugin loading and lifecycle.
 *
 * 
Plugin interface definition (metadata, init, dispatch,
cleanup), discovery via directory scan, and hot-reload
support.
 *
 * @{
 */
#ifndef HERMES_PLUGIN_H
#define HERMES_PLUGIN_H

/*
 * hermes_plugin.h — Enhanced plugin API for Hermes C agent.
 * P126-P129: Typed plugin API, discovery, lifecycle, config injection.
 *
 * This header exposes the higher-level plugin integration functions
 * that the agent uses to discover, configure, load, and manage plugins.
 */

#include "hermes.h"
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Plugin config injection (P129)
 * ================================================================
 *
 * For each loaded plugin named "foo", the agent reads config values
 * from plugin.foo.* in hermes_config_t and passes them to the plugin
 * via a plugin_configure() call or by setting environment variables.
 *
 * Config keys are mapped as: plugin.<name>.<key>
 * e.g. plugin.honcho-memory.api_key
 *
 * The plugin_cfg_inject function:
 *   - Scans hermes_config_t for "plugin.<name>.*" entries
 *   - Creates a JSON config string
 *   - Calls plugin's plugin_configure(const char *config_json) if exported
 */

/* Maximum config entries per plugin */
#define PLUGIN_CFG_MAX_ENTRIES 64
#define PLUGIN_CFG_KEY_MAX     128
#define PLUGIN_CFG_VAL_MAX     1024

/* A single config key-value pair for a plugin */
typedef struct {
    char key[PLUGIN_CFG_KEY_MAX];
    char value[PLUGIN_CFG_VAL_MAX];
} plugin_cfg_entry_t;

/* Config block for a specific plugin */
typedef struct {
    char name[256];
    plugin_cfg_entry_t entries[PLUGIN_CFG_MAX_ENTRIES];
    int count;
} plugin_cfg_block_t;

/* ================================================================
 *  Plugin lifecycle helpers
 * ================================================================ */

/**
 * Initialize the plugin system from config.
 * Steps:
 *   1. Parse plugin directories from cfg->plugin.dirs
 *   2. Discover .so files in those directories
 *   3. Filter by cfg->plugin.enabled list (if non-empty)
 *   4. Inject per-plugin config from hermes_config_t
 *   5. Initialize plugins in dependency order
 *
 * Returns a plugin_registry_t* (caller must free with plugin_registry_free).
 * Returns NULL on error.
 */
plugin_registry_t *hermes_plugin_init(const hermes_config_t *cfg);

/**
 * Inject per-plugin configuration from hermes_config_t.
 * For plugin named "foo", scans config for "plugin.foo.*" keys.
 * Then calls plugin's plugin_configure() if exported.
 *
 * Returns number of config values injected.
 */
int hermes_plugin_cfg_inject(const hermes_config_t *cfg, plugin_t *p);

/**
 * Discover per-plugin config from hermes_config_t for a named plugin.
 * Populates block with matching entries.
 * Returns number of entries found.
 */
int hermes_plugin_discover_config(const hermes_config_t *cfg,
                                  const char *plugin_name,
                                  plugin_cfg_block_t *block);

/**
 * Build a JSON config string from a config block.
 * Returns malloc'd string, caller must free.
 */
char *hermes_plugin_cfg_to_json(const plugin_cfg_block_t *block);

/**
 * Initialize only the enabled plugins from a registry.
 * Filters plugins against cfg->plugin.enabled (comma-separated names).
 * If enabled is empty, all plugins are initialized.
 * Returns number of initialized plugins.
 */
int hermes_plugin_init_enabled(plugin_registry_t *reg, const hermes_config_t *cfg);

/**
 * Shutdown all plugins and free the registry.
 * Convenience wrapper around plugin_registry_shutdown + plugin_registry_free.
 */
void hermes_plugin_shutdown(plugin_registry_t *reg);

/**
 * List loaded plugins to a buffer (for /plugins command).
 * Returns number of chars written.
 */
int hermes_plugin_list(const plugin_registry_t *reg, char *buf, size_t sz);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_plugin group */
#endif /* HERMES_PLUGIN_H */
