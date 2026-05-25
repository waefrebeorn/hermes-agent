/*
 * test_http_pool.c — Tests for connection pooling (L02).
 * Tests set_pool, acquire/release, stats, and integration with http_t.
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
    /* Test 1: http_client_set_pool with max=0 (disabled) */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 0, 0);
        /* Should not crash, no pool active */
        int active = -1, idle = -1, max = -1;
        int total = http_pool_stats(h, &active, &idle, &max);
        TEST("pool disabled: total=0", total == 0);
        TEST("pool disabled: active=0", active == 0);
        TEST("pool disabled: idle=0", idle == 0);
        TEST("pool disabled: max=0", max == 0);
        http_free(h);
    }

    /* Test 2: http_client_set_pool with max=4 */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 4, 30);
        int active = -1, idle = -1, max = -1;
        int total = http_pool_stats(h, &active, &idle, &max);
        TEST("pool enabled: total=0 initially", total == 0);
        TEST("pool enabled: active=0", active == 0);
        TEST("pool enabled: idle=0", idle == 0);
        TEST("pool enabled: max=4", max == 4);
        http_free(h);
    }

    /* Test 3: http_free with pool (no crash) */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 4, 30);
        http_free(h);
        TEST("http_free with pool: no crash", 1);
    }

    /* Test 4: Double set_pool (replaces old pool) */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 2, 10);
        http_client_set_pool(h, 4, 60);
        int max = -1;
        http_pool_stats(h, NULL, NULL, &max);
        TEST("replace pool: max=4", max == 4);
        http_free(h);
    }

    /* Test 5: Pool stats with NULL h */
    {
        int active = -1, idle = -1, max = -1;
        int total = http_pool_stats(NULL, &active, &idle, &max);
        TEST("pool stats with NULL: total=0", total == 0);
        TEST("pool stats with NULL: active=0", active == 0);
        TEST("pool stats with NULL: idle=0", idle == 0);
        TEST("pool stats with NULL: max=0", max == 0);
    }

    /* Test 6: http_client_set_pool with NULL h */
    {
        http_client_set_pool(NULL, 4, 30);
        TEST("set_pool with NULL: no crash", 1);
    }

    /* Test 7: Pool stats with partial NULL params */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 3, 60);
        int total = http_pool_stats(h, NULL, NULL, NULL);
        TEST("pool stats with NULL params: total=0", total == 0);
        http_free(h);
    }

    /* Test 8: Connection pool with real HTTP request
     * Uses a local test server on port 18999 (from existing test infrastructure).
     * This test is SKIP if no server is running. */
    {
        http_t *h = http_new_with_retry(5, 1, 500);
        http_client_set_pool(h, 2, 60);

        /* First request — should create new connection */
        http_resp_t *r1 = http_request(h, HTTP_GET,
            "http://127.0.0.1:18999/final", NULL, NULL, 0);
        if (r1 && r1->status == 200) {
            int active = -1, idle = -1;
            int total = http_pool_stats(h, &active, &idle, NULL);
            /* After first request, connection was released to pool */
            TEST("pool: after 1 request, total=1", total == 1);
            TEST("pool: after 1 request, idle=1", idle == 1);
            TEST("pool: after 1 request, active=0", active == 0);
            http_resp_free(r1);

            /* Second request to same host — should reuse pooled connection */
            http_resp_t *r2 = http_request(h, HTTP_GET,
                "http://127.0.0.1:18999/final", NULL, NULL, 0);
            if (r2 && r2->status == 200) {
                idle = -1; active = -1;
                total = http_pool_stats(h, &active, &idle, NULL);
                TEST("pool: after 2 requests, total=1", total == 1);
                TEST("pool: after 2 requests, idle=1", idle == 1);
                http_resp_free(r2);
            } else {
                printf("  SKIP: pool reuse test (second request failed)\n");
                if (r2) http_resp_free(r2);
            }
        } else {
            printf("  SKIP: pool integration test (no server on :18999)\n");
            if (r1) http_resp_free(r1);
        }
        http_free(h);
    }

    /* Test 9: Request without pool (backward compat — no regression) */
    {
        http_t *h = http_new_with_retry(5, 1, 500);
        http_resp_t *r = http_request(h, HTTP_GET,
            "http://127.0.0.1:18999/final", NULL, NULL, 0);
        if (r && r->status == 200) {
            TEST("no pool: request ok", r->status == 200);
            http_resp_free(r);
        } else {
            printf("  SKIP: no-pool request (no server on :18999)\n");
            if (r) http_resp_free(r);
        }
        http_free(h);
    }

    /* Test 10: http_client_set_pool with max beyond HTTP_POOL_MAX (clamped) */
    {
        http_t *h = http_new(5);
        http_client_set_pool(h, 999, 0);
        int max = -1;
        http_pool_stats(h, NULL, NULL, &max);
        TEST("pool max clamped: max=16 (HTTP_POOL_MAX)", max == 16);
        http_free(h);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All pool tests PASSED");
    return failures ? 1 : 0;
}
