/*
 * registry.c — Tool registry for Hermes C.
 * Tools register themselves at init time.
 * Agent loop calls registry to find + dispatch tools.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Registry state
 * ================================================================ */

static tool_registry_t g_registry = {NULL, 0, 0};

/* ================================================================
 *  Registration
 * ================================================================ */

bool registry_register(const char *name, const char *description,
                        const char *schema_json,
                        char *(*handler)(const char *args_json, const char *task_id))
{
    return registry_register_ex(name, description, schema_json, "", handler);
}

bool registry_register_ex(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id))
{
    if (!name || !handler) return false;

    /* Check if already registered */
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0)
            return false; /* Already registered */
    }

    /* Grow if needed */
    if (g_registry.count >= g_registry.capacity) {
        size_t new_cap = g_registry.capacity == 0 ? 32 : g_registry.capacity * 2;
        tool_t *new_tools = (tool_t *)realloc(g_registry.tools,
                                               new_cap * sizeof(tool_t));
        if (!new_tools) return false;
        g_registry.tools = new_tools;
        g_registry.capacity = new_cap;
    }

    tool_t *t = &g_registry.tools[g_registry.count++];
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    snprintf(t->description, sizeof(t->description), "%s", description ? description : "");
    if (schema_json)
        snprintf(t->schema_json, sizeof(t->schema_json), "%s", schema_json);
    if (toolset)
        snprintf(t->toolset, sizeof(t->toolset), "%s", toolset);
    t->handler = handler;
    t->available = true;

    return true;
}

void registry_set_available(const char *name, bool available) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].available = available;
            return;
        }
    }
}

/* ================================================================
 *  Lookup + dispatch
 * ================================================================ */

tool_t *registry_find(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0 && g_registry.tools[i].available)
            return &g_registry.tools[i];
    }
    return NULL;
}

char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id)
{
    tool_t *tool = registry_find(name);
    if (!tool) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\": \"tool '%s' not found\"}", name);
        return strdup(buf);
    }
    return tool->handler(args_json, task_id);
}

/* ================================================================
 *  Get registry for agent loop
 * ================================================================ */

tool_registry_t *registry_get(void) {
    return &g_registry;
}

size_t registry_count(void) {
    return g_registry.count;
}

/* ================================================================
 *  Generate tools JSON for LLM API call
 * ================================================================ */

json_node_t *registry_to_json(void) {
    json_node_t *tools = json_new_array();
    for (size_t i = 0; i < g_registry.count; i++) {
        if (!g_registry.tools[i].available) continue;

        json_node_t *tool = json_new_object();
        json_object_set(tool, "type", json_new_string("function"));

        json_node_t *fn = json_new_object();
        json_object_set(fn, "name", json_new_string(g_registry.tools[i].name));
        json_object_set(fn, "description", json_new_string(g_registry.tools[i].description));

        /* Parse schema from JSON string */
        if (g_registry.tools[i].schema_json[0]) {
            char *err = NULL;
            json_node_t *schema = json_parse(g_registry.tools[i].schema_json, &err);
            if (schema) {
                json_object_set(fn, "parameters", schema);
            } else {
                json_object_set(fn, "parameters", json_new_object());
                free(err);
            }
        } else {
            json_object_set(fn, "parameters", json_new_object());
        }

        json_object_set(tool, "function", fn);
        json_array_append(tools, tool);
    }
    return tools;
}

/* Accessors for testing */
size_t registry_get_count(void) {
    return g_registry.count;
}

const char *registry_get_name(size_t i) {
    if (i >= g_registry.count) return NULL;
    return g_registry.tools[i].name;
}

/* P52: Per-tool timeout */
void registry_set_timeout(const char *name, int seconds) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            g_registry.tools[i].timeout_sec = seconds;
            return;
        }
    }
}

int  registry_get_timeout(const char *name) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0)
            return g_registry.tools[i].timeout_sec;
    }
    return 0; /* Not found */
}

/* P150: Set toolset for a registered tool */
void registry_set_toolset(const char *name, const char *toolset) {
    if (!name || !toolset) return;
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0) {
            snprintf(g_registry.tools[i].toolset, sizeof(g_registry.tools[i].toolset), "%s", toolset);
            return;
        }
    }
}

/* P150: Get toolset for a registered tool */
const char *registry_get_toolset(const char *name) {
    if (!name) return "";
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.tools[i].name, name) == 0)
            return g_registry.tools[i].toolset;
    }
    return "";
}

/* P150: Check if a toolset is present in comma-separated list */
static bool toolset_in_list(const char *toolset, const char *csv) {
    if (!csv || !*csv) return true; /* empty list means "all" */
    if (!toolset || !*toolset) return true; /* unlabeled tools always visible */

    /* Tokenize the CSV list */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", csv);
    char *save;
    char *tok = strtok_r(buf, ", ", &save);
    while (tok) {
        if (strcmp(tok, toolset) == 0) return true;
        tok = strtok_r(NULL, ", ", &save);
    }
    return false;
}

/* P150: Filter a tool_registry_t copy by enabled/disabled toolsets.
 * Modifies the 'available' flag on tools. Only operates on the COPY,
 * not the global registry. Pass NULL for either to skip that filter.
 * Empty string means "all" (no filter applied). */
void registry_filter_by_toolset(tool_registry_t *reg,
                                 const char *enabled_csv,
                                 const char *disabled_csv) {
    if (!reg) return;

    for (size_t i = 0; i < reg->count; i++) {
        /* If disabled list is set and tool is in it, mark unavailable */
        if (disabled_csv && *disabled_csv) {
            if (toolset_in_list(reg->tools[i].toolset, disabled_csv)) {
                reg->tools[i].available = false;
                continue;
            }
        }
        /* If enabled list is set and tool is NOT in it, mark unavailable */
        if (enabled_csv && *enabled_csv) {
            if (!toolset_in_list(reg->tools[i].toolset, enabled_csv)) {
                reg->tools[i].available = false;
            }
        }
    }
}

/* P55: Wildcard pattern matching — simple glob support */
bool registry_name_matches(const char *name, const char *pattern) {
    if (!name || !pattern) return false;

    /* If no wildcard, exact match */
    if (!strchr(pattern, '*'))
        return strcmp(name, pattern) == 0;

    /* Find positions of '*' */
    const char *star = strchr(pattern, '*');

    /* Pattern: "prefix*" — match prefix */
    if (star[1] == '\0') {
        size_t plen = (size_t)(star - pattern);
        return strncmp(name, pattern, plen) == 0;
    }

    /* Pattern: "*suffix" — match suffix */
    if (star == pattern) {
        const char *suffix = pattern + 1;
        size_t slen = strlen(name);
        size_t suflen = strlen(suffix);
        if (slen < suflen) return false;
        return strcmp(name + slen - suflen, suffix) == 0;
    }

    /* Pattern: "prefix*suffix" — match both */
    size_t plen = (size_t)(star - pattern);
    const char *suffix = star + 1;
    size_t slen = strlen(name);
    size_t suflen = strlen(suffix);
    if (slen < plen + suflen) return false;
    return strncmp(name, pattern, plen) == 0 &&
           strcmp(name + slen - suflen, suffix) == 0;
}

void registry_set_available_pattern(const char *pattern, bool available) {
    for (size_t i = 0; i < g_registry.count; i++) {
        if (registry_name_matches(g_registry.tools[i].name, pattern))
            g_registry.tools[i].available = available;
    }
}
