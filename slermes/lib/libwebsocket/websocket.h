/*
 * websocket.h — Minimal WebSocket client for C.
 * OpenSSL-based, no external dependencies.
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   ws_t *ws = ws_connect("wss://example.com/ws", 10);
 *   ws_send(ws, WS_OP_TEXT, "hello", 5);
 *   ws_frame_t frame;
 *   if (ws_recv(ws, &frame, 10) > 0) { ... ws_frame_free(&frame); }
 *   ws_close(ws);
 */

#ifndef LIBNGWEBSOCKET_H
#define LIBNGWEBSOCKET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opcodes */
#define WS_OP_CONT  0x0
#define WS_OP_TEXT  0x1
#define WS_OP_BIN   0x2
#define WS_OP_CLOSE 0x8
#define WS_OP_PING  0x9
#define WS_OP_PONG  0xA

/* Frame received */
typedef struct {
    uint8_t opcode;
    uint8_t *payload;
    size_t   len;
} ws_frame_t;

/* Opaque WebSocket handle */
typedef struct ws_t ws_t;

/* Connect to wss:// URL. timeout_sec: 0 = default 30s. Returns NULL on error. */
ws_t *ws_connect(const char *url, int timeout_sec);

/* Send a frame */
int ws_send(ws_t *ws, uint8_t opcode, const void *data, size_t len);

/* Receive a frame (blocking with timeout). Returns 0 on timeout, -1 on error, 1 on data. */
int ws_recv(ws_t *ws, ws_frame_t *frame, int timeout_sec);

/* Free a received frame */
void ws_frame_free(ws_frame_t *frame);

/* Close and free WebSocket */
void ws_close(ws_t *ws);

/* ── WebSocket Server ── */

/* Server handle (opaque) */
typedef struct ws_server_t ws_server_t;

/* Listen on a TCP port for WebSocket connections.
 * Returns a server handle, or NULL on error.
 * For secure (wss://), pass a certificate path, otherwise NULL for ws://.
 */
ws_server_t *ws_server_listen(int port, const char *cert_path, const char *key_path);

/* Accept an incoming WebSocket connection (blocking).
 * Performs the TCP accept and WebSocket upgrade handshake.
 * Returns a ws_t handle for the connected client, or NULL on error/timeout.
 * timeout_sec: max seconds to block waiting for a connection (0 = indefinite).
 */
ws_t *ws_server_accept(ws_server_t *server, int timeout_sec);

/* Close the server and free resources. */
void ws_server_close(ws_server_t *server);

/* Get the listen port (useful when port 0 was requested). */
int ws_server_port(ws_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* LIBNGWEBSOCKET_H */
