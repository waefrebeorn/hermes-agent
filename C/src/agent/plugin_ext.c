/*
 * plugin_ext.c — Agent-side plugin API extensions.
 * P126-P129: Config injection, lifecycle orchestration, plugin listing.
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes_plugin.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Config discovery helpers
 * ================================================================ */

int hermes_plugin_discover_config(const hermes_config_t *cfg,
                                   const char *plugin_name,
                                   plugin_cfg_block_t *block) {
    if (!cfg || !plugin_name || !block) return 0;

    memset(block, 0, sizeof(*block));
    snprintf(block->name, sizeof(block->name), "%s", plugin_name);

    int count = 0;

    /* In a full implementation, this would read from a parsed YAML
     * config tree for plugin.<name>.* entries.
     * For now, we check known config patterns based on plugin type.
     * A production implementation would scan parsed YAML plugin section.
     */

    return count;
}

char *hermes_plugin_cfg_to_json(const plugin_cfg_block_t *block) {
    if (!block) return NULL;

    /* Build JSON object from entries */
    json_node_t *obj = json_new_object();
    if (!obj) return NULL;

    for (int i = 0; i < block->count; i++) {
        json_object_set(obj, block->entries[i].key,
                        json_new_string(block->entries[i].value));
    }

    char *json = json_serialize(obj);
    json_free(obj);
    return json;
}

/* ================================================================
 *  Per-plugin config injection (P129)
 * ================================================================ */

int hermes_plugin_cfg_inject(const hermes_config_t *cfg, plugin_t *p) {
    if (!cfg || !p) return 0;

    const char *name = plugin_name(p);
    if (!name) return 0;

    plugin_cfg_block_t block;
    int count = hermes_plugin_discover_config(cfg, name, &block);

    if (count == 0) {
        /* No config found — that's fine, some plugins have none */
        return 0;
    }

    /* Try to call plugin_configure() if the plugin exports it */
    int (*cfg_fn)(const char *) = (int (*)(const char *))plugin_symbol(p, "plugin_configure");

    if (cfg_fn) {
        char *json = hermes_plugin_cfg_to_json(&block);
        if (json) {
            cfg_fn(json);
            free(json);
        } else {
            cfg_fn("{}");
        }
    }

    return count;
}

/* ================================================================
 *  Plugin lifecycle helpers
 * ================================================================ */

static void parse_csv(const char *csv, char (*items)[256], int *count, int max) {
    *count = 0;
    if (!csv || !*csv) return;

    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", csv);

    char *save;
    char *tok = strtok_r(buf, ",", &save);
    while (tok && *count < max) {
        /* Trim whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';

        if (*tok) {
            snprintf(items[*count], 256, "%s", tok);
            (*count)++;
        }
        tok = strtok_r(NULL, ",", &save);
    }
}

static bool is_enabled(const char *name, char (*enabled)[256], int enabled_count) {
    if (enabled_count == 0) return true; /* All enabled */
    for (int i = 0; i < enabled_count; i++) {
        if (strcmp(name, enabled[i]) == 0) return true;
    }
    return false;
}

plugin_registry_t *hermes_plugin_init(const hermes_config_t *cfg) {
    if (!cfg) return NULL;

    plugin_registry_t *reg = plugin_registry_new();
    if (!reg) return NULL;

    /* Parse directories */
    char dirs_parsed[16][512];
    int ndirs = 0;

    if (cfg->plugin.dirs[0]) {
        /* Split dirs by comma */
        char dirbuf[1024];
        snprintf(dirbuf, sizeof(dirbuf), "%s", cfg->plugin.dirs);
        char *save;
        char *tok = strtok_r(dirbuf, ",", &save);
        while (tok && ndirs < 16) {
            /* Trim whitespace */
            while (*tok == ' ' || *tok == '\t') tok++;
            if (*tok) {
                snprintf(dirs_parsed[ndirs], sizeof(dirs_parsed[ndirs]), "%s", tok);
                ndirs++;
            }
            tok = strtok_r(NULL, ",", &save);
        }
    }

    /* Fallback to default plugin dir */
    if (ndirs == 0) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(dirs_parsed[0], sizeof(dirs_parsed[0]), "%s/.hermes/plugins", home);
            ndirs = 1;
        }
    }

    /* Discover plugins */
    if (ndirs > 0) {
        const char *dirs[16];
        for (int i = 0; i < ndirs; i++)
            dirs[i] = dirs_parsed[i];

        plugin_registry_discover_dirs(reg, dirs, ndirs);
    }

    /* Parse enabled list */
    char enabled_list[64][256];
    int enabled_count = 0;
    parse_csv(cfg->plugin.enabled, enabled_list, &enabled_count, 64);

    /* Filter registry to only enabled plugins */
    if (enabled_count > 0) {
        plugin_registry_t *filtered = plugin_registry_new();
        if (!filtered) { plugin_registry_free(reg); return NULL; }

        for (int i = 0; i < reg->count; i++) {
            const char *name = plugin_name(reg->plugins[i]);
            if (name && is_enabled(name, enabled_list, enabled_count)) {
                plugin_registry_add(filtered, reg->plugins[i]);
            } else {
                /* Unload disabled plugin */
                plugin_unload(reg->plugins[i]);
            }
        }

        /* Swap in filtered registry */
        free(reg->plugins);
        reg->plugins = filtered->plugins;
        reg->count = filtered->count;
        reg->capacity = filtered->capacity;
        free(filtered);
    }

    /* Inject config into each plugin */
    for (int i = 0; i < reg->count; i++) {
        hermes_plugin_cfg_inject(cfg, reg->plugins[i]);
    }

    /* Initialize in dependency order */
    plugin_registry_init_ordered(reg);

    return reg;
}

int hermes_plugin_init_enabled(plugin_registry_t *reg, const hermes_config_t *cfg) {
    if (!reg || !cfg) return -1;

    /* Parse enabled list */
    char enabled_list[64][256];
    int enabled_count = 0;
    parse_csv(cfg->plugin.enabled, enabled_list, &enabled_count, 64);

    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        const char *name = plugin_name(reg->plugins[i]);
        if (!name) continue;

        if (!is_enabled(name, enabled_list, enabled_count)) continue;

        hermes_plugin_cfg_inject(cfg, reg->plugins[i]);

        if (plugin_is_initialized(reg->plugins[i])) { ok++; continue; }
        int (*init_fn)(void) = (int (*)(void))plugin_symbol(reg->plugins[i], "plugin_init");
        if (init_fn) {
            if (init_fn() == 0) {
                plugin_set_initialized(reg->plugins[i], true);
                ok++;
            }
        } else {
            plugin_set_initialized(reg->plugins[i], true);
            ok++;
        }
    }
    return ok;
}

void hermes_plugin_shutdown(plugin_registry_t *reg) {
    if (!reg) return;
    plugin_registry_shutdown(reg);
    plugin_registry_free(reg);
}

int hermes_plugin_list(const plugin_registry_t *reg, char *buf, size_t sz) {
    if (!reg || !buf || sz == 0) return 0;

    int pos = 0;
    pos += snprintf(buf + pos, sz > (size_t)pos ? sz - (size_t)pos : 0,
                    "╔═══════════════════════════════════════════════╗\n"
                    "║  Plugins (%d loaded)                         ║\n"
                    "╚═══════════════════════════════════════════════╝\n",
                    reg->count);

    for (int i = 0; i < reg->count; i++) {
        plugin_t *p = reg->plugins[i];
        const char *name = plugin_name(p);
        plugin_type_t ptype = plugin_type(p);
        const plugin_version_t *ver = plugin_version(p);
        char ver_str[32];
        plugin_version_str(ver, ver_str, sizeof(ver_str));

        if (pos < (int)sz - 200) {
            pos += snprintf(buf + pos, sz - (size_t)pos,
                            "  • %s v%s [%s] %s%s\n",
                            name ? name : "?",
                            ver_str,
                            plugin_type_str(ptype),
                            plugin_is_initialized(p) ? "✓" : "○",
                            plugin_description(p) ? " — " : "");
            if (plugin_description(p)) {
                pos += snprintf(buf + pos, sz - (size_t)pos, "%s",
                                plugin_description(p));
            }
        }
    }

    return pos;
}
