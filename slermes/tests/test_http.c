/*
 * test_http.c — Compile-and-link stub for HTTP library.
 * No network calls; tests construction and URL encoding only.
 */
#include "http.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: http_new / http_free (no network) */
    http_t *h = http_new(5);
    TEST("http_new non-NULL", h != NULL);
    http_free(h);

    /* Test 2: http_new_with_retry */
    http_t *hr = http_new_with_retry(10, 3, 1000);
    TEST("http_new_with_retry non-NULL", hr != NULL);
    http_free(hr);

    /* Test 3: URL encoding (space → +, application/x-www-form-urlencoded) */
    char *enc = http_url_encode("hello world");
    TEST("http_url_encode non-NULL", enc != NULL);
    if (enc) {
        TEST("url encode spaces", strcmp(enc, "hello+world") == 0);
        free(enc);
    }

    char *enc2 = http_url_encode("a=b&c d");
    TEST("http_url_encode special chars", enc2 != NULL);
    if (enc2) {
        TEST("url encode complex", strlen(enc2) > 0);
        free(enc2);
    }

    /* Test 4: response struct layout (compile check) */
    http_resp_t resp;
    resp.status = -1;
    resp.body = NULL;
    resp.headers = NULL;
    resp.body_len = 0;
    TEST("http_resp_t struct valid", resp.status == -1);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All HTTP stub tests PASSED");
    return failures ? 1 : 0;
}
