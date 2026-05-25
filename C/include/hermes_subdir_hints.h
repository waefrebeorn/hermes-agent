#ifndef HERMES_SUBDIR_HINTS_H
#define HERMES_SUBDIR_HINTS_H

/*
 * hermes_subdir_hints.h — Progressive subdirectory hint discovery.
 * Port of Python agent/subdirectory_hints.py.
 *
 * As the agent navigates into subdirectories via tool calls (read_file,
 * terminal, search_files, etc.), this module discovers and loads project
 * context files (AGENTS.md, CLAUDE.md, .cursorrules) from those directories.
 * Discovered hints are appended to the tool result so the model gets relevant
 * context at the moment it starts working in a new area of the codebase.
 *
 * Thread-safe: uses a mutex for the visited-dirs set.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max number of directories we track */
#define SUBDIR_HINTS_MAX_DIRS 256

/* Max hint file content length */
#define SUBDIR_HINTS_MAX_CHARS 8000

/* How many ancestor directories to walk up when resolving paths */
#define SUBDIR_HINTS_MAX_ANCESTOR_WALK 5

/* Hint filenames to look for, in priority order (only first match per dir) */
#define SUBDIR_HINTS_FILENAMES \
    "AGENTS.md\0agents.md\0CLAUDE.md\0claude.md\0.cursorrules\0"

/*
 * Initialize the subdirectory hint tracker.
 * working_dir: The root project directory (from which hints are loaded).
 *              Must be an absolute path. If NULL, defaults to getcwd().
 */
void subdir_hints_init(const char *working_dir);

/*
 * Clean up all state.
 */
void subdir_hints_cleanup(void);

/*
 * Check a tool call for new directories and load hint files.
 * tool_name: The name of the tool that was called.
 * args_json: The JSON arguments string the tool was called with.
 *
 * Returns a malloc'd formatted hint string to append to the tool result,
 * or NULL if no new hints were found. Caller must free().
 */
char *subdir_hints_check(const char *tool_name, const char *args_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SUBDIR_HINTS_H */
