/* test_glob.c — J12: Python glob port tests (match, find, free). */
#include "glob.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== J12: libglob Tests ===\n\n");

    /* --- glob_match --- */
    printf("--- glob_match ---\n");
    TEST("exact match", glob_match("foo.c", "foo.c"));
    TEST("wildcard *", glob_match("*.c", "foo.c"));
    TEST("wildcard ?", glob_match("fo?.c", "foo.c"));
    TEST("no match", !glob_match("*.h", "foo.c"));
    TEST("** matches all", glob_match("**", "a/b/c/d.txt"));
    TEST("**/ prefix", glob_match("**/foo.c", "a/b/c/foo.c"));
    TEST("**/ prefix direct", glob_match("**/foo.c", "foo.c"));
    TEST("dir/** matches subdir", glob_match("src/**", "src/main.c"));
    TEST("dir/**/file", glob_match("src/**/test.c", "src/a/b/test.c"));
    TEST("mixed pattern", glob_match("src/*/test.c", "src/a/test.c"));
    TEST("multi-level **", glob_match("a/**/z", "a/b/c/d/e/f/g/z"));
    TEST("NULL pattern", !glob_match(NULL, "foo.c"));
    TEST("NULL path", !glob_match("foo.c", NULL));

    /* --- glob_find (needs temp directory) --- */
    printf("\n--- glob_find ---\n");
    /* Create test directory structure */
    system("rm -rf /tmp/globtest && mkdir -p /tmp/globtest/sub/deep");
    system("touch /tmp/globtest/a.c /tmp/globtest/b.h /tmp/globtest/sub/c.c /tmp/globtest/sub/deep/d.txt");
    system("touch /tmp/globtest/sub/deep/e.c");

    int count = 0;
    char **matches = glob_find("*.c", "/tmp/globtest", &count);
    TEST("find *.c in root", count >= 1);
    if (matches) {
        bool found = false;
        for (int i = 0; i < count; i++) {
            if (strstr(matches[i], "a.c")) found = true;
        }
        TEST("found a.c", found);
    }
    glob_free(matches, count);

    /* Recursive ** */
    matches = glob_find("**/*.c", "/tmp/globtest", &count);
    TEST("recursive **/*.c finds 3+", count >= 3);
    glob_free(matches, count);

    matches = glob_find("**", "/tmp/globtest", &count);
    TEST("** matches everything", count >= 5);
    glob_free(matches, count);

    /* Non-existent dir */
    matches = glob_find("*.c", "/tmp/globtest_nonexist", &count);
    TEST("non-existent dir returns NULL count 0", count == 0 && matches == NULL);

    /* NULL args */
    matches = glob_find(NULL, "/tmp/globtest", &count);
    TEST("NULL pattern returns NULL", matches == NULL && count == 0);

    matches = glob_find("*.c", NULL, &count);
    TEST("NULL dir returns NULL", matches == NULL && count == 0);

    /* Cleanup */
    system("rm -rf /tmp/globtest");

    /* --- glob_free NULL safety --- */
    printf("\n--- glob_free NULL safety ---\n");
    glob_free(NULL, 0);
    glob_free(NULL, 10);
    TEST("glob_free(NULL) no crash", 1);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
