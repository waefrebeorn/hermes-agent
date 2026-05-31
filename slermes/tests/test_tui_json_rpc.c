/*
 * test_tui_json_rpc.c — Test suite for TUI JSON-RPC gateway server.
 *
 * Tests: dispatch, parameter extraction, error codes, method registry,
 * ping/echo built-ins, malformed input.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/cli/tui_json_rpc.h"

/* Helper: check for error code in response */
static int has_error_code(const char *response, int code) {
    if (!response) return 0;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"code\":%d", code);
    return strstr(response, needle) != NULL;
}

static int has_result(const char *response) {
    return response && strstr(response, "\"result\"") != NULL;
}

/* ── Test: parse error on bad JSON ── */
void test_parse_error(void) {
    char buf[4096];
    tui_rpc_dispatch("{bad json}", buf, sizeof(buf));
    assert(has_error_code(buf, JSON_RPC_PARSE_ERROR));
    printf("  PASS parse_error\n");
}

/* ── Test: invalid request (no jsonrpc version) ── */
void test_no_version(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"method\":\"ping\"}", buf, sizeof(buf));
    assert(has_error_code(buf, JSON_RPC_INVALID_REQUEST));
    printf("  PASS no_version\n");
}

/* ── Test: missing method ── */
void test_no_method(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\"}", buf, sizeof(buf));
    assert(has_error_code(buf, JSON_RPC_INVALID_REQUEST));
    printf("  PASS no_method\n");
}

/* ── Test: unknown method ── */
void test_unknown_method(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"nonexistent\"}", buf, sizeof(buf));
    assert(has_error_code(buf, JSON_RPC_METHOD_NOT_FOUND));
    printf("  PASS unknown_method\n");
}

/* ── Test: ping (no id — notification) ── */
void test_ping_notification(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"ping\"}", buf, sizeof(buf));
    /* Notification returns empty string (no response expected) */
    assert(buf[0] == '\0');
    printf("  PASS ping_notification\n");
}

/* ── Test: ping (with id — request/response) ── */
void test_ping_request(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}", buf, sizeof(buf));
    assert(has_result(buf));
    assert(strstr(buf, "\"id\":1") != NULL);
    assert(strstr(buf, "\"pong\"") != NULL);
    printf("  PASS ping_request\n");
}

/* ── Test: echo ── */
void test_echo(void) {
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"echo\",\"params\":{\"key\":\"val\"},\"id\":42}",
                     buf, sizeof(buf));
    assert(has_result(buf));
    assert(strstr(buf, "\"id\":42") != NULL);
    assert(strstr(buf, "key") != NULL);
    assert(strstr(buf, "val") != NULL);
    printf("  PASS echo\n");
}

/* ── Test: custom method registration ── */
static const char *handler_add(const void *params, char *scratch, size_t sz) {
    int a = tui_rpc_param_int(params, "a", 0);
    int b = tui_rpc_param_int(params, "b", 0);
    snprintf(scratch, sz, "%d", a + b);
    return scratch;
}

void test_custom_method(void) {
    tui_rpc_register("add", handler_add, "Add two integers");
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":{\"a\":3,\"b\":7},\"id\":1}",
                     buf, sizeof(buf));
    assert(has_result(buf));
    assert(strstr(buf, "\"result\":10") != NULL);
    printf("  PASS custom_method\n");
}

/* ── Test: parameter extraction defaults ── */
void test_param_defaults(void) {
    /* No params — should use defaults */
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"id\":1}",
                     buf, sizeof(buf));
    assert(has_result(buf));
    assert(strstr(buf, "\"result\":0") != NULL);
    printf("  PASS param_defaults\n");
}

/* ── Test: get_all returns NULL-terminated array ── */
void test_get_all_methods(void) {
    const tui_rpc_method_t **all = tui_rpc_get_all();
    assert(all != NULL);
    int count = 0;
    for (int i = 0; all[i] != NULL; i++) {
        count++;
        assert(all[i]->method != NULL);
        assert(all[i]->handler != NULL);
    }
    /* At least ping + echo + add = 3 */
    assert(count >= 3);
    free(all);
    printf("  PASS get_all_methods (%d methods)\n", count);
}

/* ── Test: string param extraction ── */
static const char *handler_greet(const void *params, char *scratch, size_t sz) {
    const char *name = tui_rpc_param_string(params, "name", "world");
    snprintf(scratch, sz, "\"Hello, %s!\"", name);
    return scratch;
}

void test_string_param(void) {
    tui_rpc_register("greet", handler_greet, "Greet someone");
    char buf[4096];

    /* With param */
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"greet\",\"params\":{\"name\":\"Alice\"},\"id\":5}",
                     buf, sizeof(buf));
    assert(strstr(buf, "Hello, Alice") != NULL);

    /* Without param (default) */
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"greet\",\"id\":6}",
                     buf, sizeof(buf));
    assert(strstr(buf, "Hello, world") != NULL);

    printf("  PASS string_param\n");
}

/* ── Test: double param extraction ── */
static const char *handler_div(const void *params, char *scratch, size_t sz) {
    double a = tui_rpc_param_double(params, "a", 1.0);
    double b = tui_rpc_param_double(params, "b", 1.0);
    snprintf(scratch, sz, "%.2f", a / b);
    return scratch;
}

void test_double_param(void) {
    tui_rpc_register("div", handler_div, "Divide two numbers");
    char buf[4096];
    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"div\",\"params\":{\"a\":10.0,\"b\":3.0},\"id\":1}",
                     buf, sizeof(buf));
    assert(strstr(buf, "3.33") != NULL);
    printf("  PASS double_param\n");
}

/* ── Test: bool param extraction ── */
static const char *handler_check(const void *params, char *scratch, size_t sz) {
    bool flag = tui_rpc_param_bool(params, "flag", false);
    snprintf(scratch, sz, "%s", flag ? "true" : "false");
    return scratch;
}

void test_bool_param(void) {
    tui_rpc_register("check", handler_check, "Check bool flag");
    char buf[4096];

    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"check\",\"params\":{\"flag\":true},\"id\":1}",
                     buf, sizeof(buf));
    assert(strstr(buf, "true") != NULL);

    tui_rpc_dispatch("{\"jsonrpc\":\"2.0\",\"method\":\"check\",\"id\":2}",
                     buf, sizeof(buf));
    assert(strstr(buf, "false") != NULL);

    printf("  PASS bool_param\n");
}

int main(void) {
    printf("test_tui_json_rpc:\n");

    tui_rpc_init();

    test_parse_error();
    test_no_version();
    test_no_method();
    test_unknown_method();
    test_ping_notification();
    test_ping_request();
    test_echo();
    test_custom_method();
    test_param_defaults();
    test_get_all_methods();
    test_string_param();
    test_double_param();
    test_bool_param();

    printf("  ALL PASSED\n");
    return 0;
}
