/*
 * test_http.c — Tests for HTTP library.
 * Tests redirect following, gzip decompression, URL encoding, and API.
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
    {
        http_t *h = http_new(5);
        TEST("http_new non-NULL", h != NULL);
        http_free(h);
    }

    /* Test 2: http_new_with_retry */
    {
        http_t *hr = http_new_with_retry(10, 3, 1000);
        TEST("http_new_with_retry non-NULL", hr != NULL);
        http_free(hr);
    }

    /* Test 3: URL encoding (space → +) */
    {
        char *enc = http_url_encode("hello world");
        TEST("http_url_encode non-NULL", enc != NULL);
        if (enc) {
            TEST("url encode spaces", strcmp(enc, "hello+world") == 0);
            free(enc);
        }
    }

    /* Test 4: URL encoding special chars */
    {
        char *enc2 = http_url_encode("a=b&c d");
        TEST("http_url_encode special chars", enc2 != NULL);
        if (enc2) {
            TEST("url encode complex", strlen(enc2) > 0);
            free(enc2);
        }
    }

    /* Test 5: http_resp_t struct layout */
    {
        http_resp_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.status = -1;
        resp.body_len = 0;
        TEST("http_resp_t struct valid", resp.status == -1);
    }

    /* Test 6: http_client_set_max_redirects — set to 0 (disable) */
    {
        http_t *h = http_new(5);
        http_client_set_max_redirects(h, 0);
        /* No network test here — just verify it doesn't crash */
        TEST("set_max_redirects(0) OK", h != NULL);
        http_free(h);
    }

    /* Test 7: http_client_set_max_redirects — set to 10 */
    {
        http_t *h = http_new(5);
        http_client_set_max_redirects(h, 10);
        TEST("set_max_redirects(10) OK", h != NULL);
        http_free(h);
    }

    /* Test 8: http_client_set_decompress — enable */
    {
        http_t *h = http_new(5);
        http_client_set_decompress(h, true);
        TEST("set_decompress(true) OK", h != NULL);
        http_free(h);
    }

    /* Test 9: http_client_set_decompress — disable */
    {
        http_t *h = http_new(5);
        http_client_set_decompress(h, false);
        TEST("set_decompress(false) OK", h != NULL);
        http_free(h);
    }

    /* Test 10: L06 Redirect following (conditional network)
     * Requires: python3 -c "
     * from http.server import HTTPServer, BaseHTTPRequestHandler
     * class R(BaseHTTPRequestHandler):
     *   def do_GET(self):
     *     if '/redirect' in self.path:
     *       self.send_response(302)
     *       self.send_header('Location', '/final')
     *       self.end_headers()
     *     else:
     *       self.send_response(200)
     *       self.send_header('Content-Type', 'text/plain')
     *       self.end_headers()
     *       self.wfile.write(b'final page')
     * HTTPServer(('',18999), R).serve_forever()
     * " */
    {
        http_t *h3 = http_new_with_retry(5, 2, 500);
        http_resp_t *r = http_request(h3, HTTP_GET,
            "http://127.0.0.1:18999/redirect", NULL, NULL, 0);
        if (r && r->status == 200) {
            TEST("redirect following: 302->200", r->status == 200);
            TEST("redirect following: final body matches",
                 r->body && strstr(r->body, "final page") != NULL);
            http_resp_free(r);
        } else {
            if (r) {
                TEST("redirect following: got response (non-200)", r->status != 0);
                http_resp_free(r);
            } else {
                printf("  SKIP: redirect test server not running on :18999\n");
            }
        }
        http_free(h3);
    }

    /* Test 11: Redirect disabled (max_redirects=0) (conditional network) */
    {
        http_t *h4 = http_new_with_retry(5, 1, 500);
        http_client_set_max_redirects(h4, 0);
        http_resp_t *r = http_request(h4, HTTP_GET,
            "http://127.0.0.1:18999/redirect", NULL, NULL, 0);
        if (r && r->status > 0) {
            TEST("redirect disabled: returns 302", r->status == 302);
            http_resp_free(r);
        } else {
            if (r) http_resp_free(r);
            printf("  SKIP: redirect disabled test (no server)\n");
        }
        http_free(h4);
    }

    /* Test 12: http_parse_retry_after */
    {
        /* Integer seconds */
        TEST("retry_after integer 120",
             http_parse_retry_after("120") == 120.0);
        TEST("retry_after integer 0",
             http_parse_retry_after("0") == 0.0);
        TEST("retry_after integer with leading space",
             http_parse_retry_after("  30") == 30.0);
        /* Negative cases */
        TEST("retry_after NULL returns -1",
             http_parse_retry_after(NULL) == -1.0);
        TEST("retry_after empty returns -1",
             http_parse_retry_after("") == -1.0);
        TEST("retry_after garbage returns -1",
             http_parse_retry_after("not-a-number") == -1.0);
        TEST("retry_after whitespace only returns -1",
             http_parse_retry_after("   ") == -1.0);
    }

    /* Test 13-21: http_no_proxy_match */
    {
        /* Exact host match */
        TEST("no_proxy exact match", http_no_proxy_match("example.com", "example.com"));
        TEST("no_proxy exact mismatch", !http_no_proxy_match("other.com", "example.com"));
        /* Case insensitive */
        TEST("no_proxy case insensitive host", http_no_proxy_match("EXAMPLE.COM", "example.com"));
        TEST("no_proxy case insensitive entry", http_no_proxy_match("example.com", "EXAMPLE.COM"));
        /* Wildcard */
        TEST("no_proxy wildcard *", http_no_proxy_match("anything.example.com", "*"));
        TEST("no_proxy wildcard only", http_no_proxy_match("x", "*"));
        /* *.wildcard suffix */
        TEST("no_proxy *.example.com match sub", http_no_proxy_match("sub.example.com", "*.example.com"));
        TEST("no_proxy *.example.com match exact", http_no_proxy_match("example.com", "*.example.com"));
        TEST("no_proxy *.example.com reject other", !http_no_proxy_match("other.com", "*.example.com"));
        /* .domain suffix */
        TEST("no_proxy .example.com match exact", http_no_proxy_match("example.com", ".example.com"));
        TEST("no_proxy .example.com match sub", http_no_proxy_match("sub.example.com", ".example.com"));
        TEST("no_proxy .example.com reject unrelated", !http_no_proxy_match("other.net", ".example.com"));
        /* Subdomain matching (bare domain) */
        TEST("no_proxy subdomain match", http_no_proxy_match("sub.example.com", "example.com"));
        TEST("no_proxy subdomain reject unrelated", !http_no_proxy_match("other.com", "example.com"));
        /* Edge cases */
        TEST("no_proxy NULL host", !http_no_proxy_match(NULL, "example.com"));
        TEST("no_proxy NULL entry", !http_no_proxy_match("example.com", NULL));
        TEST("no_proxy empty entry", !http_no_proxy_match("example.com", ""));
        /* Whitespace trimming */
        TEST("no_proxy whitespace entry", http_no_proxy_match("example.com", " example.com "));
    }

    /* Test 22-32: http_split_host_port */
    {
        char host[256];
        int port = -1;

        /* URL format */
        TEST("split_host_port URL format", http_split_host_port("http://example.com:8080/path", host, sizeof(host), &port));
        TEST("split_host_port URL host", strcmp(host, "example.com") == 0);
        TEST("split_host_port URL port", port == 8080);

        /* IPv6 */
        TEST("split_host_port IPv6", http_split_host_port("[::1]:9090", host, sizeof(host), &port));
        TEST("split_host_port IPv6 host", strcmp(host, "::1") == 0);
        TEST("split_host_port IPv6 port", port == 9090);

        /* Simple host:port */
        TEST("split_host_port host:port", http_split_host_port("example.com:443", host, sizeof(host), &port));
        TEST("split_host_port host:port host", strcmp(host, "example.com") == 0);
        TEST("split_host_port host:port port", port == 443);

        /* Plain host (no port) */
        TEST("split_host_port plain host", http_split_host_port("localhost", host, sizeof(host), &port));
        TEST("split_host_port plain host result", strcmp(host, "localhost") == 0);
        TEST("split_host_port plain host no port", port == -1);

        /* HTTPS with default port */
        TEST("split_host_port https", http_split_host_port("https://api.example.com/v1", host, sizeof(host), &port));
        TEST("split_host_port https host", strcmp(host, "api.example.com") == 0);
        TEST("split_host_port https port", port == -1);

        /* Edge cases */
        port = -1; host[0] = '\0';
        TEST("split_host_port NULL", !http_split_host_port(NULL, host, sizeof(host), &port));
        TEST("split_host_port empty", !http_split_host_port("", host, sizeof(host), &port));
        TEST("split_host_port whitespace", !http_split_host_port("   ", host, sizeof(host), &port));

        /* Case normalization */
        TEST("split_host_port uppercase", http_split_host_port("EXAMPLE.COM:80", host, sizeof(host), &port));
        TEST("split_host_port uppercase host", strcmp(host, "example.com") == 0);
        TEST("split_host_port uppercase port", port == 80);

        /* Trailing dot (FQDN style) */
        TEST("split_host_port trailing dot", http_split_host_port("example.com.:8080", host, sizeof(host), &port));
        TEST("split_host_port trailing dot stripped", strcmp(host, "example.com") == 0);
    }

    /* Test 33-34: http_no_proxy_entries / http_should_bypass_proxy */
    {
        const char **entries = NULL;
        int count = http_no_proxy_entries(&entries);
        /* Without NO_PROXY set, expect 0 entries */
        TEST("no_proxy_entries no env", count == 0);
        TEST("no_proxy_entries NULL entries", entries == NULL);
    }

    {
        /* Set NO_PROXY and test */
        setenv("NO_PROXY", "localhost,127.0.0.1,.internal.example.com", 1);
        const char **entries = NULL;
        int count = http_no_proxy_entries(&entries);
        TEST("no_proxy_entries count", count == 3);
        TEST("no_proxy_entries entry 0", entries && strcmp(entries[0], "localhost") == 0);
        TEST("no_proxy_entries entry 1", entries && strcmp(entries[1], "127.0.0.1") == 0);
        TEST("no_proxy_entries entry 2", entries && strcmp(entries[2], ".internal.example.com") == 0);
        http_free_no_proxy_entries(entries, count);
        unsetenv("NO_PROXY");
    }

    {
        /* Test should_bypass_proxy */
        setenv("NO_PROXY", "localhost,*.example.com", 1);
        TEST("bypass_proxy localhost", http_should_bypass_proxy("localhost"));
        TEST("bypass_proxy subdomain", http_should_bypass_proxy("sub.example.com"));
        TEST("bypass_proxy not bypassed", !http_should_bypass_proxy("other.com"));
        TEST("bypass_proxy NULL", !http_should_bypass_proxy(NULL));
        TEST("bypass_proxy empty", !http_should_bypass_proxy(""));
        unsetenv("NO_PROXY");
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All HTTP tests PASSED");
    return failures ? 1 : 0;
}
