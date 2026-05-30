/*
 * tool_coerce.c — Tool argument type coercion implementation.
 * Port of Python model_tools _coerce_number() / _coerce_boolean().
 *
 * LLMs frequently return numbers as strings and booleans as strings.
 * These functions parse string-typed JSON values back into their
 * expected types so the tool dispatcher receives correctly-typed args.
 */

#include "hermes_tool_coerce.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Number coercion
 * ================================================================ */

bool tool_coerce_number(const char *value, double *out_value, bool *out_is_int, bool integer_only)
{
    if (!value || !out_value || !out_is_int)
        return false;

    /* Skip leading whitespace */
    while (*value && isspace((unsigned char)*value))
        value++;

    if (!*value)
        return false;

    /* strtod handles: [+-]?[0-9]+(\.[0-9]*)?([eE][+-]?[0-9]+)? */
    char *end = NULL;
    double result = strtod(value, &end);

    /* Check for parse failure */
    if (end == value)
        return false;  /* no digits consumed */

    /* Skip trailing whitespace — must consume the whole string */
    const char *p = end;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        return false;  /* trailing non-whitespace characters */

    /* Guard against inf/nan — not JSON-serializable */
    if (isnan(result) || isinf(result))
        return false;

    /* Check if it looks like an integer (no fractional part) */
    bool is_int = (result == floor(result) && !isinf(result));
    *out_is_int = is_int;

    /* If integer_only and value has decimals, reject */
    if (integer_only && !is_int)
        return false;

    *out_value = result;
    return true;
}

/* ================================================================
 *  Boolean coercion
 * ================================================================ */

bool tool_coerce_boolean(const char *value, bool *out_bool)
{
    if (!value || !out_bool)
        return false;

    /* Skip leading whitespace */
    while (*value && isspace((unsigned char)*value))
        value++;

    if (!*value)
        return false;

    /* Check for "true" */
    if ((value[0] == 't' || value[0] == 'T') &&
        (value[1] == 'r' || value[1] == 'R') &&
        (value[2] == 'u' || value[2] == 'U') &&
        (value[3] == 'e' || value[3] == 'E'))
    {
        const char *p = value + 4;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0') {
            *out_bool = true;
            return true;
        }
    }

    /* Check for "false" */
    if ((value[0] == 'f' || value[0] == 'F') &&
        (value[1] == 'a' || value[1] == 'A') &&
        (value[2] == 'l' || value[2] == 'L') &&
        (value[3] == 's' || value[3] == 'S') &&
        (value[4] == 'e' || value[4] == 'E'))
    {
        const char *p = value + 5;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0') {
            *out_bool = false;
            return true;
        }
    }

    return false;  /* not a recognized boolean string */
}
