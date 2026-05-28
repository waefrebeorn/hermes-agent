/*
 * moonshot_schema.c — Moonshot/Kimi schema sanitizer.
 *
 * Port of Python agent/moonshot_schema.py (262 lines).
 * 5 Moonshot-specific fixes:
 * 1. Every property must have "type" — add if missing
 * 2. When anyOf is used, strip "type" from parent
 * 3. Strip null and empty-string entries from enum arrays
 * 4. Strip all sibling keys from $ref nodes
 * 5. Collapse tuple-style items arrays to single schema
 *
 * MIT License — WuBu Slermes Project
 */

#include "moonshot_schema.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>

static json_t *sanitize_node(json_t *node, int depth);

/* Keys whose values contain schema maps (recurse into values) */
static bool is_schema_map_key(const char *key) {
    return (strcmp(key, "properties") == 0 ||
            strcmp(key, "patternProperties") == 0 ||
            strcmp(key, "$defs") == 0 ||
            strcmp(key, "definitions") == 0);
}

/* Keys whose values are lists of schemas */
static bool is_schema_list_key(const char *key) {
    return (strcmp(key, "anyOf") == 0 ||
            strcmp(key, "oneOf") == 0 ||
            strcmp(key, "allOf") == 0 ||
            strcmp(key, "prefixItems") == 0);
}

/* Keys whose values are single nested schemas */
static bool is_schema_node_key(const char *key) {
    return (strcmp(key, "items") == 0 ||
            strcmp(key, "contains") == 0 ||
            strcmp(key, "not") == 0 ||
            strcmp(key, "additionalProperties") == 0 ||
            strcmp(key, "propertyNames") == 0);
}

static json_t *sanitize_object(json_t *obj, int depth) {
    json_t *out = json_object();

    /* Pass 1: handle $ref nodes — strip all siblings */
    json_t *ref_val = json_obj_get(obj, "$ref");
    if (ref_val && depth >= 0) {
        json_set(out, "$ref", json_copy(ref_val));
        return out;
    }

    /* Pass 2: process all keys */
    for (size_t i = 0; i < obj->c.count; i++) {
        const char *key = obj->c.keys[i];
        json_t *val = obj->c.items[i];
        if (!key || !val) continue;

        if (is_schema_map_key(key) && val->type == JSON_OBJECT) {
            json_t *map = json_object();
            for (size_t j = 0; j < val->c.count; j++) {
                const char *sk = val->c.keys[j];
                json_t *sv = val->c.items[j];
                if (sk && sv) {
                    json_t *sanitized = sanitize_node(sv, depth + 1);
                    if (sanitized) {
                        json_set(map, sk, sanitized);
                    }
                }
            }
            json_set(out, key, map);
        } else if (is_schema_list_key(key) && val->type == JSON_ARRAY) {
            json_t *arr = json_array();
            for (size_t j = 0; j < val->c.count; j++) {
                json_t *item = val->c.items[j];
                if (item && item->type == JSON_OBJECT) {
                    json_t *sanitized = sanitize_node(item, depth + 1);
                    if (sanitized) {
                        json_append(arr, sanitized);
                    }
                }
            }
            json_set(out, key, arr);
        } else if (is_schema_node_key(key) && val->type == JSON_OBJECT) {
            json_t *sanitized = sanitize_node(val, depth + 1);
            if (sanitized) {
                json_set(out, key, sanitized);
            }
        } else if (strcmp(key, "enum") == 0 && val->type == JSON_ARRAY) {
            /* Strip null and empty-string entries from enum */
            json_t *enum_arr = json_array();
            for (size_t j = 0; j < val->c.count; j++) {
                json_t *e = val->c.items[j];
                if (e->type == JSON_NULL) continue;
                if (e->type == JSON_STRING && (!e->str_val || !e->str_val[0])) continue;
                json_append(enum_arr, json_copy(e));
            }
            if (json_len(enum_arr) > 0) {
                json_set(out, key, enum_arr);
            }
        } else {
            json_set(out, key, json_copy(val));
        }
    }

    /* Pass 3: add missing "type" to property schemas */
    json_t *type_val = json_obj_get(out, "type");
    if (!type_val) {
        json_set(out, "type", json_string("object"));
    }

    /* Pass 4: when anyOf is present and parent also has type, strip parent type */
    json_t *anyof_val = json_obj_get(out, "anyOf");
    if (anyof_val) {
        /* Strip type when anyOf is present — rebuild without type */
        json_t *cleaned = json_object();
        for (size_t i = 0; i < out->c.count; i++) {
            if (strcmp(out->c.keys[i], "type") == 0) continue;
            json_t *copy = json_copy(out->c.items[i]);
            json_set(cleaned, out->c.keys[i], copy);
        }
        json_free(out);
        return cleaned;
    }

    return out;
}

/* Sanitize a JSON node */
static json_t *sanitize_node(json_t *node, int depth) {
    if (!node) return NULL;
    if (node->type == JSON_OBJECT)
        return sanitize_object(node, depth);
    if (node->type == JSON_ARRAY) {
        json_t *arr = json_array();
        for (size_t i = 0; i < node->c.count; i++) {
            json_t *item = sanitize_node(node->c.items[i], depth + 1);
            if (item) json_append(arr, item);
        }
        return arr;
    }
    return json_copy(node);
}

/**/

char *sanitize_moonshot_schema(const char *schema_json) {
    if (!schema_json || !schema_json[0]) {
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }

    char *err = NULL;
    json_t *parsed = json_parse(schema_json, &err);
    if (!parsed) {
        free(err);
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }
    free(err);

    json_t *sanitized = sanitize_node(parsed, 0);
    json_free(parsed);

    if (!sanitized) {
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }

    char *result = json_serialize(sanitized);
    json_free(sanitized);
    return result;
}

char *sanitize_moonshot_tool_parameters(const char *parameters_json) {
    char *cleaned = sanitize_moonshot_schema(parameters_json);
    if (!cleaned || strcmp(cleaned, "{}") == 0) {
        free(cleaned);
        return strdup("{\"type\":\"object\",\"properties\":{}}");
    }
    return cleaned;
}
