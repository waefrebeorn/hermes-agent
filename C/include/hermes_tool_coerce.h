#ifndef HERMES_TOOL_COERCE_H
#define HERMES_TOOL_COERCE_H

/*
 * hermes_tool_coerce.h — Tool argument type coercion for Hermes C.
 * Port of Python model_tools _coerce_number / _coerce_boolean.
 *
 * LLMs frequently return numbers as strings ("42" instead of 42)
 * and booleans as strings ("true" instead of true). These functions
 * coerce string-typed arguments to their expected JSON types.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Try to parse value as a number.
 * Returns NULL and sets *out_value / *out_is_int on success.
 *
 * @param value        Input string (null-terminated).
 * @param out_value    Set to the parsed number on success.
 * @param out_is_int   Set to true if the number has no fractional part.
 * @param integer_only When true, values with decimals are rejected.
 * @return true if coercion succeeded, false if value was kept as string.
 *
 * Port of Python _coerce_number().
 */
bool tool_coerce_number(const char *value, double *out_value, bool *out_is_int, bool integer_only);

/**
 * Try to parse value as a boolean.
 * Accepts: "true" / "false" (case-insensitive, with leading/trailing whitespace).
 *
 * @param value     Input string.
 * @param out_bool  Set to the parsed boolean on success.
 * @return true if coercion succeeded, false if value was kept as string.
 *
 * Port of Python _coerce_boolean().
 */
bool tool_coerce_boolean(const char *value, bool *out_bool);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_COERCE_H */
