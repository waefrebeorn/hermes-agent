#ifndef TUI_WEBSOCKET_H
#define TUI_WEBSOCKET_H

/*
 * tui_websocket.h — TUI WebSocket server for remote TUI connections.
 *
 * Wraps the libwebsocket server to provide WebSocket transport for
 * the TUI backend. Allows remote clients to connect to the TUI
 * via WebSocket and exchange JSON-RPC messages.
 *
 * Usage:
 *   tui_ws_server_t *ws = tui_ws_start(8765, NULL, NULL);
 *   // In main loop:
 *   tui_ws_client_t *client = tui_ws_accept(ws, 0);
 *   if (client) {
 *       tui_ws_send(client, "{\"jsonrpc\":...}", 20);
 *       char buf[4096];
 *       int n = tui_ws_recv(client, buf, sizeof(buf), 100);
 *   }
 *   tui_ws_stop(ws);
 */

#include <stdbool.h>
#include <stddef.h>

/* Opaque types */
typedef struct tui_ws_server_t tui_ws_server_t;
typedef struct tui_ws_client_t tui_ws_client_t;

/* ── Server lifecycle ── */

/* Start a WebSocket server on the given port.
 * Pass NULL for cert/key for ws:// (plain), paths for wss:// (TLS).
 * Returns NULL on error.
 */
tui_ws_server_t *tui_ws_start(int port, const char *cert_path, const char *key_path);

/* Get the port the server is listening on (useful when port 0 was passed). */
int tui_ws_port(tui_ws_server_t *srv);

/* Stop the server and free resources. Closes all connected clients. */
void tui_ws_stop(tui_ws_server_t *srv);

/* ── Client management ── */

/* Accept an incoming WebSocket connection (non-blocking with timeout).
 * timeout_ms: max milliseconds to wait (-1 = indefinite, 0 = non-blocking).
 * Returns a client handle, or NULL if no connection available.
 * The returned client must be freed with tui_ws_client_close().
 */
tui_ws_client_t *tui_ws_accept(tui_ws_server_t *srv, int timeout_ms);

/* Send a message to a WebSocket client (text frame). */
bool tui_ws_send(tui_ws_client_t *client, const char *data, size_t len);

/* Receive a message from a WebSocket client.
 * timeout_ms: max milliseconds to wait (-1 = indefinite, 0 = non-blocking).
 * Returns number of bytes read, 0 on timeout, -1 on error/disconnect.
 */
int tui_ws_recv(tui_ws_client_t *client, char *buf, size_t sz, int timeout_ms);

/* Close a WebSocket client and free resources. */
void tui_ws_client_close(tui_ws_client_t *client);

/* Check if client is still connected. */
bool tui_ws_is_connected(tui_ws_client_t *client);

#endif /* TUI_WEBSOCKET_H */
