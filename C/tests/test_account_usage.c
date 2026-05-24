/* test_account_usage.c — Tests for account_usage module.
 * Port of Python agent/account_usage.py test concepts.
 */
#include "hermes_account_usage.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/* Test render with NULL snapshot */
static void test_render_null(void) {
    char **lines = account_usage_render(NULL, false);
    assert(lines != NULL);
    assert(lines[0] == NULL);
    account_usage_free_lines(lines);
}

/* Test render with minimal snapshot */
static void test_render_empty(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "test-provider");
    snprintf(snap.title, sizeof(snap.title), "Account limits");
    snap.fetched_at = (int64_t)time(NULL);
    snap.available = false;

    char **lines = account_usage_render(&snap, false);
    assert(lines != NULL);
    assert(lines[0] != NULL);
    assert(strstr(lines[0], "Account limits") != NULL);
    assert(lines[1] != NULL);
    assert(strstr(lines[1], "test-provider") != NULL);
    assert(lines[2] == NULL);

    account_usage_free_lines(lines);
}

/* Test render with a window */
static void test_render_window(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "test");
    snprintf(snap.title, sizeof(snap.title), "Test");

    snap.window_count = 1;
    snprintf(snap.windows[0].label, sizeof(snap.windows[0].label), "Quota");
    snap.windows[0].used_percent = 45.0;

    char **lines = account_usage_render(&snap, false);
    assert(lines != NULL);

    /* Find the window line */
    int found = 0;
    for (int i = 0; lines[i]; i++) {
        if (strstr(lines[i], "55% remaining") && strstr(lines[i], "45% used"))
            found = 1;
    }
    assert(found);

    account_usage_free_lines(lines);
}

/* Test render with details */
static void test_render_details(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "test");
    snprintf(snap.title, sizeof(snap.title), "Test");

    snap.detail_count = 1;
    snprintf(snap.details[0], sizeof(snap.details[0]),
        "Credits balance: $10.50");

    char **lines = account_usage_render(&snap, false);
    assert(lines != NULL);

    int found = 0;
    for (int i = 0; lines[i]; i++) {
        if (strstr(lines[i], "$10.50"))
            found = 1;
    }
    assert(found);

    account_usage_free_lines(lines);
}

/* Test render with markdown */
static void test_render_markdown(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "test");
    snprintf(snap.title, sizeof(snap.title), "Usage");

    char **lines = account_usage_render(&snap, true);
    assert(lines != NULL);
    assert(strstr(lines[0], "**") != NULL); /* Bold markers in markdown mode */
    account_usage_free_lines(lines);
}

/* Test render with plan */
static void test_render_plan(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "openrouter");
    snprintf(snap.title, sizeof(snap.title), "Account limits");
    snprintf(snap.plan, sizeof(snap.plan), "Pro");

    char **lines = account_usage_render(&snap, false);
    assert(lines != NULL);
    assert(strstr(lines[1], "Pro") != NULL);

    account_usage_free_lines(lines);
}

/* Test free/set lifecycle */
static void test_free_null(void) {
    account_usage_free(NULL);
    account_usage_free_lines(NULL);
}

/* Test unavailable_reason rendering */
static void test_unavailable(void) {
    account_usage_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snprintf(snap.provider, sizeof(snap.provider), "test");
    snprintf(snap.title, sizeof(snap.title), "Test");
    snprintf(snap.unavailable_reason, sizeof(snap.unavailable_reason),
        "No OAuth token");

    char **lines = account_usage_render(&snap, false);
    assert(lines != NULL);

    int found = 0;
    for (int i = 0; lines[i]; i++) {
        if (strstr(lines[i], "Unavailable"))
            found = 1;
    }
    assert(found);

    account_usage_free_lines(lines);
}

/* Test fetch returns NULL for unsupported provider */
static void test_fetch_unsupported(void) {
    account_usage_snapshot_t *snap = account_usage_fetch("nonexistent", NULL, NULL);
    assert(snap == NULL);
}

/* Test fetch returns NULL for empty provider */
static void test_fetch_empty(void) {
    account_usage_snapshot_t *snap = account_usage_fetch("", NULL, NULL);
    assert(snap == NULL);

    snap = account_usage_fetch(NULL, NULL, NULL);
    assert(snap == NULL);
}

/* Test fetch openrouter without api key returns NULL */
static void test_fetch_openrouter_no_key(void) {
    account_usage_snapshot_t *snap = account_usage_fetch("openrouter", NULL, "");
    assert(snap == NULL);
}

int main(void) {
    test_render_null();
    test_render_empty();
    test_render_window();
    test_render_details();
    test_render_markdown();
    test_render_plan();
    test_free_null();
    test_unavailable();
    test_fetch_unsupported();
    test_fetch_empty();
    test_fetch_openrouter_no_key();

    printf("account_usage: 11/11 pass\n");
    return 0;
}
