/*
 * plugin_honcho.c — Example memory plugin (P130).
 * Stub showing typed plugin API usage pattern.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_honcho.c -o plugin_honcho.so
 */

#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Plugin metadata (exported symbols)
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "honcho-memory";
}

const char *plugin_meta_version(void) {
    return "1.0.0";
}

const char *plugin_meta_type(void) {
    return "memory";
}

const char *plugin_meta_description(void) {
    return "Honcho memory provider backend for persistent agent memory";
}

/* ================================================================
 *  Dependencies
 * ================================================================ */

static plugin_dep_t deps[] = {
    { .name = "base-memory", .min_version = {0, 1, 0}, .optional = true }
};

int plugin_deps_count(void) {
    return 1;
}

const plugin_dep_t *plugin_deps_list(void) {
    return deps;
}

/* ================================================================
 *  Interface function pointers (for type-safe dispatch)
 * ================================================================ */

static char *memory_store(const char *content, const char *metadata_json) {
    (void)content;
    (void)metadata_json;
    /* In a real implementation, this would call Honcho's API */
    char *result = strdup("{\"status\":\"ok\",\"memory_id\":\"stub-123\"}");
    return result;
}

static char *memory_search(const char *query, int limit) {
    (void)query;
    (void)limit;
    /* In a real implementation, this would search Honcho's API */
    char *result = strdup("{\"results\":[],\"query\":\"stub\"}");
    return result;
}

static void memory_clear(void) {
    /* In a real implementation, this would clear Honcho's storage */
    fprintf(stderr, "[honcho-memory] cleared\n");
}

static plugin_interface_t interface = {
    .type = PLUGIN_MEMORY,
    .memory_store  = memory_store,
    .memory_search = memory_search,
    .memory_clear  = memory_clear,
};

void *plugin_get_interface(void) {
    return &interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    fprintf(stderr, "[honcho-memory] initializing...\n");
    /* TODO: Initialize Honcho client connection */
    fprintf(stderr, "[honcho-memory] ready\n");
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[honcho-memory] shutting down...\n");
    memory_clear();
    return 0;
}

/* Optional: receive config from agent */
int plugin_configure(const char *config_json) {
    fprintf(stderr, "[honcho-memory] configure: %s\n", config_json);
    /* Parse JSON config and apply settings */
    return 0;
}
