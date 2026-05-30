/*
 * plugin.c — Dynamic plugin loader for C (dlopen-based).
 * P126-P129: Typed plugin API, enhanced discovery, lifecycle, config injection.
 *
 * MIT License — WuBu Hermes Project
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* YAML metadata file extension */
#define PLUGIN_META_EXT ".yaml"
#define PLUGIN_META_EXT2 ".yml"

static char last_error[1024] = "";

static void set_error(const char *msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg ? msg : "unknown error");
}

/* Actually implement with variadic macro support */
#define set_error_fmt(fmt, ...) \
    snprintf(last_error, sizeof(last_error), fmt, ##__VA_ARGS__)

static void set_error_dl(void) {
    const char *dlerr = dlerror();
    snprintf(last_error, sizeof(last_error), "%s", dlerr ? dlerr : "dl error");
}

const char *plugin_error(void) {
    return last_error;
}

/* ================================================================
 *  Version utilities
 * ================================================================ */

int plugin_version_cmp(const plugin_version_t *a, const plugin_version_t *b) {
    if (!a || !b) return 0;
    if (a->major != b->major) return a->major > b->major ? 1 : -1;
    if (a->minor != b->minor) return a->minor > b->minor ? 1 : -1;
    if (a->patch != b->patch) return a->patch > b->patch ? 1 : -1;
    return 0;
}

bool plugin_version_parse(const char *s, plugin_version_t *v) {
    if (!s || !v) return false;
    v->major = 0; v->minor = 0; v->patch = 0;
    int n = sscanf(s, "%d.%d.%d", &v->major, &v->minor, &v->patch);
    return n >= 1;
}

const char *plugin_version_str(const plugin_version_t *v, char *buf, size_t sz) {
    if (!v || !buf || sz == 0) return buf;
    snprintf(buf, sz, "%d.%d.%d", v->major, v->minor, v->patch);
    return buf;
}

/* ================================================================
 *  Type enum helpers
 * ================================================================ */

const char *plugin_type_str(plugin_type_t type) {
    switch (type) {
        case PLUGIN_TOOL:          return "tool";
        case PLUGIN_MEMORY:        return "memory";
        case PLUGIN_PROVIDER:      return "provider";
        case PLUGIN_CONTEXT:       return "context";
        case PLUGIN_KANBAN:        return "kanban";
        case PLUGIN_SPOTIFY:       return "spotify";
        case PLUGIN_OBSERVABILITY: return "observability";
        case PLUGIN_IMAGE_GEN:     return "image_gen";
        case PLUGIN_PLATFORM:      return "platform";
        case PLUGIN_SKILL:         return "skill";
        case PLUGIN_ACHIEVEMENTS:  return "achievements";
        case PLUGIN_DISK_CLEANUP:  return "disk_cleanup";
        case PLUGIN_GOOGLE_MEET:   return "google_meet";
        case PLUGIN_STRIKE:        return "strike";
        default:                   return "unknown";
    }
}

plugin_type_t plugin_type_from_str(const char *s) {
    if (!s) return PLUGIN_UNKNOWN;
    /* Case-insensitive comparison */
    char lower[64];
    size_t i;
    for (i = 0; s[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    lower[i] = '\0';

    if (strcmp(lower, "tool") == 0)            return PLUGIN_TOOL;
    if (strcmp(lower, "memory") == 0)           return PLUGIN_MEMORY;
    if (strcmp(lower, "provider") == 0)         return PLUGIN_PROVIDER;
    if (strcmp(lower, "context") == 0)          return PLUGIN_CONTEXT;
    if (strcmp(lower, "kanban") == 0)           return PLUGIN_KANBAN;
    if (strcmp(lower, "spotify") == 0)          return PLUGIN_SPOTIFY;
    if (strcmp(lower, "observability") == 0)    return PLUGIN_OBSERVABILITY;
    if (strcmp(lower, "image_gen") == 0)        return PLUGIN_IMAGE_GEN;
    if (strcmp(lower, "platform") == 0)         return PLUGIN_PLATFORM;
    if (strcmp(lower, "skill") == 0)            return PLUGIN_SKILL;
    if (strcmp(lower, "achievements") == 0)     return PLUGIN_ACHIEVEMENTS;
    if (strcmp(lower, "disk_cleanup") == 0)     return PLUGIN_DISK_CLEANUP;
    if (strcmp(lower, "google_meet") == 0)      return PLUGIN_GOOGLE_MEET;
    if (strcmp(lower, "strike") == 0)           return PLUGIN_STRIKE;
    return PLUGIN_UNKNOWN;
}

/* ================================================================
 *  Simple YAML parser (minimal, for metadata files)
 * ================================================================
 * Only handles the subset needed for plugin metadata:
 *   key: value
 *   list:
 *     - key: value
 */

/* Read a text file into a malloc'd buffer. Caller must free. */
static char *read_file_content(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Get size */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t nread = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[nread] = '\0';
    return buf;
}

/* Find value for a key at top level. Returns malloc'd string or NULL. */
static char *yaml_get_value(const char *yaml, const char *key) {
    if (!yaml || !key) return NULL;

    size_t klen = strlen(key);
    const char *p = yaml;

    while (*p) {
        /* Skip leading whitespace (but not indentation, we want top-level) */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') { /* comment, skip to end of line */
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Check if this line starts with our key */
        if (strncmp(p, key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            /* Skip whitespace after colon */
            while (*p == ' ' || *p == '\t') p++;
            /* Find end of line */
            const char *end = p;
            while (*end && *end != '\n' && *end != '\r') end++;
            /* Check if this is a simple value (not a list) */
            if (*p != '\n' && *p != '\r' && *p != '\0') {
                size_t len = (size_t)(end - p);
                /* Trim trailing whitespace */
                while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
                if (len > 0) {
                    char *val = (char *)malloc(len + 1);
                    if (val) {
                        memcpy(val, p, len);
                        val[len] = '\0';
                    }
                    return val;
                }
            }
        }

        /* Skip to next line */
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    return NULL;
}

/* Parse YAML sub-items under a list entry.
 * Given text like:
 *   - name: "foo"
 *     version: "1.0"
 * Returns the sub-item value for a given sub-key within the entry text. */
static char *yaml_entry_get_value(const char *entry_text, const char *sub_key) {
    if (!entry_text || !sub_key) return NULL;

    size_t klen = strlen(sub_key);
    const char *p = entry_text;

    while (*p) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, sub_key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            while (*p == ' ' || *p == '\t') p++;
            const char *end = p;
            while (*end && *end != '\n' && *end != '\r') end++;
            size_t len = (size_t)(end - p);
            /* Trim trailing space/comments */
            while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
            if (len > 0) {
                char *val = (char *)malloc(len + 1);
                if (val) {
                    memcpy(val, p, len);
                    val[len] = '\0';
                }
                return val;
            }
            return NULL;
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    return NULL;
}

/* Parse dependencies from YAML metadata. Returns number parsed. */
static int parse_yaml_deps(const char *yaml, plugin_dep_t *deps, int max) {
    /* Find the dependencies section first */
    const char *deps_key = "dependencies";
    size_t klen = strlen(deps_key);
    const char *p = yaml;
    int count = 0;

    /* Find the dependencies: key */
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r') { p++; continue; }
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, deps_key, klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            while (*p && *p != '\n') p++;
            if (*p) p++;
            break;
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }

    if (!*p) return 0;

    /* Now parse list items */
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (*p == '-') {
            if (count >= max) break;
            p++;
            /* Collect the rest of this entry (multiline) */
            char entry[1024];
            size_t epos = 0;
            while (*p && *p != '\n') {
                if (epos < sizeof(entry) - 1) entry[epos++] = *p;
                p++;
            }
            entry[epos] = '\0';
            if (*p) p++;

            /* Parse sub-keys from entry */
            char *name_v = yaml_entry_get_value(entry, "name");
            if (name_v) {
                snprintf(deps[count].name, sizeof(deps[count].name), "%s", name_v);
                free(name_v);
            }

            char *ver_v = yaml_entry_get_value(entry, "min_version");
            if (ver_v) {
                plugin_version_parse(ver_v, &deps[count].min_version);
                free(ver_v);
            }

            char *opt_v = yaml_entry_get_value(entry, "optional");
            deps[count].optional = (opt_v && (strcmp(opt_v, "true") == 0 ||
                                               strcmp(opt_v, "yes") == 0 ||
                                               strcmp(opt_v, "1") == 0));
            free(opt_v);

            if (deps[count].name[0]) count++;
        } else {
            break; /* Not a list item, we're done */
        }
    }

    return count;
}

/* ================================================================
 *  Plugin struct (extended)
 * ================================================================ */

struct plugin_t {
    void           *handle;       /* dlopen handle */
    char            path[1024];   /* full path */
    char            name[256];    /* derived name */
    bool            loaded;       /* dlopen succeeded */
    /* P126: Extended metadata */
    plugin_version_t version;     /* parsed version */
    plugin_type_t    type;        /* plugin type */
    char             description[512]; /* human-readable description */
    plugin_dep_t     deps[PLUGIN_MAX_DEPS]; /* dependencies */
    int              ndeps;       /* number of deps */
    /* Cached interface pointer */
    plugin_interface_t *iface;    /* cached interface pointer */
    bool             iface_cached;
    /* Init state */
    bool             initialized;
};

/* Extract name from path: "path/to/plugin.so" → "plugin" */
static const char *name_from_path(const char *path, char *buf, size_t cap) {
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    snprintf(buf, cap, "%s", p);
    /* Strip .so extension */
    char *dot = strstr(buf, ".so");
    if (dot) *dot = '\0';
    return buf;
}

/* Build metadata file path from .so path */
static void metadata_path(const char *so_path, char *buf, size_t cap) {
    snprintf(buf, cap, "%s", so_path);
    /* Replace .so with .yaml */
    char *dot = strstr(buf, ".so");
    if (dot) {
        size_t remain = cap - (size_t)(dot - buf);
        snprintf(dot, remain, "%s", PLUGIN_META_EXT);
    } else {
        strncat(buf, PLUGIN_META_EXT, cap - strlen(buf) - 1);
    }
}

/* Try to load metadata from a YAML file adjacent to the .so */
static bool load_metadata(plugin_t *p) {
    char meta_path[2048];
    metadata_path(p->path, meta_path, sizeof(meta_path));

    /* Try .yaml */
    char *yaml = read_file_content(meta_path);
    if (!yaml) {
        /* Try .yml */
        char *dot = strstr(meta_path, PLUGIN_META_EXT);
        if (dot) {
            snprintf(dot, sizeof(meta_path) - (size_t)(dot - meta_path), "%s", PLUGIN_META_EXT2);
            yaml = read_file_content(meta_path);
        }
    }

    if (!yaml) return false;

    /* Parse fields */
    char *val;

    /* name: override from filename */
    val = yaml_get_value(yaml, "name");
    if (val) {
        snprintf(p->name, sizeof(p->name), "%s", val);
        free(val);
    }

    /* version: */
    val = yaml_get_value(yaml, "version");
    if (val) {
        plugin_version_parse(val, &p->version);
        free(val);
    }

    /* type: */
    val = yaml_get_value(yaml, "type");
    if (val) {
        p->type = plugin_type_from_str(val);
        free(val);
    }

    /* description: */
    val = yaml_get_value(yaml, "description");
    if (val) {
        snprintf(p->description, sizeof(p->description), "%s", val);
        free(val);
    }

    /* Dependencies */
    p->ndeps = parse_yaml_deps(yaml, p->deps, PLUGIN_MAX_DEPS);

    free(yaml);
    return true;
}

/* Probe the loaded .so for exported metadata symbols (runtime overrides YAML) */
static void probe_symbols(plugin_t *p) {
    if (!p || !p->handle) return;

    /* Try exported metadata functions (these override YAML) */
    const char *(*name_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_name");
    if (name_fn) {
        const char *n = name_fn();
        if (n) snprintf(p->name, sizeof(p->name), "%s", n);
    }

    const char *(*ver_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_version");
    if (ver_fn) {
        const char *v = ver_fn();
        if (v) plugin_version_parse(v, &p->version);
    }

    const char *(*type_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_type");
    if (type_fn) {
        const char *t = type_fn();
        if (t) p->type = plugin_type_from_str(t);
    }

    const char *(*desc_fn)(void) = (const char *(*)(void))dlsym(p->handle, "plugin_meta_description");
    if (desc_fn) {
        const char *d = desc_fn();
        if (d) snprintf(p->description, sizeof(p->description), "%s", d);
    }

    /* Dependencies from .so */
    int (*deps_count_fn)(void) = (int (*)(void))dlsym(p->handle, "plugin_deps_count");
    const plugin_dep_t *(*deps_list_fn)(void) = (const plugin_dep_t *(*)(void))dlsym(p->handle, "plugin_deps_list");

    if (deps_count_fn && deps_list_fn) {
        int dc = deps_count_fn();
        if (dc > 0 && dc <= PLUGIN_MAX_DEPS) {
            const plugin_dep_t *dl = deps_list_fn();
            if (dl) {
                memcpy(p->deps, dl, (size_t)dc * sizeof(plugin_dep_t));
                p->ndeps = dc;
            }
        }
    }

    /* Cache interface pointer */
    void *(*get_iface)(void) = (void *(*)(void))dlsym(p->handle, "plugin_get_interface");
    if (get_iface) {
        p->iface = (plugin_interface_t *)get_iface();
        p->iface_cached = (p->iface != NULL);
    }
}

/* ================================================================
 *  Load/Unload
 * ================================================================ */

plugin_t *plugin_load(const char *path) {
    if (!path || !*path) {
        set_error("NULL path");
        return NULL;
    }

    plugin_t *p = (plugin_t *)calloc(1, sizeof(plugin_t));
    if (!p) { set_error("OOM"); return NULL; }

    snprintf(p->path, sizeof(p->path), "%s", path);
    name_from_path(path, p->name, sizeof(p->name));

    /* Set defaults */
    p->version = (plugin_version_t){0, 0, 0};
    p->type = PLUGIN_UNKNOWN;

    /* Open with RTLD_NOW to catch unresolved symbols immediately */
    p->handle = dlopen(path, RTLD_NOW);
    if (!p->handle) {
        set_error_dl();
        free(p);
        return NULL;
    }

    p->loaded = true;

    /* Load YAML metadata */
    load_metadata(p);

    /* Probe exported symbols (overrides YAML) */
    probe_symbols(p);

    return p;
}

const char *plugin_name(const plugin_t *p) {
    if (!p) return NULL;
    return p->name;
}

const plugin_version_t *plugin_version(const plugin_t *p) {
    if (!p) return NULL;
    return &p->version;
}

plugin_type_t plugin_type(const plugin_t *p) {
    if (!p) return PLUGIN_UNKNOWN;
    return p->type;
}

const char *plugin_description(const plugin_t *p) {
    if (!p) return NULL;
    return p->description[0] ? p->description : NULL;
}

const plugin_dep_t *plugin_deps(const plugin_t *p, int *count) {
    if (!p) { *count = 0; return NULL; }
    *count = p->ndeps;
    return p->deps;
}

const char *plugin_path(const plugin_t *p) {
    if (!p) return NULL;
    return p->path;
}

bool plugin_is_initialized(const plugin_t *p) {
    if (!p) return false;
    return p->initialized;
}

void plugin_set_initialized(plugin_t *p, bool initialized) {
    if (!p) return;
    p->initialized = initialized;
}

void *plugin_symbol(plugin_t *p, const char *name) {
    if (!p || !p->handle || !name) return NULL;
    return dlsym(p->handle, name);
}

bool plugin_has(plugin_t *p, const char *name) {
    return plugin_symbol(p, name) != NULL;
}

void plugin_unload(plugin_t *p) {
    if (!p) return;
    if (p->handle) {
        /* Look for cleanup function */
        int (*cleanup)(void) = (int (*)(void))dlsym(p->handle, "plugin_cleanup");
        if (cleanup) cleanup();
        dlclose(p->handle);
    }
    free(p);
}

/* ================================================================
 *  Version checking
 * ================================================================ */

bool plugin_satisfies_dep(const plugin_t *p, const plugin_dep_t *dep) {
    if (!p || !dep) return false;
    if (dep->optional) return true;
    return plugin_version_cmp(&p->version, &dep->min_version) >= 0;
}

int plugin_check_deps(const plugin_t *p, plugin_registry_t *reg) {
    if (!p || !reg) return -1;
    int unsatisfied = 0;
    for (int i = 0; i < p->ndeps; i++) {
        if (p->deps[i].optional) continue;
        plugin_t *dep = plugin_registry_find(reg, p->deps[i].name);
        if (!dep || !plugin_satisfies_dep(dep, &p->deps[i])) {
            unsatisfied++;
        }
    }
    return unsatisfied;
}

/* ================================================================
 *  Registry
 * ================================================================ */

plugin_registry_t *plugin_registry_new(void) {
    plugin_registry_t *reg = (plugin_registry_t *)calloc(1, sizeof(plugin_registry_t));
    if (!reg) return NULL;
    reg->capacity = 16;
    reg->plugins = (plugin_t **)calloc((size_t)reg->capacity, sizeof(plugin_t *));
    if (!reg->plugins) { free(reg); return NULL; }
    return reg;
}

static bool reg_grow(plugin_registry_t *reg) {
    if (reg->count < reg->capacity) return true;
    int newcap = reg->capacity * 2;
    plugin_t **np = (plugin_t **)realloc(reg->plugins, (size_t)newcap * sizeof(plugin_t *));
    if (!np) return false;
    reg->plugins = np;
    reg->capacity = newcap;
    return true;
}

bool plugin_registry_add(plugin_registry_t *reg, plugin_t *p) {
    if (!reg || !p) return false;
    if (!reg_grow(reg)) return false;
    reg->plugins[reg->count++] = p;
    return true;
}

int plugin_registry_discover(plugin_registry_t *reg, const char *dir) {
    if (!reg || !dir) return -1;

    DIR *d = opendir(dir);
    if (!d) {
        set_error_fmt("Cannot open directory: %s", dir);
        return -1;
    }

    int loaded = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
        size_t len = strlen(entry->d_name);
        if (len < 4 || strcmp(entry->d_name + len - 3, ".so") != 0) continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);

        plugin_t *p = plugin_load(fullpath);
        if (p) {
            plugin_registry_add(reg, p);
            loaded++;
        } else {
            fprintf(stderr, "[plugin] FAILED to load %s: %s\n",
                    fullpath, plugin_error());
        }
    }

    closedir(d);
    return loaded;
}

/* P127: Discover plugins in multiple directories */
int plugin_registry_discover_dirs(plugin_registry_t *reg,
                                  const char **dirs, int ndirs) {
    if (!reg || !dirs || ndirs <= 0) return -1;
    int total = 0;
    for (int i = 0; i < ndirs; i++) {
        int n = plugin_registry_discover(reg, dirs[i]);
        if (n > 0) total += n;
    }
    return total;
}

/* ================================================================
 *  Topological sort for dependency resolution (P128)
 * ================================================================ */

/* Internal: find index of plugin by name */
static int find_plugin_idx(plugin_registry_t *reg, const char *name) {
    if (!reg || !name) return -1;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(plugin_name(reg->plugins[i]), name) == 0)
            return i;
    }
    return -1;
}

/* Internal DFS-based topological sort */
static bool tsort_dfs(plugin_registry_t *reg, int idx,
                      int *visited, int *order, int *order_idx,
                      int *rec_stack) {
    if (rec_stack[idx]) {
        set_error_fmt("Circular dependency detected involving plugin '%s'",
                      plugin_name(reg->plugins[idx]));
        return false; /* circular */
    }
    if (visited[idx]) return true;

    rec_stack[idx] = 1;
    visited[idx] = 1;

    /* Visit dependencies first */
    plugin_t *p = reg->plugins[idx];
    for (int d = 0; d < p->ndeps; d++) {
        int dep_idx = find_plugin_idx(reg, p->deps[d].name);
        if (dep_idx >= 0) {
            if (!tsort_dfs(reg, dep_idx, visited, order, order_idx, rec_stack))
                return false;
        }
    }

    rec_stack[idx] = 0;
    order[(*order_idx)++] = idx;
    return true;
}

int *plugin_registry_resolve_deps(plugin_registry_t *reg, int *count_out) {
    if (!reg || !count_out) return NULL;

    *count_out = 0;
    if (reg->count == 0) {
        int *empty = (int *)malloc(sizeof(int));
        if (empty) *empty = 0;
        return empty;
    }

    int *order = (int *)calloc((size_t)reg->count, sizeof(int));
    int *visited = (int *)calloc((size_t)reg->count, sizeof(int));
    int *rec_stack = (int *)calloc((size_t)reg->count, sizeof(int));

    if (!order || !visited || !rec_stack) {
        free(order); free(visited); free(rec_stack);
        set_error("OOM in dependency resolution");
        return NULL;
    }

    int order_idx = 0;
    for (int i = 0; i < reg->count; i++) {
        if (!visited[i]) {
            if (!tsort_dfs(reg, i, visited, order, &order_idx, rec_stack)) {
                /* Circular dependency detected */
                free(order); free(visited); free(rec_stack);
                return NULL;
            }
            /* order_idx was updated by the DFS */
        }
    }

    /* Re-count: DFS may not have visited all if there were skips */
    order_idx = 0;
    memset(visited, 0, (size_t)reg->count * sizeof(int));
    memset(rec_stack, 0, (size_t)reg->count * sizeof(int));
    for (int i = 0; i < reg->count; i++) {
        if (!visited[i]) {
            if (!tsort_dfs(reg, i, visited, order, &order_idx, rec_stack)) {
                free(order); free(visited); free(rec_stack);
                return NULL;
            }
        }
    }

    free(visited);
    free(rec_stack);

    *count_out = order_idx;
    return order;
}

/* P128: Initialize all plugins in dependency order */
int plugin_registry_init_ordered(plugin_registry_t *reg) {
    if (!reg) return -1;

    int sorted_count = 0;
    int *order = plugin_registry_resolve_deps(reg, &sorted_count);

    int ok = 0;
    if (order) {
        for (int i = 0; i < sorted_count; i++) {
            plugin_t *p = reg->plugins[order[i]];
            if (p->initialized) { ok++; continue; }
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_init");
            int (*init_fn)(void) = u.fn;
            if (init_fn) {
                if (init_fn() == 0) {
                    p->initialized = true;
                    ok++;
                }
            } else {
                p->initialized = true; /* no init needed */
                ok++;
            }
        }
        free(order);
    } else {
        /* Fall back to simple init */
        ok = plugin_registry_init_all(reg);
    }

    return ok;
}

/* P128: Shutdown all plugins in reverse dependency order */
int plugin_registry_shutdown(plugin_registry_t *reg) {
    if (!reg) return -1;

    int sorted_count = 0;
    int *order = plugin_registry_resolve_deps(reg, &sorted_count);

    int ok = 0;
    if (order) {
        /* Reverse order */
        for (int i = sorted_count - 1; i >= 0; i--) {
            plugin_t *p = reg->plugins[order[i]];
            if (!p->initialized) continue;
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_cleanup");
            int (*cleanup_fn)(void) = u.fn;
            if (cleanup_fn) {
                if (cleanup_fn() == 0) ok++;
            } else {
                ok++;
            }
            p->initialized = false;
        }
        free(order);
    } else {
        /* Fall back: shutdown in reverse order */
        for (int i = reg->count - 1; i >= 0; i--) {
            plugin_t *p = reg->plugins[i];
            if (!p->initialized) continue;
            union { void *ptr; int (*fn)(void); } u;
            u.ptr = dlsym(p->handle, "plugin_cleanup");
            int (*cleanup_fn)(void) = u.fn;
            if (cleanup_fn) {
                if (cleanup_fn() == 0) ok++;
            }
            p->initialized = false;
        }
    }

    return ok;
}

int plugin_registry_init_all(plugin_registry_t *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->plugins[i]->initialized) { ok++; continue; }
        union { void *ptr; int (*fn)(void); } u;
        u.ptr = dlsym(reg->plugins[i]->handle, "plugin_init");
        int (*init_fn)(void) = u.fn;
        if (init_fn) {
            if (init_fn() == 0) {
                reg->plugins[i]->initialized = true;
                ok++;
            }
        } else {
            reg->plugins[i]->initialized = true; /* no init needed */
            ok++;
        }
    }
    return ok;
}

plugin_t *plugin_registry_find(plugin_registry_t *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(plugin_name(reg->plugins[i]), name) == 0)
            return reg->plugins[i];
    }
    return NULL;
}

void plugin_registry_free(plugin_registry_t *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->count; i++)
        plugin_unload(reg->plugins[i]);
    free(reg->plugins);
    free(reg);
}
