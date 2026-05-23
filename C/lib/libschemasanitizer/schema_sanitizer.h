#ifndef HERMES_SCHEMA_SANITIZER_H
#define HERMES_SCHEMA_SANITIZER_H

/*
 * schema_sanitizer.h — Tool JSON schema sanitizer for broad LLM-backend compat.
 *
 * Port of Python tools/schema_sanitizer.py.
 *
 * Sanitizes tool JSON schemas for strict backends (llama.cpp, xAI, Codex)
 * that reject shapes OpenAI/Anthropic silently accept. Transforms:
 *   - Bare-string schema values ("object" → {"type": "object"})
 *   - Array-type ("type": ["string", "null"] → "type": "string")
 *   - Object nodes missing properties → inject {}
 *   - Nullable anyOf/oneOf unions → collapse to non-null branch
 *   - Pattern/format keywords (reactive strip for llama.cpp recovery)
 *   - Enum values containing '/' (xAI Responses grammar compat)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Deep-copy and sanitize a JSON array of OpenAI-format tool schemas.
 *
 * @param tools_json  Serialized JSON array of tool schemas, or NULL.
 * @return Malloc'd serialized JSON array (caller must free), or NULL on error.
 */
char *sanitize_tool_schemas(const char *tools_json);

/**
 * Strip "pattern" and "format" keywords from tool schemas (reactive recovery
 * for llama.cpp grammar-parse failures).
 *
 * Operates on a shallow copy (mutates only the JSON parsed from input).
 * @param tools_json     Serialized JSON array of tool schemas.
 * @param stripped_count [out] Receives count of keywords stripped.
 * @return Malloc'd serialized JSON array, or NULL on error.
 */
char *strip_pattern_and_format(const char *tools_json, int *stripped_count);

/**
 * Strip "enum" keywords whose values contain '/' (xAI Responses compat).
 *
 * Operates on a shallow copy.
 * @param tools_json     Serialized JSON array of tool schemas.
 * @param stripped_count [out] Receives count of enum keywords stripped.
 * @return Malloc'd serialized JSON array, or NULL on error.
 */
char *strip_slash_enum(const char *tools_json, int *stripped_count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SCHEMA_SANITIZER_H */
