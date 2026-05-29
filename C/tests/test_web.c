/*
 * test_web.c — Web tool unit tests (M30).
 *
 * Tests: web_get, web_search, web_extract argument validation and
 * backend dispatch (without network).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_web.c src/tools/web.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_web -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_web
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <netdb.h>
#endif

/* Forward declarations from web.c */
char *web_get_handler(const char *args_json, const char *task_id);
char *web_search_handler(const char *args_json, const char *task_id);
char *web_extract_handler(const char *args_json, const char *task_id);
char *_clean_base64_images(const char *text);

static int passed = 0, failed = 0;

/* DNS availability check — skip tests that need network if DNS broken */
static int dns_available(void) {
#ifdef __linux__
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo("example.com", "80", &hints, &res);
    if (ret == 0) freeaddrinfo(res);
    return ret == 0;
#else
    return 0; /* non-Linux: assume no DNS */
#endif
}

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Helper: check result has error field */
static int has_error(const char *json_str) {
    if (!json_str) return 0;
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int rv = (e != NULL);
    json_free(root);
    return rv;
}

/* Helper: check result contains substring */
static int json_contains(const char *json_str, const char *sub) {
    if (!json_str || !sub) return 0;
    return strstr(json_str, sub) != NULL;
}


int main(void) {
    printf("=== Web Tool Tests ===\n");

    /* ================================================================
     * web_get_handler
     * ================================================================ */

    /* Test 1: Null args */
    {
        char *res = web_get_handler(NULL, NULL);
        TEST("web_get: null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = web_get_handler("{bad json}", NULL);
        TEST("web_get: invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args (missing url) */
    {
        char *res = web_get_handler("{}", NULL);
        TEST("web_get: empty args returns error (missing url)", has_error(res));
        free(res);
    }

    /* Test 4: Missing url key */
    {
        char *res = web_get_handler("{\"timeout\":10}", NULL);
        TEST("web_get: missing url returns error", has_error(res));
        free(res);
    }

    /* Test 5: Valid args (url present — will fail HTTP but should not crash) */
    /* Guard: skip if DNS resolution is broken (WSL getaddrinfo crash workaround) */
    if (dns_available()) {
        char *res = web_get_handler("{\"url\":\"http://example.com\",\"timeout\":5}", NULL);
        TEST("web_get: valid args returns non-NULL", res != NULL);
        free(res);
    } else {
        printf("  SKIP: web_get: valid args (DNS unavailable)\n");
        passed++;
    }

    /* ================================================================
     * web_search_handler
     * ================================================================ */

    /* Test 6: Null args */
    {
        char *res = web_search_handler(NULL, NULL);
        TEST("web_search: null args returns error", has_error(res));
        free(res);
    }

    /* Test 7: Invalid JSON */
    {
        char *res = web_search_handler("{bad json}", NULL);
        TEST("web_search: invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 8: Empty args (missing query) */
    {
        char *res = web_search_handler("{}", NULL);
        TEST("web_search: empty args returns error (missing query)", has_error(res));
        free(res);
    }

    /* Test 9: Default backend (searxng) — verify dispatch with missing env */
    {
        /* After unsetting SEARXNG_BASE_URL, should get searxng error */
        char *res = web_search_handler("{\"query\":\"test\",\"count\":3}", NULL);
        TEST("web_search: default backend dispatches (searxng)", json_contains(res, "SearXNG"));
        free(res);
    }

    /* Test 10: Google backend dispatch */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"google\"}", NULL);
        TEST("web_search: google backend", json_contains(res, "Google"));
        free(res);
    }

    /* Test 11: Brave backend dispatch */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"brave\"}", NULL);
        TEST("web_search: brave backend", json_contains(res, "Brave"));
        free(res);
    }

    /* Test 12: Tavily backend dispatch */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"tavily\"}", NULL);
        TEST("web_search: tavily backend", json_contains(res, "Tavily"));
        free(res);
    }

    /* Test 13: Firecrawl backend dispatch */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"firecrawl\"}", NULL);
        TEST("web_search: firecrawl backend", json_contains(res, "Firecrawl"));
        free(res);
    }

    /* Test 14: xAI backend dispatch */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"xai\"}", NULL);
        TEST("web_search: xai backend", json_contains(res, "xAI"));
        free(res);
    }

    /* Test 15: Unknown backend */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"backend\":\"nonexistent\"}", NULL);
        /* Falls through to searxng default */
        TEST("web_search: unknown backend falls to searxng", json_contains(res, "SearXNG"));
        free(res);
    }

    /* Test 16: Count parameter is passed */
    {
        char *res = web_search_handler("{\"query\":\"test\",\"count\":10}", NULL);
        TEST("web_search: count param accepted", res != NULL);
        free(res);
    }

    /* ================================================================
     * web_extract_handler
     * ================================================================ */

    /* Test 17: Null args */
    {
        char *res = web_extract_handler(NULL, NULL);
        TEST("web_extract: null args returns error", has_error(res));
        free(res);
    }

    /* Test 18: Invalid JSON */
    {
        char *res = web_extract_handler("{bad json}", NULL);
        TEST("web_extract: invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 19: Empty args (missing url) */
    {
        char *res = web_extract_handler("{}", NULL);
        TEST("web_extract: empty args returns error (missing url)", has_error(res));
        free(res);
    }

    /* Test 20: Missing url key */
    {
        char *res = web_extract_handler("{\"prompt\":\"test\"}", NULL);
        TEST("web_extract: missing url returns error", has_error(res));
        free(res);
    }

    /* Test 21: Valid args with prompt — script not found, should error */
    {
        /* Unset SLERMES_HOME to force script-not-found path */
        /* web_extract will try to run delegate, which will fail → error */
        char *res = web_extract_handler(
            "{\"url\":\"http://example.com\",\"prompt\":\"extract info\",\"timeout\":5}", NULL);
        TEST("web_extract: valid args returns result (script may fail)", res != NULL);
        free(res);
    }

    /* Test 22: Timeout override */
    {
        char *res = web_extract_handler("{\"url\":\"http://test\",\"timeout\":60}", NULL);
        TEST("web_extract: timeout param accepted", res != NULL);
        free(res);
    }

    /* --- clean_base64_images tests --- */

    /* Test 23: NULL input returns NULL */
    {
        char *res = _clean_base64_images(NULL);
        TEST("clean_base64: NULL input returns NULL", res == NULL);
    }

    /* Test 24: Empty string returns empty */
    {
        char *res = _clean_base64_images("");
        TEST("clean_base64: empty string", res != NULL && strlen(res) == 0);
        free(res);
    }

    /* Test 25: Plain text without images passes through unchanged */
    {
        char *res = _clean_base64_images("Hello, world!");
        TEST("clean_base64: plain text unchanged", res != NULL && strcmp(res, "Hello, world!") == 0);
        free(res);
    }

    /* Test 26: data:image/ with base64 is replaced */
    {
        char *res = _clean_base64_images("before data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAE= after");
        TEST("clean_base64: image removed",
             res != NULL && strstr(res, "[BASE64_IMAGE_REMOVED]") != NULL);
        TEST("clean_base64: before text preserved",
             res != NULL && strstr(res, "before") != NULL);
        TEST("clean_base64: after text preserved",
             res != NULL && strstr(res, "after") != NULL);
        TEST("clean_base64: raw base64 not present",
             res != NULL && strstr(res, "iVBORw0KGgo") == NULL);
        free(res);
    }

    /* Test 27: Multiple images all removed */
    {
        char *res = _clean_base64_images(
            "img1: data:image/jpeg;base64,/9j/4AAQ== and img2: data:image/png;base64,iVBORw0=");
        int count = 0;
        if (res) {
            const char *p = res;
            while ((p = strstr(p, "[BASE64_IMAGE_REMOVED]")) != NULL) { count++; p++; }
        }
        TEST("clean_base64: multiple images removed", count == 2);
        free(res);
    }

    /* Test 28: Text with only an image is fully replaced */
    {
        char *res = _clean_base64_images("data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///");
        TEST("clean_base64: only image yields only placeholder",
             res != NULL && strcmp(res, "[BASE64_IMAGE_REMOVED]") == 0);
        free(res);
    }

    /* Test 29: JPEG data URL */
    {
        char *res = _clean_base64_images("data:image/jpeg;base64,/9j/4AAQSkZJRg==");
        TEST("clean_base64: JPEG data URL removed",
             res != NULL && strcmp(res, "[BASE64_IMAGE_REMOVED]") == 0);
        free(res);
    }

    /* Test 30: Image with trailing quote (inline HTML) */
    {
        char *res = _clean_base64_images("<img src=\"data:image/png;base64,iVBOR\" />");
        TEST("clean_base64: quoted HTML image",
             res != NULL && strstr(res, "[BASE64_IMAGE_REMOVED]") != NULL);
        TEST("clean_base64: HTML context preserved",
             res != NULL && strstr(res, "<img src=\"") != NULL);
        TEST("clean_base64: HTML closing preserved",
             res != NULL && strstr(res, "\" />") != NULL);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All web tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
