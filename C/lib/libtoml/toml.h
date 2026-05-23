/*
 * toml.h — C TOML parser library (J16: Python tomllib port).
 *
 * Minimal TOML v1.0 compliant parser for config files.
 * Supports: key-value pairs, tables, arrays, strings, numbers, booleans.
 */

#ifndef HERMES_TOML_H
#define HERMES_TOML_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max TOML key depth */
#define TOML_MAX_DEPTH 16

/** Max TOML table entries */
#define TOML_MAX_ENTRIES 512

/** Value types */
typedef enum {
    TOML_STRING,
    TOML_INTEGER,
    TOML_FLOAT,
    TOML_BOOL,
    TOML_ARRAY,
    TOML_TABLE,
    TOML_NULL,
} toml_type_t;

/** A TOML value node */
typedef struct toml_node {
    toml_type_t type;
    char *key;           /* malloc'd key name */
    union {
        char   *string_val;
        long    int_val;
        double  float_val;
        bool    bool_val;
    };
    struct toml_node **children; /* for arrays and tables */
    int    child_count;
    int    child_capacity;
} toml_node_t;

/** Top-level TOML document */
typedef struct {
    toml_node_t *root;  /* table node containing all top-level keys */
} toml_doc_t;

/**
 * Parse TOML string into document.
 * Returns NULL on parse error.
 * Caller must free with toml_free().
 */
toml_doc_t *toml_parse(const char *input);

/**
 * Get value at dotted key path (e.g., "server.host").
 * Returns node or NULL if not found.
 */
toml_node_t *toml_get(toml_doc_t *doc, const char *key_path);

/**
 * Get string value at key path. Returns NULL if not found or wrong type.
 */
const char *toml_get_string(toml_doc_t *doc, const char *key_path);

/**
 * Get integer value at key path. Returns 0 if not found.
 */
long toml_get_int(toml_doc_t *doc, const char *key_path);

/**
 * Get float value at key path. Returns 0.0 if not found.
 */
double toml_get_float(toml_doc_t *doc, const char *key_path);

/**
 * Get boolean value at key path. Returns false if not found.
 */
bool toml_get_bool(toml_doc_t *doc, const char *key_path);

/**
 * Serialize document back to TOML string. Caller free().
 */
char *toml_serialize(toml_doc_t *doc);

/**
 * Free entire document.
 */
void toml_free(toml_doc_t *doc);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOML_H */
