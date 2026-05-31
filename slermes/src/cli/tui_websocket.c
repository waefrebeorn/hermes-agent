/*
 * tui_websocket.c — TUI WebSocket server for remote TUI connections.
 *
 * Wraps libwebsocket server to provide WebSocket transport for the
 * TUI backend, allowing remote JSON-RPC messaging.
 *
 * MIT License — WuBu Hermes Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "tui_websocket.h"
#include "../../lib/libwebsocket/websocket.h"

/* ── Server struct ── */
struct tui_ws_server_t {
    ws_server_t *server;
    int port;
};

/* ── Client struct ── */
struct tui_ws_client_t {
    ws_t *ws;
    bool connected;
};

/* ── API: Start server ── */
tui_ws_server_t *tui_ws_start(int port, const char *cert_path, const char *key_path) {
    tui_ws_server_t *srv = (tui_ws_server_t *)calloc(1, sizeof(tui_ws_server_t));
    if (!srv) return NULL;

    srv->server = ws_server_listen(port, cert_path, key_path);
    if (!srv->server) {
        free(srv);
        return NULL;
    }

    srv->port = ws_server_port(srv->server);
    return srv;
}

/* ── API: Get port ── */
int tui_ws_port(tui_ws_server_t *srv) {
    return srv ? srv->port : -1;
}

/* ── API: Stop server ── */
void tui_ws_stop(tui_ws_server_t *srv) {
    if (!srv) return;
    if (srv->server) ws_server_close(srv->server);
    free(srv);
}

/* ── API: Accept client ── */
tui_ws_client_t *tui_ws_accept(tui_ws_server_t *srv, int timeout_ms) {
    if (!srv || !srv->server) return NULL;

    /* Convert ms to sec for the underlying API (fractional) */
    int timeout_sec = 0;
    if (timeout_ms > 0) {
        timeout_sec = (timeout_ms + 999) / 1000;
    } else if (timeout_ms < 0) {
        timeout_sec = -1;  /* Let ws_server_accept handle as indefinite */
    }
    /* For non-blocking (timeout_ms == 0), use timeout_sec = 0 */
    /* But ws_server_accept with timeout_sec=0 skips poll and blocks on accept.
       Instead, poll ourselves for non-blocking behavior. */
    if (timeout_ms == 0) {
        /* Non-blocking: poll with 0 timeout */
        /* We need to check if there's a connection pending without blocking */
        /* The underlying ws_server_accept with timeout_sec < 0 waits indefinitely.
           Let's poll the listen fd ourselves. */
        /* Since we don't expose the listen fd, do a short poll: */
        timeout_sec = 0; /* Use 0 which in our impl does poll with 0 timeout */
        /* Actually ws_server_accept with timeout_sec=0 will return immediately */
    }

    ws_t *ws = ws_server_accept(srv->server, timeout_sec);
    if (!ws) return NULL;

    tui_ws_client_t *client = (tui_ws_client_t *)calloc(1, sizeof(tui_ws_client_t));
    if (!client) {
        ws_close(ws);
        return NULL;
    }

    client->ws = ws;
    client->connected = true;
    return client;
}

/* ── API: Send message ── */
bool tui_ws_send(tui_ws_client_t *client, const char *data, size_t len) {
    if (!client || !client->ws || !client->connected) return false;

    int ret = ws_send(client->ws, WS_OP_TEXT, data, len);
    return ret == 0;
}

/* ── API: Receive message ── */
int tui_ws_recv(tui_ws_client_t *client, char *buf, size_t sz, int timeout_ms) {
    if (!client || !client->ws || !client->connected) return -1;

    ws_frame_t frame;
    int timeout_sec = (timeout_ms + 999) / 1000;
    if (timeout_ms < 0) timeout_sec = -1;

    int ret = ws_recv(client->ws, &frame, timeout_sec);
    if (ret <= 0) {
        if (ret < 0) client->connected = false;
        return ret;
    }

    /* Copy frame payload */
    size_t copy = frame.len < sz - 1 ? frame.len : sz - 1;
    memcpy(buf, frame.payload, copy);
    buf[copy] = '\0';
    ws_frame_free(&frame);

    return (int)copy;
}

/* ── API: Close client ── */
void tui_ws_client_close(tui_ws_client_t *client) {
    if (!client) return;
    if (client->ws) ws_close(client->ws);
    free(client);
}

/* ── API: Is connected ── */
bool tui_ws_is_connected(tui_ws_client_t *client) {
    return client && client->connected;
}
