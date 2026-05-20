#ifndef HERMES_YAML_H
#define HERMES_YAML_H

/*
 * hermes_yaml.h — Minimal YAML config parser for Hermes C.
 * Reads only the subset of YAML used by hermes config.yaml:
 *   - Top-level key: value pairs
 *   - Nested keys via indentation
 *   - String, bool, number values
 *   - Lists with '- ' prefix
 *   - Comments (#...)
 * No anchors, aliases, multi-doc, or complex structures.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct yaml_doc yaml_doc_t;

/* Parse a YAML string. Returns NULL on error. */
yaml_doc_t *yaml_parse(const char *input, char **error_msg);

/* Parse a YAML file. */
yaml_doc_t *yaml_parse_file(const char *path, char **error_msg);

/* Get a string value by dotted path. Returns NULL if not found. */
const char *yaml_get_string(const yaml_doc_t *doc, const char *path);

/* Get a bool value by dotted path. */
bool yaml_get_bool(const yaml_doc_t *doc, const char *path, bool def);

/* Get an integer value by dotted path. */
int yaml_get_int(const yaml_doc_t *doc, const char *path, int def);

/* Get list item count at path. Returns 0 if not a list or not found. */
size_t yaml_list_count(const yaml_doc_t *doc, const char *path);

/* Get list item as string at path[index]. Returns NULL if not found. */
const char *yaml_list_get(const yaml_doc_t *doc, const char *path, size_t index);

/* Iterate all top-level keys. Call fn(key, value) for each. */
void yaml_iterate(const yaml_doc_t *doc,
                  void (*fn)(const char *key, const char *value, void *user),
                  void *user);

/* Free document */
void yaml_free(yaml_doc_t *doc);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_YAML_H */
