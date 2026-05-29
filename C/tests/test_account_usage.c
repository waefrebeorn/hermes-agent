/*
 * test_account_usage.c — Tests for account_usage module.
 * Tests: NULL safety, edge cases on render/free/fetch.
 * Compile: gcc -O2 -Wall -Wextra -DTEST_BUILD -I../include -I../lib/libjson
 *     ../src/tools/account_usage.c ../lib/libjson/json.c ../lib/libhttp/http.c
 *     -o /tmp/t_auth -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes_account_usage.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Account Usage Tests ===\n");

    /* Test 1: account_usage_free(NULL) no crash */
    {
        account_usage_free(NULL);
        TEST("free(NULL) no crash", 1);
    }

    /* Test 2: fetch with NULL provider returns NULL */
    {
        account_usage_snapshot_t *s = account_usage_fetch(NULL, NULL, NULL);
        TEST("fetch(NULL) returns NULL", s == NULL);
    }

    /* Test 3: fetch with empty provider returns NULL */
    {
        account_usage_snapshot_t *s = account_usage_fetch("", NULL, NULL);
        TEST("fetch('') returns NULL", s == NULL);
    }

    /* Test 4: fetch with unknown provider returns NULL */
    {
        account_usage_snapshot_t *s = account_usage_fetch("nonexistent_provider", NULL, NULL);
        TEST("fetch(unknown) returns NULL", s == NULL);
    }

    /* Test 5: fetch with valid provider but no API key returns NULL
     * (network ops skipped — provider unsupported without credentials) */
    {
        account_usage_snapshot_t *s = account_usage_fetch("openrouter", NULL, NULL);
        TEST("fetch(openrouter, no key) returns NULL", s == NULL);
    }

    /* Test 6: fetch with valid provider but no API key (anthropic) */
    {
        account_usage_snapshot_t *s = account_usage_fetch("anthropic", NULL, NULL);
        TEST("fetch(anthropic, no key) returns NULL", s == NULL);
    }

    /* Test 7: render(NULL) should produce empty array (first entry NULL) */
    {
        char **lines = account_usage_render(NULL, false);
        TEST("render(NULL) first entry NULL", lines != NULL && lines[0] == NULL);
        if (lines) free(lines);
    }

    /* Test 8: Render a minimal snapshot */
    {
        account_usage_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        snprintf(snap.provider, sizeof(snap.provider), "test_prov");
        snprintf(snap.source, sizeof(snap.source), "unit_test");
        snap.fetched_at = (int64_t)1717000000;
        snprintf(snap.title, sizeof(snap.title), "Test limits");
        snap.available = true;

        char **lines = account_usage_render(&snap, false);
        TEST("render(snapshot) returns non-NULL", lines != NULL);
        if (lines) {
            TEST("render first line non-NULL", lines[0] != NULL);
            TEST("render contains title", strstr(lines[0], "Test") != NULL);
            TEST("render contains provider", strstr(lines[1], "test_prov") != NULL);
            /* Free lines */
            for (int i = 0; lines[i]; i++) free(lines[i]);
            free(lines);
        }
    }

    /* Test 9: Render with windows */
    {
        account_usage_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        snprintf(snap.provider, sizeof(snap.provider), "openrouter");
        snprintf(snap.source, sizeof(snap.source), "credits_api");
        snap.fetched_at = (int64_t)1717000000;
        snprintf(snap.title, sizeof(snap.title), "Account limits");
        snap.available = true;
        snap.window_count = 1;
        snprintf(snap.windows[0].label, sizeof(snap.windows[0].label), "API key quota");
        snap.windows[0].used_percent = 45.5;
        snap.windows[0].reset_at = (int64_t)1718000000;

        char **lines = account_usage_render(&snap, false);
        TEST("render(windows) returns non-NULL", lines != NULL);
        if (lines) {
            TEST("render has title", lines[0] != NULL);
            int line_count = 0;
            for (int i = 0; lines[i]; i++) {
                line_count++;
                free(lines[i]);
            }
            TEST("render produces 2+ lines", line_count >= 2);
            free(lines);
        }
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All account_usage tests PASSED");
    return failures ? 1 : 0;
}
