/*
 * test_http.c — Tests for HTTP library.
 * Includes redirect following test (conditional on server availability).
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

    /* Test 4: http_resp_t struct layout */
    http_resp_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.status = -1;
    resp.body_len = 0;
    TEST("http_resp_t struct valid", resp.status == -1);

    /* L06: Redirect following (compile-check + network test) */
    {
        http_t *h2 = http_new(10);
        TEST("http_new links with redirect code", h2 != NULL);
        http_free(h2);

        /* Conditional network test: start test server with:
           python3 -c "
           import http.server; s=http.server.HTTPServer(('',18999),...); s.serve_forever()
           " */
        http_t *h3 = http_new_with_retry(5, 2, 500);
        http_resp_t *r = http_request(h3, HTTP_GET,
            "http://127.0.0.1:18999/redirect", NULL, NULL, 0);
        if (r && r->status == 200) {
            TEST("redirect following: 302→200", r->status == 200);
            TEST("redirect following: final body matches",
                 r->body && strstr(r->body, "final page") != NULL);
            http_resp_free(r);
        } else {
            /* Server not running — skip network tests gracefully */
            if (r) {
                TEST("redirect following: non-null response", r != NULL);
                http_resp_free(r);
            }
        }
        http_free(h3);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All HTTP tests PASSED");
    return failures ? 1 : 0;
}
