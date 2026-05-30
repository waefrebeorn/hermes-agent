#ifndef HERMES_MARKDOWN_TABLES_H
#define HERMES_MARKDOWN_TABLES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Split a table row like "| a | b | c |" into an array of cell strings.
 *
 * @param row      Input row (may be NULL).
 * @param out_len  Output: number of cells.
 * @return Newly allocated array of malloc'd strings. Caller must free each
 *         element and the array itself. NULL on error or empty input.
 */
char **hermes_split_table_row(const char *row, size_t *out_len);

/**
 * True when row is a markdown table separator line (e.g. "|---|---|").
 */
bool hermes_is_table_divider(const char *row);

/**
 * True when row could plausibly be a markdown table row.
 */
bool hermes_looks_like_table_row(const char *row);

/**
 * Rewrite every "| ... |" + divider block with wcwidth-aware padding.
 *
 * Lines outside recognised tables are returned verbatim.
 * If available_width > 0, tables wider than that are rendered as
 * vertical key-value pairs.
 *
 * @param text             Input text (may be NULL).
 * @param available_width  Terminal width (0 = don't wrap).
 * @return Newly allocated string. Caller must free().
 */
char *hermes_realign_markdown_tables(const char *text, int available_width);

/**
 * Display width of a string using wcwidth (clamped to non-negative).
 */
int hermes_disp_width(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MARKDOWN_TABLES_H */
