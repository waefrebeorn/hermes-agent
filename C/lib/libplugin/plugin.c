/*
 * plugin.c — Dynamic plugin loader for C (dlopen-based).
 * MIT License — WuBu Hermes Project
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>

/* ================================================================
 *  Error handling
 * ================================================================ */

static char last_error[1024] = "";

static void set_error(const char *msg) {
    snprintf(last_error, sizeof(last_error), "%s", msg ? msg : "unknown error");
}

static void set_error_dl(void) {
    const char *dlerr = dlerror();
    snprintf(last_error, sizeof(last_error), "%s", dlerr ? dlerr : "dl error");
}

const char *plugin_error(void) {
    return last_error;
}

/* ================================================================
 *  Plugin struct
 * ================================================================ */

struct plugin_t {
    void   *handle;      /* dlopen handle */
    char    path[1024];   /* full path */
    char    name[256];    /* derived name */
    bool    loaded;       /* dlopen succeeded */
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

    /* Open with RTLD_NOW to catch unresolved symbols immediately */
    p->handle = dlopen(path, RTLD_NOW);
    if (!p->handle) {
        set_error_dl();
        free(p);
        return NULL;
    }

    p->loaded = true;
    return p;
}

const char *plugin_name(const plugin_t *p) {
    if (!p) return NULL;
    return p->name;
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
        void (*cleanup)(void) = (void (*)(void))dlsym(p->handle, "plugin_cleanup");
        if (cleanup) cleanup();
        dlclose(p->handle);
    }
    free(p);
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
        snprintf(last_error, sizeof(last_error), "Cannot open directory: %s", dir);
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
        }
    }

    closedir(d);
    return loaded;
}

int plugin_registry_init_all(plugin_registry_t *reg) {
    if (!reg) return -1;
    int ok = 0;
    for (int i = 0; i < reg->count; i++) {
        int (*init_fn)(void) = (int (*)(void))plugin_symbol(reg->plugins[i], "plugin_init");
        if (init_fn) {
            if (init_fn() == 0) ok++;
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
