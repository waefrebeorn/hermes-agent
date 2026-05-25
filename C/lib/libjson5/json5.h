#ifndef LIBJSON5_H
#define LIBJSON5_H

/*
 * libjson5.h -- JSON5 parser for C.
 * Preprocesses JSON5 input into standard JSON, then delegates to libjson.
 * Supports: // and slash-star-star-slash comments, trailing commas,
 *           single-quoted strings, unquoted keys, hex/octal/bin numbers.
 *
 * MIT License - WuBu Hermes Project
 *
 * Usage:
 *   json_t *doc = json5_parse("{ key: 'value', }", NULL);
 *   json5_free(doc);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare json_t from libjson */
typedef struct json_t json_t;

/* === Parser === */
json_t *json5_parse(const char *input, char **error_msg);

/* === Free (alias for json_free) === */
void json5_free(json_t *node);

#ifdef __cplusplus
}
#endif

#endif /* LIBJSON5_H */
