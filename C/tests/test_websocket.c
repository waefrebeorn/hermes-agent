/*
 * test_websocket.c — WebSocket library unit tests (J18)
 * Tests: URL parsing (via ws_connect error paths), NULL safety,
 *        frame lifecycle, send/recv edge cases.
 * No network dependency: all tests exercise error-handling paths.
 */
#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ================================================================
 * 1. ws_connect — error paths (no network dependency)
 * ================================================================ */

static void test_connect_null(void) {
    TEST("ws_connect(NULL) returns NULL");
    ws_t *ws = ws_connect(NULL, 5);
    assert(ws == NULL);
    PASS();
}

static void test_connect_empty(void) {
    TEST("ws_connect('') returns NULL");
    ws_t *ws = ws_connect("", 5);
    assert(ws == NULL);
    PASS();
}

static void test_connect_http(void) {
    TEST("ws_connect(http://) returns NULL (only wss supported)");
    ws_t *ws = ws_connect("http://example.com/ws", 5);
    assert(ws == NULL);
    PASS();
}

static void test_connect_invalid_host(void) {
    TEST("ws_connect(invalid host) returns NULL");
    ws_t *ws = ws_connect("wss://!invalid!host!/path", 2);
    assert(ws == NULL);
    PASS();
}

static void test_connect_nonexistent_host(void) {
    TEST("ws_connect(nonexistent host) returns NULL");
    ws_t *ws = ws_connect("wss://nonexistent.domain.local/test", 1);
    assert(ws == NULL);
    PASS();
}

static void test_connect_with_port(void) {
    TEST("ws_connect(custom port, no server) returns NULL");
    ws_t *ws = ws_connect("wss://localhost:9999/ws", 1);
    assert(ws == NULL);
    PASS();
}

/* ================================================================
 * 2. ws_close — NULL safety
 * ================================================================ */

static void test_close_null(void) {
    TEST("ws_close(NULL) no crash");
    ws_close(NULL);
    PASS();
}

/* ================================================================
 * 3. ws_frame_free — NULL safety + lifecycle
 * ================================================================ */

static void test_frame_free_null(void) {
    TEST("ws_frame_free(NULL) no crash");
    ws_frame_free(NULL);
    PASS();
}

static void test_frame_free_null_frame(void) {
    TEST("ws_frame_free(&frame with NULL payload) no crash");
    ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.opcode = 0x1;
    frame.payload = NULL;
    frame.len = 0;
    ws_frame_free(&frame);
    assert(frame.payload == NULL);
    PASS();
}

static void test_frame_free_allocated(void) {
    TEST("ws_frame_free frees payload and sets NULL");
    ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.payload = malloc(10);
    assert(frame.payload != NULL);
    frame.len = 10;
    ws_frame_free(&frame);
    assert(frame.payload == NULL);
    PASS();
}

/* ================================================================
 * 4. ws_send — NULL safety
 * ================================================================ */

static void test_send_null_ws(void) {
    TEST("ws_send(NULL, ...) returns -1");
    int rc = ws_send(NULL, WS_OP_TEXT, "hello", 5);
    assert(rc == -1);
    PASS();
}

/* ================================================================
 * 5. ws_recv — NULL safety
 * ================================================================ */

static void test_recv_null_ws(void) {
    TEST("ws_recv(NULL, ...) returns -1");
    ws_frame_t frame;
    int rc = ws_recv(NULL, &frame, 1);
    assert(rc == -1);
    PASS();
}

/* ws_recv with NULL frame requires a connected ws_t, which we can't
 * create without a real server. The ws==NULL case is covered above. */

/* ================================================================
 * 6. ws_connect — garbage URLs
 * ================================================================ */

static void test_connect_garbage(void) {
    TEST("ws_connect(garbage) returns NULL");
    ws_t *ws = ws_connect("wss://", 5);
    assert(ws == NULL);
    PASS();
}

static void test_connect_long_path(void) {
    TEST("ws_connect(long path) returns NULL (no server)");
    char url[2048];
    snprintf(url, sizeof(url), "wss://localhost:9999/%s", 
             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    ws_t *ws = ws_connect(url, 1);
    assert(ws == NULL);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== WebSocket Library Tests (J18) ===\n");

    test_connect_null();
    test_connect_empty();
    test_connect_http();
    test_connect_invalid_host();
    test_connect_nonexistent_host();
    test_connect_with_port();
    test_close_null();
    test_frame_free_null();
    test_frame_free_null_frame();
    test_frame_free_allocated();
    test_send_null_ws();
    test_recv_null_ws();
    test_connect_garbage();
    test_connect_long_path();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
