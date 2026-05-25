#ifndef LIBTOOLOUTPUT_H
#define LIBTOOLOUTPUT_H

/*
 * libtooloutput.h — Configurable tool-output truncation limits.
 * Port of Python tools/tool_output_limits.py.
 *
 * Reads tool_output config from environment (HERMES_TOOL_OUTPUT_* env vars)
 * and falls back to hardcoded defaults matching the Python values:
 *   max_bytes:        50000  (terminal stdout/stderr cap)
 *   max_lines:        2000   (read_file pagination cap)
 *   max_line_length:  2000   (per-line truncation cap)
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Defaults === */
#define TOOL_OUTPUT_DEFAULT_MAX_BYTES       50000
#define TOOL_OUTPUT_DEFAULT_MAX_LINES       2000
#define TOOL_OUTPUT_DEFAULT_MAX_LINE_LENGTH 2000

/* === Public API === */

/* Get max bytes (terminal output cap). Reads HERMES_TOOL_OUTPUT_MAX_BYTES. */
int tool_output_get_max_bytes(void);

/* Get max lines (read_file pagination cap). Reads HERMES_TOOL_OUTPUT_MAX_LINES. */
int tool_output_get_max_lines(void);

/* Get max line length (per-line truncation cap). Reads HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH. */
int tool_output_get_max_line_length(void);

/* Check if a byte count exceeds the output limit */
bool tool_output_exceeds_byte_limit(size_t byte_count);

/* Check if a line count exceeds the line limit */
bool tool_output_exceeds_line_limit(int line_count);

#ifdef __cplusplus
}
#endif

#endif /* LIBTOOLOUTPUT_H */
