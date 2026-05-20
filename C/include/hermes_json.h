#ifndef HERMES_JSON_H
#define HERMES_JSON_H

/*
 * hermes_json.h — Minimal JSON parser/serializer for Hermes C.
 * No external deps. Supports objects, arrays, strings, numbers,
 * booleans, null. Used for LLM API, config, tool calls.
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* JSON value types */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} json_type_t;

/* Forward declaration */
typedef struct json_node json_node_t;

/* A JSON value node */
struct json_node {
    json_type_t type;
    union {
        bool        bool_val;
        double      num_val;
        char       *str_val;        /* owned string */
        struct {
            json_node_t **items;    /* array elements or object values */
            char       **keys;      /* object keys (NULL for arrays) */
            size_t       count;
        } collection;
    };
};

/* === Parser === */

/* Parse JSON string. Returns NULL on error (sets *error_msg). */
json_node_t *json_parse(const char *input, char **error_msg);

/* Parse JSON from file. Returns NULL on error. */
json_node_t *json_parse_file(const char *path, char **error_msg);

/* === Builder / Accessors === */

/* Create nodes */
json_node_t *json_new_null(void);
json_node_t *json_new_bool(bool val);
json_node_t *json_new_number(double val);
json_node_t *json_new_string(const char *val);
json_node_t *json_new_array(void);
json_node_t *json_new_object(void);

/* Array operations */
void json_array_append(json_node_t *arr, json_node_t *item);
json_node_t *json_array_get(const json_node_t *arr, size_t index);
size_t json_array_count(const json_node_t *arr);

/* Object operations */
void json_object_set(json_node_t *obj, const char *key, json_node_t *val);
json_node_t *json_object_get(const json_node_t *obj, const char *key);
const char *json_object_get_string(const json_node_t *obj, const char *key, const char *def);
double json_object_get_number(const json_node_t *obj, const char *key, double def);
bool json_object_get_bool(const json_node_t *obj, const char *key, bool def);

/* Deep copy */
json_node_t *json_copy(const json_node_t *node);

/* === Serialization === */

/* Serialize node to JSON string. Caller must free result. */
char *json_serialize(const json_node_t *node);

/* Serialize with pretty-print (newlines + indentation). */
char *json_serialize_pretty(const json_node_t *node, int indent);

/* === Memory === */

/* Free a node tree recursively */
void json_free(json_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_JSON_H */
