#ifndef HERMES_TOOL_RESULT_H
#define HERMES_TOOL_RESULT_H

/*
 * hermes_tool_result.h — Tool result classification helpers.
 * Port of Python agent/tool_result_classification.py (26 lines).
 *
 * Checks whether a tool result indicates a successful file mutation
 * (write_file, patch).
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Check if a tool result indicates a successful file mutation.
 * Returns true if the tool_name is write_file or patch and the result
 * contains success indicators.
 * tool_name: "write_file" or "patch"
 * result: the JSON result string from the tool handler */
bool tool_result_file_mutation_landed(const char *tool_name, const char *result);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_RESULT_H */
