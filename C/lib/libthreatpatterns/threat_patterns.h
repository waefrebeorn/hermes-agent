#ifndef HERMES_THREAT_PATTERNS_H
#define HERMES_THREAT_PATTERNS_H

/*
 * threat_patterns.h — Shared threat-pattern library for context scanning.
 * Port of Python tools/threat_patterns.py.
 *
 * Organizes pattern detection into three scopes:
 *   "all"     — applied everywhere (classic prompt injection, exfiltration)
 *   "context" — applied to context files + memory + tool results
 *   "strict"  — applied to memory writes + skill installs only
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pattern scope */
typedef enum {
    THREAT_SCOPE_ALL = 0,
    THREAT_SCOPE_CONTEXT,
    THREAT_SCOPE_STRICT,
    THREAT_SCOPE_COUNT
} threat_scope_t;

/* Compiled pattern */
typedef struct {
    char   pattern_id[48];   /* e.g. "prompt_injection" */
    int    scope;            /* threat_scope_t value */
    void  *re;              /* compiled regex (hregex_t *) */
} threat_pattern_t;

/* Match result */
typedef struct {
    bool    matched;
    char    pattern_id[48];
    char    match_text[256];  /* first 255 chars of match */
} threat_match_t;

/* ── Public API ── */

/* Initialize the threat pattern library. Compiles all built-in patterns.
 * Returns the number of patterns compiled. */
int threat_patterns_init(void);

/* Check content against all patterns of a given scope (bitmask: 1<<THREAT_SCOPE_*).
 * Returns true if any pattern matched. Sets *match_out with first match details. */
bool threat_patterns_check(const char *content, int scope_mask, threat_match_t *match_out);

/* Check content against ALL scopes. Returns first match. */
bool threat_patterns_check_all(const char *content, threat_match_t *match_out);

/* Get the total number of compiled patterns. */
int threat_patterns_count(void);

/* Free all compiled patterns. */
void threat_patterns_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_THREAT_PATTERNS_H */
