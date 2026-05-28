/*
 * test_line_edit.c — Line editor test suite.
 *
 * Tests: create/free, history save/load.
 * Note: line_edit_read requires a PTY and is tested via integration.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/liblineedit \
 *       tests/test_line_edit.c lib/liblineedit/line_edit.c \
 *       -o /tmp/hermes_test_line_edit -lm
 *
 * Run:
 *   /tmp/hermes_test_line_edit
 */

#include "line_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* Dummy completion callback */
static char **dummy_complete(const char *partial, void *user_data) {
    (void)partial;
    (void)user_data;
    return NULL;
}

/* ================================================================
 *  1. Create / Free
 * ================================================================ */
static void test_create_free(void) {
    printf("\n--- create/free ---\n");

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create with completion callback", le);
    line_edit_free(le);

    le = line_edit_create(NULL, NULL);
    TEST_NOT_NULL("create with NULL completion callback", le);
    line_edit_free(le);

    line_edit_free(NULL);
    TEST("free(NULL) no crash", 1);
}

/* ================================================================
 *  2. History save/load
 * ================================================================ */
static void test_history(void) {
    printf("\n--- history save/load ---\n");

    char tmpfile[] = "/tmp/hermes_test_hist_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) { TEST("mkstemp failed", 0); return; }
    close(fd);

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for history test", le);
    if (!le) { unlink(tmpfile); return; }

    /* Save empty history */
    bool ok = line_edit_save_history(le, tmpfile);
    TEST("save empty history", ok);

    /* Load empty history */
    ok = line_edit_load_history(le, tmpfile);
    TEST("load empty history", ok);

    line_edit_free(le);
    unlink(tmpfile);
}

/* ================================================================
 *  3. History with NULL/bad paths
 * ================================================================ */
static void test_history_errors(void) {
    printf("\n--- history error paths ---\n");

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for error test", le);
    if (!le) return;

    /* Save with NULL path */
    bool ok = line_edit_save_history(le, NULL);
    TEST("save with NULL path returns false", !ok);

    /* Load with NULL path */
    ok = line_edit_load_history(le, NULL);
    TEST("load with NULL path returns false", !ok);

    /* Load from nonexistent path (should succeed — treated as empty) */
    ok = line_edit_load_history(le, "/nonexistent/path/history.txt");
    TEST("load from nonexistent path succeeds (empty history)", ok);

    /* Save to a read-only location */
    ok = line_edit_save_history(le, "/nonexistent/dir/history.txt");
    TEST("save to nonexistent dir returns false", !ok);

    line_edit_free(le);
}

int main(void) {
    printf("=== Line Editor Test Suite ===\n");

    test_create_free();
    test_history();
    test_history_errors();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
