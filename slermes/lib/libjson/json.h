#ifndef LIBJSON_H
#define LIBJSON_H

/*
 * libjson.h — Standalone JSON parser/serializer for C.
 * Zero external dependencies. Recursive-descent parser.
 * Supersedes Python 'json' / 'jiter' / partially 'pydantic'.
 *
 * MIT License — WuBu Hermes Project
 * https://github.com/waefrebeorn/slermes
 *
 * Usage:
 *   json_t *doc = json_parse("{\"key\": \"value\"}", NULL);
 *   const char *v = json_get_string(doc, "key", "default");
 *   char *out = json_serialize(doc);
 *   json_free(doc);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Types === */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} json_type_t;

typedef struct json_t json_t;

struct json_t {
    json_type_t type;
    union {
        bool        bool_val;
        double      num_val;
        char       *str_val;        /* owned */
        struct {
            json_t  **items;        /* array/object values */
            char    **keys;         /* object keys, NULL for arrays */
            size_t    count;
        } c;
    };
};

/* === Parser === */
json_t *json_parse(const char *input, char **error_msg);
json_t *json_parse_file(const char *path, char **error_msg);

/* === Builders === */
json_t *json_null(void);
json_t *json_bool(bool val);
json_t *json_number(double val);
json_t *json_string(const char *val);
json_t *json_array(void);
json_t *json_object(void);

/* === Array ops === */
void    json_append(json_t *arr, json_t *item);
json_t *json_get(const json_t *arr, size_t index);
size_t  json_len(const json_t *arr);

/* === Object ops === */
void    json_set(json_t *obj, const char *key, json_t *val);
json_t *json_obj_get(const json_t *obj, const char *key);
const char *json_get_str(const json_t *obj, const char *key, const char *def);
double  json_get_num(const json_t *obj, const char *key, double def);
bool    json_get_bool(const json_t *obj, const char *key, bool def);

/* Check if key exists in object */
static inline bool json_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* === Serialization === */
char   *json_serialize(const json_t *node);
char   *json_serialize_pretty(const json_t *node, int indent);

/* === Copy / Free === */
json_t *json_copy(const json_t *node);
void    json_free(json_t *node);

/* === JSON Pointer (RFC 6901) === */
/** Traverse a JSON tree along a JSON Pointer path (e.g. "/foo/bar/0").
 *  Returns the referenced node, or NULL if path doesn't exist. */
json_t *json_pointer_get(const json_t *root, const char *path);

/* === Error handling === */
/* Check if last OOM occurred. libjson never calls exit(). */
bool    json_oom_occurred(void);

/* === JSON Schema Validation (draft-07 subset) === */
/** Validate a JSON value against a JSON Schema (draft-07 subset).
 *  Supports: type, properties, required, items, enum, minimum/maximum,
 *  minLength/maxLength, minItems/maxItems, description (ignored).
 *  schema: JSON Schema object.
 *  value: JSON value to validate.
 *  error_out: if non-NULL, set to malloc'd error message on failure (caller free).
 *  Returns true if valid, false if invalid. */
bool json_validate_schema(const json_t *schema, const json_t *value, char **error_out);

#ifdef __cplusplus
}
#endif

#endif /* LIBJSON_H */
