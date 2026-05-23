#ifndef HERMES_TOOL_DISPATCH_HELPERS_H
#define HERMES_TOOL_DISPATCH_HELPERS_H

/*
 * tool_dispatch_helpers.h — Stateless tool-dispatch utility functions.
 *
 * Port of Python agent/tool_dispatch_helpers.py:
 *   - is_destructive_command: heuristic for terminal commands that modify files
 *   - extract_error_preview: pull a one-line error summary from tool result
 *   - extract_file_mutation_targets: extract file paths from write_file/patch args
 *   - is_multimodal_tool_result: check for _multimodal envelope
 *   - paths_overlap: check if two filesystem paths refer to same subtree
 *
 * All functions are thread-safe (no global state).
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Destructive command detection
 * ================================================================ */

/* Heuristic: does this terminal command look like it modifies/deletes files?
 * Checks for rm, rmdir, cp, install, mv, sed -i, truncate, dd, shred,
 * git reset/clean/checkout, and output redirects (> single, not >>).
 * Returns true if the command appears destructive. */
bool is_destructive_command(const char *cmd);

/* ================================================================
 *  Error preview extraction
 * ================================================================ */

/* Extract a one-line error summary from a tool result string.
 * If the string is JSON with an "error" field, extracts that value.
 * Collapses whitespace, truncates to max_len.
 * Returns a malloc'd string (caller frees), or NULL if no error found. */
char *extract_error_preview(const char *result, size_t max_len);

/* ================================================================
 *  File mutation target extraction
 * ================================================================ */

/* Known mutating tool names */
#define MUTATING_TOOL_WRITE_FILE "write_file"
#define MUTATING_TOOL_PATCH      "patch"

/* Extract file paths from write_file/patch args JSON.
 * For write_file: extracts "path" field.
 * For patch in "replace" mode: extracts "path" field.
 * For patch in "patch" mode: parses patch body for "*** Update/Add/Delete File:" headers.
 * Returns malloc'd array of strings. count_out receives number of paths.
 * Caller must free each string and the array. */
char **extract_file_mutation_targets(const char *tool_name,
                                      const char *args_json,
                                      size_t *count_out);

/* Free results from extract_file_mutation_targets */
void free_mutation_targets(char **targets, size_t count);

/* ================================================================
 *  Multimodal tool result detection
 * ================================================================ */

/* Check if a JSON tool result string is a multimodal envelope
 * (has _multimodal=True and content array). */
bool is_multimodal_tool_result(const char *result_json);

/* ================================================================
 *  Path overlap detection
 * ================================================================ */

/* Return true when two paths may refer to the same subtree.
 * Both paths should be absolute or relative to same cwd.
 * Does NOT resolve symlinks or check file existence. */
bool paths_overlap(const char *left, const char *right);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_DISPATCH_HELPERS_H */
