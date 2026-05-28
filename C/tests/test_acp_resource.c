/*
 * test_acp_resource.c — ACP resource content-to-text unit tests.
 *
 * Tests: acp_content_to_text with string input.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_acp_resource.c src/acp/resource.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_acp_resource -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_acp_resource
 */

#include "acp/resource.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); fflush(stdout); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * 1. acp_content_to_text
 * ================================================================ */

static void test_content_null(void) {
    TEST("NULL content returns NULL");
    char *result = acp_content_to_text(NULL);
    assert(result == NULL);
    PASS();
}

static void test_content_string(void) {
    TEST("string content returns verbatim");
    json_node_t *s = json_new_string("hello world");
    char *result = acp_content_to_text(s);
    assert(result != NULL);
    assert(strcmp(result, "hello world") == 0);
    free(result);
    json_free(s);
    PASS();
}

static void test_content_empty_string(void) {
    TEST("empty string content returns empty string");
    json_node_t *s = json_new_string("");
    char *result = acp_content_to_text(s);
    assert(result != NULL);
    assert(strcmp(result, "") == 0);
    free(result);
    json_free(s);
    PASS();
}

static void test_content_number(void) {
    TEST("number content returns NULL (unhandled type)");
    json_node_t *n = json_new_number(42.0);
    char *result = acp_content_to_text(n);
    assert(result == NULL);
    json_free(n);
    PASS();
}

static void test_content_bool(void) {
    TEST("bool content returns NULL (unhandled type)");
    json_node_t *b = json_bool(true);
    char *result = acp_content_to_text(b);
    assert(result == NULL);
    json_free(b);
    PASS();
}

static void test_content_null_value(void) {
    TEST("null JSON value returns NULL");
    json_node_t *n = json_null();
    char *result = acp_content_to_text(n);
    assert(result == NULL);
    json_free(n);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== ACP Resource Tests ===\n\n");

    printf("--- acp_content_to_text ---\n");
    test_content_null();
    test_content_string();
    test_content_empty_string();
    test_content_number();
    test_content_bool();
    test_content_null_value();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
