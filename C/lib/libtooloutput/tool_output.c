/*
 * tool_output.c — Configurable tool-output truncation limits.
 * Port of Python tools/tool_output_limits.py.
 *
 * Reads from HERMES_TOOL_OUTPUT_* env vars. Falls back to hardcoded defaults.
 *
 * MIT License — WuBu Hermes Project
 */

#include "tool_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

static int read_env_int(const char *name, int default_val) {
    const char *val = getenv(name);
    if (!val || !*val) return default_val;
    char *end = NULL;
    long n = strtol(val, &end, 10);
    if (end == val || *end != '\0' || n <= 0 || n > 100000000)
        return default_val;
    return (int)n;
}

/* ================================================================
 *  Public API
 * ================================================================ */

int tool_output_get_max_bytes(void) {
    return read_env_int("HERMES_TOOL_OUTPUT_MAX_BYTES",
                         TOOL_OUTPUT_DEFAULT_MAX_BYTES);
}

int tool_output_get_max_lines(void) {
    return read_env_int("HERMES_TOOL_OUTPUT_MAX_LINES",
                         TOOL_OUTPUT_DEFAULT_MAX_LINES);
}

int tool_output_get_max_line_length(void) {
    return read_env_int("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH",
                         TOOL_OUTPUT_DEFAULT_MAX_LINE_LENGTH);
}

bool tool_output_exceeds_byte_limit(size_t byte_count) {
    return byte_count > (size_t)tool_output_get_max_bytes();
}

bool tool_output_exceeds_line_limit(int line_count) {
    return line_count > tool_output_get_max_lines();
}
