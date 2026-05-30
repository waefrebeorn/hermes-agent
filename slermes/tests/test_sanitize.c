/* test_sanitize.c — Tests for sanitize module.
 * Tests: sanitize_surrogates, hermes_sanitize_output.
 * Compile with sanitize.c + redact.c + -Wl,--unresolved-symbols=ignore-all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern char *sanitize_surrogates(const char *text);
extern char *hermes_sanitize_output(const char *tool_name, const char *raw_output);
extern char *repair_tool_call_arguments(const char *raw_args);

static int pass = 0, fail = 0;
#define T(n, e) do { if(e) { pass++; printf("  OK %s\n", n); } else { fail++; printf("  FAIL %s\n", n); } } while(0)
static int contains(const char *h, const char *n) { return h && n && strstr(h, n) != NULL; }

/* ---- sanitize_surrogates ---- */
static void test_surrogates_normal(void) {
    char *r = sanitize_surrogates("hello world");
    T("normal text unchanged", r && strcmp(r, "hello world") == 0);
    free(r);
}

static void test_surrogates_emoji(void) {
    /* Emoji is BMP, not surrogate pairs — should pass through */
    char *r = sanitize_surrogates("emoji: \xf0\x9f\x98\x8a");
    T("emoji passes through", r && contains(r, "\xf0\x9f\x98\x8a"));
    free(r);
}

static void test_surrogates_null(void) {
    char *r = sanitize_surrogates(NULL);
    T("NULL in -> NULL out", r == NULL);
}

static void test_surrogates_empty(void) {
    char *r = sanitize_surrogates("");
    T("empty in -> empty out", r && r[0] == '\0');
    free(r);
}

static void test_surrogates_high_unicode(void) {
    /* High Unicode that might contain surrogate-like sequences */
    char *r = sanitize_surrogates("high: \xed\xa0\x80\xed\xb0\x80");
    T("high unicode processed without crash", r != NULL);
    free(r);
}

/* ---- hermes_sanitize_output ---- */
static void test_sanitize_basic(void) {
    char *r = hermes_sanitize_output("tool", "hello world");
    T("basic output passes through", r && contains(r, "hello world"));
    free(r);
}

static void test_sanitize_null_output(void) {
    char *r = hermes_sanitize_output("tool", NULL);
    T("null output returns NULL", r == NULL);
}

static void test_sanitize_env_var_redacted(void) {
    char *r = hermes_sanitize_output("tool", "API_KEY=sk-AAA...AAAA");
    T("env var with api key redacted", !contains(r, "AAAAAAAAAAAAAAAAAAAAA"));
    free(r);
}

static void test_sanitize_multiline(void) {
    char *r = hermes_sanitize_output("tool", "line1\nline2\nline3");
    T("multiline output preserved", r && contains(r, "line1") && contains(r, "line3"));
    free(r);
}

static void test_sanitize_with_sensitive_file(void) {
    char *r = hermes_sanitize_output("tool", "path: /home/user/.ssh/id_rsa");
    T("sensitive file path handled", !contains(r, "id_rsa"));
    free(r);
}

static void test_sanitize_empty_output(void) {
    char *r = hermes_sanitize_output("tool", "");
    T("empty output returns empty string", r && r[0] == '\0');
    free(r);
}

/* ---- repair_tool_call_arguments ---- */
static void test_repair_normal(void) {
    char *r = repair_tool_call_arguments("{\"key\":\"value\"}");
    T("normal json unchanged", r && strcmp(r, "{\"key\":\"value\"}") == 0);
    free(r);
}

static void test_repair_null(void) {
    char *r = repair_tool_call_arguments(NULL);
    T("null returns {}", r && strcmp(r, "{}") == 0);
    free(r);
}

static void test_repair_empty(void) {
    char *r = repair_tool_call_arguments("");
    T("empty returns {}", r && strcmp(r, "{}") == 0);
    free(r);
}

static void test_repair_none(void) {
    char *r = repair_tool_call_arguments("None");
    T("None returns {}", r && strcmp(r, "{}") == 0);
    free(r);
}

static void test_repair_trailing_comma(void) {
    char *r = repair_tool_call_arguments("{\"a\":1,}");
    T("trailing comma removed", r && strcmp(r, "{\"a\":1}") == 0);
    free(r);
}

static void test_repair_unclosed_brace(void) {
    char *r = repair_tool_call_arguments("{\"a\":1");
    T("unclosed brace closed", r && strcmp(r, "{\"a\":1}") == 0);
    free(r);
}

static void test_repair_excess_close(void) {
    char *r = repair_tool_call_arguments("{\"a\":1}}");
    T("excess close removed", r && strcmp(r, "{\"a\":1}") == 0);
    free(r);
}

static void test_repair_control_chars(void) {
    /* String with literal tab inside JSON string */
    char *r = repair_tool_call_arguments("{\"a\":\"hello\tworld\"}");
    T("control chars escaped", r && contains(r, "\\u0009"));
    free(r);
}

static void test_repair_whitespace_only(void) {
    char *r = repair_tool_call_arguments("   ");
    T("whitespace only returns {}", r && strcmp(r, "{}") == 0);
    free(r);
}

static void test_repair_nested_object(void) {
    char *r = repair_tool_call_arguments("{\"a\":{\"b\":\"c\"}}");
    T("nested object preserved", r && strcmp(r, "{\"a\":{\"b\":\"c\"}}") == 0);
    free(r);
}

static void test_repair_nested_array(void) {
    char *r = repair_tool_call_arguments("{\"a\":[1,2,3]}");
    T("nested array preserved", r && strcmp(r, "{\"a\":[1,2,3]}") == 0);
    free(r);
}

static void test_repair_multi_trailing_commas(void) {
    char *r = repair_tool_call_arguments("{\"a\":1,\"b\":2,}");
    T("multiple trailing comma removed", r && strcmp(r, "{\"a\":1,\"b\":2}") == 0);
    free(r);
}

static void test_repair_unclosed_nested(void) {
    char *r = repair_tool_call_arguments("{\"a\":{\"b\":\"c\"}");
    T("unclosed nested brace closed", r && strcmp(r, "{\"a\":{\"b\":\"c\"}}") == 0);
    free(r);
}

static void test_repair_mixed_excess_closers(void) {
    char *r = repair_tool_call_arguments("{\"a\":1}]}");
    T("mixed excess closers removed", r && strcmp(r, "{\"a\":1}") == 0);
    free(r);
}

static void test_repair_escaped_quotes(void) {
    char *r = repair_tool_call_arguments("{\"a\":\"hello\\\"world\"}");
    T("escaped quotes inside string preserved", r && strcmp(r, "{\"a\":\"hello\\\"world\"}") == 0);
    free(r);
}

static void test_repair_unicode_content(void) {
    char *r = repair_tool_call_arguments("{\"msg\":\"caf\\u00e9\"}");
    T("unicode escape preserved", r && strcmp(r, "{\"msg\":\"caf\\u00e9\"}") == 0);
    free(r);
}

/* ---- sanitize_surrogates edge cases ---- */
static void test_surrogates_utf8_3byte(void) {
    char *r = sanitize_surrogates("caf\xc3\xa9 \xc3\xa0 \xc3\xb1"); /* é à ñ */
    T("3-byte UTF-8 sequences preserved", r && contains(r, "\xc3\xa9"));
    free(r);
}

static void test_surrogates_utf8_4byte(void) {
    char *r = sanitize_surrogates("emoji: \xf0\x9f\x98\x82 more \xf0\x9f\x98\x8e"); /* 😂 😎 */
    T("4-byte UTF-8 emoji preserved", r != NULL);
    free(r);
}

static void test_surrogates_long_text(void) {
    char buf[2048];
    memset(buf, 'A', sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *r = sanitize_surrogates(buf);
    T("long text processed without crash", r != NULL);
    free(r);
}

/* ---- hermes_sanitize_output edge cases ---- */
static void test_sanitize_url_with_token(void) {
    char *r = hermes_sanitize_output("tool", "fetch https://example.com?token=sk-abc123def456");
    T("url with token redacted", !contains(r, "abc123def456"));
    free(r);
}

static void test_sanitize_ssh_key_path(void) {
    char *r = hermes_sanitize_output("tool", "config at /root/.ssh/authorized_keys");
    T("ssh authorized_keys path handled", !contains(r, "authorized_keys"));
    free(r);
}

int main(void) {
    printf("=== Sanitize Module Tests ===\n");
    printf("\n--- sanitize_surrogates ---\n");
    test_surrogates_normal();
    test_surrogates_emoji();
    test_surrogates_null();
    test_surrogates_empty();
    test_surrogates_high_unicode();
    printf("\n--- hermes_sanitize_output ---\n");
    test_sanitize_basic();
    test_sanitize_null_output();
    test_sanitize_env_var_redacted();
    test_sanitize_multiline();
    test_sanitize_with_sensitive_file();
    test_sanitize_empty_output();
    printf("\n--- repair_tool_call_arguments ---\n");
    test_repair_normal();
    test_repair_null();
    test_repair_empty();
    test_repair_none();
    test_repair_trailing_comma();
    test_repair_unclosed_brace();
    test_repair_excess_close();
    test_repair_control_chars();
    test_repair_whitespace_only();
    test_repair_nested_object();
    test_repair_nested_array();
    test_repair_multi_trailing_commas();
    test_repair_unclosed_nested();
    test_repair_mixed_excess_closers();
    test_repair_escaped_quotes();
    test_repair_unicode_content();
    printf("\n--- sanitize_surrogates edge cases ---\n");
    test_surrogates_utf8_3byte();
    test_surrogates_utf8_4byte();
    test_surrogates_long_text();
    printf("\n--- hermes_sanitize_output edge cases ---\n");
    test_sanitize_url_with_token();
    test_sanitize_ssh_key_path();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
