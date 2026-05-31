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
 * 2. acp_content_to_text — array content
 * ================================================================ */

static void test_content_array_strings(void) {
    TEST("array of strings joined with newline");
    json_node_t *arr = json_new_array();
    json_append(arr, json_new_string("hello"));
    json_append(arr, json_new_string("world"));
    char *result = acp_content_to_text(arr);
    assert(result != NULL);
    assert(strcmp(result, "hello\nworld") == 0);
    free(result);
    json_free(arr);
    PASS();
}

static void test_content_array_single(void) {
    TEST("array with single string returns verbatim");
    json_node_t *arr = json_new_array();
    json_append(arr, json_new_string("only one"));
    char *result = acp_content_to_text(arr);
    assert(result != NULL);
    assert(strcmp(result, "only one") == 0);
    free(result);
    json_free(arr);
    PASS();
}

static void test_content_array_empty(void) {
    TEST("empty array returns NULL");
    json_node_t *arr = json_new_array();
    char *result = acp_content_to_text(arr);
    assert(result == NULL);
    json_free(arr);
    PASS();
}

static void test_content_array_text_blocks(void) {
    TEST("array of text type objects joined with newline");
    json_node_t *arr = json_new_array();
    json_node_t *b1 = json_new_object();
    json_set(b1, "type", json_new_string("text"));
    json_set(b1, "text", json_new_string("block1"));
    json_append(arr, b1);
    json_node_t *b2 = json_new_object();
    json_set(b2, "type", json_new_string("text"));
    json_set(b2, "text", json_new_string("block2"));
    json_append(arr, b2);
    char *result = acp_content_to_text(arr);
    assert(result != NULL);
    assert(strcmp(result, "block1\nblock2") == 0);
    free(result);
    json_free(arr);
    PASS();
}

static void test_content_array_mixed_skip_nontext(void) {
    TEST("array skips non-text blocks, joins text ones");
    json_node_t *arr = json_new_array();
    json_append(arr, json_new_string("visible"));
    json_node_t *img = json_new_object();
    json_set(img, "type", json_new_string("image"));
    json_set(img, "source", json_new_object());
    json_append(arr, img);
    json_append(arr, json_new_string("also visible"));
    char *result = acp_content_to_text(arr);
    assert(result != NULL);
    /* Image block produces placeholder text, not skipped entirely */
    const char *expected = "visible\n[Attached image";
    assert(strncmp(result, expected, strlen(expected)) == 0);
    /* Last element is still there */
    assert(strstr(result, "also visible") != NULL);
    free(result);
    json_free(arr);
    PASS();
}

/* ================================================================
 * 3. acp_content_to_text — object content
 * ================================================================ */

static void test_content_object_text(void) {
    TEST("object with text type returns text field");
    json_node_t *obj = json_new_object();
    json_set(obj, "type", json_new_string("text"));
    json_set(obj, "text", json_new_string("object text content"));
    char *result = acp_content_to_text(obj);
    assert(result != NULL);
    assert(strcmp(result, "object text content") == 0);
    free(result);
    json_free(obj);
    PASS();
}

static void test_content_object_nontext(void) {
    TEST("object with non-text type returns placeholder");
    json_node_t *obj = json_new_object();
    json_set(obj, "type", json_new_string("image"));
    json_set(obj, "source", json_new_object());
    char *result = acp_content_to_text(obj);
    /* Image blocks produce placeholder text, not NULL */
    assert(result != NULL);
    assert(strstr(result, "[Attached image") != NULL);
    free(result);
    json_free(obj);
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

    printf("\n--- acp_content_to_text arrays ---\n");
    test_content_array_strings();
    test_content_array_single();
    test_content_array_empty();
    test_content_array_text_blocks();
    test_content_array_mixed_skip_nontext();

    printf("\n--- acp_content_to_text objects ---\n");
    test_content_object_text();
    test_content_object_nontext();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
