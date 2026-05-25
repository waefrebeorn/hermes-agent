#ifndef HERMES_ACP_H
#define HERMES_ACP_H

/*
 * acp.h — ACP (Agent Communication Protocol) server for Hermes C.
 *
 * Implements a JSON-RPC 2.0 server over stdio that exposes
 * the Hermes agent as an ACP-compatible provider. Clients
 * (Copilot, Codex ACP, OpenCode) connect via subprocess.
 *
 * Protocol: JSON-RPC 2.0 with Content-Length framing over stdio.
 * Methods:
 *   initialize                     → InitializeResponse
 *   new_session                    → NewSessionResponse
 *   list_sessions                  → ListSessionsResponse
 *   load_session                   → LoadSessionResponse
 *   resume_session                 → ResumeSessionResponse
 *   available_commands             → AvailableCommandsUpdate
 *   set_session_model              → SetSessionModelResponse
 *   set_session_config_option      → SetSessionConfigOptionResponse
 *   set_session_mode               → SetSessionModeResponse
 */

#include "hermes.h"
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  ACP Protocol constants
 * ================================================================ */

#define ACP_PROTOCOL_VERSION "2025-03-26"
#define ACP_SERVER_NAME      "hermes-c"
#define ACP_SERVER_VERSION   "0.14.1-wubu"

/* Max JSON-RPC message size (10 MB) */
#define ACP_MAX_MESSAGE_BYTES (10 * 1024 * 1024)

/* ================================================================
 *  ACP Server
 * ================================================================ */

typedef struct acp_server_t acp_server_t;

/* Create/destroy ACP server */
acp_server_t *acp_server_new(void);
void acp_server_free(acp_server_t *srv);

/* Run the ACP event loop: read from stdin, process, write to stdout.
 * Logs go to stderr. Returns on EOF or fatal error. */
void acp_server_run(acp_server_t *srv);

/* ================================================================
 *  JSON-RPC framing (Content-Length headers over stdio)
 * ================================================================ */

/* Read a JSON-RPC message from stdin. Returns parsed JSON, or NULL on EOF/error.
 * Caller must free with json_free(). */
json_node_t *acp_read_message(void);

/* Write a JSON-RPC message to stdout with Content-Length framing.
 * Returns true on success. */
bool acp_write_message(json_node_t *msg);

/* ================================================================
 *  JSON-RPC helpers
 * ================================================================ */

/* Build a JSON-RPC success response */
json_node_t *acp_make_response(const char *id, json_node_t *result);

/* Build a JSON-RPC error response */
json_node_t *acp_make_error(const char *id, int code, const char *message, json_node_t *data);

/* Extract method string from a JSON-RPC request */
const char *acp_get_method(json_node_t *req);

/* Extract id string from a JSON-RPC request (returns NULL for notifications) */
const char *acp_get_id(json_node_t *req);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACP_H */
