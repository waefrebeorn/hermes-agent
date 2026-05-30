/*
 * csv.h — C CSV parsing/writing library (J06: Python csv port).
 *
 * Public API for CSV row read/write with configurable delimiter,
 * quoting, and escape handling. Follows RFC 4180 conventions.
 * All returned strings are malloc'd — caller must free().
 */

#ifndef HERMES_CSV_H
#define HERMES_CSV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────────── */

#define CSV_MAX_FIELDS   256   /* max fields per row */
#define CSV_MAX_LINE     65536 /* max bytes per line */

/* ─── Writer ─────────────────────────────────────────────── */

/** csv_writer_t — Opaque CSV writer handle.
 *  Created by csv_writer_open(), destroyed by csv_writer_close(). */
typedef struct csv_writer_t csv_writer_t;

/** csv_writer_open(path, delimiter) — Open CSV file for writing.
 *  delimiter: ',' for standard CSV, '\t' for TSV, etc.
 *  Returns NULL on error (file open failure). */
csv_writer_t *csv_writer_open(const char *path, char delimiter);

/** csv_writer_open_file(fp, delimiter) — Open CSV writer on existing FILE*.
 *  Does NOT take ownership of fp (caller must fclose). */
csv_writer_t *csv_writer_open_file(FILE *fp, char delimiter);

/** csv_writer_write_row(w, fields, count) — Write one CSV row.
 *  Each field is properly escaped (quoted if contains delimiter/quote/newline).
 *  Returns true on success. */
bool csv_writer_write_row(csv_writer_t *w, const char **fields, int count);

/** csv_writer_write_row_va(w, count, ...) — Write row with variadic args.
 *  Example: csv_writer_write_row_va(w, 3, "a", "b", "c");
 *  Returns true on success. */
bool csv_writer_write_row_va(csv_writer_t *w, int count, ...);

/** csv_writer_close(w) — Flush and close CSV writer. Frees all resources.
 *  Safe to call with NULL. */
void csv_writer_close(csv_writer_t *w);

/* ─── Reader ─────────────────────────────────────────────── */

/** csv_reader_t — Opaque CSV reader handle.
 *  Created by csv_reader_open(), destroyed by csv_reader_close(). */
typedef struct csv_reader_t csv_reader_t;

/** csv_reader_open(path, delimiter) — Open CSV file for reading.
 *  Returns NULL on error. */
csv_reader_t *csv_reader_open(const char *path, char delimiter);

/** csv_reader_open_file(fp, delimiter) — Open CSV reader on existing FILE*.
 *  Does NOT take ownership of fp. */
csv_reader_t *csv_reader_open_file(FILE *fp, char delimiter);

/** csv_reader_open_string(data, len, delimiter) — Open CSV reader on string.
 *  Makes a copy of the data. len=0 means strlen(data). */
csv_reader_t *csv_reader_open_string(const char *data, size_t len, char delimiter);

/** csv_reader_read_row(r) — Read next row. Returns fields array (malloc'd).
 *  Sets *count to number of fields. Returns NULL at EOF or on error.
 *  Caller must free each field AND the array. */
char **csv_reader_read_row(csv_reader_t *r, int *count);

/** csv_reader_row_number(r) — Current row number (1-indexed). 0 if no rows read. */
int csv_reader_row_number(csv_reader_t *r);

/** csv_reader_close(r) — Close CSV reader. Frees all resources.
 *  Safe to call with NULL. */
void csv_reader_close(csv_reader_t *r);

/* ─── String helpers (no FILE I/O) ───────────────────────── */

/** csv_escape(field, delimiter) — Escape a single CSV field.
 *  Wraps in quotes if needed. Caller free(). */
char *csv_escape(const char *field, char delimiter);

/** csv_unescape(field, delimiter) — Unescape a single CSV field.
 *  Removes surrounding quotes and unescapes internal quotes.
 *  Caller free(). */
char *csv_unescape(const char *field, char delimiter);

/** csv_parse_line(line, delimiter, *count) — Parse one CSV line.
 *  Returns malloc'd fields array (*count = number of fields).
 *  Caller must free each field AND the array.
 *  Returns NULL on error (unterminated quote). */
char **csv_parse_line(const char *line, char delimiter, int *count);

/** csv_free_fields(fields, count) — Free fields array returned by read/parse.
 *  Safe to call with NULL. */
void csv_free_fields(char **fields, int count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CSV_H */
