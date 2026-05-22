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
