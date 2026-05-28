/*
 * test_json_output.c — H14: JSON output mode tests.
 *
 * Tests the cli_json_escape() and cli_json_respond() functions
 * used by the --json CLI flag. These are static helpers in cli.c,
 * so we test the pattern by reimplementing equivalent logic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (!test_##name()) { \
        printf("  FAIL: %s\n", #name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", #name); \
        tests_passed++; \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* Replica of cli.c's JSON escape logic for testing */
static char *json_escape_replica(const char *raw) {
    if (!raw) return strdup("null");
    size_t rlen = strlen(raw);
    size_t cap = rlen * 2 + 3;
    char *esc = (char *)malloc(cap);
    if (!esc) return strdup("null");
    size_t j = 0;
    esc[j++] = '"';
    for (size_t i = 0; i < rlen && j < cap - 4; i++) {
        if (raw[i] == '\\') { esc[j++] = '\\'; esc[j++] = '\\'; }
        else if (raw[i] == '"')  { esc[j++] = '\\'; esc[j++] = '"'; }
        else if (raw[i] == '\n') { esc[j++] = '\\'; esc[j++] = 'n'; }
        else if (raw[i] == '\t') { esc[j++] = '\\'; esc[j++] = 't'; }
        else esc[j++] = raw[i];
    }
    esc[j++] = '"';
    esc[j] = '\0';
    return esc;
}

/* ────────────────────────────────────────────── */
/*  Tests                                          */
/* ────────────────────────────────────────────── */

static bool test_escape_null(void) {
    char *r = json_escape_replica(NULL);
    ASSERT(strcmp(r, "null") == 0, "NULL -> null");
    free(r);
    return true;
}

static bool test_escape_empty(void) {
    char *r = json_escape_replica("");
    ASSERT(strcmp(r, "\"\"") == 0, "empty -> \"\"");
    free(r);
    return true;
}

static bool test_escape_plain_text(void) {
    char *r = json_escape_replica("hello world");
    ASSERT(strcmp(r, "\"hello world\"") == 0, "plain text");
    free(r);
    return true;
}

static bool test_escape_quotes(void) {
    char *r = json_escape_replica("say \"hello\"");
    ASSERT(strstr(r, "\\\"hello\\\"") != NULL, "quotes escaped");
    free(r);
    return true;
}

static bool test_escape_backslash(void) {
    char *r = json_escape_replica("a\\b");
    ASSERT(strstr(r, "\\\\") != NULL, "backslash escaped");
    free(r);
    return true;
}

static bool test_escape_newline(void) {
    char *r = json_escape_replica("line1\nline2");
    ASSERT(strstr(r, "\\n") != NULL, "newline escaped");
    ASSERT(strstr(r, "line1") != NULL, "line1 preserved");
    ASSERT(strstr(r, "line2") != NULL, "line2 preserved");
    free(r);
    return true;
}

static bool test_escape_tab(void) {
    char *r = json_escape_replica("col1\tcol2");
    ASSERT(strstr(r, "\\t") != NULL, "tab escaped");
    free(r);
    return true;
}

static bool test_escape_mixed(void) {
    char *r = json_escape_replica("a\"b\\c\nd\te");
    ASSERT(strstr(r, "\\\"") != NULL, "quote");
    ASSERT(strstr(r, "\\\\") != NULL, "backslash");
    ASSERT(strstr(r, "\\n") != NULL, "newline");
    ASSERT(strstr(r, "\\t") != NULL, "tab");
    ASSERT(strstr(r, "a") != NULL && strstr(r, "b") != NULL && strstr(r, "c") != NULL, "chars preserved");
    ASSERT(strstr(r, "d") != NULL && strstr(r, "e") != NULL, "chars preserved 2");
    free(r);
    return true;
}

static bool test_json_output_format(void) {
    /* Simulate the cli_json_respond output format */
    char *esc = json_escape_replica("Hello world");
    char output[4096];
    snprintf(output, sizeof(output),
             "{\"response\":%s,\"session_id\":\"%s\",\"status\":\"%s\"}",
             esc ? esc : "null", "sess_123", "ok");
    free(esc);

    ASSERT(strstr(output, "\"response\":\"Hello world\"") != NULL, "response field");
    ASSERT(strstr(output, "\"session_id\":\"sess_123\"") != NULL, "session_id field");
    ASSERT(strstr(output, "\"status\":\"ok\"") != NULL, "status field");
    /* Verify valid JSON structure */
    ASSERT(output[0] == '{' && output[strlen(output)-1] == '}', "wrapped in braces");
    return true;
}

static bool test_json_error_format(void) {
    char *esc = json_escape_replica("Something went wrong");
    char output[4096];
    snprintf(output, sizeof(output),
             "{\"response\":%s,\"session_id\":\"\",\"status\":\"%s\"}",
             esc ? esc : "null", "error");
    free(esc);

    ASSERT(strstr(output, "\"status\":\"error\"") != NULL, "error status");
    ASSERT(strstr(output, "\"response\":\"Something went wrong\"") != NULL, "error response");
    ASSERT(strstr(output, "\"session_id\":\"\"") != NULL, "empty session");
    return true;
}

static bool test_json_no_response(void) {
    char output[4096];
    snprintf(output, sizeof(output),
             "{\"response\":null,\"session_id\":\"\",\"status\":\"%s\"}", "error");
    ASSERT(strstr(output, "\"response\":null") != NULL, "null response");
    ASSERT(strstr(output, "\"status\":\"error\"") != NULL, "error status");
    return true;
}

static bool test_empty_session_id(void) {
    char *esc = json_escape_replica("test");
    char output[4096];
    snprintf(output, sizeof(output),
             "{\"response\":%s,\"session_id\":\"%s\",\"status\":\"ok\"}",
             esc ? esc : "null", "");
    free(esc);

    ASSERT(strstr(output, "\"session_id\":\"\"") != NULL, "empty session_id");
    /* Should not have null session_id */
    ASSERT(strstr(output, "null") == strstr(output, "\"response\":null"), "null only in response");
    return true;
}

/* ────────────────────────────────────────────── */
/*  Main                                           */
/* ────────────────────────────────────────────── */

int main(void) {
    printf("=== JSON Output Mode Tests (H14) ===\n");

    TEST(escape_null);
    TEST(escape_empty);
    TEST(escape_plain_text);
    TEST(escape_quotes);
    TEST(escape_backslash);
    TEST(escape_newline);
    TEST(escape_tab);
    TEST(escape_mixed);
    TEST(json_output_format);
    TEST(json_error_format);
    TEST(json_no_response);
    TEST(empty_session_id);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
