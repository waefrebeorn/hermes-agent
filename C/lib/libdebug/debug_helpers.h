#ifndef HERMES_DEBUG_HELPERS_H
#define HERMES_DEBUG_HELPERS_H

/*
 * debug_helpers.h — Per-tool debug session logging for Hermes C.
 * Port of Python tools/debug_helpers.py.
 *
 * Provides DebugSession infrastructure: each tool can create its own
 * debug session, log calls, and save to JSON files when the tool-specific
 * env var is enabled (e.g. WEB_TOOLS_DEBUG=true).
 *
 * When disabled, all operations are cheap no-ops.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** Maximum debug calls per session */
#define DEBUG_MAX_CALLS 256

/** Maximum string length for call data */
#define DEBUG_MAX_ENTRY_LEN 4096

/**
 * A single debug call entry.
 */
typedef struct {
    char timestamp[64];    /**< ISO-8601 timestamp */
    char tool_name[128];   /**< Tool/module name */
    char call_data[DEBUG_MAX_ENTRY_LEN]; /**< JSON-like call data string */
} debug_call_t;

/**
 * Per-tool debug session state.
 */
typedef struct {
    char session_id[64];  /**< UUID session identifier */
    bool  enabled;         /**< Whether debug mode is active */
    char  tool_name[64];   /**< Tool name for this session */
    char  start_time[64];  /**< ISO-8601 session start time */
    char  log_dir[512];    /**< Directory for debug log files */
    debug_call_t calls[DEBUG_MAX_CALLS];
    int   call_count;
} debug_session_t;

/**
 * Initialize a debug session for a specific tool.
 * Debug mode is enabled if the tool-specific env var is set to "true".
 *
 * @param session    Pointer to uninitialized debug_session_t.
 * @param tool_name  Tool/module name (e.g. "web_tools").
 * @param env_var    Env var name to check (e.g. "WEB_TOOLS_DEBUG").
 * @param hermes_home Path to HERMES_HOME (for logs/ directory).
 */
void debug_session_init(debug_session_t *session,
                         const char *tool_name,
                         const char *env_var,
                         const char *hermes_home);

/**
 * Check if debug mode is active for this session.
 */
bool debug_session_active(const debug_session_t *session);

/**
 * Log a tool call with its data.
 * No-op when debug mode is disabled.
 *
 * @param session    Debug session.
 * @param call_name  Name of the tool call.
 * @param call_data  JSON-like data string or NULL.
 */
void debug_session_log_call(debug_session_t *session,
                             const char *call_name,
                             const char *call_data);

/**
 * Save the in-memory debug log to a JSON file in the logs directory.
 * No-op when debug mode is disabled.
 *
 * @param session  Debug session.
 * @param hermes_home Path to HERMES_HOME (for logs/ directory).
 */
void debug_session_save(debug_session_t *session,
                         const char *hermes_home);

/**
 * Get a JSON string with session info for tool response.
 * Caller must free the returned string.
 * Returns NULL when debug mode is disabled.
 *
 * @param session  Debug session.
 * @return  JSON string or NULL (caller must free).
 */
char *debug_session_get_info(const debug_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DEBUG_HELPERS_H */
