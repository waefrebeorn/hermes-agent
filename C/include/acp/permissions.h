#ifndef HERMES_ACP_PERMISSIONS_H
#define HERMES_ACP_PERMISSIONS_H

/*
 * acp/permissions.h — ACP permission bridging for Hermes C.
 *
 * Bridges ACP permission requests (terminal command approvals) to
 * Hermes' existing approval system. Mirrors Python's
 * acp_adapter/permissions.py.
 *
 * When ACP client (VS Code, Zed) wants to approve/deny a dangerous
 * terminal command, this module:
 *   1. Builds ACP-compatible permission options
 *   2. Sends a permission request notification
 *   3. Wait for the client response via permission_response method
 *   4. Maps the response to a Hermes approval result string
 */

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Permission Option Constants
 * ================================================================ */

/* Max permission options returned */
#define ACP_PERMISSION_MAX_OPTIONS 8

/* Hermes approval result strings (returned by approval callback) */
#define ACP_HERMES_APPROVE_ONCE    "once"
#define ACP_HERMES_APPROVE_SESSION "session"
#define ACP_HERMES_APPROVE_ALWAYS  "always"
#define ACP_HERMES_DENY            "deny"

/* ================================================================
 *  Permission Option Building
 * ================================================================ */

/* Build an ACP permission options array for a given allow_permanent flag.
 * Returns a JSON array of option objects, each with option_id/kind/name fields.
 * Caller must free with json_free(). */
json_node_t *acp_build_permission_options(bool allow_permanent);

/* ================================================================
 *  Permission Tool Call Building
 * ================================================================ */

/* Build an ACP permission tool call (ToolCallUpdate) for a command.
 * Returns a JSON object with tool_call_id, title, kind, status, content.
 * Caller must free with json_free(). */
json_node_t *acp_build_permission_tool_call(const char *command, const char *description);

/* ================================================================
 *  Outcome Mapping
 * ================================================================ */

/* Map an ACP permission outcome option_id to a Hermes approval result string.
 * allowed_option_ids_str is a string containing comma-separated valid option_ids.
 * Returns a pointer to a static string (one of the ACP_HERMES_* constants). */
const char *acp_map_outcome_to_hermes(const char *option_id, const char *allowed_option_ids_str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACP_PERMISSIONS_H */
