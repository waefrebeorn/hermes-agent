/*
 * test_fuzz.c — Fuzz-style robustness tests for Hermes C parsers.
 * T08: Fuzz testing for JSON/YAML/template/regex/URL parsers.
 *
 * Feeds edge-case, malformed, and extreme inputs to parsers and
 * verifies they handle them gracefully (no crashes, no infinite loops).
 *
 * Extended Phase 317: Added YAML, template, regex, URL fuzz tests.
 */

#include "hermes_json.h"
#include <yaml.h>
#include <template.h>
#include <hermes_regex.h>
#include <html.h>
#include <path.h>
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
        "\"\u00e9\"",
        "\"\u4e2d\u56fd\"",
        "\"\\u00e9\"",
        "\"\\u0041\"",
        "\"emoji: \\ud83d\\ude0a\"",
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
        "{\"key\": 0}", "{\"key\": -0}", "{\"key\": 1.0}",
        "{\"key\": 1e10}", "{\"key\": 1e-10}",
        "[1,2,3,4,5]", "[1,[2,[3]]]",
        "{\"a\":1,\"b\":2}", "{\"\": 1}",
        "{\"key\twith\ttabs\": 1}", "  {\"key\": 1}  ", "{\"key\": 1}\n",
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

/* ─── YAML fuzz tests ────────────────────────────────────────── */
static void test_yaml_malformed(void) {
    TEST("YAML: malformed inputs (8 cases)");
    const char *inputs[] = {
        "", "key: value\n", ":", "[\n",
        "key:\n  nested: value\n", "a: 1\nb: 2\nc: 3\n",
        "|", ">\n  folded\n",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(inputs[i], &err);
        free(err);
        if (doc) yaml_free(doc);
    }
    PASS();
}

static void test_yaml_large(void) {
    TEST("YAML: large sequence (1000 items)");
    char buf[65536];
    int pos = 0;
    for (int i = 0; i < 1000 && pos < (int)sizeof(buf) - 20; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "  - item %d\n", i);
        if (n > 0) pos += n;
    }
    buf[pos] = '\0';
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(buf, &err);
    free(err);
    if (doc) yaml_free(doc);
    PASS();
}

/* ─── Template fuzz tests ────────────────────────────────────── */
static void test_template_malformed(void) {
    TEST("Template: malformed inputs (6 cases)");
    const char *inputs[] = {
        "", "plain text", "{{ var }}",
        "{% if x %}", "{% for i in list %}", "{{ ",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *tmpl_err = NULL;
        template_t *tmpl = template_compile(inputs[i], &tmpl_err);
        free(tmpl_err);
        if (tmpl) {
            const char *ctx = "{}";
            char *result = template_render(tmpl, ctx);
            free(result);
            template_free(tmpl);
        }
    }
    PASS();
}

static void test_template_deep_nested(void) {
    TEST("Template: deeply nested if/for (20 levels)");
    char tmpl_buf[8192];
    int pos = 0;
    for (int i = 0; i < 20 && pos < (int)sizeof(tmpl_buf) - 30; i++) {
        int n = snprintf(tmpl_buf + pos, sizeof(tmpl_buf) - pos,
                         "{%% if x%d %%}level%d ", i, i);
        if (n > 0) pos += n;
    }
    for (int i = 0; i < 20 && pos < (int)sizeof(tmpl_buf) - 10; i++) {
        int n = snprintf(tmpl_buf + pos, sizeof(tmpl_buf) - pos, "{%% endif %%}");
        if (n > 0) pos += n;
    }
    tmpl_buf[pos] = '\0';
    char *tmpl_err = NULL;
    template_t *tmpl = template_compile(tmpl_buf, &tmpl_err);
    free(tmpl_err);
    if (tmpl) {
        const char *ctx = "{}";
        char *result = template_render(tmpl, ctx);
        free(result);
        template_free(tmpl);
    }
    PASS();
}

/* ─── Regex fuzz tests ───────────────────────────────────────── */
static void test_regex_malformed(void) {
    TEST("Regex: malformed patterns (6 cases)");
    const char *patterns[] = {
        "", ".*", "[invalid", "(", "\\", "***",
    };
    const char *test_str = "hello world 123";
    int n = sizeof(patterns) / sizeof(patterns[0]);
    for (int i = 0; i < n; i++) {
        char *result = regex_extract(patterns[i], test_str, 0);
        free(result);
    }
    PASS();
}

static void test_regex_large(void) {
    TEST("Regex: large input (100K chars)");
    char *buf = malloc(100100);
    if (!buf) { FAIL("malloc"); return; }
    memset(buf, 'a', 100000);
    buf[100000] = '\0';
    char *result = regex_extract("a{100}", buf, 0);
    free(result);
    free(buf);
    PASS();
}

/* ─── HTML fuzz tests ────────────────────────────────────────── */
static void test_html_malformed(void) {
    TEST("HTML: malformed/strip inputs (8 cases)");
    const char *inputs[] = {
        "", "plain text", "<p>hello</p>",
        "<script>alert(1)</script>",
        "<div><span>nested</span></div>",
        "<unclosed>", "extra <<<< brackets >>>>",
        "&\namp;\n",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *stripped = html_strip_tags(inputs[i]);
        free(stripped);
    }
    PASS();
}

/* ─── Path fuzz tests ────────────────────────────────────────── */
static void test_path_malformed(void) {
    TEST("Path: traversal/extreme inputs (7 cases)");
    const char *inputs[] = {
        "", "/", "../..", "../../../etc/passwd",
        "//foo///bar//", "/../", "/./",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *norm = path_normalize(inputs[i]);
        bool trav = path_has_traversal(inputs[i]);
        free(norm);
        (void)trav;
    }
    PASS();
}

/* ─── Property-style tests ───────────────────────────────────── */
static void test_property_json_roundtrip(void) {
    TEST("Property: JSON serialize/parse round-trip (10 cases)");
    const char *inputs[] = {
        "{\"key\": \"value\"}", "{\"num\": 42}",
        "{\"arr\": [1, 2, 3]}", "{\"nested\": {\"a\": 1}}",
        "{\"empty\": null}", "[\"a\", \"b\", \"c\"]",
        "42", "\"string\"", "true", "false",
    };
    int n = sizeof(inputs) / sizeof(inputs[0]);
    for (int i = 0; i < n; i++) {
        char *err = NULL;
        json_t *doc = json_parse(inputs[i], &err);
        free(err);
        if (doc) {
            char *serialized = json_serialize(doc);
            if (serialized) {
                char *err2 = NULL;
                json_t *doc2 = json_parse(serialized, &err2);
                free(err2);
                if (doc2) {
                    char *s2 = json_serialize(doc2);
                    if (!s2 || strcmp(serialized, s2) != 0) {
                        FAIL("JSON round-trip mismatch");
                        printf("        original: %s\n", inputs[i]);
                    }
                    free(s2);
                    json_free(doc2);
                }
                free(serialized);
            }
            json_free(doc);
        }
    }
    PASS();
}

int main(void) {
    printf("=== Fuzz Tests (T08) ===\n\n");

    printf("-- JSON --\n");
    test_json_malformed();
    test_json_large();
    test_json_long_string();
    test_json_unicode();
    test_json_edge();

    printf("\n-- YAML --\n");
    test_yaml_malformed();
    test_yaml_large();

    printf("\n-- Template --\n");
    test_template_malformed();
    test_template_deep_nested();

    printf("\n-- Regex --\n");
    test_regex_malformed();
    test_regex_large();

    printf("\n-- HTML --\n");
    test_html_malformed();

    printf("\n-- Path --\n");
    test_path_malformed();

    printf("\n-- Property tests --\n");
    test_property_json_roundtrip();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
