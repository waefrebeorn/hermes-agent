/*
 * test_tool_error.c — Tests for tool_error_sanitize().
 * Port of Python model_tools._sanitize_tool_error().
 *
 * Tests: XML role tag stripping, code fence stripping, CDATA stripping,
 * truncation, NULL/empty input, edge cases.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include \
 *       tests/test_tool_error.c src/agent/tool_error.c \
 *       -o /tmp/hermes_test_tool_error -lm
 *
 * Run:
 *   /tmp/hermes_test_tool_error
 */

#include "hermes_tool_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR(name, result, expected) do { \
    bool ok = (result) && strcmp(result, expected) == 0; \
    if (ok) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s — expected \"%s\", got \"%s\" (line %d)\n", name, expected, (result ? result : "NULL"), __LINE__); } \
} while(0)

#define TEST_SUBSTR(name, result, expected) do { \
    bool ok = (result) && strstr(result, expected) != NULL; \
    if (ok) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s — expected to contain \"%s\", got \"%s\" (line %d)\n", name, expected, (result ? result : "NULL"), __LINE__); } \
} while(0)

/* ================================================================
 * 1. Basic prefix
 * ================================================================ */

static void test_prefix(void) {
    printf("\n--- Prefix ---\n");
    char *r = tool_error_sanitize(NULL);
    TEST_STR("NULL input", r, "[TOOL_ERROR] ");
    free(r);

    r = tool_error_sanitize("");
    TEST_STR("empty input", r, "[TOOL_ERROR] ");
    free(r);

    r = tool_error_sanitize("hello");
    TEST_STR("plain text", r, "[TOOL_ERROR] hello");
    free(r);
}

/* ================================================================
 * 2. XML role tag stripping
 * ================================================================ */

static void test_role_tags(void) {
    printf("\n--- XML role tags ---\n");

    char *r = tool_error_sanitize("<tool_call>some error</tool_call>");
    TEST_STR("tool_call tags stripped", r, "[TOOL_ERROR] some error");
    free(r);

    r = tool_error_sanitize("Error: </function_call> unexpected");
    TEST_STR("close function_call stripped", r, "[TOOL_ERROR] Error:  unexpected");
    free(r);

    r = tool_error_sanitize("<result>not found</result>");
    TEST_STR("result tags stripped", r, "[TOOL_ERROR] not found");
    free(r);

    r = tool_error_sanitize("line1\n<response>\nline2\n</response>\nline3");
    TEST_STR("response tags stripped with newlines", r, "[TOOL_ERROR] line1\n\nline2\n\nline3");
    free(r);

    r = tool_error_sanitize("mixed <system>and <user>tags</user></system>");
    TEST_STR("nested tags stripped", r, "[TOOL_ERROR] mixed and tags");
    free(r);

    /* Case insensitive */
    r = tool_error_sanitize("<TOOL_CALL>error</Tool_Call>");
    TEST_STR("case insensitive tags stripped", r, "[TOOL_ERROR] error");
    free(r);

    /* Non-role tags preserved */
    r = tool_error_sanitize("keep <b>bold</b> and <custom>");
    TEST_STR("non-role tags preserved", r, "[TOOL_ERROR] keep <b>bold</b> and <custom>");
    free(r);
}

/* ================================================================
 * 3. Code fence stripping
 * ================================================================ */

static void test_code_fences(void) {
    printf("\n--- Code fences ---\n");

    char *r = tool_error_sanitize("Error:\n```\ncode block\n```\nend");
    TEST_STR("basic fences stripped", r, "[TOOL_ERROR] Error:\n\ncode block\n\nend");
    free(r);

    r = tool_error_sanitize("JSON:\n```json\n{\"key\": \"value\"}\n```\nparsed");
    TEST_STR("json fence stripped", r, "[TOOL_ERROR] JSON:\n\n{\"key\": \"value\"}\n\nparsed");
    free(r);

    r = tool_error_sanitize("XML error:\n```xml\n<root/>\n```\ndone");
    TEST_STR("xml fence stripped", r, "[TOOL_ERROR] XML error:\n\n<root/>\n\ndone");
    free(r);

    r = tool_error_sanitize("```markdown\ncontent\n```");
    TEST_STR("markdown fence stripped", r, "[TOOL_ERROR] \ncontent\n");
    free(r);
}

/* ================================================================
 * 4. CDATA stripping
 * ================================================================ */

static void test_cdata(void) {
    printf("\n--- CDATA ---\n");

    char *r = tool_error_sanitize("before<![CDATA[secret]]>after");
    TEST_STR("CDATA block stripped", r, "[TOOL_ERROR] beforeafter");
    free(r);

    r = tool_error_sanitize("only<![CDATA[hidden]]>");
    TEST_STR("CDATA at end", r, "[TOOL_ERROR] only");
    free(r);

    r = tool_error_sanitize("<![CDATA[lead]]>trail");
    TEST_STR("CDATA at start", r, "[TOOL_ERROR] trail");
    free(r);

    r = tool_error_sanitize("multi<![CDATA[one]]>mid<![CDATA[two]]>end");
    TEST_STR("multiple CDATA blocks handled", r, "[TOOL_ERROR] multimidend");
    free(r);
}

/* ================================================================
 * 5. Combined
 * ================================================================ */

static void test_combined(void) {
    printf("\n--- Combined ---\n");

    char *r = tool_error_sanitize(
        "<tool_call>Error details:\n"
        "```json\n"
        "{\"code\": 500, \"message\": \"<![CDATA[internal]]>\"}\n"
        "```\n"
        "</tool_call>"
    );
    TEST_SUBSTR("combined sanitization", r, "[TOOL_ERROR]");
    TEST("role tags stripped", r && strstr(r, "<tool_call>") == NULL);
    TEST("fences stripped", r && strstr(r, "```") == NULL);
    TEST("CDATA stripped", r && strstr(r, "CDATA") == NULL);
    TEST("content preserved", r && strstr(r, "Error details") != NULL);
    free(r);
}

/* ================================================================
 * 6. Truncation
 * ================================================================ */

static void test_truncation(void) {
    printf("\n--- Truncation ---\n");

    /* Generate a long string */
    char long_buf[4096];
    int pos = 0;
    pos += snprintf(long_buf + pos, sizeof(long_buf) - (size_t)pos,
        "A very long error message that should be truncated: ");
    for (int i = 0; i < 200; i++) {
        int n = snprintf(long_buf + pos, sizeof(long_buf) - (size_t)pos,
            "word%d ", i);
        if (n < 0 || (size_t)(pos + n) >= sizeof(long_buf)) break;
        pos += n;
    }
    long_buf[pos] = '\0';

    char *r = tool_error_sanitize(long_buf);
    TEST("result is not NULL", r != NULL);
    TEST("result length within limit", r && strlen(r) <= TOOL_ERROR_MAX_LEN + 32);
    TEST("result starts with prefix", r && strncmp(r, "[TOOL_ERROR] ", 13) == 0);
    free(r);
}

int main(void) {
    printf("=== Tool Error Sanitization Tests ===\n");

    test_prefix();
    test_role_tags();
    test_code_fences();
    test_cdata();
    test_combined();
    test_truncation();

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
