/*
 * gemini_schema.h — Gemini Schema sanitizer for tool parameters.
 *
 * Port of Python agent/gemini_schema.py (99 lines).
 * Strips OpenAI-flavored JSON Schema keys that Gemini's Schema object
 * rejects, then recursively sanitizes nested properties/items/anyOf.
 *
 * MIT License — WuBu Hermes Project
 */

#ifndef GEMINI_SCHEMA_H
#define GEMINI_SCHEMA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sanitize a JSON schema string for Gemini compatibility.
 * Strips keys not in Gemini's allowed set, recursively sanitizes
 * nested properties/items/anyOf, and drops typed enums that Gemini
 * rejects.
 *
 * @param schema_json  Input schema as JSON string (OpenAI-flavored)
 * @return             malloc'd JSON string sanitized for Gemini,
 *                     or NULL on error. Caller must free.
 */
char *sanitize_gemini_schema(const char *schema_json);

/**
 * Normalize tool parameters to a valid Gemini object schema.
 * Wraps sanitize_gemini_schema() with fallback to {"type":"object",
 * "properties":{}} when the result would be empty.
 *
 * @param parameters_json  Tool parameters as JSON string
 * @return                 malloc'd JSON string, caller must free
 */
char *sanitize_gemini_tool_parameters(const char *parameters_json);

#ifdef __cplusplus
}
#endif

#endif /* GEMINI_SCHEMA_H */
