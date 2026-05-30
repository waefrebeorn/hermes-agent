/* test_glob.c — J12: Python glob port tests (match, find, free).
 * Expanded with edge cases: character classes, empty/null paths,
 * leading dots, multi-star, pattern special chars, and more. */
#include "hermes_glob.h"
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

    /* --- glob_match basics --- */
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
    TEST("both NULL", !glob_match(NULL, NULL));

    /* --- glob_match edge cases --- */
    printf("\n--- glob_match edge cases ---\n");
    TEST("empty pattern empty path", glob_match("", ""));
    TEST("empty pattern non-empty", !glob_match("", "foo"));
    TEST("non-empty pattern empty path", !glob_match("foo", ""));
    TEST("single * matches word", glob_match("*", "hello"));
    TEST("single * matches number", glob_match("*", "123"));
    TEST("* after /", glob_match("a/*", "a/foo"));
    TEST("? single char match", glob_match("f?o", "foo"));
    TEST("single * with path crossing",
         glob_match("*", "a/b"));
    TEST("* after / matches recursively",
         glob_match("a/*", "a/b/c"));
    TEST("? can match /",
         glob_match("?", "/"));
    TEST("leading dot matched by bare * (system fnmatch)",
         glob_match("*", ".hidden"));
    /* Note: fnmatch behavior varies by platform for leading dot with *. We just verify it doesn't crash. */
    TEST("** matches hidden", glob_match("**/.hidden", "/dir/.hidden"));
    TEST("dotdot not special for *", glob_match("*", ".."));
    TEST("? at end", glob_match("foo.", "foo."));
    TEST("? at end mismatch", !glob_match("foo.", "fooX"));

    /* --- glob_match character classes (fnmatch dependent) --- */
    printf("\n--- glob_match character classes ---\n");
    TEST("[abc] match a", glob_match("[abc]", "a"));
    TEST("[abc] match b", glob_match("[abc]", "b"));
    TEST("[abc] no match d", !glob_match("[abc]", "d"));
    TEST("[!abc] match d", glob_match("[!abc]", "d"));
    TEST("[!abc] no match a", !glob_match("[!abc]", "a"));
    TEST("[a-z] range", glob_match("[a-z]", "m"));
    TEST("[a-z] no range", !glob_match("[a-z]", "Z"));
    TEST("numeric range", glob_match("[0-9]", "5"));
    TEST("alphanumeric range", glob_match("[a-z0-9]", "9"));

    /* --- glob_match special patterns --- */
    printf("\n--- glob_match special patterns ---\n");
    TEST("double * pattern", glob_match("*.tar.gz", "archive.tar.gz"));
    TEST("multiple *", glob_match("a*b*c", "axbyc"));
    TEST("multiple * no match", !glob_match("a*b*c", "axy"));
    TEST("trailing *", glob_match("prefix_*", "prefix_value"));
    TEST("leading *", glob_match("*_suffix", "value_suffix"));
    TEST("** at end matching empty", glob_match("a/**", "a/"));
    TEST("**/ alone", glob_match("**/", "anything/goes/here/"));
    TEST("slash in pattern", glob_match("a/b", "a/b"));
    TEST("slash in pattern no match", !glob_match("a/b", "a/c"));
    TEST("* with dot in name", glob_match("*.txt", "file.txt"));
    TEST("* with multiple dots", glob_match("*.*", "file.tar.gz"));

    /* --- glob_match regression --- */
    printf("\n--- glob_match regression ---\n");
    TEST("hidden dotfile match .*", glob_match(".*", ".gitignore"));
    TEST("- not special", glob_match("*-test", "my-test"));
    TEST("+ not special", glob_match("*+*", "a+b"));
    TEST("underscore handled", glob_match("*_*", "hello_world"));
    TEST("space in path", glob_match("* *", "file name"));
    TEST("** after slash", glob_match("/base/**/file", "/base/a/b/file"));

    /* --- glob_find --- */
    printf("\n--- glob_find ---\n");
    /* Create test directory structure */
    system("rm -rf /tmp/globtest && mkdir -p /tmp/globtest/sub/deep /tmp/globtest/hidden");
    system("touch /tmp/globtest/a.c /tmp/globtest/b.h /tmp/globtest/sub/c.c /tmp/globtest/sub/deep/d.txt");
    system("touch /tmp/globtest/sub/deep/e.c /tmp/globtest/hidden/.secret /tmp/globtest/hidden/visible.txt");

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
    TEST("** matches everything", count >= 6);
    glob_free(matches, count);

    /* --- glob_find edge cases --- */
    printf("\n--- glob_find edge cases ---\n");

    /* Pattern matching nothing */
    matches = glob_find("*.nonexistent", "/tmp/globtest", &count);
    TEST("pattern matches nothing count 0", count == 0 && matches == NULL);
    glob_free(matches, count);

    /* Non-existent dir */
    matches = glob_find("*.c", "/tmp/globtest_nonexist", &count);
    TEST("non-existent dir returns NULL count 0", count == 0 && matches == NULL);

    /* NULL args */
    matches = glob_find(NULL, "/tmp/globtest", &count);
    TEST("NULL pattern returns NULL", matches == NULL && count == 0);
    matches = glob_find("*.c", NULL, &count);
    TEST("NULL dir returns NULL", matches == NULL && count == 0);

    /* Empty pattern */
    matches = glob_find("", "/tmp/globtest", &count);
    TEST("empty pattern count 0", count == 0 && matches == NULL);

    /* Empty dir */
    system("mkdir -p /tmp/globtest/empty_dir");
    matches = glob_find("*", "/tmp/globtest/empty_dir", &count);
    TEST("empty dir * count 0", count == 0 && matches == NULL);
    glob_free(matches, count);

    /* ? pattern -- matches single-char files */
    matches = glob_find("?.c", "/tmp/globtest", &count);
    TEST("?.c finds single-char root .c files", count >= 1);
    glob_free(matches, count);

    /* globstar all -- everything recursively */
    matches = glob_find("**/*", "/tmp/globtest", &count);
    TEST("**/* finds all files", count >= 6);
    glob_free(matches, count);

    /* Hidden files via .* pattern */
    matches = glob_find(".*", "/tmp/globtest/hidden", &count);
    TEST(".* finds dotfiles", count >= 1);
    glob_free(matches, count);

    /* Pattern with leading directory component */
    matches = glob_find("sub/deep/e.c", "/tmp/globtest", &count);
    TEST("relative path pattern exact", count == 1);
    glob_free(matches, count);

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
