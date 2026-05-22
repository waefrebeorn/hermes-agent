#ifndef LIBMCP_H
#define LIBMCP_H

/*
 * libmcp.h — MCP (Model Context Protocol) client library for C.
 *
 * Implements the Model Context Protocol for discovering and calling
 * tools from external MCP servers. Supports stdio and SSE transport.
 *
 * JSON-RPC 2.0 + MCP protocol messages:
 *   - initialize / initialized
 *   - ping
 *   - tools/list
 *   - tools/call
 *   - resources/list / resources/read
 *   - prompts/list / prompts/get
 *   - logging/setLevel
 *
 * Usage:
 *   mcp_server_t *srv = mcp_server_new("my-server");
 *   mcp_server_set_stdio(srv, "npx", (char*[]){"-y","server",NULL});
 *   mcp_server_set_timeout(srv, 120);
 *   bool ok = mcp_server_connect(srv);
 *   mcp_tool_t *tools = NULL;
 *   int count = mcp_server_list_tools(srv, &tools);
 *   for (int i = 0; i < count; i++)
 *       printf("Tool: %s - %s\n", tools[i].name, tools[i].description);
 *   mcp_tool_list_free(tools, count);
 *   mcp_server_disconnect(srv);
 *   mcp_server_free(srv);
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Version
 * ================================================================ */
#define MCP_PROTOCOL_VERSION "2025-03-26"
#define MCP_JSONRPC_VERSION  "2.0"

/* ================================================================
 *  JSON-RPC types
 * ================================================================ */

/* JSON-RPC error codes */
typedef enum {
    MCP_ERR_PARSE       = -32700,
    MCP_ERR_INVALID_REQ = -32600,
    MCP_ERR_METHOD      = -32601,
    MCP_ERR_PARAMS      = -32602,
    MCP_ERR_INTERNAL    = -32603,
    MCP_ERR_SERVER      = -32000,  /* Server error range: -32000 to -32099 */
} mcp_error_code_t;

/* JSON-RPC message types */
typedef enum {
    MCP_MSG_REQUEST,
    MCP_MSG_RESPONSE,
    MCP_MSG_NOTIFICATION,
    MCP_MSG_ERROR,
} mcp_msg_type_t;

/* MCP server capabilities */
typedef struct {
    bool supports_tools;      /* tools/list and tools/call */
    bool supports_resources;  /* resources/list and resources/read */
    bool supports_prompts;    /* prompts/list and prompts/get */
    bool supports_logging;    /* logging/setLevel */
    bool supports_sampling;   /* sampling/createMessage */
} mcp_capabilities_t;

/* ================================================================
 *  Tool types (discovered via tools/list)
 * ================================================================ */

/* An input schema property for an MCP tool */
typedef struct {
    char  name[128];
    char  type[32];        /* string, integer, boolean, number, array, object */
    char  description[512];
    bool  required;
} mcp_tool_param_t;

/* An MCP tool discovered from a server */
typedef struct {
    char                name[128];
    char                description[1024];
    char                input_schema[4096];  /* JSON schema string */
    mcp_tool_param_t    params[32];          /* parsed from schema */
    int                 param_count;
} mcp_tool_t;

/* Free a tool list returned by mcp_server_list_tools */
void mcp_tool_list_free(mcp_tool_t *tools, int count);

/* ================================================================
 *  Resource types (P67)
 * ================================================================ */

/* An MCP resource discovered from a server */
typedef struct {
    char                uri[512];
    char                name[256];
    char                description[1024];
    char                mime_type[64];
} mcp_resource_t;

/* An MCP resource content (result of resources/read) */
typedef struct {
    char                uri[512];
    char                mime_type[64];
    char               *text;           /* malloc'd text content */
    size_t              text_len;
    bool                is_text;        /* false = binary blob */
    unsigned char      *blob;
    size_t              blob_len;
} mcp_resource_content_t;

/* Free a resource list returned by mcp_server_list_resources */
void mcp_resource_list_free(mcp_resource_t *resources, int count);

/* Free resource content returned by mcp_server_read_resource */
void mcp_resource_content_free(mcp_resource_content_t *content);

/* ================================================================
 *  Prompt types (P69)
 * ================================================================ */

/* An MCP prompt template discovered from a server */
typedef struct {
    char                name[128];
    char                description[1024];
    char                arguments_schema[4096]; /* JSON schema string */
} mcp_prompt_t;

/* Free a prompt list returned by mcp_server_list_prompts */
void mcp_prompt_list_free(mcp_prompt_t *prompts, int count);

/* ================================================================
 *  Server state
 * ================================================================ */

/* Transport type */
typedef enum {
    MCP_TRANSPORT_NONE,
    MCP_TRANSPORT_STDIO,
    MCP_TRANSPORT_SSE,
} mcp_transport_type_t;

/* Opaque server handle */
typedef struct mcp_server mcp_server_t;

/* ================================================================
 *  Server lifecycle
 * ================================================================ */

/* Create a new MCP server config. name is used for logging/display. */
mcp_server_t *mcp_server_new(const char *name);

/* Set stdio transport: spawn command with args */
void mcp_server_set_stdio(mcp_server_t *srv, const char *command, char **args);

/* Set SSE transport: connect to URL (P62+) */
void mcp_server_set_sse(mcp_server_t *srv, const char *url);

/* Set optional environment variables for stdio transport (NULL-terminated array) */
void mcp_server_set_env(mcp_server_t *srv, char **env);

/* Set per-server timeouts */
void mcp_server_set_timeout(mcp_server_t *srv, int tool_timeout_sec);
void mcp_server_set_connect_timeout(mcp_server_t *srv, int connect_timeout_sec);

/* P61: Set max reconnect attempts (0 = no reconnect, -1 = infinite) */
void mcp_server_set_max_retries(mcp_server_t *srv, int max_retries);

/* Set HTTP headers for SSE transport */
void mcp_server_set_headers(mcp_server_t *srv, const char *headers);

/* Connect to MCP server, perform initialization handshake.
 * Returns true on success. */
bool mcp_server_connect(mcp_server_t *srv);

/* Disconnect from server, clean up transport */
void mcp_server_disconnect(mcp_server_t *srv);

/* Free server (calls disconnect first) */
void mcp_server_free(mcp_server_t *srv);

/* ================================================================
 *  Root types (P70)
 * ================================================================ */

#define MAX_MCP_ROOTS 32

/* A root URI (workspace directory) to expose to MCP servers */
typedef struct {
    char                uri[512];     /* file:///path/to/dir */
    char                name[256];    /* optional display name */
} mcp_root_t;

/* P70: Set workspace roots for a server (server-to-client capability).
 * roots: array of mcp_root_t, count: number of roots.
 * Call before mcp_server_connect(). */
void mcp_server_set_roots(mcp_server_t *srv, const mcp_root_t *roots, int count);

/* C08-C10: Dynamic roots management. Add, remove, or list workspace roots. */
/* Add a root URI. Returns true on success. */
bool mcp_server_add_root(mcp_server_t *srv, const char *uri, const char *name);
/* Remove a root by URI. Returns true if found and removed. */
bool mcp_server_remove_root(mcp_server_t *srv, const char *uri);
/* Get the root count. */
int  mcp_server_root_count(mcp_server_t *srv);
/* Get root at index. Returns NULL if out of range. */
const mcp_root_t *mcp_server_get_root(mcp_server_t *srv, int index);

/* Handle a roots/list request from the server.
 * Returns JSON-RPC response with root URIs. */
char *mcp_server_handle_roots_request(mcp_server_t *srv);

/* ================================================================
 *  MCP Protocol calls
 * ================================================================ */

/* Ping — health check. Returns true if server responds. */
bool mcp_server_ping(mcp_server_t *srv);

/* List tools — returns array of discovered tools.
 * tools_out: set to malloc'd array, caller must free with mcp_tool_list_free().
 * Returns number of tools, or -1 on error. */
int  mcp_server_list_tools(mcp_server_t *srv, mcp_tool_t **tools_out);

/* Call a tool. Returns JSON result string (malloc'd, caller frees).
 * Returns NULL on error (check mcp_server_last_error for details). */
char *mcp_server_call_tool(mcp_server_t *srv, const char *tool_name,
                            const char *args_json);

/* ================================================================
 *  P67: Resource access
 * ================================================================ */

/* List resources — returns array of discovered resources.
 * resources_out: set to malloc'd array, caller must free with mcp_resource_list_free().
 * Returns number of resources, or -1 on error. */
int  mcp_server_list_resources(mcp_server_t *srv, mcp_resource_t **resources_out);

/* Read a specific resource by URI.
 * Returns malloc'd content struct, caller must free with mcp_resource_content_free().
 * Returns NULL on error (check mcp_server_last_error). */
mcp_resource_content_t *mcp_server_read_resource(mcp_server_t *srv,
                                                   const char *resource_uri);

/* C01-C03: Resource subscriptions. Subscribe to resource change notifications. */
/* Subscribe to resource changes. Returns true on success. */
bool mcp_server_subscribe_resource(mcp_server_t *srv, const char *resource_uri);
/* Unsubscribe from resource changes. Returns true on success. */
bool mcp_server_unsubscribe_resource(mcp_server_t *srv, const char *resource_uri);
/* Check if a resource URI is currently subscribed. */
bool mcp_server_is_subscribed(mcp_server_t *srv, const char *resource_uri);
/* Set callback for resource change notifications.
 * The callback receives server_name, resource_uri, and userdata pointer. */
void mcp_server_set_resource_callback(mcp_server_t *srv,
    void (*callback)(const char *server_name, const char *resource_uri, void *userdata),
    void *userdata);

/* C03: Handle an incoming notification from the server.
 * Dispatches to appropriate callback. Returns true if handled. */
bool mcp_server_handle_notification(mcp_server_t *srv, const char *method,
                                     const char *params_json);

/* ================================================================
 *  C04-C05: Sampling protocol
 * ================================================================ */

/* Maximum pending incoming requests (server→client) */
#define MCP_MAX_INCOMING 8

/* Sampling message content struct (result of LLM call) */
typedef struct {
    char  role[16];          /* "assistant" */
    char  type[16];          /* "text" */
    char  text[16384];       /* content text */
} mcp_sampling_content_t;

/* Sampling parameters from server request */
typedef struct {
    char  system_prompt[4096];    /* optional system prompt */
    char  messages[16384];        /* JSON array of messages to sample */
    int   max_tokens;
    char  model_preference[64];   /* optional model hint */
    double temperature;           /* 0=disabled */
    char  stop_sequences[1024];   /* JSON array of stop sequences */
    bool  include_context;        /* include MCP context */
} mcp_sampling_params_t;

/* Callback for handling sampling/createMessage requests from server.
 * Receives server name, sampling params, and userdata.
 * Must call mcp_server_sampling_respond() to send the response.
 * Return true if handled, false to send error response. */
typedef bool (*mcp_sampling_callback_t)(const char *server_name,
                                         const mcp_sampling_params_t *params,
                                         mcp_sampling_content_t *result,
                                         void *userdata);

/* Set sampling callback for a server. Must be called before connect. */
void mcp_server_set_sampling_callback(mcp_server_t *srv,
                                       mcp_sampling_callback_t callback,
                                       void *userdata);

/* Send a sampling/createMessage response back to the server.
 * Returns true if the response was sent successfully. */
bool mcp_server_sampling_respond(mcp_server_t *srv, const char *request_id,
                                  const mcp_sampling_content_t *content,
                                  const char *model);

/* Send a sampling/notify notification to the server.
 * Notifies the server of a completed sampling operation without
 * an explicit request. */
bool mcp_server_sampling_notify(mcp_server_t *srv,
                                 const mcp_sampling_content_t *content,
                                 const char *model);

/* Process any pending incoming server→client requests (including sampling).
 * Call periodically to handle incoming requests. Returns number handled. */
int mcp_server_process_incoming(mcp_server_t *srv);

/* ================================================================
 *  P69: Prompt templates
 * ================================================================ */

/* List prompt templates — returns array of available prompts.
 * prompts_out: set to malloc'd array, caller must free with mcp_prompt_list_free().
 * Returns number of prompts, or -1 on error. */
int  mcp_server_list_prompts(mcp_server_t *srv, mcp_prompt_t **prompts_out);

/* Get a specific prompt by name with optional arguments.
 * Returns JSON string (malloc'd, caller frees), or NULL on error. */
char *mcp_server_get_prompt(mcp_server_t *srv, const char *prompt_name,
                              const char *args_json);

/* ================================================================
 *  Error handling
 * ================================================================ */

/* Get last error message (NULL if no error). Valid until next call. */
const char *mcp_server_last_error(mcp_server_t *srv);

/* Get server capabilities after initialization */
mcp_capabilities_t mcp_server_capabilities(mcp_server_t *srv);

/* Get server name */
const char *mcp_server_name(mcp_server_t *srv);

/* Get connection status */
bool mcp_server_is_connected(mcp_server_t *srv);

/* P61: Server lifecycle management */

/* Server status */
typedef enum {
    MCP_STATUS_DISCONNECTED,
    MCP_STATUS_CONNECTING,
    MCP_STATUS_CONNECTED,
    MCP_STATUS_RECONNECTING,
    MCP_STATUS_FAILED,
} mcp_server_status_t;

/* Get current server status */
mcp_server_status_t mcp_server_status(mcp_server_t *srv);

/* Health check with ping. Returns true if server is healthy.
 * If server is unresponsive, triggers reconnect attempt. */
bool mcp_server_health_check(mcp_server_t *srv);

/* Force reconnect. Returns true on success. */
bool mcp_server_reconnect(mcp_server_t *srv);

/* Get reconnect count (total attempts) */
int mcp_server_reconnect_count(mcp_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* LIBMCP_H */
