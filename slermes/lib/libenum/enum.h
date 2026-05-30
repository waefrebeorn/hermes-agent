/*
 * enum.h — C enum-to-string helpers (J14: Python enum port).
 *
 * Header-only library. Use X-macro pattern for enum + string tables.
 *
 * Usage:
 *   #define COLOR_ENUM(X) \
 *       X(COLOR_RED, "red") \
 *       X(COLOR_GREEN, "green")
 *   ENUM_DECLARE(color_t, COLOR_ENUM)
 *
 *   const char *name = ENUM_NAME(color_t, COLOR_ENUM, COLOR_RED);  // "red"
 *   color_t val = ENUM_PARSE(color_t, COLOR_ENUM, "green", COLOR_RED);  // COLOR_GREEN
 *   int cnt = ENUM_COUNT(color_t, COLOR_ENUM);  // 2
 *
 * Requires C99 for compound literals.
 */

#ifndef HERMES_ENUM_H
#define HERMES_ENUM_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generate enum value from X-macro */
#define ENUM_VAL(name, str) name,

/* Generate string table entry */
#define ENUM_STR(name, str) str,

/* Generate string table entry using macro name as fallback */
#define ENUM_STR_NAME(name, str) #name,

/* Declare enum type + string table from X-macro list */
#define ENUM_DECLARE(type, names) \
    typedef enum { names(ENUM_VAL) } type; \
    static const char *type##_names[] = { names(ENUM_STR) }; \
    static const int type##_count = sizeof(type##_names) / sizeof(type##_names[0])

/* Get string name for enum value. Returns name string or NULL if out of range. */
#define ENUM_NAME(type, names, val) \
    ((int)(val) >= 0 && (int)(val) < type##_count ? type##_names[(int)(val)] : NULL)

/* Parse string to enum value. Returns matching value or default_val. */
#define ENUM_PARSE(type, names, str, default_val) \
    _enum_parse_impl(type##_names, type##_count, (str), (int)(default_val))

/* Internal: linear search for string match */
static inline int _enum_parse_impl(const char **names, int count,
                                    const char *str, int default_val) {
    if (!str) return default_val;
    for (int i = 0; i < count; i++) {
        if (names[i] && strcmp(names[i], str) == 0)
            return i;
    }
    return default_val;
}

/* Get count of enum values */
#define ENUM_COUNT(type, names) type##_count

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ENUM_H */
