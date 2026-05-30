#ifndef HERMES_MCP_SERVE_H
#define HERMES_MCP_SERVE_H

/*
 * hermes_mcp_serve.h — MCP serve: expose Hermes tools as MCP server.
 *
 * C11-C13: Runs an HTTP JSON-RPC server that allows external MCP clients
 * (Claude Desktop, VS Code, etc.) to discover and call Hermes C tools.
 *
 * Usage:
 *   mcp_serve_set_port(9100);
 *   mcp_serve_start();
 *   // ... agent runs ...
 *   mcp_serve_stop();
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set the port the MCP serve HTTP server listens on (default: 9100).
 * Must be called before mcp_serve_start(). */
void mcp_serve_set_port(int port);

/* Check if the MCP serve server is running. */
bool mcp_serve_is_running(void);

/* Start the MCP serve HTTP server in a background thread.
 * Returns true on success. Non-blocking. */
bool mcp_serve_start(void);

/* Stop the MCP serve HTTP server. Blocks briefly for clean shutdown. */
void mcp_serve_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MCP_SERVE_H */
