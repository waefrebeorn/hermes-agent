/*
 * test_json5.c -- Tests for J20 libjson5 JSON5 parser.
 *
 * Tests: standard JSON passthrough, code comments, single-quoted strings,
 * unquoted keys, trailing commas, hex/octal/bin numbers, leading/trailing decimals.
 */

#include "json5.h"
#include "../lib/libjson/json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int passed = 0, failed = 0, skipped = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)
#define SKIP(msg) do { printf("SKIP: %s\n", msg); skipped++; } while(0)

/* Helper: parse JSON5, extract string value by key, compare */
static void test_str(const char *json5, const char *key, const char *expected, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    const char *val = json_get_str(doc, key, "");
    if (strcmp(val, expected) != 0) {
        char buf[256]; snprintf(buf, sizeof(buf), "got '%s', expected '%s'", val, expected);
        FAIL(buf);
    } else { PASS; }
    json5_free(doc);
}

/* Helper: parse JSON5, extract number value by key, compare */
static void test_num(const char *json5, const char *key, double expected, double tol, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    double val = json_get_num(doc, key, 0.0);
    if (fabs(val - expected) > tol) {
        char buf[256]; snprintf(buf, sizeof(buf), "got %f, expected %f", val, expected);
        FAIL(buf);
    } else { PASS; }
    json5_free(doc);
}

/* Helper: parse JSON5, extract bool value by key, compare */
static void test_bool(const char *json5, const char *key, bool expected, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    bool val = json_get_bool(doc, key, false);
    if (val != expected) {
        char buf[256]; snprintf(buf, sizeof(buf), "got %s, expected %s", val ? "true" : "false", expected ? "true" : "false");
        FAIL(buf);
    } else { PASS; }
    json5_free(doc);
}

/* Helper: parse JSON5, check array element */
static void test_arr(const char *json5, int idx, const char *expected, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    json_t *elem = json_get(doc, (size_t)idx);
    if (!elem) { FAIL("null array element"); json5_free(doc); return; }
    if (elem->type != JSON_STRING) { FAIL("element not a string"); json5_free(doc); return; }
    if (strcmp(elem->str_val, expected) != 0) {
        char buf[256]; snprintf(buf, sizeof(buf), "got '%s', expected '%s'", elem->str_val, expected);
        FAIL(buf);
    } else { PASS; }
    json5_free(doc);
}

/* Helper: parse JSON5, check nested object value */
static void test_nested(const char *json5, const char *outer, const char *inner, const char *expected, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    json_t *obj = json_obj_get(doc, outer);
    if (!obj) { FAIL("null outer object"); json5_free(doc); return; }
    const char *val = json_get_str(obj, inner, "");
    if (strcmp(val, expected) != 0) {
        char buf[256]; snprintf(buf, sizeof(buf), "got '%s', expected '%s'", val, expected);
        FAIL(buf);
    } else { PASS; }
    json5_free(doc);
}

/* Helper: parse JSON5, verify NULL on error */
static void test_err(const char *json5, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (doc) { FAIL("expected NULL but got document"); json5_free(doc); free(err); return; }
    if (!err) { FAIL("expected error message but got NULL"); return; }
    PASS;
    free(err);
}

/* Helper: parse valid JSON5, verify non-NULL result */
static void test_valid(const char *json5, const char *tc) {
    TEST(tc);
    char *err = NULL;
    json_t *doc = json5_parse(json5, &err);
    if (!doc) { FAIL(err ? err : "null result"); free(err); return; }
    PASS;
    json5_free(doc);
}

int main(void) {
    printf("=== libjson5 Tests (J20) ===\n");

    /* Standard JSON passthrough */
    printf("\n--- Standard JSON Passthrough ---\n");
    test_str("{\"key\": \"value\"}", "key", "value", "simple string");
    test_num("{\"a\":1,\"b\":2}", "a", 1.0, 0.001, "numeric passthrough");
    test_bool("{\"x\":true}", "x", true, "boolean passthrough");
    test_num("{\"pi\":3.14}", "pi", 3.14, 0.001, "float passthrough");

    /* Single-line code-comments */
    printf("\n--- Single-line Comments ---\n");
    test_str("// comment\n{\"key\": \"val\"}", "key", "val", "leading comment");
    test_str("{\"key\": \"val\" // inline\n}", "key", "val", "inline comment");
    test_num("// comment\n// another\n{\"a\":1}", "a", 1.0, 0.001, "multiple comments");
    test_str("{\"key\": \"http://example.com\"}", "key", "http://example.com", "URL preserved in string");

    /* Multi-line block comments */
    printf("\n--- Multi-line Comments ---\n");
    test_str("/* comment */{\"key\": \"val\"}", "key", "val", "leading block comment");
    test_str("{\"key\": \"val\" /* inline */}", "key", "val", "inline block comment");
    test_num("/* multi\nline\ncomment */{\"a\":1}", "a", 1.0, 0.001, "multi-line block comment");
    test_str("{\"key\": \"a/*not a comment*/b\"}", "key", "a/*not a comment*/b", "comment-looking text in string");

    /* Trailing commas */
    printf("\n--- Trailing Commas ---\n");
    test_valid("{\"a\":1,}", "trailing comma in object");
    test_valid("[\"a\",\"b\",]", "trailing comma in array");
    test_valid("{\"a\":1,\"b\":2,}", "multiple trailing commas");
    test_valid("{\"nested\":{\"x\":1,},}", "nested trailing commas");
    test_num("{ \"a\": 1, }", "a", 1.0, 0.001, "trailing comma values preserved");

    /* Single-quoted strings */
    printf("\n--- Single-quoted Strings ---\n");
    test_str("{'key': 'value'}", "key", "value", "both keys and values");
    test_str("{'key': \"value\"}", "key", "value", "single key, double value");
    test_str("{\"key\": 'value'}", "key", "value", "double key, single value");
    test_str("{'a b c': 'x y z'}", "a b c", "x y z", "spaces in key/value");
    test_str("{'key': 'it\\'s fine'}", "key", "it's fine", "escaped single quote inside");
    test_str("{'key': 'contains \"double\"'}", "key", "contains \"double\"", "double quotes inside single-quoted");

    /* Unquoted keys */
    printf("\n--- Unquoted Keys ---\n");
    test_str("{key: \"value\"}", "key", "value", "simple unquoted key");
    test_str("{key1: \"a\", key2: \"b\"}", "key1", "a", "multiple unquoted keys");
    test_nested("{outer: {inner: \"deep\"}}", "outer", "inner", "deep", "nested unquoted keys");
    test_str("{$key: \"dollar\"}", "$key", "dollar", "dollar prefix");
    test_str("{_key: \"underscore\"}", "_key", "underscore", "underscore prefix");
    test_str("{key_with_underscores: \"val\"}", "key_with_underscores", "val", "underscores in key");

    /* Hex numbers */
    printf("\n--- Hex Numbers ---\n");
    test_num("{\"x\":0xFF}", "x", 255.0, 0.001, "0xFF -> 255");
    test_num("{\"x\":0x1A}", "x", 26.0, 0.001, "0x1A -> 26");
    test_num("{\"x\":0x0}", "x", 0.0, 0.001, "0x0 -> 0");
    test_num("{\"x\":0X10}", "x", 16.0, 0.001, "0X10 -> 16");

    /* Octal numbers */
    printf("\n--- Octal Numbers ---\n");
    test_num("{\"x\":0o10}", "x", 8.0, 0.001, "0o10 -> 8");
    test_num("{\"x\":0o77}", "x", 63.0, 0.001, "0o77 -> 63");
    test_num("{\"x\":0O0}", "x", 0.0, 0.001, "0O0 -> 0");

    /* Binary numbers */
    printf("\n--- Binary Numbers ---\n");
    test_num("{\"x\":0b1010}", "x", 10.0, 0.001, "0b1010 -> 10");
    test_num("{\"x\":0B1}", "x", 1.0, 0.001, "0B1 -> 1");
    test_num("{\"x\":0b0}", "x", 0.0, 0.001, "0b0 -> 0");
    test_num("{\"x\":0b11111111}", "x", 255.0, 0.001, "0b11111111 -> 255");

    /* Leading/trailing decimals */
    printf("\n--- Leading/Trailing Decimals ---\n");
    test_num("{\"x\":.5}", "x", 0.5, 0.001, ".5 -> 0.5");
    test_num("{\"x\":.25}", "x", 0.25, 0.001, ".25 -> 0.25");
    test_num("{\"x\":1.}", "x", 1.0, 0.001, "1. -> 1.0");
    test_num("{\"x\":-.5}", "x", -0.5, 0.001, "-.5 -> -0.5");

    /* Explicit + sign */
    printf("\n--- Explicit Plus Sign ---\n");
    test_num("{\"x\":+42}", "x", 42.0, 0.001, "+42 -> 42");
    test_num("{\"x\":+.5}", "x", 0.5, 0.001, "+.5 -> 0.5");
    test_num("{\"x\":+0xFF}", "x", 255.0, 0.001, "+0xFF -> 255");

    /* Mixed JSON5 features */
    printf("\n--- Mixed Features ---\n");
    test_valid("// config\n{key: 'value', /* comment */}", "multi-feature snippet");
    test_num("{\n  name: 'Bob',\n  age: 0x1E,\n}", "age", 30.0, 0.001, "mixed hex+unquoted+single");
    test_arr("['a', 'b', 'c',]", 1, "b", "trailing comma + single-quoted array");

    /* Error handling */
    printf("\n--- Error Handling ---\n");
    test_err("", "empty string");
    test_err("{invalid", "truncated object");
    test_err("{{{{", "nested unclosed braces");

    /* Edge cases */
    printf("\n--- Edge Cases ---\n");
    /* NULL input should return NULL without crash */
    {
        TEST("NULL input");
        char *err = NULL;
        json_t *d = json5_parse(NULL, &err);
        if (d == NULL && err == NULL) { PASS; }
        else { FAIL("unexpected result"); }
        free(err); json5_free(d);
    }
    test_valid("{}", "empty object");
    test_valid("[]", "empty array");
    test_valid("{ }", "whitespace-only object");
    test_str("{key: ''}", "key", "", "empty single-quoted value");
    test_str("{key: \"\"}", "key", "", "empty double-quoted value");
    test_str("{$private_key: \"secret\"}", "$private_key", "secret", "dollar-sign key");

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
