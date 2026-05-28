/*
 * schema_sanitizer.c — Tool JSON schema sanitizer for broad LLM-backend compat.
 *
 * Port of Python tools/schema_sanitizer.py.
 *
 * Walks tool JSON schemas and fixes constructs that strict backends
 * (llama.cpp, xAI Responses, OpenAI Codex) reject.
 */

#include "hermes_json.h"
#include "schema_sanitizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ─── Forward declarations ──────────────────────────────────────────── */
static json_node_t *_sanitize_node(json_node_t *node, const char *path);
static json_node_t *_sanitize_single_tool(json_node_t *tool);
static json_node_t *_strip_top_level_combinators(json_node_t *params,
                                                  const char *path);
static json_node_t *_collapse_nullable_unions(json_node_t *schema,
                                               bool keep_nullable_hint);
static void _walk_strip_pattern(json_node_t *node, int *stripped);
static void _walk_strip_slash(json_node_t *node, int *stripped);

/* ─── Helpers ───────────────────────────────────────────────────────── */

/* Check if a JSON value is a dict */
static inline bool is_dict(const json_node_t *n)
{
    return n && n->type == JSON_OBJECT;
}

/* Check if a JSON value is a list */
static inline bool is_list(const json_node_t *n)
{
    return n && n->type == JSON_ARRAY;
}

/* Check if a JSON value is a string */
static inline bool is_str(const json_node_t *n)
{
    return n && n->type == JSON_STRING;
}

/* Get array of object keys — caller must free returned array + strings */
static char **get_object_keys(const json_node_t *obj, int *count)
{
    if (!obj || obj->type != JSON_OBJECT) {
        *count = 0;
        return NULL;
    }
    *count = (int)obj->c.count;
    if (*count == 0) return NULL;

    char **keys = (char **)calloc(*count, sizeof(char *));
    if (!keys) { *count = 0; return NULL; }
    for (int i = 0; i < *count; i++) {
        if (obj->c.keys[i])
            keys[i] = strdup(obj->c.keys[i]);
    }
    return keys;
}

static void free_keys(char **keys, int count)
{
    if (!keys) return;
    for (int i = 0; i < count; i++) free(keys[i]);
    free(keys);
}

/* Remove a key from an object node (mutates in-place). Returns true if removed. */
static bool remove_key(json_node_t *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT || !key) return false;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (obj->c.keys[i] && strcmp(obj->c.keys[i], key) == 0) {
            json_free(obj->c.items[i]);
            free(obj->c.keys[i]);
            /* Shift remaining entries */
            for (size_t j = i + 1; j < obj->c.count; j++) {
                obj->c.keys[j - 1] = obj->c.keys[j];
                obj->c.items[j - 1] = obj->c.items[j];
            }
            obj->c.count--;
            return true;
        }
    }
    return false;
}

/* ─── Public: sanitize_tool_schemas ─────────────────────────────────── */

char *sanitize_tool_schemas(const char *tools_json)
{
    if (!tools_json || !tools_json[0])
        return strdup("[]");

    char *err = NULL;
    json_node_t *tools = json_parse(tools_json, &err);
    if (!tools) {
        free(err);
        return strdup("[]");
    }
    if (!is_list(tools)) {
        json_free(tools);
        return strdup("[]");
    }

    json_node_t *result = json_new_array();
    size_t count = json_len(tools);
    for (size_t i = 0; i < count; i++) {
        json_node_t *tool = json_get(tools, i);
        if (is_dict(tool)) {
            json_node_t *sanitized = _sanitize_single_tool(tool);
            json_append(result, sanitized ? sanitized : json_copy(tool));
        } else {
            json_append(result, json_copy(tool));
        }
    }

    json_free(tools);
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* ─── Internal: tool-level sanitizer ────────────────────────────────── */

static json_node_t *_sanitize_single_tool(json_node_t *tool)
{
    json_node_t *out = json_copy(tool);

    /* Get "function" dict */
    json_node_t *fn = json_obj_get(out, "function");
    if (!is_dict(fn)) {
        /* Try Responses-format: {name, parameters} directly on tool */
        json_node_t *params = json_obj_get(out, "parameters");
        if (is_dict(params)) {
            json_node_t *sanitized = _sanitize_node(params, "<tool>");
            json_set(out, "parameters", sanitized);
        }
        return out;
    }

    /* OpenAI-format: {function: {name, parameters}} */
    const char *tool_name = json_get_str(fn, "name", "<tool>");
    json_node_t *params = json_obj_get(fn, "parameters");

    if (!is_dict(params)) {
        /* Missing or non-dict → substitute minimal valid shape */
        json_node_t *minimal = json_new_object();
        json_set(minimal, "type", json_new_string("object"));
        json_set(minimal, "properties", json_new_object());
        json_set(fn, "parameters", minimal);
        return out;
    }

    /* Recursive sanitize */
    json_node_t *sanitized = _sanitize_node(params, tool_name);
    json_set(fn, "parameters", sanitized);

    /* Guarantee top-level is object with properties */
    json_node_t *top = json_obj_get(fn, "parameters");
    if (is_dict(top)) {
        const char *type = json_get_str(top, "type", "");
        if (strcmp(type, "object") != 0 || !json_has(top, "properties")) {
            json_set(top, "type", json_new_string("object"));
            if (!is_dict(json_obj_get(top, "properties")))
                json_set(top, "properties", json_new_object());
        }
        /* Collapse nullable unions */
        json_node_t *collapsed = _collapse_nullable_unions(top, true);
        json_set(fn, "parameters", collapsed);

        /* Strip top-level combinators */
        json_node_t *stripped = _strip_top_level_combinators(
            json_obj_get(fn, "parameters"), tool_name);
        json_set(fn, "parameters", stripped);
    }

    return out;
}

/* ─── Internal: recursive schema node sanitizer ─────────────────────── */

/*
 * Known JSON Schema keywords that are NOT schema nodes themselves
 * (they are sibling metadata). We pass these through unchanged.
 */
static bool is_metadata_key(const char *key)
{
    if (!key) return false;
    return strcmp(key, "required") == 0 ||
           strcmp(key, "enum") == 0 ||
           strcmp(key, "examples") == 0 ||
           strcmp(key, "default") == 0 ||
           strcmp(key, "title") == 0 ||
           strcmp(key, "description") == 0 ||
           strcmp(key, "deprecated") == 0 ||
           strcmp(key, "readOnly") == 0 ||
           strcmp(key, "writeOnly") == 0;
}

/* Keys that contain sub-schemas (recurse into these) */
static bool is_schema_container_key(const char *key)
{
    if (!key) return false;
    return strcmp(key, "properties") == 0 ||
           strcmp(key, "$defs") == 0 ||
           strcmp(key, "definitions") == 0 ||
           strcmp(key, "items") == 0 ||
           strcmp(key, "additionalProperties") == 0 ||
           strcmp(key, "anyOf") == 0 ||
           strcmp(key, "oneOf") == 0 ||
           strcmp(key, "allOf") == 0 ||
           strcmp(key, "not") == 0;
}

/* Recognized JSON Schema type strings */
static bool is_valid_type(const char *t)
{
    if (!t) return false;
    return strcmp(t, "object") == 0 ||
           strcmp(t, "string") == 0 ||
           strcmp(t, "number") == 0 ||
           strcmp(t, "integer") == 0 ||
           strcmp(t, "boolean") == 0 ||
           strcmp(t, "array") == 0 ||
           strcmp(t, "null") == 0;
}

static json_node_t *_sanitize_node(json_node_t *node, const char *path)
{
    (void)path;

    /* Bare string like "object" → {"type": "object"} */
    if (is_str(node)) {
        const char *val = node->str_val;
        if (is_valid_type(val)) {
            json_node_t *obj = json_new_object();
            json_set(obj, "type", json_new_string(val));
            if (strcmp(val, "object") == 0)
                json_set(obj, "properties", json_new_object());
            return obj;
        }
        /* Non-schema string → empty object schema */
        json_node_t *obj = json_new_object();
        json_set(obj, "type", json_new_string("object"));
        json_set(obj, "properties", json_new_object());
        return obj;
    }

    /* Array → recurse into elements */
    if (is_list(node)) {
        json_node_t *out = json_new_array();
        size_t len = json_len(node);
        for (size_t i = 0; i < len; i++) {
            json_node_t *item = json_get(node, i);
            json_append(out, _sanitize_node(item, path));
        }
        return out;
    }

    /* Non-dict primitive → pass through */
    if (!is_dict(node)) {
        return json_copy(node);
    }

    /* Dict — process each key */
    json_node_t *out = json_new_object();

    int key_count = 0;
    char **keys = get_object_keys(node, &key_count);

    for (int k = 0; k < key_count; k++) {
        const char *key = keys[k];
        json_node_t *val = json_obj_get(node, key);
        if (!val) continue;

        /* "type": [X, "null"] → type: X, nullable: true */
        if (strcmp(key, "type") == 0 && is_list(val)) {
            size_t arr_len = json_len(val);
            json_node_t *first_non_null = NULL;
            bool has_null = false;
            bool has_other __attribute__((unused)) = false;
            for (size_t i = 0; i < arr_len; i++) {
                json_node_t *item = json_get(val, i);
                if (is_str(item) && strcmp(item->str_val, "null") == 0) {
                    has_null = true;
                } else if (is_str(item)) {
                    if (!first_non_null)
                        first_non_null = item;
                    has_other = true;
                }
            }
            if (first_non_null && has_null) {
                json_set(out, "type", json_new_string(first_non_null->str_val));
                json_set(out, "nullable", json_bool(true));
            } else if (first_non_null) {
                json_set(out, "type", json_new_string(first_non_null->str_val));
            } else {
                json_set(out, "type", json_new_string("object"));
            }
            continue;
        }

        /* Metadata keys → copy verbatim */
        if (is_metadata_key(key)) {
            json_set(out, key, json_copy(val));
            continue;
        }

        /* Schema container keys → recurse */
        if (is_schema_container_key(key)) {
            if (is_dict(val)) {
                /* Recurse into each sub-value */
                json_node_t *recurse_out = json_new_object();
                int sub_count = 0;
                char **sub_keys = get_object_keys(val, &sub_count);
                for (int s = 0; s < sub_count; s++) {
                    json_node_t *sub_val = json_obj_get(val, sub_keys[s]);
                    if (sub_val)
                        json_set(recurse_out, sub_keys[s],
                                 _sanitize_node(sub_val, path));
                }
                free_keys(sub_keys, sub_count);
                json_set(out, key, recurse_out);
            } else if (is_list(val)) {
                json_node_t *arr_out = json_new_array();
                size_t alen = json_len(val);
                for (size_t i = 0; i < alen; i++) {
                    json_append(arr_out,
                                _sanitize_node(json_get(val, i), path));
                }
                json_set(out, key, arr_out);
            } else if (val->type == JSON_BOOL) {
                json_set(out, key, json_copy(val));
            } else {
                json_set(out, key, _sanitize_node(val, path));
            }
            continue;
        }

        /* Everything else → recurse if dict/list, else copy */
        if (is_dict(val) || is_list(val))
            json_set(out, key, _sanitize_node(val, path));
        else
            json_set(out, key, json_copy(val));
    }

    free_keys(keys, key_count);

    /* Object nodes without properties → inject empty properties */
    const char *type = json_get_str(out, "type", "");
    if (strcmp(type, "object") == 0) {
        json_node_t *props = json_obj_get(out, "properties");
        if (!is_dict(props))
            json_set(out, "properties", json_new_object());
    }

    /* Prune required entries that don't exist in properties */
    if (strcmp(type, "object") == 0) {
        json_node_t *req = json_obj_get(out, "required");
        json_node_t *props = json_obj_get(out, "properties");
        if (is_list(req) && is_dict(props)) {
            json_node_t *valid = json_new_array();
            size_t req_len = json_len(req);
            for (size_t i = 0; i < req_len; i++) {
                json_node_t *r = json_get(req, i);
                if (is_str(r) && json_obj_get(props, r->str_val) != NULL)
                    json_append(valid, json_copy(r));
            }
            if (json_len(valid) == 0) {
                json_free(valid);
                /* Remove required entirely */
                /* Handled by not setting it */
            } else {
                json_set(out, "required", valid);
            }
        }
    }

    return out;
}

/* ─── Top-level combinator stripper ─────────────────────────────────── */

static const char *FORBIDDEN_TOP_KEYS[] = {
    "allOf", "anyOf", "oneOf", "enum", "not", NULL
};

static json_node_t *_strip_top_level_combinators(json_node_t *params,
                                                  const char *path)
{
    (void)path;
    if (!is_dict(params)) return json_copy(params);

    json_node_t *out = json_copy(params);
    for (int i = 0; FORBIDDEN_TOP_KEYS[i]; i++) {
        if (json_has(out, FORBIDDEN_TOP_KEYS[i]))
            remove_key(out, FORBIDDEN_TOP_KEYS[i]);
    }
    return out;
}

/* ─── Nullable union collapser ──────────────────────────────────────── */

static json_node_t *_collapse_nullable_unions(json_node_t *schema,
                                               bool keep_nullable_hint)
{
    if (is_list(schema)) {
        json_node_t *out = json_new_array();
        size_t len = json_len(schema);
        for (size_t i = 0; i < len; i++) {
            json_append(out, _collapse_nullable_unions(
                json_get(schema, i), keep_nullable_hint));
        }
        return out;
    }
    if (!is_dict(schema)) return json_copy(schema);

    /* Deep copy and recurse into all values */
    json_node_t *out = json_new_object();
    int key_count = 0;
    char **keys = get_object_keys(schema, &key_count);
    for (int k = 0; k < key_count; k++) {
        json_node_t *val = json_obj_get(schema, keys[k]);
        if (val)
            json_set(out, keys[k],
                     _collapse_nullable_unions(val, keep_nullable_hint));
    }
    free_keys(keys, key_count);

    /* Check anyOf/oneOf for nullable unions */
    const char *union_keys[] = {"anyOf", "oneOf", NULL};
    for (int u = 0; union_keys[u]; u++) {
        json_node_t *variants = json_obj_get(out, union_keys[u]);
        if (!is_list(variants)) continue;

        /* Collect non-null variants */
        size_t len = json_len(variants);
        json_node_t *non_null_arr = json_new_array();
        for (size_t i = 0; i < len; i++) {
            json_node_t *item = json_get(variants, i);
            if (!is_dict(item)) {
                json_append(non_null_arr, json_copy(item));
                continue;
            }
            const char *t = json_get_str(item, "type", "");
            if (strcmp(t, "null") != 0)
                json_append(non_null_arr, json_copy(item));
        }

        size_t nn_len = json_len(non_null_arr);
        /* Collapse: exactly 1 non-null branch AND at least 1 null was dropped */
        if (nn_len == 1 && nn_len != len) {
            json_node_t *replacement = json_get(non_null_arr, 0);
            json_node_t *collapsed = json_copy(replacement);

            if (keep_nullable_hint)
                json_set(collapsed, "nullable", json_bool(true));

            /* Carry over metadata from outer node */
            const char *meta_keys[] = {
                "title", "description", "default", "examples", NULL
            };
            for (int m = 0; meta_keys[m]; m++) {
                if (json_has(out, meta_keys[m]) &&
                    !json_has(collapsed, meta_keys[m])) {
                    json_set(collapsed, meta_keys[m],
                             json_copy(json_obj_get(out, meta_keys[m])));
                }
            }

            json_free(non_null_arr);
            json_free(out);
            return _collapse_nullable_unions(collapsed, keep_nullable_hint);
        }
        json_free(non_null_arr);
    }

    return out;
}

/* ─── Public: strip_pattern_and_format ──────────────────────────────── */

char *strip_pattern_and_format(const char *tools_json, int *stripped_count)
{
    if (stripped_count) *stripped_count = 0;
    if (!tools_json || !tools_json[0])
        return strdup("[]");

    char *err = NULL;
    json_node_t *tools = json_parse(tools_json, &err);
    if (!tools) { free(err); return strdup("[]"); }
    if (!is_list(tools)) { json_free(tools); return strdup("[]"); }

    int stripped = 0;
    size_t count = json_len(tools);
    for (size_t i = 0; i < count; i++) {
        json_node_t *tool = json_get(tools, i);
        if (!is_dict(tool)) continue;

        /* OpenAI-format */
        json_node_t *fn = json_obj_get(tool, "function");
        if (is_dict(fn)) {
            json_node_t *params = json_obj_get(fn, "parameters");
            if (is_dict(params))
                _walk_strip_pattern(params, &stripped);
            continue;
        }

        /* Responses-format */
        json_node_t *params = json_obj_get(tool, "parameters");
        if (is_dict(params))
            _walk_strip_pattern(params, &stripped);
    }

    if (stripped_count) *stripped_count = stripped;
    char *out = json_serialize(tools);
    json_free(tools);
    return out;
}

static void _walk_strip_pattern(json_node_t *node, int *stripped)
{
    if (!node) return;

    if (node->type == JSON_OBJECT) {
        /* Is this node a schema node (has "type" or a combinator)? */
        bool is_schema = json_has(node, "type") ||
                         json_has(node, "anyOf") ||
                         json_has(node, "oneOf") ||
                         json_has(node, "allOf");
        if (is_schema) {
            if (remove_key(node, "pattern")) (*stripped)++;
            if (remove_key(node, "format")) (*stripped)++;
        }
        /* Recurse into all values */
        int key_count = 0;
        char **keys = get_object_keys(node, &key_count);
        for (int i = 0; i < key_count; i++) {
            json_node_t *val = json_obj_get(node, keys[i]);
            _walk_strip_pattern(val, stripped);
        }
        free_keys(keys, key_count);
    } else if (node->type == JSON_ARRAY) {
        size_t len = json_len(node);
        for (size_t i = 0; i < len; i++)
            _walk_strip_pattern(json_get(node, i), stripped);
    }
}

/* ─── Public: strip_slash_enum ──────────────────────────────────────── */

char *strip_slash_enum(const char *tools_json, int *stripped_count)
{
    if (stripped_count) *stripped_count = 0;
    if (!tools_json || !tools_json[0])
        return strdup("[]");

    char *err = NULL;
    json_node_t *tools = json_parse(tools_json, &err);
    if (!tools) { free(err); return strdup("[]"); }
    if (!is_list(tools)) { json_free(tools); return strdup("[]"); }

    int stripped = 0;
    size_t count = json_len(tools);
    for (size_t i = 0; i < count; i++) {
        json_node_t *tool = json_get(tools, i);
        if (!is_dict(tool)) continue;

        json_node_t *fn = json_obj_get(tool, "function");
        if (is_dict(fn)) {
            json_node_t *params = json_obj_get(fn, "parameters");
            if (is_dict(params))
                _walk_strip_slash(params, &stripped);
            continue;
        }

        json_node_t *params = json_obj_get(tool, "parameters");
        if (is_dict(params))
            _walk_strip_slash(params, &stripped);
    }

    if (stripped_count) *stripped_count = stripped;
    char *out = json_serialize(tools);
    json_free(tools);
    return out;
}

static void _walk_strip_slash(json_node_t *node, int *stripped)
{
    if (!node) return;

    if (node->type == JSON_OBJECT) {
        /* Check if this node has an "enum" with '/' values */
        json_node_t *enum_val = json_obj_get(node, "enum");
        if (is_list(enum_val)) {
            bool has_slash = false;
            size_t len = json_len(enum_val);
            for (size_t i = 0; i < len && !has_slash; i++) {
                json_node_t *item = json_get(enum_val, i);
                if (is_str(item) && strchr(item->str_val, '/') != NULL)
                    has_slash = true;
            }
            if (has_slash) {
                remove_key(node, "enum");
                (*stripped)++;
            }
        }
        /* Recurse into all values */
        int key_count = 0;
        char **keys = get_object_keys(node, &key_count);
        for (int i = 0; i < key_count; i++) {
            json_node_t *val = json_obj_get(node, keys[i]);
            _walk_strip_slash(val, stripped);
        }
        free_keys(keys, key_count);
    } else if (node->type == JSON_ARRAY) {
        size_t len = json_len(node);
        for (size_t i = 0; i < len; i++)
            _walk_strip_slash(json_get(node, i), stripped);
    }
}
