/*
 * moonshot_schema.h — Moonshot/Kimi schema sanitizer for tool parameters.
 *
 * Port of Python agent/moonshot_schema.py (262 lines).
 * Sanitizes OpenAI-flavored JSON Schema for Moonshot's stricter subset,
 * handling: missing type repair, anyOf/type conflict resolution,
 * enum cleanup, $ref sibling stripping, and tuple items collapsing.
 *
 * MIT License — WuBu Slermes Project
 */

#ifndef MOONSHOT_SCHEMA_H
#define MOONSHOT_SCHEMA_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sanitize a JSON schema string for Moonshot/Kimi compatibility.
 * Applies all 5 known rejection-mode fixes.
 *
 * @param schema_json  Input schema as JSON string
 * @return             malloc'd sanitized JSON string, caller must free
 */
char *sanitize_moonshot_schema(const char *schema_json);

/**
 * Normalize tool parameters to a valid Moonshot object schema.
 * Wraps sanitize_moonshot_schema() with fallback.
 */
char *sanitize_moonshot_tool_parameters(const char *parameters_json);

#ifdef __cplusplus
}
#endif

#endif /* MOONSHOT_SCHEMA_H */
