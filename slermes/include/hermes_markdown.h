/**
 * @file hermes_markdown.h
 * @brief Render markdown text to ANSI-escaped terminal output.
 *
 * Converts common markdown elements to ANSI escape sequences:
 *   - # headers → colored bold
 *   - **bold** → bold
 *   - *italic* → italic
 *   - `code` → color background
 *   - ```fenced blocks → formatted
 *   - [links](url) → underlined color
 *   - lists → bullets
 *   - blockquotes → dim prefix
 *   - tables → passes through (handled by markdown_tables.c)
 *
 * Returns a heap-allocated string with ANSI escapes embedded.
 * Caller must free the result.
 */

#ifndef HERMES_MARKDOWN_H
#define HERMES_MARKDOWN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render markdown text to ANSI-formatted terminal output.
 *
 * @param md       Null-terminated markdown input string
 * @param term_width Terminal width in columns (0 = use default 80)
 * @return Heap-allocated string with ANSI escapes, or NULL on OOM.
 *         Caller must free() the result.
 */
char *hermes_markdown_render(const char *md, int term_width);

/**
 * Render markdown to plain text (strip markdown syntax, no ANSI).
 * Useful for non-TTY output or JSON mode.
 *
 * @param md       Null-terminated markdown input
 * @return Heap-allocated plain text string, or NULL on OOM.
 *         Caller must free() the result.
 */
char *hermes_markdown_strip(const char *md);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MARKDOWN_H */
