/*
 * test_path.c — J04: path manipulation library tests.
 *
 * Tests: join, basename, dirname, stem, suffix, suffixes,
 * ext_swap, exists, is_dir, is_file, resolve, abs,
 * is_absolute, glob, fnmatch, normalize.
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
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    const char *_a = (a); \
    const char *_b = (b); \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        printf("    assertion failed: %s -- expected \"%s\" got \"%s\" (%s:%d)\n", \
               msg, _b ? _b : "(null)", _a ? _a : "(null)", __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* join */
static bool test_join_basic(void) {
    char *r = path_join("a", "b");
    ASSERT_STR_EQ(r, "a/b", ""); free(r); return true;
}
static bool test_join_trailing_slash(void) {
    char *r = path_join("a/", "b");
    ASSERT_STR_EQ(r, "a/b", ""); free(r); return true;
}
static bool test_join_absolute_child(void) {
    char *r = path_join("a", "/absolute");
    ASSERT_STR_EQ(r, "/absolute", ""); free(r); return true;
}
static bool test_join_null_parent(void) {
    char *r = path_join(NULL, "child");
    ASSERT_STR_EQ(r, "child", ""); free(r); return true;
}
static bool test_join_null_child(void) {
    char *r = path_join("parent", NULL);
    ASSERT_STR_EQ(r, "parent", ""); free(r); return true;
}
static bool test_join_empty(void) {
    char *r = path_join("", "child");
    ASSERT_STR_EQ(r, "child", ""); free(r); return true;
}
static bool test_join_multiple(void) {
    char *r = path_join_n(3, "a", "b", "c");
    ASSERT_STR_EQ(r, "a/b/c", ""); free(r); return true;
}
static bool test_join_n_single(void) {
    char *r = path_join_n(1, "only");
    ASSERT_STR_EQ(r, "only", ""); free(r); return true;
}
static bool test_join_n_absolute(void) {
    char *r = path_join_n(3, "a", "/abs", "c");
    ASSERT_STR_EQ(r, "/abs/c", ""); free(r); return true;
}

/* basename */
static bool test_basename_simple(void) {
    ASSERT_STR_EQ(path_basename("foo.txt"), "foo.txt", ""); return true;
}
static bool test_basename_path(void) {
    ASSERT_STR_EQ(path_basename("/a/b/c.txt"), "c.txt", ""); return true;
}
static bool test_basename_root(void) {
    ASSERT_STR_EQ(path_basename("/"), "", ""); return true;
}
static bool test_basename_trailing_slash(void) {
    ASSERT_STR_EQ(path_basename("/a/b/"), "", ""); return true;
}
static bool test_basename_no_dir(void) {
    ASSERT_STR_EQ(path_basename("no-slash"), "no-slash", ""); return true;
}
static bool test_basename_null(void) {
    ASSERT_STR_EQ(path_basename(NULL), "", ""); return true;
}
static bool test_basename_hidden(void) {
    ASSERT_STR_EQ(path_basename("/dir/.hidden"), ".hidden", ""); return true;
}

/* dirname */
static bool test_dirname_simple(void) {
    char *r = path_dirname("a/b"); ASSERT_STR_EQ(r, "a", ""); free(r); return true;
}
static bool test_dirname_single(void) {
    char *r = path_dirname("foo"); ASSERT_STR_EQ(r, ".", ""); free(r); return true;
}
static bool test_dirname_root(void) {
    char *r = path_dirname("/"); ASSERT_STR_EQ(r, "/", ""); free(r); return true;
}
static bool test_dirname_root_child(void) {
    char *r = path_dirname("/foo"); ASSERT_STR_EQ(r, "/", ""); free(r); return true;
}
static bool test_dirname_deep(void) {
    char *r = path_dirname("/a/b/c/d"); ASSERT_STR_EQ(r, "/a/b/c", ""); free(r); return true;
}
static bool test_dirname_trailing_slash(void) {
    char *r = path_dirname("/a/b/"); ASSERT_STR_EQ(r, "/a/b", ""); free(r); return true;
}
static bool test_dirname_null(void) {
    char *r = path_dirname(NULL); ASSERT_STR_EQ(r, ".", ""); free(r); return true;
}

/* stem (now allocates) */
static bool test_stem_simple(void) {
    char *r = path_stem("foo.txt"); ASSERT_STR_EQ(r, "foo", ""); free(r); return true;
}
static bool test_stem_path(void) {
    char *r = path_stem("/a/b/archive.tar.gz"); ASSERT_STR_EQ(r, "/a/b/archive.tar", ""); free(r); return true;
}
static bool test_stem_no_ext(void) {
    char *r = path_stem("README"); ASSERT_STR_EQ(r, "README", ""); free(r); return true;
}
static bool test_stem_hidden(void) {
    char *r = path_stem("/d/.hidden"); ASSERT_STR_EQ(r, "/d/.hidden", ""); free(r); return true;
}

/* suffix (non-owning) */
static bool test_suffix_simple(void) {
    ASSERT_STR_EQ(path_suffix("foo.txt"), ".txt", ""); return true;
}
static bool test_suffix_no_ext(void) {
    ASSERT_STR_EQ(path_suffix("README"), "", ""); return true;
}
static bool test_suffix_double(void) {
    ASSERT_STR_EQ(path_suffix("a.tar.gz"), ".gz", ""); return true;
}
static bool test_suffix_hidden(void) {
    ASSERT_STR_EQ(path_suffix(".hidden"), "", ""); return true;
}
static bool test_suffix_hidden_ext(void) {
    ASSERT_STR_EQ(path_suffix(".hidden.txt"), ".txt", ""); return true;
}
static bool test_suffix_null(void) {
    ASSERT_STR_EQ(path_suffix(NULL), "", ""); return true;
}

/* suffixes (allocates array) */
static bool test_suffixes_single(void) {
    char **out = NULL; int n = path_suffixes("file.txt", &out);
    ASSERT(n == 1, ""); ASSERT_STR_EQ(out[0], ".txt", ""); free(out[0]); free(out); return true;
}
static bool test_suffixes_double(void) {
    char **out = NULL; int n = path_suffixes("a.tar.gz", &out);
    ASSERT(n == 2, ""); ASSERT_STR_EQ(out[0], ".tar", ""); ASSERT_STR_EQ(out[1], ".gz", "");
    free(out[0]); free(out[1]); free(out); return true;
}
static bool test_suffixes_none(void) {
    char **out = (void*)1; int n = path_suffixes("README", &out);
    ASSERT(n == 0, ""); ASSERT(out == NULL, ""); return true;
}
static bool test_suffixes_empty(void) {
    char **out = (void*)1; int n = path_suffixes("", &out);
    ASSERT(n == 0, ""); ASSERT(out == NULL, ""); return true;
}
static bool test_suffixes_hidden(void) {
    char **out = NULL; int n = path_suffixes(".hidden.yml", &out);
    ASSERT(n == 1, ""); ASSERT_STR_EQ(out[0], ".yml", ""); free(out[0]); free(out); return true;
}
static bool test_suffixes_path(void) {
    char **out = NULL; int n = path_suffixes("/dir/sub/file.txt", &out);
    ASSERT(n == 1, ""); ASSERT_STR_EQ(out[0], ".txt", ""); free(out[0]); free(out); return true;
}

/* ext_swap */
static bool test_ext_swap_simple(void) {
    char *r = path_ext_swap("a.txt", ".md"); ASSERT_STR_EQ(r, "a.md", ""); free(r); return true;
}
static bool test_ext_swap_no_dot(void) {
    char *r = path_ext_swap("a.txt", "md"); ASSERT_STR_EQ(r, "a.md", ""); free(r); return true;
}
static bool test_ext_swap_path(void) {
    char *r = path_ext_swap("/d/f.txt", ".bak"); ASSERT_STR_EQ(r, "/d/f.bak", ""); free(r); return true;
}
static bool test_ext_swap_add(void) {
    char *r = path_ext_swap("README", ".md"); ASSERT_STR_EQ(r, "README.md", ""); free(r); return true;
}
static bool test_ext_swap_remove(void) {
    char *r = path_ext_swap("a.txt", ""); ASSERT_STR_EQ(r, "a", ""); free(r); return true;
}
static bool test_ext_swap_hidden(void) {
    char *r = path_ext_swap("/d/.hidden.yml", ".bak"); ASSERT_STR_EQ(r, "/d/.hidden.bak", ""); free(r); return true;
}
static bool test_ext_swap_null_path(void) {
    char *r = path_ext_swap(NULL, ".ext"); ASSERT_STR_EQ(r, "", ""); free(r); return true;
}

/* is_absolute */
static bool test_is_absolute_yes(void) {
    ASSERT(path_is_absolute("/etc"), ""); return true;
}
static bool test_is_absolute_no(void) {
    ASSERT(!path_is_absolute("rel"), ""); return true;
}
static bool test_is_absolute_empty(void) {
    ASSERT(!path_is_absolute(""), ""); return true;
}
static bool test_is_absolute_null(void) {
    ASSERT(!path_is_absolute(NULL), ""); return true;
}

/* abs */
static bool test_abs_absolute(void) {
    char *r = path_abs("/etc"); ASSERT_STR_EQ(r, "/etc", ""); free(r); return true;
}
static bool test_abs_relative(void) {
    char *r = path_abs("."); ASSERT(r != NULL && r[0] == '/', ""); free(r); return true;
}
static bool test_abs_null(void) {
    ASSERT(path_abs(NULL) == NULL, ""); return true;
}

/* exists / is_dir / is_file */
static bool test_exists_no(void) {
    ASSERT(!path_exists("/nonexistent_xyz"), ""); return true;
}
static bool test_exists_dir(void) {
    ASSERT(path_exists("/tmp"), ""); return true;
}
static bool test_is_dir_yes(void) {
    ASSERT(path_is_dir("/tmp"), ""); return true;
}
static bool test_is_dir_no(void) {
    ASSERT(!path_is_dir("/nonexistent"), ""); return true;
}
static bool test_is_file_null(void) {
    ASSERT(!path_is_file(NULL), ""); return true;
}

/* resolve */
static bool test_resolve_no(void) {
    ASSERT(path_resolve("/nonexistent_xyz") == NULL, ""); return true;
}
static bool test_resolve_tmp(void) {
    char *r = path_resolve("/tmp"); ASSERT(r != NULL, ""); free(r); return true;
}
static bool test_resolve_null(void) {
    ASSERT(path_resolve(NULL) == NULL, ""); return true;
}

/* normalize */
static bool test_normalize_simple(void) {
    char *r = path_normalize("a/b/c"); ASSERT_STR_EQ(r, "a/b/c", ""); free(r); return true;
}
static bool test_normalize_dot(void) {
    char *r = path_normalize("a/./b"); ASSERT_STR_EQ(r, "a/b", ""); free(r); return true;
}
static bool test_normalize_dotdot(void) {
    char *r = path_normalize("a/b/../c"); ASSERT_STR_EQ(r, "a/c", ""); free(r); return true;
}
static bool test_normalize_double_slash(void) {
    char *r = path_normalize("a//b"); ASSERT_STR_EQ(r, "a/b", ""); free(r); return true;
}
static bool test_normalize_absolute(void) {
    char *r = path_normalize("/a/b/../c"); ASSERT_STR_EQ(r, "/a/c", ""); free(r); return true;
}
static bool test_normalize_to_cwd(void) {
    char *r = path_normalize("."); ASSERT_STR_EQ(r, ".", ""); free(r); return true;
}
static bool test_normalize_dotdot_above(void) {
    char *r = path_normalize("a/../../b"); ASSERT_STR_EQ(r, "b", ""); free(r); return true;
}
static bool test_normalize_null(void) {
    char *r = path_normalize(NULL); ASSERT_STR_EQ(r, ".", ""); free(r); return true;
}
static bool test_normalize_trailing(void) {
    char *r = path_normalize("a/b/"); ASSERT_STR_EQ(r, "a/b", ""); free(r); return true;
}

/* fnmatch */
static bool test_fnmatch_wildcard(void) {
    ASSERT(path_fnmatch("*.txt", "foo.txt"), ""); return true;
}
static bool test_fnmatch_no_match(void) {
    ASSERT(!path_fnmatch("*.txt", "foo.md"), ""); return true;
}
static bool test_fnmatch_question(void) {
    ASSERT(path_fnmatch("file.???", "file.txt"), ""); return true;
}
static bool test_fnmatch_null(void) {
    ASSERT(!path_fnmatch(NULL, "foo"), ""); ASSERT(!path_fnmatch("*.txt", NULL), ""); return true;
}

/* glob */
static bool test_glob_no_match(void) {
    char **out = NULL; int n = path_glob("/nonexistent_glob_xyz/*", &out);
    ASSERT(n == 0, ""); return true;
}
static bool test_glob_null(void) {
    char **out = NULL; ASSERT(path_glob(NULL, &out) == -1, ""); return true;
}

int main(void) {
    printf("=== Path Library Tests (J04) ===\n\n");

    TEST(join_basic); TEST(join_trailing_slash); TEST(join_absolute_child);
    TEST(join_null_parent); TEST(join_null_child); TEST(join_empty);
    TEST(join_multiple); TEST(join_n_single); TEST(join_n_absolute);

    TEST(basename_simple); TEST(basename_path); TEST(basename_root);
    TEST(basename_trailing_slash); TEST(basename_no_dir); TEST(basename_null);
    TEST(basename_hidden);

    TEST(dirname_simple); TEST(dirname_single); TEST(dirname_root);
    TEST(dirname_root_child); TEST(dirname_deep); TEST(dirname_trailing_slash);
    TEST(dirname_null);

    TEST(stem_simple); TEST(stem_path); TEST(stem_no_ext); TEST(stem_hidden);

    TEST(suffix_simple); TEST(suffix_no_ext); TEST(suffix_double);
    TEST(suffix_hidden); TEST(suffix_hidden_ext); TEST(suffix_null);

    TEST(suffixes_single); TEST(suffixes_double); TEST(suffixes_none);
    TEST(suffixes_empty); TEST(suffixes_hidden); TEST(suffixes_path);

    TEST(ext_swap_simple); TEST(ext_swap_no_dot); TEST(ext_swap_path);
    TEST(ext_swap_add); TEST(ext_swap_remove); TEST(ext_swap_hidden);
    TEST(ext_swap_null_path);

    TEST(is_absolute_yes); TEST(is_absolute_no); TEST(is_absolute_empty);
    TEST(is_absolute_null);

    TEST(abs_absolute); TEST(abs_relative); TEST(abs_null);

    TEST(exists_no); TEST(exists_dir); TEST(is_dir_yes); TEST(is_dir_no);
    TEST(is_file_null);

    TEST(resolve_no); TEST(resolve_tmp); TEST(resolve_null);

    TEST(normalize_simple); TEST(normalize_dot); TEST(normalize_dotdot);
    TEST(normalize_double_slash); TEST(normalize_absolute);
    TEST(normalize_to_cwd); TEST(normalize_dotdot_above);
    TEST(normalize_null); TEST(normalize_trailing);

    TEST(fnmatch_wildcard); TEST(fnmatch_no_match); TEST(fnmatch_question);
    TEST(fnmatch_null);

    TEST(glob_no_match); TEST(glob_null);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
