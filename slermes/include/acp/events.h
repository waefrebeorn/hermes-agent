#ifndef HERMES_ACP_EVENTS_H
#define HERMES_ACP_EVENTS_H

/*
 * acp/events.h — ACP event notification bridge for Hermes C.
 *
 * Mirrors Python's acp_adapter/events.py — creates callback factories
 * that bridge agent events (tool start/complete, thinking, streaming)
 * to ACP notifications over the stdio JSON-RPC transport.
 *
 * These callbacks are installed on agent_state_t when an ACP session
 * is created, and emit ACP session_update notifications that IDEs
 * (VS Code, Zed, Copilot) render in their tool call UI.
 */

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  ACP Event Callback Factories
 * ================================================================ */

/* tool_event_cb implementation: bridges tool.started/completed/failed
 * to ACP ToolCallStart / ToolCallComplete notifications.
 *
 * userdata must be a pointer to the acp_server_t that owns the session.
 * Returns 1 to continue (never suppresses events). */
int acp_tool_event_cb(const char *event_type, const char *tool_name,
                       const char *args_json, void *userdata);

/* ================================================================
 *  Tool Call ID Tracking
 * ================================================================ */

/* Maximum tool call IDs tracked per ACP server (across all sessions) */
#define ACP_EVENTS_MAX_TOOL_CALLS 512

/* Track a tool call ID for a given (session_id, tool_name) pair.
 * Returns a newly allocated tool_call_id string. Must be freed after
 * sending the matching completion notification. */
const char *acp_tool_call_id_register(const char *session_id, const char *tool_name);

/* Pop the oldest tool_call_id for a (session_id, tool_name) pair.
 * Returns NULL if no IDs are tracked. Caller must free. */
char *acp_tool_call_id_pop(const char *session_id, const char *tool_name);

/* ================================================================
 *  ACP Notification Builders
 * ================================================================ */

/* Build a ToolCallStart ACP notification node.
 * Returns a JSON-RPC notification message (caller must free). */
json_node_t *acp_build_tool_start_notification(
    const char *session_id,
    const char *tool_call_id,
    const char *tool_name,
    const char *args_json
);

/* Build a ToolCallComplete ACP notification node.
 * Returns a JSON-RPC notification message (caller must free). */
json_node_t *acp_build_tool_complete_notification(
    const char *session_id,
    const char *tool_call_id,
    const char *tool_name,
    const char *result_json,
    bool failed
);

/* Build a plan_update notification from a todo tool result.
 * Parses the result as JSON, extracts the "todos" array, and
 * builds an ACP-compatible plan update. Returns NULL if the
 * result is not a valid todo update. Caller must free. */
json_node_t *acp_build_plan_update_notification(
    const char *session_id,
    const char *result_json
);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACP_EVENTS_H */
