/*
 * test_hermes_error.c — Tests for typed error system (K01-K05).
 *
 * Tests: hermes_error_name for all codes, hermes_error_format
 * with various combinations, inline helpers (hermes_ok, hermes_failed).
 */
#include "hermes_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)

static void test_error_name(void) {
    TEST_STR_EQ("HERMES_OK name", hermes_error_name(HERMES_OK), "OK");
    TEST_STR_EQ("ValueError name", hermes_error_name(HERMES_ERR_VALUE), "ValueError");
    TEST_STR_EQ("TypeError name", hermes_error_name(HERMES_ERR_TYPE), "TypeError");
    TEST_STR_EQ("RuntimeError name", hermes_error_name(HERMES_ERR_RUNTIME), "RuntimeError");
    TEST_STR_EQ("IOError name", hermes_error_name(HERMES_ERR_IO), "IOError");
    TEST_STR_EQ("TimeoutError name", hermes_error_name(HERMES_ERR_TIMEOUT), "TimeoutError");
    TEST_STR_EQ("AuthError name", hermes_error_name(HERMES_ERR_AUTH), "AuthenticationError");
    TEST_STR_EQ("NotFound name", hermes_error_name(HERMES_ERR_NOT_FOUND), "NotFoundError");
    TEST_STR_EQ("NotImpl name", hermes_error_name(HERMES_ERR_NOT_IMPLEMENTED), "NotImplementedError");
    TEST_STR_EQ("RateLimited name", hermes_error_name(HERMES_ERR_RATE_LIMITED), "RateLimitedError");
    TEST_STR_EQ("MemoryError name", hermes_error_name(HERMES_ERR_MEMORY), "MemoryError");
    TEST_STR_EQ("Cancelled name", hermes_error_name(HERMES_ERR_CANCELLED), "CancelledError");
    TEST_STR_EQ("Interrupted name", hermes_error_name(HERMES_ERR_INTERRUPTED), "InterruptedError");
    TEST_STR_EQ("Default for unknown code", hermes_error_name((hermes_error_code_t)999), "UnknownError");
}

static void test_inline_ok_create(void) {
    hermes_error_t e = hermes_error(HERMES_OK, "all good");
    TEST("hermes_ok with OK code", hermes_ok(e));
    TEST("hermes_failed with OK code", !hermes_failed(e));
    TEST_STR_EQ("OK message", e.message, "all good");
    TEST("OK code zero", e.code == HERMES_OK);
}

static void test_inline_error_create(void) {
    hermes_error_t e = hermes_error(HERMES_ERR_VALUE, "invalid input");
    TEST("!hermes_ok with error", !hermes_ok(e));
    TEST("hermes_failed with error", hermes_failed(e));
    TEST_STR_EQ("ValueError message", e.message, "invalid input");
    TEST("ValueError code matches", e.code == HERMES_ERR_VALUE);
}

static void test_inline_error_truncation(void) {
    /* Message > 1024 chars should be truncated */
    char long_msg[2048];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';
    hermes_error_t e = hermes_error(HERMES_ERR_RUNTIME, long_msg);
    TEST("truncation prevents overflow", strlen(e.message) < sizeof(e.message));
    TEST("truncated message starts with A", e.message[0] == 'A');
    TEST("truncated message ends with A", e.message[strlen(e.message)-1] == 'A');
    TEST("truncated message < 1024", strlen(e.message) < 1024);
}

static void test_inline_error_null_msg(void) {
    hermes_error_t e = hermes_error(HERMES_ERR_NOT_FOUND, NULL);
    TEST("error with NULL msg is OK", hermes_failed(e));
    TEST("NULL msg results in empty string", e.message[0] == '\0');
}

static void test_inline_error_ctx(void) {
    hermes_error_t e = hermes_error_ctx(HERMES_ERR_TIMEOUT, "connection timed out", "connect()");
    TEST_STR_EQ("ctx message", e.message, "connection timed out");
    TEST_STR_EQ("ctx context", e.context, "connect()");
    TEST("ctx code matches", e.code == HERMES_ERR_TIMEOUT);
}

static void test_inline_error_ctx_null(void) {
    hermes_error_t e = hermes_error_ctx(HERMES_ERR_FILE_NOT_FOUND, "no such file", NULL);
    TEST_STR_EQ("ctx null msg", e.message, "no such file");
    TEST("ctx null context", e.context[0] == '\0');
}

static void test_error_format_ok(void) {
    hermes_error_t e = hermes_error(HERMES_OK, "fine");
    char buf[128] = {0};
    hermes_error_format(&e, buf, sizeof(buf));
    TEST_STR_EQ("format OK", buf, "OK");
}

static void test_error_format_simple(void) {
    hermes_error_t e = hermes_error(HERMES_ERR_VALUE, "bad arg");
    char buf[128] = {0};
    hermes_error_format(&e, buf, sizeof(buf));
    TEST_STR_EQ("format ValueError", buf, "ValueError: bad arg");
}

static void test_error_format_with_context(void) {
    hermes_error_t e = hermes_error_ctx(HERMES_ERR_PERMISSION, "access denied", "open_file()");
    char buf[256] = {0};
    hermes_error_format(&e, buf, sizeof(buf));
    TEST_STR_EQ("format with context", buf, "[PermissionError] open_file(): access denied");
}

static void test_error_format_null_inputs(void) {
    char buf[128] = {0};
    hermes_error_format(NULL, buf, sizeof(buf));
    TEST("format NULL error doesn't crash", 1);

    hermes_error_t e = hermes_error(HERMES_OK, "x");
    hermes_error_format(&e, NULL, 0);
    TEST("format NULL buf doesn't crash", 1);

    hermes_error_format(&e, buf, 0);
    TEST("format zero buf doesn't crash", buf[0] == '\0');
}

static void test_error_format_truncation(void) {
    /* Long message */
    hermes_error_t e = hermes_error(HERMES_ERR_RUNTIME,
        "This is a very long error message that should be truncated "
        "because the output buffer is deliberately small");
    char buf[32] = {0};
    hermes_error_format(&e, buf, sizeof(buf));
    TEST("format truncation no overflow", strlen(buf) < sizeof(buf));
    TEST("format truncated starts correctly", strncmp(buf, "RuntimeError:", 13) == 0);
}

static void test_error_format_buf_size_one(void) {
    hermes_error_t e = hermes_error(HERMES_ERR_VALUE, "x");
    char buf[1] = {0};
    hermes_error_format(&e, buf, 1);
    TEST("format buf_size=1 buf[0] stays null", buf[0] == '\0');
}

int main(void) {
    printf("=== Hermes Error System Tests ===\n");

    printf("--- Error Name Lookup ---\n");
    test_error_name();

    printf("--- Error Creation (Inline) ---\n");
    test_inline_ok_create();
    test_inline_error_create();
    test_inline_error_truncation();
    test_inline_error_null_msg();

    printf("--- Error with Context ---\n");
    test_inline_error_ctx();
    test_inline_error_ctx_null();

    printf("--- Error Formatting ---\n");
    test_error_format_ok();
    test_error_format_simple();
    test_error_format_with_context();
    test_error_format_null_inputs();
    test_error_format_truncation();
    test_error_format_buf_size_one();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
