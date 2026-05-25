/*
 * test_fal_common.c — Tests for libfal_common (FAL shared utilities).
 * Tests: API key resolution, JSON escaping, error response builders.
 */

#include "fal_common.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  fal_get_api_key tests
 * ================================================================ */

static void test_api_key_missing(void)
{
    /* Unset both env vars */
    unsetenv("FAL_API_KEY");
    unsetenv("SLERMES_FAL_KEY");
    const char *key = fal_get_api_key();
    TEST("returns NULL when no env vars set", key == NULL);
}

static void test_api_key_fal_key(void)
{
    unsetenv("SLERMES_FAL_KEY");
    setenv("FAL_API_KEY", "test-fal-key-123", 1);
    const char *key = fal_get_api_key();
    TEST("returns FAL_API_KEY when set", key != NULL);
    if (key) {
        TEST("FAL_API_KEY value correct", strcmp(key, "test-fal-key-123") == 0);
    }
}

static void test_api_key_slermes_key(void)
{
    setenv("SLERMES_FAL_KEY", "test-slermes-key-456", 1);
    unsetenv("FAL_API_KEY");
    const char *key = fal_get_api_key();
    TEST("returns SLERMES_FAL_KEY when set", key != NULL);
    if (key) {
        TEST("SLERMES_FAL_KEY value correct", strcmp(key, "test-slermes-key-456") == 0);
    }
}

static void test_api_key_priority(void)
{
    /* Both set — FAL_API_KEY should win */
    setenv("FAL_API_KEY", "primary-key", 1);
    setenv("SLERMES_FAL_KEY", "fallback-key", 1);
    const char *key = fal_get_api_key();
    TEST("FAL_API_KEY takes priority", key != NULL);
    if (key) {
        TEST("priority value correct", strcmp(key, "primary-key") == 0);
    }
}

/* ================================================================
 *  fal_escape_json tests
 * ================================================================ */

static void test_escape_json_empty(void)
{
    char out[64];
    size_t n = fal_escape_json("", out, sizeof(out));
    TEST("empty input returns 0", n == 0);
    TEST("empty output is empty string", out[0] == '\0');
}

static void test_escape_json_plain(void)
{
    char out[64];
    size_t n = fal_escape_json("hello", out, sizeof(out));
    TEST("plain text length correct", n == 5);
    TEST("plain text unchanged", strcmp(out, "hello") == 0);
}

static void test_escape_json_quotes(void)
{
    char out[64];
    size_t n = fal_escape_json("say \"hi\"", out, sizeof(out));
    TEST("quote escaping length correct", n == 10);
    TEST("quote escaping correct", strcmp(out, "say \\\"hi\\\"") == 0);
}

static void test_escape_json_backslash(void)
{
    char out[64];
    size_t n = fal_escape_json("a\\b", out, sizeof(out));
    TEST("backslash escaping correct", n == 4);
    TEST("backslash escaped", strcmp(out, "a\\\\b") == 0);
}

static void test_escape_json_newline(void)
{
    char out[64];
    size_t n = fal_escape_json("line1\nline2", out, sizeof(out));
    TEST("newline escaping correct", n == 12);
    TEST("newline as \\n", strcmp(out, "line1\\nline2") == 0);
}

static void test_escape_json_tab(void)
{
    char out[64];
    size_t n = fal_escape_json("a\tb", out, sizeof(out));
    TEST("tab escaping correct", n == 4);
    TEST("tab as \\t", strcmp(out, "a\\tb") == 0);
}

static void test_escape_json_null_input(void)
{
    char out[64];
    size_t n = fal_escape_json(NULL, out, sizeof(out));
    TEST("null input returns 0", n == 0);
}

static void test_escape_json_null_output(void)
{
    size_t n = fal_escape_json("test", NULL, 10);
    TEST("null output returns 0", n == 0);
}

static void test_escape_json_truncation(void)
{
    char out[8];
    size_t n = fal_escape_json("abcdefghijklmnop", out, sizeof(out));
    TEST("truncation length <= max-1", n <= 7);
}

/* ================================================================
 *  fal_error_response tests
 * ================================================================ */

static void test_error_response_simple(void)
{
    char *err = fal_error_response("something went wrong");
    TEST("error response not null", err != NULL);
    if (err) {
        TEST("error response contains error key", strstr(err, "\"error\"") != NULL);
        TEST("error response contains message", strstr(err, "something went wrong") != NULL);
        TEST("error response has success false", strstr(err, "\"success\":false") != NULL);
        free(err);
    }
}

static void test_error_response_format(void)
{
    char *err = fal_error_response("HTTP %d: %s", 500, "Internal Server Error");
    TEST("formatted error not null", err != NULL);
    if (err) {
        TEST("formatted error contains values", strstr(err, "HTTP 500") != NULL);
        free(err);
    }
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== FAL Common Library Tests ===\n");

    /* API key tests */
    test_api_key_missing();
    test_api_key_fal_key();
    test_api_key_slermes_key();
    test_api_key_priority();

    /* JSON escaping tests */
    test_escape_json_empty();
    test_escape_json_plain();
    test_escape_json_quotes();
    test_escape_json_backslash();
    test_escape_json_newline();
    test_escape_json_tab();
    test_escape_json_null_input();
    test_escape_json_null_output();
    test_escape_json_truncation();

    /* Error response tests */
    test_error_response_simple();
    test_error_response_format();

    printf("Tests: %d  Passed: %d  Failed: %d\n", tests, passed, failed);
    return failed > 0 ? 1 : 0;
}
