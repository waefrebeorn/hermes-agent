#ifndef HERMES_FUZZY_MATCH_H
#define HERMES_FUZZY_MATCH_H

/*
 * fuzzy_match.h — Multi-strategy fuzzy matching for find-and-replace.
 *
 * Port of Python tools/fuzzy_match.py (8-strategy matching chain).
 *
 * Implements a multi-strategy matching chain to robustly find and replace
 * text, accommodating variations in whitespace, indentation, and escaping
 * common in LLM-generated code.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Result of a fuzzy find-and-replace operation.
 */
typedef struct {
    char *content;       /**< Resulting content (malloc'd), or NULL on error. */
    int   match_count;   /**< Number of matches found and replaced. */
    int   strategy;      /**< Index of the strategy that matched (1-8), or 0 if none. */
    char *error;         /**< Error message (malloc'd), or NULL if success. */
} fuzzy_result_t;

/**
 * Perform multi-strategy fuzzy find-and-replace.
 *
 * Tries 8 strategies in order; returns on first successful match.
 *
 * Strategies:
 *   1. Exact match          — Direct strstr comparison
 *   2. Line-trimmed         — Strip leading/trailing whitespace per line
 *   3. Whitespace normalized — Collapse multiple spaces/tabs to single space
 *   4. Indentation flexible  — Ignore indentation differences entirely
 *   5. Escape normalized     — Convert \\n literals to actual newlines
 *   6. Trimmed boundary      — Trim first/last line whitespace only
 *   7. Block anchor          — Match first+last lines, ratio for middle
 *   8. Context-aware         — 50% line similarity threshold
 *
 * @param content      The full text content to search in.
 * @param old_string   The string to find.
 * @param new_string   The replacement string.
 * @param replace_all  If true, replace ALL occurrences; if false, replace first only.
 * @return fuzzy_result_t with result content, match count, strategy, or error.
 *         Caller must free content and error fields.
 */
fuzzy_result_t fuzzy_find_and_replace(const char *content,
                                       const char *old_string,
                                       const char *new_string,
                                       bool replace_all);

/**
 * Free a fuzzy_result_t (frees content and error fields if non-NULL).
 */
void fuzzy_result_free(fuzzy_result_t *result);

/**
 * Compute similarity ratio between two strings (0.0 = different, 1.0 = identical).
 * Uses longest common subsequence on lines.
 */
double fuzzy_ratio(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FUZZY_MATCH_H */
