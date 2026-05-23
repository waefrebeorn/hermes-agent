/*
 * difflib.h — C diff library (J15: Python difflib port).
 *
 * Provides unified diff generation and sequence matching.
 */

#ifndef HERMES_DIFFLIB_H
#define HERMES_DIFFLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max lines in diff output */
#define DIFFLIB_MAX_LINES 4096

/** Max line length */
#define DIFFLIB_MAX_LINE 4096

/**
 * Compute similarity ratio between two strings (0.0 = completely different, 1.0 = identical).
 * Uses longest common subsequence heuristic.
 */
double difflib_ratio(const char *a, const char *b);

/**
 * Generate unified diff between two multi-line strings.
 * Returns malloc'd string. Caller must free().
 * lines: number of context lines (default 3, pass -1 for 3)
 */
char *difflib_unified_diff(const char *a, const char *b, int context_lines);

/**
 * Generate a simplified side-by-side diff (for terminal display).
 * Returns malloc'd string. Caller must free().
 */
char *difflib_simple_diff(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DIFFLIB_H */
