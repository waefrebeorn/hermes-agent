/*
 * test_toml.c — TOML library tests (J19: Python tomllib port).
 * Tests: parse key-value pairs, tables, dotted keys, strings,
 * integers (dec/hex/oct/bin), floats, booleans, arrays, comments,
 * error handling, null safety.
 */
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ================================================================
 * 1. toml_parse — basic key-value
 * ================================================================ */

static void test_parse_string(void) {
    TEST("parse string value");
    toml_doc_t *doc = toml_parse("name = \"Hello\"");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "name");
    assert(v != NULL);
    assert(strcmp(v, "Hello") == 0);
    toml_free(doc);
    PASS();
}

static void test_parse_literal_string(void) {
    TEST("parse literal string (single quotes)");
    toml_doc_t *doc = toml_parse("path = 'C:\\Windows\\System32'");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "path");
    assert(v != NULL);
    assert(strcmp(v, "C:\\Windows\\System32") == 0);
    toml_free(doc);
    PASS();
}

static void test_parse_integer(void) {
    TEST("parse integer value");
    toml_doc_t *doc = toml_parse("count = 42");
    assert(doc != NULL);
    assert(toml_get_int(doc, "count") == 42);
    toml_free(doc);
    PASS();
}

static void test_parse_negative_int(void) {
    TEST("parse negative integer");
    toml_doc_t *doc = toml_parse("val = -17");
    assert(doc != NULL);
    assert(toml_get_int(doc, "val") == -17);
    toml_free(doc);
    PASS();
}

static void test_parse_hex_int(void) {
    TEST("parse hex integer (0x)");
    toml_doc_t *doc = toml_parse("val = 0xFF");
    assert(doc != NULL);
    assert(toml_get_int(doc, "val") == 255);
    toml_free(doc);
    PASS();
}

static void test_parse_oct_int(void) {
    TEST("parse octal integer (0o)");
    toml_doc_t *doc = toml_parse("val = 0o77");
    assert(doc != NULL);
    assert(toml_get_int(doc, "val") == 63);
    toml_free(doc);
    PASS();
}

static void test_parse_float(void) {
    TEST("parse float value");
    toml_doc_t *doc = toml_parse("pi = 3.14");
    assert(doc != NULL);
    double v = toml_get_float(doc, "pi");
    assert(fabs(v - 3.14) < 0.001);
    toml_free(doc);
    PASS();
}

static void test_parse_bool_true(void) {
    TEST("parse boolean true");
    toml_doc_t *doc = toml_parse("enabled = true");
    assert(doc != NULL);
    assert(toml_get_bool(doc, "enabled") == true);
    toml_free(doc);
    PASS();
}

static void test_parse_bool_false(void) {
    TEST("parse boolean false");
    toml_doc_t *doc = toml_parse("enabled = false");
    assert(doc != NULL);
    assert(toml_get_bool(doc, "enabled") == false);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 2. Tables
 * ================================================================ */

static void test_parse_table(void) {
    TEST("parse [table] section");
    toml_doc_t *doc = toml_parse(
        "[server]\n"
        "host = \"localhost\"\n"
        "port = 8080\n");
    assert(doc != NULL);
    const char *host = toml_get_string(doc, "server.host");
    assert(host != NULL);
    assert(strcmp(host, "localhost") == 0);
    assert(toml_get_int(doc, "server.port") == 8080);
    toml_free(doc);
    PASS();
}

static void test_parse_nested_table(void) {
    TEST("parse nested table [a.b.c]");
    toml_doc_t *doc = toml_parse(
        "[a.b.c]\n"
        "key = \"deep\"\n");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "a.b.c.key");
    assert(v != NULL);
    assert(strcmp(v, "deep") == 0);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 3. Dotted keys
 * ================================================================ */

static void test_dotted_key(void) {
    TEST("dotted key at top level");
    toml_doc_t *doc = toml_parse("a.b.c = \"value\"\n");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "a.b.c");
    assert(v != NULL);
    assert(strcmp(v, "value") == 0);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 4. Arrays
 * ================================================================ */

static void test_parse_array(void) {
    TEST("parse integer array");
    toml_doc_t *doc = toml_parse("nums = [1, 2, 3]");
    assert(doc != NULL);
    toml_node_t *n = toml_get(doc, "nums");
    assert(n != NULL);
    assert(n->type == TOML_ARRAY);
    assert(n->child_count == 3);
    for (int i = 0; i < 3; i++) {
        assert(n->children[i]->type == TOML_INTEGER);
        assert(n->children[i]->int_val == i + 1);
    }
    toml_free(doc);
    PASS();
}

static void test_parse_string_array(void) {
    TEST("parse string array");
    toml_doc_t *doc = toml_parse("fruits = [\"apple\", \"banana\"]");
    assert(doc != NULL);
    toml_node_t *n = toml_get(doc, "fruits");
    assert(n != NULL);
    assert(n->type == TOML_ARRAY);
    assert(n->child_count == 2);
    assert(strcmp(n->children[0]->string_val, "apple") == 0);
    assert(strcmp(n->children[1]->string_val, "banana") == 0);
    toml_free(doc);
    PASS();
}

static void test_parse_empty_array(void) {
    TEST("parse empty array");
    toml_doc_t *doc = toml_parse("empty = []");
    assert(doc != NULL);
    toml_node_t *n = toml_get(doc, "empty");
    assert(n != NULL);
    assert(n->type == TOML_ARRAY);
    assert(n->child_count == 0);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 5. Comments
 * ================================================================ */

static void test_comment_inline(void) {
    TEST("inline comment");
    toml_doc_t *doc = toml_parse("key = \"val\" # this is a comment");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "key");
    assert(v != NULL);
    assert(strcmp(v, "val") == 0);
    toml_free(doc);
    PASS();
}

static void test_comment_only_line(void) {
    TEST("comment-only line");
    toml_doc_t *doc = toml_parse(
        "# This is a comment\n"
        "key = 1\n");
    assert(doc != NULL);
    assert(toml_get_int(doc, "key") == 1);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 6. Escape sequences in strings
 * ================================================================ */

static void test_escape_newline(void) {
    TEST("escape sequence \\n");
    toml_doc_t *doc = toml_parse("text = \"line1\\nline2\"");
    assert(doc != NULL);
    const char *v = toml_get_string(doc, "text");
    assert(v != NULL);
    /* "line1\nline2" — newline at position 5 */
    assert(v[0] == 'l');
    assert(v[5] == '\n');
    assert(strcmp(v + 6, "line2") == 0);
    PASS();
}

/* ================================================================
 * 7. Multiple key-value pairs
 * ================================================================ */

static void test_multiple_keys(void) {
    TEST("multiple key-value pairs");
    toml_doc_t *doc = toml_parse(
        "title = \"Config\"\n"
        "port = 9000\n"
        "debug = true\n"
        "ratio = 0.5\n");
    assert(doc != NULL);
    assert(strcmp(toml_get_string(doc, "title"), "Config") == 0);
    assert(toml_get_int(doc, "port") == 9000);
    assert(toml_get_bool(doc, "debug") == true);
    assert(fabs(toml_get_float(doc, "ratio") - 0.5) < 0.001);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * 8. Error handling
 * ================================================================ */

static void test_parse_null(void) {
    TEST("toml_parse(NULL) returns NULL");
    assert(toml_parse(NULL) == NULL);
    PASS();
}

static void test_parse_empty(void) {
    TEST("toml_parse empty string returns valid doc");
    toml_doc_t *doc = toml_parse("");
    assert(doc != NULL);
    toml_free(doc);
    PASS();
}

static void test_get_nonexistent(void) {
    TEST("toml_get nonexistent key returns NULL");
    toml_doc_t *doc = toml_parse("a = 1\n");
    assert(doc != NULL);
    assert(toml_get(doc, "nonexistent") == NULL);
    assert(toml_get_string(doc, "nonexistent") == NULL);
    assert(toml_get_int(doc, "nonexistent") == 0);
    toml_free(doc);
    PASS();
}

static void test_get_bad_type(void) {
    TEST("toml_get_string on int returns NULL");
    toml_doc_t *doc = toml_parse("x = 42\n");
    assert(doc != NULL);
    assert(toml_get_string(doc, "x") == NULL);
    assert(toml_get_bool(doc, "x") == false);
    toml_free(doc);
    PASS();
}

static void test_free_null(void) {
    TEST("toml_free(NULL) no crash");
    toml_free(NULL);
    PASS();
}

/* ================================================================
 * 9. Full round-trip parsing
 * ================================================================ */

static void test_full_config(void) {
    TEST("parse realistic config");
    const char *input =
        "# Server configuration\n"
        "[server]\n"
        "host = \"0.0.0.0\"\n"
        "port = 8080\n"
        "\n"
        "[database]\n"
        "name = \"mydb\"\n"
        "pool_size = 10\n"
        "ssl = true\n"
        "\n"
        "[logging]\n"
        "level = \"debug\"\n"
        "file = \"/var/log/app.log\"\n";
    toml_doc_t *doc = toml_parse(input);
    assert(doc != NULL);
    assert(strcmp(toml_get_string(doc, "server.host"), "0.0.0.0") == 0);
    assert(toml_get_int(doc, "server.port") == 8080);
    assert(strcmp(toml_get_string(doc, "database.name"), "mydb") == 0);
    assert(toml_get_int(doc, "database.pool_size") == 10);
    assert(toml_get_bool(doc, "database.ssl") == true);
    assert(strcmp(toml_get_string(doc, "logging.level"), "debug") == 0);
    assert(strcmp(toml_get_string(doc, "logging.file"), "/var/log/app.log") == 0);
    toml_free(doc);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== TOML Library Tests (J19) ===\n");

    test_parse_string();
    test_parse_literal_string();
    test_parse_integer();
    test_parse_negative_int();
    test_parse_hex_int();
    test_parse_oct_int();
    test_parse_float();
    test_parse_bool_true();
    test_parse_bool_false();
    test_parse_table();
    test_parse_nested_table();
    test_dotted_key();
    test_parse_array();
    test_parse_string_array();
    test_parse_empty_array();
    test_comment_inline();
    test_comment_only_line();
    test_escape_newline();
    test_multiple_keys();
    test_parse_null();
    test_parse_empty();
    test_get_nonexistent();
    test_get_bad_type();
    test_free_null();
    test_full_config();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
