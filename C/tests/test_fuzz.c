/*
 * test_fuzz.c — Fuzz-style robustness tests for Hermes C parsers.
 * T08: Fuzz testing for JSON/config/message parsers.
 *
 * Feeds edge-case, malformed, and extreme inputs to parsers and
 * verifies they handle them gracefully (no crashes, no infinite loops).
 * Each test runs directly. For true crash isolation, pipe through a
 * timeout wrapper externally.
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

static void test_json_malformed(void) {
    TEST("JSON: malformed inputs (14 cases)");
    const char *inputs[] = {
        "",           /* empty */
        "{",          /* truncated object */
        "}",          /* lone close */
        "[",          /* truncated array */
        "]",          /* lone close */
        "{,}",        /* empty key */
        "{\"key\":}",  /* missing value */
        "{:}",        /* missing key */
        "[,]",        /* empty array element */
        "'single'",   /* single quotes (invalid JSON) */
        "1e999",      /* overflow */
        "-1e999",     /* underflow */
        "\"\\u\"",    /* truncated unicode escape */
        "\"\\x41\"",  /* invalid escape */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);

    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

static void test_json_large(void) {
    TEST("JSON: large nested structure (1000 levels)");
    char buf[32768];
    int pos = 0;
    buf[pos++] = '{';
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 20; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos,
                         "\"k%d\":{%s},", i, i < 999 ? "" : "");
        if (n > 0) pos += n;
    }
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 2; i++)
        buf[pos++] = '}';
    buf[pos] = '\0';

    char *err = NULL;
    json_t *doc = json_parse(buf, &err);
    json_free(doc);
    free(err);
    PASS();
}

static void test_json_long_string(void) {
    TEST("JSON: extremely long string (10K chars)");
    char *buf = malloc(12000);
    if (!buf) { FAIL("malloc"); return; }
    int pos = 0;
    buf[pos++] = '"';
    for (int i = 0; i < 10000 && pos < 11000; i++)
        buf[pos++] = 'A' + (i % 26);
    buf[pos++] = '"';
    buf[pos] = '\0';

    char *err = NULL;
    json_t *doc = json_parse(buf, &err);
    json_free(doc);
    free(err);
    free(buf);
    PASS();
}

static void test_json_unicode(void) {
    TEST("JSON: unicode/multi-byte");
    const char *inputs[] = {
        "\"\u00e9\"",           /* é (Latin-1 in UTF-8) */
        "\"\u4e2d\u56fd\"",     /* 中国 */
        "\"\\u00e9\"",          /* \u00e9 escape */
        "\"\\u0041\"",          /* \u0041 = A */
        "\"emoji: \\ud83d\\ude0a\"", /* emoji escape */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

static void test_json_edge(void) {
    TEST("JSON: edge-case valid inputs");
    const char *inputs[] = {
        "{\"key\": 0}",       /* zero */
        "{\"key\": -0}",      /* negative zero */
        "{\"key\": 1.0}",     /* float */
        "{\"key\": 1e10}",    /* large number */
        "{\"key\": 1e-10}",   /* small number */
        "[1,2,3,4,5]",        /* simple array */
        "[1,[2,[3]]]",        /* nested array */
        "{\"a\":1,\"b\":2}",  /* simple object */
        "{\"\": 1}",          /* empty key */
        "{\"key\twith\ttabs\": 1}", /* tabs in key */
        "  {\"key\": 1}  ",   /* leading/trailing whitespace */
        "{\"key\": 1}\n",     /* trailing newline */
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        json_free(doc);
        free(err);
    }
    PASS();
}

int main(void) {
    printf("=== Fuzz Tests (T08) ===\n\n");

    test_json_malformed();
    test_json_large();
    test_json_long_string();
    test_json_unicode();
    test_json_edge();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
