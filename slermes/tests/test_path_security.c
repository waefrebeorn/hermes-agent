/*
 * test_path_security.c — Tests for path security functions.
 *
 * Tests path_within_dir() and path_has_traversal() added to libpath.
 */

#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while(0)

/* ─── path_has_traversal tests ────────────────────────────── */

static int test_has_traversal_simple(void) {
    ASSERT(path_has_traversal("foo/../bar") == true, "Should detect '..' component");
    return 1;
}

static int test_has_traversal_leading(void) {
    ASSERT(path_has_traversal("../foo") == true, "Should detect leading '..'");
    return 1;
}

static int test_has_traversal_no_traversal(void) {
    ASSERT(path_has_traversal("foo/bar/baz") == false, "No traversal present");
    return 1;
}

static int test_has_traversal_double_dot_in_name(void) {
    /* ".." as substring should not match — only whole components */
    ASSERT(path_has_traversal("foo..bar/file.txt") == false, "Double-dot in name is not traversal");
    return 1;
}

static int test_has_traversal_empty(void) {
    ASSERT(path_has_traversal("") == false, "Empty path has no traversal");
    return 1;
}

static int test_has_traversal_null(void) {
    ASSERT(path_has_traversal(NULL) == false, "NULL path has no traversal");
    return 1;
}

static int test_has_traversal_root_relative(void) {
    ASSERT(path_has_traversal("/foo/bar/../../baz") == true, "Root-relative path with '..'");
    return 1;
}

static int test_has_traversal_dotdot_alone(void) {
    ASSERT(path_has_traversal("..") == true, "Single '..' component");
    return 1;
}

/* ─── path_within_dir tests ───────────────────────────────── */

static int test_within_dir_simple(void) {
    /* Create temp dir with a file inside */
    char tmpdir[] = "/tmp/pathsec_test_XXXXXX";
    ASSERT(mkdtemp(tmpdir) != NULL, "Create temp dir");

    char subfile[256];
    snprintf(subfile, sizeof(subfile), "%s/test.txt", tmpdir);
    FILE *f = fopen(subfile, "w");
    ASSERT(f != NULL, "Create test file");
    fclose(f);

    /* Should be within dir */
    char *err = path_within_dir(subfile, tmpdir);
    ASSERT(err == NULL, "File within dir should be safe");
    if (err) free(err);

    /* Cleanup */
    remove(subfile);
    rmdir(tmpdir);
    return 1;
}

static int test_within_dir_outside(void) {
    char tmpdir[] = "/tmp/pathsec_test_XXXXXX";
    ASSERT(mkdtemp(tmpdir) != NULL, "Create temp dir");

    /* Path outside should fail */
    char *err = path_within_dir("/etc/passwd", tmpdir);
    ASSERT(err != NULL, "Path outside dir should error");
    ASSERT(strstr(err, "escapes") != NULL, "Error should mention escape");
    free(err);

    rmdir(tmpdir);
    return 1;
}

static int test_within_dir_exact_root(void) {
    char tmpdir[] = "/tmp/pathsec_test_XXXXXX";
    ASSERT(mkdtemp(tmpdir) != NULL, "Create temp dir");

    /* Root itself should be acceptable */
    char *err = path_within_dir(tmpdir, tmpdir);
    ASSERT(err == NULL, "Root path equals root dir");
    if (err) free(err);

    rmdir(tmpdir);
    return 1;
}

static int test_within_dir_traversal_attack(void) {
    char tmpdir[] = "/tmp/pathsec_test_XXXXXX";
    ASSERT(mkdtemp(tmpdir) != NULL, "Create temp dir");

    char attack[256];
    snprintf(attack, sizeof(attack), "%s/../../etc/passwd", tmpdir);

    /* Traversal attack should fail — realpath resolves '..' */
    char *err = path_within_dir(attack, tmpdir);
    ASSERT(err != NULL, "Traversal attack should be rejected");
    free(err);

    rmdir(tmpdir);
    return 1;
}

static int test_within_dir_nonexistent_path(void) {
    char tmpdir[] = "/tmp/pathsec_test_XXXXXX";
    ASSERT(mkdtemp(tmpdir) != NULL, "Create temp dir");

    char nob[256];
    snprintf(nob, sizeof(nob), "%s/nonexistent_file_xyz", tmpdir);

    /* Non-existent path inside root should pass (path may not exist but would be within) */
    char *err = path_within_dir(nob, tmpdir);
    /* realpath fails on non-existent, so this returns error */
    ASSERT(err != NULL, "Non-existent path should error");
    free(err);

    rmdir(tmpdir);
    return 1;
}

static int test_within_dir_null_args(void) {
    char *err = path_within_dir(NULL, "/tmp");
    ASSERT(err != NULL, "NULL path should error");
    ASSERT(strstr(err, "NULL") != NULL, "Error should mention NULL");
    free(err);
    return 1;
}

static int test_within_dir_empty_args(void) {
    char *err = path_within_dir("", "/tmp");
    ASSERT(err != NULL, "Empty path should error");
    ASSERT(strstr(err, "empty") != NULL, "Error should mention empty");
    free(err);
    return 1;
}

/* ─── Runner ──────────────────────────────────────────────── */

int main(void) {
    printf("=== path_security tests ===\n\n");

    printf("-- path_has_traversal --\n");
    TEST(has_traversal_simple);
    TEST(has_traversal_leading);
    TEST(has_traversal_no_traversal);
    TEST(has_traversal_double_dot_in_name);
    TEST(has_traversal_empty);
    TEST(has_traversal_null);
    TEST(has_traversal_root_relative);
    TEST(has_traversal_dotdot_alone);

    printf("\n-- path_within_dir --\n");
    TEST(within_dir_simple);
    TEST(within_dir_outside);
    TEST(within_dir_exact_root);
    TEST(within_dir_traversal_attack);
    TEST(within_dir_nonexistent_path);
    TEST(within_dir_null_args);
    TEST(within_dir_empty_args);

    printf("\n%s\n", tests_failed > 0 ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    printf("Results: %d/%d passed, %d failed, %d total (sum %d)\n",
           tests_passed, tests_run, tests_failed,
           tests_run, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
