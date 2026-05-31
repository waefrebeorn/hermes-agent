/*
 * test_tui_websocket.c — Test suite for TUI WebSocket server.
 * Tests: start/stop server, port allocation, non-blocking accept,
 * and server struct invariants.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/cli/tui_websocket.h"

/* ── Test: start and stop server ── */
void test_start_stop(void) {
    tui_ws_server_t *srv = tui_ws_start(0, NULL, NULL);
    assert(srv != NULL);
    int port = tui_ws_port(srv);
    assert(port > 0 && port < 65536);
    tui_ws_stop(srv);
    printf("  PASS start_stop (port %d)\n", port);
}

/* ── Test: non-blocking accept returns NULL ── */
void test_accept_nonblock(void) {
    tui_ws_server_t *srv = tui_ws_start(0, NULL, NULL);
    assert(srv != NULL);
    tui_ws_client_t *client = tui_ws_accept(srv, 0);
    assert(client == NULL);
    tui_ws_stop(srv);
    printf("  PASS accept_nonblock\n");
}

/* ── Test: port 0 auto-allocation ── */
void test_port_zero(void) {
    tui_ws_server_t *srv = tui_ws_start(0, NULL, NULL);
    assert(srv != NULL);
    int port = tui_ws_port(srv);
    assert(port > 0);
    tui_ws_stop(srv);
    printf("  PASS port_zero (%d)\n", port);
}

/* ── Test: multiple start/stop cycles ── */
void test_restart(void) {
    for (int i = 0; i < 3; i++) {
        tui_ws_server_t *srv = tui_ws_start(0, NULL, NULL);
        assert(srv != NULL);
        assert(tui_ws_port(srv) > 0);
        tui_ws_stop(srv);
    }
    printf("  PASS restart\n");
}

int main(void) {
    printf("test_tui_websocket:\n");
    test_start_stop();
    test_accept_nonblock();
    test_port_zero();
    test_restart();
    printf("  ALL PASSED\n");
    return 0;
}
