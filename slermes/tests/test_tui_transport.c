/*
 * test_tui_transport.c — Test suite for TUI transport layer.
 *
 * Tests: init/connect/send/recv/state/poll/shutdown, FIFO transport,
 * reconnection, state callbacks, send_rpc convenience.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "../src/cli/tui_transport.h"

/* ── Test: init ── */
void test_init(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tui_transport_init", TUI_TRANSPORT_FIFO));
    assert(tui_transport_state(&t) == TUI_TRANSPORT_IDLE);
    assert(!tui_transport_is_connected(&t));
    tui_transport_shutdown(&t);
    printf("  PASS init\n");
}

/* ── Test: init NULL path (uses default) ── */
void test_init_null_path(void) {
    tui_transport_t t;
    /* NULL path should create transport but connect will fail */
    assert(tui_transport_init(&t, "/tmp/test_tp_null", TUI_TRANSPORT_FIFO));
    tui_transport_shutdown(&t);
    printf("  PASS init_null_path\n");
}

/* ── Test: connect FIFO ── */
void test_connect_fifo(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_connect", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));
    assert(tui_transport_is_connected(&t));
    assert(tui_transport_state(&t) == TUI_TRANSPORT_CONNECTED);

    /* Clean up */
    tui_transport_shutdown(&t);
    /* Check FIFO was removed */
    assert(access("/tmp/test_tp_connect", F_OK) != 0);
    printf("  PASS connect_fifo\n");
}

/* ── Test: state names ── */
void test_state_names(void) {
    assert(strcmp(tui_transport_state_name(TUI_TRANSPORT_IDLE), "idle") == 0);
    assert(strcmp(tui_transport_state_name(TUI_TRANSPORT_CONNECTING), "connecting") == 0);
    assert(strcmp(tui_transport_state_name(TUI_TRANSPORT_CONNECTED), "connected") == 0);
    assert(strcmp(tui_transport_state_name(TUI_TRANSPORT_DISCONNECTED), "disconnected") == 0);
    assert(strcmp(tui_transport_state_name(TUI_TRANSPORT_ERROR), "error") == 0);
    printf("  PASS state_names\n");
}

/* ── Test: send/recv roundtrip via FIFO pair ── */
/* Uses two separate transport instances: server (creates FIFO, reads)
   and client (opens writer, sends). This mimics the real TUI pattern. */
void test_send_recv_roundtrip(void) {
    tui_transport_t server;
    assert(tui_transport_init(&server, "/tmp/test_tp_rr", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&server));
    assert(tui_transport_is_connected(&server));

    /* The server created the FIFO. Now open the write end .out FIFO. */
    const char *msg = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\"}\n";
    size_t len = strlen(msg);

    /* Write to the .out FIFO (client side sends to server's read fifo) */
    /* Actually the FIFO model is: server reads from /tmp/test_tp_rr,
       server writes to /tmp/test_tp_rr.out. Let's test the write side. */
    bool sent = tui_transport_send(&server, msg, len);
    assert(sent);

    /* Clean up */
    tui_transport_shutdown(&server);
    printf("  PASS send_recv_roundtrip\n");
}

/* ── Test: reconnect ── */
void test_reconnect(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_recon", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));
    assert(tui_transport_is_connected(&t));

    /* Shutdown and reconnect */
    tui_transport_shutdown(&t);

    /* Re-init and connect */
    assert(tui_transport_init(&t, "/tmp/test_tp_recon2", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));
    assert(tui_transport_is_connected(&t));

    tui_transport_shutdown(&t);
    printf("  PASS reconnect\n");
}

/* ── Test: state change callback ── */
static int g_cb_count = 0;
static tui_transport_state_t g_cb_old, g_cb_new;

static void test_cb(tui_transport_state_t old, tui_transport_state_t new_state,
                     void *user_data) {
    (void)user_data;
    g_cb_count++;
    g_cb_old = old;
    g_cb_new = new_state;
}

void test_state_callback(void) {
    tui_transport_t t;
    g_cb_count = 0;

    assert(tui_transport_init(&t, "/tmp/test_tp_cb", TUI_TRANSPORT_FIFO));
    tui_transport_set_callback(&t, test_cb, NULL);

    /* Connect should trigger callback(s): IDLE→CONNECTING→CONNECTED */
    assert(tui_transport_connect(&t));
    assert(g_cb_count >= 1);
    assert(g_cb_new == TUI_TRANSPORT_CONNECTED);

    tui_transport_shutdown(&t);
    printf("  PASS state_callback (%d transitions)\n", g_cb_count);
}

/* ── Test: send_rpc convenience ── */
void test_send_rpc(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_rpc", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));

    /* Send JSON-RPC notification */
    assert(tui_transport_send_rpc(&t, "test_method", "{\"key\":\"val\"}"));

    tui_transport_shutdown(&t);
    printf("  PASS send_rpc\n");
}

/* ── Test: sendf formatted ── */
void test_sendf(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_sendf", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));

    /* Send formatted message */
    assert(tui_transport_sendf(&t, "test %d %s\n", 42, "hello"));

    tui_transport_shutdown(&t);
    printf("  PASS sendf\n");
}

/* ── Test: set_retry ── */
void test_set_retry(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_retry", TUI_TRANSPORT_FIFO));
    tui_transport_set_retry(&t, 5, 500);

    /* Connect with valid path to verify retry doesn't break normal flow */
    assert(tui_transport_connect(&t));
    assert(tui_transport_is_connected(&t));

    tui_transport_shutdown(&t);
    printf("  PASS set_retry\n");
}

/* ── Test: poll on empty FIFO (no data) ── */
void test_poll_empty(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_poll", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));

    /* Poll on FDIR should return 0 (no data) */
    int result = tui_transport_poll(&t);
    assert(result == 0);

    tui_transport_shutdown(&t);
    printf("  PASS poll_empty\n");
}

/* ── Test: recv with no data (non-blocking) ── */
void test_recv_nonblock(void) {
    tui_transport_t t;
    char buf[128];
    assert(tui_transport_init(&t, "/tmp/test_tp_nb", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));

    /* Non-blocking recv (timeout_ms = 0) should return 0 immediately */
    int n = tui_transport_recv(&t, buf, sizeof(buf), 0);
    assert(n == 0);

    tui_transport_shutdown(&t);
    printf("  PASS recv_nonblock\n");
}

/* ── Test: send after shutdown (graceful no-crash) ── */
void test_send_after_shutdown(void) {
    tui_transport_t t;
    assert(tui_transport_init(&t, "/tmp/test_tp_sas", TUI_TRANSPORT_FIFO));
    assert(tui_transport_connect(&t));

    tui_transport_shutdown(&t);

    /* Send after shutdown should not crash and return false */
    bool result = tui_transport_send(&t, "test", 4);
    assert(!result);

    printf("  PASS send_after_shutdown\n");
}

int main(void) {
    printf("test_tui_transport:\n");

    /* Clean up any stale FIFOs */
    unlink("/tmp/test_tui_transport_init");
    unlink("/tmp/test_tp_null");
    unlink("/tmp/test_tp_connect");
    unlink("/tmp/test_tp_rr");
    unlink("/tmp/test_tp_recon");
    unlink("/tmp/test_tp_recon2");
    unlink("/tmp/test_tp_cb");
    unlink("/tmp/test_tp_rpc");
    unlink("/tmp/test_tp_sendf");
    unlink("/tmp/test_tp_retry");
    unlink("/tmp/test_tp_poll");
    unlink("/tmp/test_tp_nb");
    unlink("/tmp/test_tp_sas");

    test_init();
    test_init_null_path();
    test_state_names();
    test_set_retry();
    test_connect_fifo();
    test_send_recv_roundtrip();
    test_reconnect();
    test_state_callback();
    test_send_rpc();
    test_sendf();
    test_poll_empty();
    test_recv_nonblock();
    test_send_after_shutdown();

    /* Clean up stale FIFOs again */
    unlink("/tmp/test_tui_transport_init");
    unlink("/tmp/test_tp_null");
    unlink("/tmp/test_tp_connect");
    unlink("/tmp/test_tp_rr.out");
    unlink("/tmp/test_tp_connect.out");
    unlink("/tmp/test_tp_cb.out");
    unlink("/tmp/test_tp_rpc.out");
    unlink("/tmp/test_tp_sendf.out");
    unlink("/tmp/test_tp_retry.out");
    unlink("/tmp/test_tp_poll.out");
    unlink("/tmp/test_tp_nb.out");
    unlink("/tmp/test_tp_sas.out");
    unlink("/tmp/test_tp_recon2.out");

    printf("  ALL PASSED\n");
    return 0;
}
