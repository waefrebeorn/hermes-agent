/*
 * textwrap.h — C text wrapping library (J11: Python textwrap port).
 *
 * Provides text wrapping, filling, and dedent for CLI output.
 * Thread-safe — no global state.
 */

#ifndef HERMES_TEXTWRAP_H
#define HERMES_TEXTWRAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max line length for wrapping */
#define TEXTWRAP_MAX_LINE 4096

/** Max wrapped lines */
#define TEXTWRAP_MAX_LINES 1024

/** Wrapped result: array of lines */
typedef struct {
    char  *lines[TEXTWRAP_MAX_LINES];
    int    count;
} textwrap_result_t;

/**
 * Wrap text to fit within `width` columns.
 * Breaks on word boundaries. Tabs expanded, runs of spaces collapsed.
 * Returns result struct with individual lines. Caller free()s each line.
 */
textwrap_result_t textwrap_wrap(const char *text, int width);

/**
 * Fill text: wrap + join with newlines. Caller free()s result.
 */
char *textwrap_fill(const char *text, int width);

/**
 * Dedent: remove common leading whitespace from all lines.
 * Caller free()s result.
 */
char *textwrap_dedent(const char *text);

/**
 * Shorten text to fit within max_len, appending "..." if truncated.
 * Caller free()s result.
 */
char *textwrap_shorten(const char *text, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TEXTWRAP_H */
