/*
 * test_allowlist.c — Command allowlist test suite (G126).
 *
 * Tests: add entries, remove entries, clear, check command
 * matching, NULL safety, duplicate handling, empty state.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_allowlist.c src/tools/approval.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_al -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_al
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))

/* ================================================================
 *  1. Basic add/check
 * ================================================================ */
static void test_basic_add_check(void) {
    printf("\n--- Basic Add & Check ---\n");

    allowlist_clear();

    /* Empty allowlist — all pass */
    TEST_TRUE("empty allowlist allows all",
              allowlist_check("terminal", "rm -rf /"));

    /* Add an entry */
    TEST_TRUE("add terminal rm pattern",
              allowlist_add("terminal", "rm"));

    /* Now rm should be allowed, other commands blocked */
    TEST_TRUE("terminal rm -rf allowed",
              allowlist_check("terminal", "rm -rf /"));
    TEST_TRUE("terminal rm file allowed",
              allowlist_check("terminal", "rm file.txt"));
    TEST_FALSE("terminal ls blocked",
               allowlist_check("terminal", "ls -la"));

    /* Different tool unaffected */
    TEST_TRUE("web curl allowed (no entries)",
              allowlist_check("web", "curl http://example.com"));
}

/* ================================================================
 *  2. Multiple patterns per tool
 * ================================================================ */
static void test_multiple_patterns(void) {
    printf("\n--- Multiple Patterns ---\n");

    allowlist_clear();

    allowlist_add("file", "read");
    allowlist_add("file", "write");
    allowlist_add("file", "list");

    TEST_TRUE("file read allowed",    allowlist_check("file", "read_file /etc/passwd"));
    TEST_TRUE("file write allowed",   allowlist_check("file", "write_file /tmp/x"));
    TEST_TRUE("file list allowed",    allowlist_check("file", "list_dir /home"));
    TEST_FALSE("file delete blocked", allowlist_check("file", "delete_file /tmp/x"));
    TEST_FALSE("file copy blocked",   allowlist_check("file", "copy_file a b"));
}

/* ================================================================
 *  3. Remove entries
 * ================================================================ */
static void test_remove(void) {
    printf("\n--- Remove ---\n");

    allowlist_clear();

    allowlist_add("terminal", "ls");
    allowlist_add("terminal", "cat");
    TEST_TRUE("ls allowed before remove",
              allowlist_check("terminal", "ls /tmp"));
    TEST_TRUE("cat allowed before remove",
              allowlist_check("terminal", "cat /etc/passwd"));

    /* Remove ls pattern only */
    TEST_TRUE("remove ls pattern",
              allowlist_remove("terminal", "ls"));
    TEST_FALSE("ls blocked after remove",
               allowlist_check("terminal", "ls /tmp"));
    TEST_TRUE("cat still allowed",
              allowlist_check("terminal", "cat /etc/passwd"));

    /* Remove all for tool (pattern=NULL removes all patterns for tool) */
    TEST_TRUE("remove all for terminal",
              allowlist_remove("terminal", NULL));
    /* After removing all entries for a tool, everything passes again */
    TEST_TRUE("cat allowed after remove all (no entries = allow all)",
              allowlist_check("terminal", "cat /etc/passwd"));
}

/* ================================================================
 *  4. Clear
 * ================================================================ */
static void test_clear(void) {
    printf("\n--- Clear ---\n");

    allowlist_clear();
    allowlist_add("terminal", "rm");
    allowlist_add("file", "delete");

    /* Verify restricted */
    TEST_TRUE("rm allowed before clear",
              allowlist_check("terminal", "rm file"));
    TEST_FALSE("echo blocked before clear",
               allowlist_check("terminal", "echo hi"));

    /* Clear all */
    allowlist_clear();

    /* After clear, everything allowed */
    TEST_TRUE("all allowed after clear",
              allowlist_check("terminal", "echo hi"));
    TEST_TRUE("delete_allowed after clear",
              allowlist_check("file", "delete_everything"));
}

/* ================================================================
 *  5. NULL safety
 * ================================================================ */
static void test_null_safety(void) {
    printf("\n--- NULL Safety ---\n");

    allowlist_clear();

    TEST_FALSE("add NULL tool",   allowlist_add(NULL, "pattern"));
    TEST_FALSE("add NULL pattern", allowlist_add("tool", NULL));
    TEST_FALSE("add both NULL",   allowlist_add(NULL, NULL));
    TEST_FALSE("remove NULL tool", allowlist_remove(NULL, "pattern"));
    TEST_FALSE("check NULL tool",  allowlist_check(NULL, "command"));
    TEST_FALSE("check NULL cmd",   allowlist_check("tool", NULL));

    /* allowlist_remove with NULL pattern is valid (removes all for tool) */
    /* On empty allowlist, remove with NULL returns false (nothing to remove) */
    TEST_FALSE("remove NULL pattern on empty returns false",
               allowlist_remove("tool", NULL));
}

/* ================================================================
 *  6. Duplicate entries
 * ================================================================ */
static void test_duplicates(void) {
    printf("\n--- Duplicates ---\n");

    allowlist_clear();

    TEST_TRUE("add first", allowlist_add("tool", "pattern"));
    TEST_TRUE("add duplicate (returns true, not stored again)",
              allowlist_add("tool", "pattern"));
    /* Check still works with single entry */
    TEST_TRUE("check after duplicate",
              allowlist_check("tool", "pattern123"));

    /* Remove — removes the single entry */
    TEST_TRUE("remove entry", allowlist_remove("tool", "pattern"));
    /* No entries left — everything passes */
    TEST_TRUE("all allowed after remove",
              allowlist_check("tool", "anything"));
}

int main(void) {
    printf("=== Command Allowlist Test Suite (G126) ===\n");

    test_basic_add_check();
    test_multiple_patterns();
    test_remove();
    test_clear();
    test_null_safety();
    test_duplicates();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
