/*
 * test_fal_common.c -- Tests for lib/libfal_common/fal_common.c
 */
#include "fal_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_key_fal_env(void) {
    TEST("FAL_API_KEY env");
    unsetenv("SLERMES_FAL_KEY");
    setenv("FAL_API_KEY", "key-123", 1);
    const char *k = fal_get_api_key();
    ASSERT(k != NULL, "got key");
    ASSERT(strcmp(k, "key-123") == 0, "matches");
    PASS();
}

static void test_key_slermes_env(void) {
    TEST("SLERMES_FAL_KEY");
    unsetenv("FAL_API_KEY");
    setenv("SLERMES_FAL_KEY", "skey", 1);
    const char *k = fal_get_api_key();
    ASSERT(k != NULL, "got key");
    ASSERT(strcmp(k, "skey") == 0, "matches");
    PASS();
}

static void test_key_precedence(void) {
    TEST("precedence");
    setenv("FAL_API_KEY", "pri", 1);
    setenv("SLERMES_FAL_KEY", "sec", 1);
    const char *k = fal_get_api_key();
    ASSERT(k != NULL, "got key");
    ASSERT(strcmp(k, "pri") == 0, "precedence");
    PASS();
}

static void test_key_missing(void) {
    TEST("no key");
    unsetenv("FAL_API_KEY");
    unsetenv("SLERMES_FAL_KEY");
    const char *k = fal_get_api_key();
    ASSERT(k == NULL, "null");
    PASS();
}

static void test_escape_normal(void) {
    TEST("normal text");
    char out[64];
    size_t n = fal_escape_json("hello", out, sizeof(out));
    ASSERT(n == 5, "len 5");
    ASSERT(strcmp(out, "hello") == 0, "unchanged");
    PASS();
}

static void test_escape_null_in(void) {
    TEST("NULL input");
    char out[64];
    size_t n = fal_escape_json(NULL, out, sizeof(out));
    ASSERT(n == 0, "zero");
    PASS();
}

static void test_post_no_key(void) {
    TEST("no key = NULL");
    unsetenv("FAL_API_KEY");
    unsetenv("SLERMES_FAL_KEY");
    http_resp_t *r = fal_post_json("https://fal.ai/x", "{}", 10);
    ASSERT(r == NULL, "null");
    PASS();
}

static void test_err_response(void) {
    TEST("error_response");
    char *e = fal_error_response("err %d", 500);
    ASSERT(e != NULL, "not null");
    ASSERT(strstr(e, "500") != NULL, "has code");
    free(e);
    PASS();
}

static void test_err_from_null(void) {
    TEST("error_from_http(NULL)");
    char *e = fal_error_from_http(NULL);
    ASSERT(e != NULL, "not null");
    free(e);
    PASS();
}

int main(void) {
    printf("=== fal_common tests ===\n\n");
    printf("1. API key:\n");
    test_key_fal_env();
    test_key_slermes_env();
    test_key_precedence();
    test_key_missing();
    printf("\n2. JSON escaping:\n");
    test_escape_normal();
    test_escape_null_in();
    printf("\n3. POST:\n");
    test_post_no_key();
    printf("\n4. Error builders:\n");
    test_err_response();
    test_err_from_null();
    printf("\n=== %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
