/*
 * gemini_schema.c — Gemini Schema sanitizer for tool parameters.
 *
 * Port of Python agent/gemini_schema.py (99 lines).
 * Strips OpenAI-flavored JSON Schema keys that Google's Gemini
 * Schema object rejects, recursively sanitizes nested structures,
 * and drops typed enums that collide with Gemini's string-only enum rule.
 *
 * Build: gcc ... -Iinclude -Ilib/libjson lib/libjson/json.c src/agent/gemini_schema.c
 *
 * MIT License — WuBu Hermes Project
 */

#include "gemini_schema.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>

/* Forward: recursive sanitize */
static json_t *sanitize_node(json_t *node);

/* Gemini Schema allowed keys */
static const char *ALLOWED_KEYS[] = {
    "type", "format", "title", "description", "nullable", "enum",
    "maxItems", "minItems", "properties", "required",
    "minProperties", "maxProperties", "minLength", "maxLength",
    "pattern", "example", "anyOf", "propertyOrdering", "default",
    "items", "minimum", "maximum", NULL
};

static bool is_allowed(const char *key) {
    for (int i = 0; ALLOWED_KEYS[i]; i++)
        if (strcmp(key, ALLOWED_KEYS[i]) == 0) return true;
    return false;
}

/* Check if there's a non-string entry in the enum array */
static bool enum_has_non_string(json_t *enum_arr) {
    if (!enum_arr || enum_arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < enum_arr->c.count; i++) {
        json_t *item = enum_arr->c.items[i];
        if (item->type != JSON_STRING) return true;
    }
    return false;
}

static json_t *sanitize_object(json_t *obj) {
    json_t *out = json_object();

    for (size_t i = 0; i < obj->c.count; i++) {
        const char *key = obj->c.keys[i];
        json_t *val = obj->c.items[i];
        if (!key || !val) continue;
        if (!is_allowed(key)) continue;

        if (strcmp(key, "properties") == 0 && val->type == JSON_OBJECT) {
            json_t *props = json_object();
            for (size_t j = 0; j < val->c.count; j++) {
                const char *pkey = val->c.keys[j];
                json_t *pval = val->c.items[j];
                if (!pkey || !pval) continue;
                json_t *sanitized = sanitize_node(pval);
                if (sanitized) {
                    json_set(props, pkey, sanitized);
                }
            }
            json_set(out, key, props);
        } else if (strcmp(key, "items") == 0 && val->type == JSON_OBJECT) {
            json_t *sanitized = sanitize_node(val);
            if (sanitized) {
                json_set(out, key, sanitized);
            }
        } else if (strcmp(key, "anyOf") == 0 && val->type == JSON_ARRAY) {
            json_t *arr = json_array();
            for (size_t j = 0; j < val->c.count; j++) {
                json_t *item = val->c.items[j];
                if (item && item->type == JSON_OBJECT) {
                    json_t *sanitized = sanitize_node(item);
                    if (sanitized) {
                        json_append(arr, sanitized);
                    }
                }
            }
            json_set(out, key, arr);
        } else {
            json_t *copy = json_copy(val);
            json_set(out, key, copy);
        }
    }

    /* Drop typed enums — Gemini requires string-only enums */
    json_t *enum_val = json_obj_get(out, "enum");
    json_t *type_val = json_obj_get(out, "type");
    if (enum_val && enum_val->type == JSON_ARRAY && type_val && type_val->type == JSON_STRING) {
        const char *type_str = type_val->str_val;
        if ((strcmp(type_str, "integer") == 0 ||
             strcmp(type_str, "number") == 0 ||
             strcmp(type_str, "boolean") == 0) &&
            enum_has_non_string(enum_val)) {
            /* Can't remove key from object without json_obj_del.
             * Build a new object without the enum key. */
            json_t *cleaned = json_object();
            for (size_t i = 0; i < out->c.count; i++) {
                if (strcmp(out->c.keys[i], "enum") == 0) continue;
                json_t *copy = json_copy(out->c.items[i]);
                json_set(cleaned, out->c.keys[i], copy);
            }
            json_free(out);
            return cleaned;
        }
    }

    return out;
}

static json_t *sanitize_node(json_t *node) {
    if (!node) return NULL;
    switch (node->type) {
        case JSON_OBJECT:
            return sanitize_object(node);
        case JSON_ARRAY: {
            json_t *arr = json_array();
            for (size_t i = 0; i < node->c.count; i++) {
                json_t *sanitized = sanitize_node(node->c.items[i]);
                if (sanitized) {
                    json_append(arr, sanitized);
                    json_free(sanitized);
                } else if (node->c.items[i]) {
                    json_append(arr, node->c.items[i]);
                }
            }
            return arr;
        }
        default:
            return json_copy(node);
    }
}

char *sanitize_gemini_schema(const char *schema_json) {
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

    json_t *sanitized = sanitize_node(parsed);
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

char *sanitize_gemini_tool_parameters(const char *parameters_json) {
    char *cleaned = sanitize_gemini_schema(parameters_json);
    if (!cleaned || strcmp(cleaned, "{}") == 0) {
        free(cleaned);
        return strdup("{\"type\":\"object\",\"properties\":{}}");
    }
    return cleaned;
}
