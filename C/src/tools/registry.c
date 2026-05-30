/*
 * registry.c — Tool registry for Hermes C.
 * Tools register themselves at init time.
 * Agent loop calls registry to find + dispatch tools.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <ctype.h>
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

/* ================================================================
 *  Tool name repair (port of Python repair_tool_call())
 * ================================================================ */

/* Levenshtein distance between two strings */
static size_t levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1), len2 = strlen(s2);
    size_t *row = malloc((len2 + 1) * sizeof(size_t));
    if (!row) return SIZE_MAX;
    for (size_t j = 0; j <= len2; j++) row[j] = j;
    for (size_t i = 1; i <= len1; i++) {
        size_t prev = row[0];
        row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            size_t min = row[j - 1] + 1; /* insert */
            size_t del = row[j] + 1;     /* delete */
            if (del < min) min = del;
            size_t sub = prev + cost;    /* substitute */
            if (sub < min) min = sub;
            prev = row[j];
            row[j] = min;
        }
    }
    size_t dist = row[len2];
    free(row);
    return dist;
}

/* Levenshtein similarity ratio 0.0-1.0 */
static double levenshtein_ratio(const char *s1, const char *s2) {
    if (!s1 || !s2) return 0.0;
    size_t len1 = strlen(s1), len2 = strlen(s2);
    if (len1 == 0 && len2 == 0) return 1.0;
    size_t max_len = len1 > len2 ? len1 : len2;
    size_t dist = levenshtein_distance(s1, s2);
    return 1.0 - (double)dist / (double)max_len;
}

/* Normalize: lowercase + hyphens/spaces -> underscores.
 * Returns malloc'd string (caller frees). */
static char *name_norm(const char *s) {
    if (!s) return NULL;
    char *r = strdup(s);
    if (!r) return NULL;
    for (char *p = r; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '-' || *p == ' ') *p = '_';
    }
    return r;
}

/* CamelCase -> snake_case: "TodoTool" -> "todo_tool".
 * Returns malloc'd string (caller frees). */
static char *name_camel_to_snake(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s), out_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            out_len++;
        out_len++;
    }
    char *r = malloc(out_len + 1);
    if (!r) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            r[pos++] = '_';
        r[pos++] = (char)tolower((unsigned char)s[i]);
    }
    r[pos] = '\0';
    return r;
}

/* Strip _tool/-tool/tool suffix (case-insensitive).
 * Returns malloc'd string (caller frees), or NULL if no suffix found. */
static char *name_strip_tool_suffix(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s);
    static const char *suffixes[] = {"_tool", "-tool", "tool"};
    for (size_t i = 0; i < 3; i++) {
        size_t slen = strlen(suffixes[i]);
        if (len > slen) {
            const char *tail = s + len - slen;
            bool match = true;
            for (size_t j = 0; j < slen; j++) {
                if (tolower((unsigned char)tail[j]) != (unsigned char)suffixes[i][j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                size_t new_len = len - slen;
                while (new_len > 0 && (s[new_len - 1] == '_' || s[new_len - 1] == '-'))
                    new_len--;
                return strndup(s, new_len);
            }
        }
    }
    return NULL;
}

/* Attempt to repair a mismatched tool name.
 *
 * Port of Python agent/agent_runtime_helpers.py:repair_tool_call().
 * Applies normalization pipeline: lowercase, hyphens->underscores,
 * CamelCase->snake_case, _tool suffix stripping (up to 2x), fuzzy match.
 *
 * Returns malloc'd repaired name (caller frees), or NULL if no match. */
char *registry_repair_tool_name(const char *tool_name) {
    if (!tool_name || !tool_name[0]) return NULL;

    /* Fast path: exact match already handled by caller */
    char *lowered = strdup(tool_name);
    if (!lowered) return NULL;
    for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* 1. Lowercase direct match */
    if (registry_find(lowered)) { return lowered; }

    /* 2. Normalized: lowercase + hyphens/spaces -> underscores */
    char *normalized = name_norm(tool_name);
    if (normalized && registry_find(normalized)) { free(lowered); return normalized; }

    /* 3. CamelCase -> snake_case */
    char *camel = name_camel_to_snake(tool_name);
    if (camel && registry_find(camel)) { free(lowered); free(normalized); return camel; }

    /* 4. Build variant set and try suffix stripping (up to 2x) */
    #define MAX_VARIANTS 64
    char *variants[MAX_VARIANTS];
    size_t n_variants = 0;

    #define ADD_VAR(v) do { \
        if ((v) && n_variants < MAX_VARIANTS) { \
            bool _dup = false; \
            for (size_t _i = 0; _i < n_variants; _i++) { \
                if (strcmp(variants[_i], (v)) == 0) { _dup = true; break; } \
            } \
            if (!_dup) variants[n_variants++] = strdup(v); \
        } \
    } while(0)

    /* Seed variants */
    ADD_VAR(tool_name);
    ADD_VAR(lowered);
    ADD_VAR(normalized);
    ADD_VAR(camel);

    /* Apply suffix stripping iteratively */
    for (int round = 0; round < 2; round++) {
        size_t old_count = n_variants;
        for (size_t i = 0; i < old_count; i++) {
            char *stripped = name_strip_tool_suffix(variants[i]);
            if (stripped) {
                ADD_VAR(stripped);
                char *sn = name_norm(stripped);
                ADD_VAR(sn);
                free(sn);
                char *sc = name_camel_to_snake(stripped);
                ADD_VAR(sc);
                free(sc);
            }
            free(stripped);
        }
    }

    /* Check all variants */
    for (size_t i = 0; i < n_variants; i++) {
        if (variants[i][0] && registry_find(variants[i])) {
            char *result = strdup(variants[i]);
            for (size_t j = 0; j < n_variants; j++) free(variants[j]);
            free(lowered); free(normalized); free(camel);
            return result;
        }
    }

    /* 5. Fuzzy match (Levenshtein ratio >= 0.7) */
    const char *best_match = NULL;
    double best_ratio = 0.7;
    for (size_t i = 0; i < g_registry.count; i++) {
        double ratio = levenshtein_ratio(lowered, g_registry.tools[i].name);
        if (ratio > best_ratio) {
            best_ratio = ratio;
            best_match = g_registry.tools[i].name;
        }
    }

    for (size_t j = 0; j < n_variants; j++) free(variants[j]);
    free(lowered); free(normalized); free(camel);

    if (best_match) return strdup(best_match);
    return NULL;

    #undef ADD_VAR
    #undef MAX_VARIANTS
}

char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id)
{
    tool_t *tool = registry_find(name);
    if (!tool) {
        /* Try to repair the tool name before giving up */
        char *repaired = registry_repair_tool_name(name);
        if (repaired) {
            tool = registry_find(repaired);
            free(repaired);
        }
        if (tool) {
            return tool->handler(args_json, task_id);
        }
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
