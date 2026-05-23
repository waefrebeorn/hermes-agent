/*
 * test_fal_common.c — Tests for libfal_common (FAL shared utilities).
 * Tests: URL normalization, HTTP status extraction, managed client.
 */

#include "fal_common.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  URL normalizer tests
 * ================================================================ */

static void test_url_normalize_basic(void)
{
    char *r = fal_queue_url_normalize("https://queue.fal.run");
    TEST("basic url not null", r != NULL);
    if (r) {
        TEST("basic url has trailing slash", strcmp(r, "https://queue.fal.run/") == 0);
        free(r);
    }
}

static void test_url_normalize_trailing_slash(void)
{
    char *r = fal_queue_url_normalize("https://queue.fal.run/");
    TEST("trailing slash url not null", r != NULL);
    if (r) {
        TEST("trailing slash deduped", strcmp(r, "https://queue.fal.run/") == 0);
        free(r);
    }
}

static void test_url_normalize_whitespace(void)
{
    char *r = fal_queue_url_normalize("  https://queue.fal.run  ");
    TEST("whitespace url not null", r != NULL);
    if (r) {
        TEST("whitespace trimmed", strcmp(r, "https://queue.fal.run/") == 0);
        free(r);
    }
}

static void test_url_normalize_null(void)
{
    char *r = fal_queue_url_normalize(NULL);
    TEST("null url returns NULL", r == NULL);
}

static void test_url_normalize_empty(void)
{
    char *r = fal_queue_url_normalize("");
    TEST("empty url returns NULL", r == NULL);
}

/* ================================================================
 *  HTTP status extraction tests
 * ================================================================ */

static void test_extract_http_status_basic(void)
{
    int s = fal_extract_http_status("HTTP 401: Unauthorized");
    TEST("http 401", s == 401);
}

static void test_extract_http_status_429(void)
{
    int s = fal_extract_http_status("HTTP 429: Too Many Requests");
    TEST("http 429", s == 429);
}

static void test_extract_http_status_json_status_code(void)
{
    int s = fal_extract_http_status("{\"error\": {\"status_code\": 503}}");
    TEST("json status_code 503", s == 503);
}

static void test_extract_http_status_json_status(void)
{
    int s = fal_extract_http_status("{\"status\": 429}");
    TEST("json status 429", s == 429);
}

static void test_extract_http_status_no_match(void)
{
    int s = fal_extract_http_status("Connection refused");
    TEST("no match returns 0", s == 0);
}

static void test_extract_http_status_null(void)
{
    int s = fal_extract_http_status(NULL);
    TEST("null returns 0", s == 0);
}

static void test_extract_http_status_empty(void)
{
    int s = fal_extract_http_status("");
    TEST("empty returns 0", s == 0);
}

/* ================================================================
 *  Managed client tests (no-network)
 * ================================================================ */

static void test_submit_request_null_args(void)
{
    fal_request_handle_t *h = fal_submit_request(NULL, NULL, NULL, NULL, NULL, 0);
    TEST("submit null args not null", h != NULL);
    if (h) {
        TEST("submit null args not success", !h->success);
        TEST("submit null args has error", strlen(h->error) > 0);
        fal_handle_free(h);
    }
}

static void test_submit_request_empty_args(void)
{
    fal_request_handle_t *h = fal_submit_request("", "", "", "{}", "", 10);
    TEST("submit empty args not null", h != NULL);
    if (h) {
        TEST("submit empty args not success", !h->success);
        fal_handle_free(h);
    }
}

static void test_submit_request_missing_api_key(void)
{
    fal_request_handle_t *h = fal_submit_request(NULL, "https://queue.fal.run",
                                                   "fal-ai/flux", "{\"key\":\"val\"}", "", 10);
    TEST("submit missing key not null", h != NULL);
    if (h) {
        TEST("submit missing key not success", !h->success);
        fal_handle_free(h);
    }
}

/* ================================================================
 *  Client field access test (struct layout)
 * ================================================================ */

static void test_handle_struct(void)
{
    fal_request_handle_t h;
    memset(&h, 0, sizeof(h));
    TEST("handle request_id array size", sizeof(h.request_id) >= 64);
    TEST("handle response_url array size", sizeof(h.response_url) >= 128);
    TEST("handle status_url array size", sizeof(h.status_url) >= 128);
    TEST("handle cancel_url array size", sizeof(h.cancel_url) >= 128);
    TEST("handle error array size", sizeof(h.error) >= 128);
    TEST("handle has success field", sizeof(h.success) == 1 || sizeof(h.success) == sizeof(bool));
    TEST("handle has http_status field", sizeof(h.http_status) == sizeof(int));
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== FAL Common Library Tests ===\n");

    test_url_normalize_basic();
    test_url_normalize_trailing_slash();
    test_url_normalize_whitespace();
    test_url_normalize_null();
    test_url_normalize_empty();
    test_extract_http_status_basic();
    test_extract_http_status_429();
    test_extract_http_status_json_status_code();
    test_extract_http_status_json_status();
    test_extract_http_status_no_match();
    test_extract_http_status_null();
    test_extract_http_status_empty();
    test_submit_request_null_args();
    test_submit_request_empty_args();
    test_submit_request_missing_api_key();
    test_handle_struct();

    printf("\nResults: %d passed, %d failed, %d total\n", passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
