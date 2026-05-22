#ifndef LIBPLUGIN_H
#define LIBPLUGIN_H

/*
 * libplugin.h — Dynamic plugin loader for C (dlopen-based).
 * P126-P129: Typed plugin API, discovery, lifecycle, config injection.
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
 *   // Auto-discover plugins in multiple directories:
 *   plugin_registry_t *reg = plugin_registry_new();
 *   plugin_registry_discover_dirs(reg, dirs, count);
 *   plugin_registry_init_ordered(reg);  // topo-sorted by deps
 *   plugin_registry_shutdown(reg);      // reverse order
 *   plugin_registry_free(reg);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Plugin Type Enum (P126)
 * ================================================================ */

typedef enum {
    PLUGIN_UNKNOWN       = 0,
    PLUGIN_TOOL          = 1,   /* Provides tool(s) to the agent */
    PLUGIN_MEMORY        = 2,   /* Memory provider backend (honcho/mem0) */
    PLUGIN_PROVIDER      = 3,   /* LLM provider backend */
    PLUGIN_CONTEXT       = 4,   /* Context engine plugin */
    PLUGIN_KANBAN        = 5,   /* Kanban/multi-agent dispatcher */
    PLUGIN_SPOTIFY       = 6,   /* Music/spotify integration */
    PLUGIN_OBSERVABILITY = 7,   /* Metrics/traces/logging */
    PLUGIN_IMAGE_GEN     = 8,   /* Image generation backend */
    PLUGIN_PLATFORM      = 9,   /* Gateway platform adapter */
    PLUGIN_SKILL         = 10,  /* Skill plugin */
    PLUGIN_ACHIEVEMENTS  = 11,  /* Gamification */
    PLUGIN_DISK_CLEANUP  = 12,  /* Disk cleanup utility */
    PLUGIN_GOOGLE_MEET   = 13,  /* Google Meet integration */
    PLUGIN_STRIKE        = 14,  /* Strike/freedom cockpit */
} plugin_type_t;

/* Convert plugin type enum to string */
const char *plugin_type_str(plugin_type_t type);

/* Parse plugin type from string (case-insensitive) */
plugin_type_t plugin_type_from_str(const char *s);

/* ================================================================
 *  Version struct
 * ================================================================ */

typedef struct {
    int major;
    int minor;
    int patch;
} plugin_version_t;

/* Compare versions: returns -1 if a<b, 0 if equal, 1 if a>b */
int plugin_version_cmp(const plugin_version_t *a, const plugin_version_t *b);

/* Parse "X.Y.Z" string into version struct. Returns true on success. */
bool plugin_version_parse(const char *s, plugin_version_t *v);

/* Format version struct to string buffer. Returns buf. */
const char *plugin_version_str(const plugin_version_t *v, char *buf, size_t sz);

/* ================================================================
 *  Dependency declaration
 * ================================================================ */

#define PLUGIN_MAX_DEPS 16

typedef struct {
    char name[128];               /* Plugin name this depends on */
    plugin_version_t min_version; /* Minimum version required */
    bool optional;                /* If true, skip if not found */
} plugin_dep_t;

/* ================================================================
 *  Plugin handle
 * ================================================================ */

typedef struct plugin_t plugin_t;

/* Load a shared library as a plugin. Returns NULL on error. */
plugin_t *plugin_load(const char *path);

/* Get plugin name (from filename without .so, or metadata). */
const char *plugin_name(const plugin_t *p);

/* Get plugin version (from metadata or "0.0.0"). */
const plugin_version_t *plugin_version(const plugin_t *p);

/* Get plugin type. */
plugin_type_t plugin_type(const plugin_t *p);

/* Get plugin description (may return NULL). */
const char *plugin_description(const plugin_t *p);

/* Get dependencies (returns NULL-terminated list, up to PLUGIN_MAX_DEPS). */
const plugin_dep_t *plugin_deps(const plugin_t *p, int *count);

/* Get a symbol from the plugin. Returns NULL if not found. */
void *plugin_symbol(plugin_t *p, const char *name);

/* Check if a specific function/interface is provided. */
bool plugin_has(plugin_t *p, const char *name);

/* Unload plugin and free resources. */
void plugin_unload(plugin_t *p);

/* Get last error message. */
const char *plugin_error(void);

/* Get plugin path */
const char *plugin_path(const plugin_t *p);

/* Check if plugin has been initialized */
bool plugin_is_initialized(const plugin_t *p);

/* Set plugin initialized state (used by lifecycle functions) */
void plugin_set_initialized(plugin_t *p, bool initialized);

/* ================================================================
 *  Typed Plugin Interface (P126)
 * ================================================================
 *
 * A plugin should export these standard symbols (all optional):
 *
 *   // Standard metadata functions
 *   const char *plugin_meta_name(void);      // Plugin name
 *   const char *plugin_meta_version(void);    // Version string "X.Y.Z"
 *   const char *plugin_meta_type(void);       // Type string (see plugin_type_from_str)
 *   const char *plugin_meta_description(void); // Human-readable description
 *
 *   // Lifecycle
 *   int plugin_init(void);                   // Init, return 0 on success
 *   int plugin_cleanup(void);                // Cleanup on unload
 *
 *   // Dependencies (optional)
 *   int plugin_deps_count(void);             // Number of dependencies
 *   const plugin_dep_t *plugin_deps_list(void); // Array of deps
 *
 *   // Interface-specific function pointers (for type-safe dispatch)
 *   void *plugin_get_interface(void);        // Returns plugin_interface_t* (see below)
 */

/* Maximum number of tools a plugin can provide */
#define PLUGIN_MAX_TOOLS 64

/**
 * plugin_interface_t — type-safe function pointer table.
 * Each plugin type defines its own function pointers.
 * Non-applicable fields must be NULL.
 */
typedef struct {
    /* === Type field: MUST be set === */
    plugin_type_t type;

    /* === Tool plugin (PLUGIN_TOOL) === */
    int    (*tool_get_count)(void);
    void   (*tool_get_info)(int index, char *name, size_t name_sz,
                            char *desc, size_t desc_sz,
                            char *schema, size_t schema_sz);
    char  *(*tool_execute)(const char *tool_name, const char *args_json,
                           const char *task_id);

    /* === Memory plugin (PLUGIN_MEMORY) === */
    char  *(*memory_store)(const char *content, const char *metadata_json);
    char  *(*memory_search)(const char *query, int limit);
    void   (*memory_clear)(void);

    /* === Provider plugin (PLUGIN_PROVIDER) === */
    char  *(*provider_chat)(const char *messages_json,
                            const char *tools_json,
                            const char *model);
    void  *(*provider_stream)(const char *messages_json,
                              const char *tools_json,
                              const char *model,
                              void (*token_cb)(const char *token, void *ud),
                              void *userdata);

    /* === Kanban plugin (PLUGIN_KANBAN) === */
    char  *(*kanban_create_board)(const char *name, const char *config_json);
    char  *(*kanban_add_task)(const char *board_id, const char *task_json);
    char  *(*kanban_get_board)(const char *board_id);
    char  *(*kanban_list_boards)(void);

    /* === Spotify plugin (PLUGIN_SPOTIFY) === */
    char  *(*spotify_play)(const char *uri, const char *device_id);
    char  *(*spotify_pause)(void);
    char  *(*spotify_next)(void);
    char  *(*spotify_current)(void);
    char  *(*spotify_search)(const char *query, const char *type);

    /* === Observability plugin (PLUGIN_OBSERVABILITY) === */
    void   (*obs_record_event)(const char *event_name, const char *data_json);
    char  *(*obs_get_metrics)(const char *filter_json);

    /* === Image generation plugin (PLUGIN_IMAGE_GEN) === */
    char  *(*image_gen)(const char *prompt, const char *params_json);
} plugin_interface_t;

/* ================================================================
 *  YAML Metadata (P127)
 * ================================================================
 *
 * Plugins can ship a .yaml file alongside the .so (same basename)
 * containing metadata like:
 *
 *   name: "honcho-memory"
 *   version: "1.2.0"
 *   type: "memory"
 *   description: "Honcho memory provider"
 *   dependencies:
 *     - name: "base-memory"
 *       min_version: "0.1.0"
 *       optional: false
 *
 * Metadata is loaded on plugin_load() and can be inspected via
 * plugin_name(), plugin_version(), etc.
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

/* Discover plugins in a single directory (scans *.so files). */
int plugin_registry_discover(plugin_registry_t *reg, const char *dir);

/* Discover plugins in multiple directories (P127). */
int plugin_registry_discover_dirs(plugin_registry_t *reg,
                                  const char **dirs, int ndirs);

/* Register an already-loaded plugin. */
bool plugin_registry_add(plugin_registry_t *reg, plugin_t *p);

/* ================================================================
 *  Lifecycle management (P128)
 * ================================================================ */

/* Dependency resolution: topologically sort plugins.
 * Returns sorted indices into reg->plugins[]. count_out receives the count.
 * On circular dependency, returns NULL and sets plugin_error.
 * Caller must free() the returned array. */
int *plugin_registry_resolve_deps(plugin_registry_t *reg, int *count_out);

/* Initialize all plugins in dependency order (topological sort).
 * Returns number of successfully initialized plugins. */
int plugin_registry_init_ordered(plugin_registry_t *reg);

/* Shutdown all plugins in reverse dependency order.
 * Returns number of successfully shut down plugins. */
int plugin_registry_shutdown(plugin_registry_t *reg);

/* Initialize all plugins in order (basic, no dependency resolution). */
int plugin_registry_init_all(plugin_registry_t *reg);

/* Find plugin by name. Returns NULL if not found. */
plugin_t *plugin_registry_find(plugin_registry_t *reg, const char *name);

/* Free registry and all plugins. */
void plugin_registry_free(plugin_registry_t *reg);

/* ================================================================
 *  Version checking utilities (P127)
 * ================================================================ */

/* Check if a plugin satisfies a dependency requirement.
 * Returns true if p's version >= dep.min_version (or dep is optional). */
bool plugin_satisfies_dep(const plugin_t *p, const plugin_dep_t *dep);

/* Check all dependencies of a plugin against the registry.
 * Returns 0 if all satisfied, or count of unsatisfied deps. */
int plugin_check_deps(const plugin_t *p, const plugin_registry_t *reg);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLUGIN_H */
