/*
 * test_api_helpers.c -- Tests for P51 tool API helpers (non-HTTP paths).
 *
 * Tests: tool_api_result_free NULL safety, tool_api_error_str edge cases,
 * tool_api_call NULL url handling, tool_api_call_with_headers NULL/empty url.
 */

#include "hermes_tool_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

int main(void) {
    printf("=== Tool API Helpers Tests (P51) ===\n");

    /* --- tool_api_result_free --- */
    printf("\n--- Result Free ---\n");
    {
        TEST("free(NULL) no crash");
        tool_api_result_free(NULL);
        PASS;
    }
    {
        TEST("free empty result");
        tool_api_result_t *r = (tool_api_result_t *)calloc(1, sizeof(tool_api_result_t));
        tool_api_result_free(r);
        PASS;
    }
    {
        TEST("free with body and error");
        tool_api_result_t *r = (tool_api_result_t *)calloc(1, sizeof(tool_api_result_t));
        r->body = strdup("response data");
        r->error = strdup("some error");
        r->status = 500;
        tool_api_result_free(r);
        PASS;
    }

    /* --- tool_api_error_str --- */
    printf("\n--- Error String ---\n");
    {
        TEST("error_str(NULL) returns static string");
        const char *e = tool_api_error_str(NULL);
        if (e && strcmp(e, "NULL result") == 0) { PASS; }
        else { FAIL("unexpected result"); }
    }
    {
        TEST("error_str with explicit error");
        tool_api_result_t r;
        memset(&r, 0, sizeof(r));
        r.error = (char *)"HTTP 404";
        r.status = 404;
        const char *e = tool_api_error_str(&r);
        if (e && strcmp(e, "HTTP 404") == 0) { PASS; }
        else { FAIL("unexpected"); }
    }
    {
        TEST("error_str with status >= 400 but no error string");
        tool_api_result_t r;
        memset(&r, 0, sizeof(r));
        r.error = NULL;
        r.status = 503;
        const char *e = tool_api_error_str(&r);
        if (e && strcmp(e, "HTTP error") == 0) { PASS; }
        else { FAIL("unexpected"); }
    }
    {
        TEST("error_str with no error returns NULL");
        tool_api_result_t r;
        memset(&r, 0, sizeof(r));
        r.status = 200;
        const char *e = tool_api_error_str(&r);
        if (e == NULL) { PASS; }
        else { FAIL("expected NULL"); }
    }

    /* --- tool_api_call --- */
    printf("\n--- API Call ---\n");
    {
        TEST("call with NULL URL returns error");
        tool_api_result_t *r = tool_api_call(NULL, NULL, "GET", NULL, 30, 0);
        if (r && r->error) { PASS; }
        else { FAIL("no error"); }
        tool_api_result_free(r);
    }
    {
        TEST("call with empty URL returns error");
        tool_api_result_t *r = tool_api_call("", NULL, "GET", NULL, 30, 0);
        if (r && r->error) { PASS; }
        else { FAIL("no error"); }
        tool_api_result_free(r);
    }
    {
        TEST("call with headers, NULL URL returns error");
        tool_api_result_t *r = tool_api_call_with_headers(NULL, "Authorization: Bearer x", "GET", NULL, 30, 0);
        if (r && r->error) { PASS; }
        else { FAIL("no error"); }
        tool_api_result_free(r);
    }

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
