/*
 * tests/test_dotenv.c — unit tests for libdotenv (.env file parser)
 *
 * Tests: parse, get, set, iter, count, export, NULL safety, edge cases.
 */

#include "dotenv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    fprintf(stderr, "  TEST: %s ... ", name); \
} while (0)

#define PASS() do { \
    tests_passed++; \
    fprintf(stderr, "PASS\n"); \
} while (0)

#define FAIL(msg) do { \
    tests_failed++; \
    fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
    if (!a || !b || strcmp(a, b) != 0) { \
        fprintf(stderr, "FAIL: %s: expected '%s', got '%s' (line %d)\n", \
                msg, b ? b : "(null)", a ? a : "(null)", __LINE__); \
        tests_failed++; \
        return; \
    } \
} while (0)

/* ================================================================
 *  Tests
 * ================================================================ */

static void test_parse_empty(void) {
    TEST("parse empty string");
    env_t *env = dotenv_parse("", NULL);
    ASSERT(env != NULL, "parse empty should not return NULL");
    ASSERT(dotenv_count(env) == 0, "empty env should have 0 entries");
    dotenv_free(env);
    PASS();
}

static void test_parse_single(void) {
    TEST("parse single KEY=VALUE");
    env_t *env = dotenv_parse("FOO=bar", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 1, "should have 1 entry");
    const char *val = dotenv_get(env, "FOO");
    ASSERT_STREQ(val, "bar", "FOO should equal bar");
    dotenv_free(env);
    PASS();
}

static void test_parse_multiple(void) {
    TEST("parse multiple KEY=VALUE lines");
    env_t *env = dotenv_parse("A=1\nB=2\nC=3", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 3, "should have 3 entries");
    ASSERT_STREQ(dotenv_get(env, "A"), "1", "A=1");
    ASSERT_STREQ(dotenv_get(env, "B"), "2", "B=2");
    ASSERT_STREQ(dotenv_get(env, "C"), "3", "C=3");
    dotenv_free(env);
    PASS();
}

static void test_parse_quoted(void) {
    TEST("parse quoted value");
    env_t *env = dotenv_parse("MSG=\"hello world\"", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT_STREQ(dotenv_get(env, "MSG"), "hello world", "MSG should strip quotes");
    dotenv_free(env);
    PASS();
}

static void test_parse_single_quotes(void) {
    TEST("parse single-quoted value");
    env_t *env = dotenv_parse("MSG='single quoted'", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT_STREQ(dotenv_get(env, "MSG"), "single quoted", "MSG should strip single quotes");
    dotenv_free(env);
    PASS();
}

static void test_parse_export_prefix(void) {
    TEST("parse 'export KEY=VALUE' syntax");
    env_t *env = dotenv_parse("export DB_HOST=localhost", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 1, "should have 1 entry");
    ASSERT_STREQ(dotenv_get(env, "DB_HOST"), "localhost", "DB_HOST should match");
    dotenv_free(env);
    PASS();
}

static void test_parse_comment(void) {
    TEST("parse with comments (# lines)");
    const char *input = "# This is a comment\nKEY=val\n# Another comment\nFOO=bar\n";
    env_t *env = dotenv_parse(input, NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 2, "should skip comment lines");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "val", "KEY=val");
    ASSERT_STREQ(dotenv_get(env, "FOO"), "bar", "FOO=bar");
    dotenv_free(env);
    PASS();
}

static void test_parse_inline_comment(void) {
    TEST("parse inline comment (# after value)");
    env_t *env = dotenv_parse("KEY=val # this is a comment", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "val", "KEY should strip inline comment");
    dotenv_free(env);
    PASS();
}

static void test_parse_empty_lines(void) {
    TEST("parse with blank lines");
    env_t *env = dotenv_parse("\n\nKEY=val\n\nFOO=bar\n\n", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 2, "should skip blank lines");
    dotenv_free(env);
    PASS();
}

static void test_parse_whitespace_trimming(void) {
    TEST("parse trims whitespace around key and value");
    env_t *env = dotenv_parse("  KEY  =  val  ", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "val", "KEY value should be trimmed");
    dotenv_free(env);
    PASS();
}

static void test_parse_trailing_newline(void) {
    TEST("parse with trailing newline");
    env_t *env = dotenv_parse("KEY=val\n", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 1, "should have 1 entry");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "val", "KEY=val");
    dotenv_free(env);
    PASS();
}

static void test_parse_value_with_equals(void) {
    TEST("parse value containing = sign");
    env_t *env = dotenv_parse("URL=http://example.com/path?q=1", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT_STREQ(dotenv_get(env, "URL"), "http://example.com/path?q=1",
                "URL should contain full value with =");
    dotenv_free(env);
    PASS();
}

static void test_parse_null_input(void) {
    TEST("parse NULL input");
    char *err = NULL;
    env_t *env = dotenv_parse(NULL, &err);
    ASSERT(env == NULL, "NULL input should return NULL");
    ASSERT(err != NULL, "error_msg should be set");
    free(err);
    PASS();
}

static void test_get_nonexistent(void) {
    TEST("get nonexistent key returns NULL");
    env_t *env = dotenv_parse("A=1", NULL);
    const char *val = dotenv_get(env, "NONEXISTENT");
    ASSERT(val == NULL, "nonexistent key should return NULL");
    dotenv_free(env);
    PASS();
}

static void test_get_null_env(void) {
    TEST("get with NULL env");
    const char *val = dotenv_get(NULL, "KEY");
    ASSERT(val == NULL, "NULL env should return NULL");
    PASS();
}

static void test_set_new(void) {
    TEST("set new key");
    env_t *env = dotenv_parse("", NULL);
    ASSERT(dotenv_set(env, "NEW", "value"), "set should succeed");
    ASSERT(dotenv_count(env) == 1, "should have 1 entry");
    ASSERT_STREQ(dotenv_get(env, "NEW"), "value", "NEW=value");
    dotenv_free(env);
    PASS();
}

static void test_set_override(void) {
    TEST("set override existing key");
    env_t *env = dotenv_parse("KEY=old", NULL);
    ASSERT(dotenv_set(env, "KEY", "new"), "override should succeed");
    ASSERT(dotenv_count(env) == 1, "count unchanged");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "new", "KEY should be 'new'");
    dotenv_free(env);
    PASS();
}

static void test_set_null_params(void) {
    TEST("set with NULL params returns false");
    env_t *env = dotenv_parse("", NULL);
    ASSERT(!dotenv_set(NULL, "K", "v"), "NULL env should fail");
    ASSERT(!dotenv_set(env, NULL, "v"), "NULL key should fail");
    ASSERT(!dotenv_set(env, "K", NULL), "NULL value should fail");
    dotenv_free(env);
    PASS();
}

static void test_iter(void) {
    TEST("iterate all entries");
    env_t *env = dotenv_parse("A=1\nB=2\nC=3", NULL);
    size_t idx = 0;
    const char *key, *value;
    int found[3] = {0, 0, 0};
    while (dotenv_iter(env, &idx, &key, &value)) {
        if (strcmp(key, "A") == 0 && strcmp(value, "1") == 0) found[0] = 1;
        if (strcmp(key, "B") == 0 && strcmp(value, "2") == 0) found[1] = 1;
        if (strcmp(key, "C") == 0 && strcmp(value, "3") == 0) found[2] = 1;
    }
    ASSERT(found[0] && found[1] && found[2], "should find all 3 entries");
    ASSERT(idx == 3, "idx should be 3 after full iteration");
    /* Second pass should yield nothing */
    ASSERT(!dotenv_iter(env, &idx, &key, &value), "second pass should yield nothing");
    dotenv_free(env);
    PASS();
}

static void test_iter_null_params(void) {
    TEST("iter with NULL params returns false");
    env_t *env = dotenv_parse("A=1", NULL);
    size_t idx = 0;
    const char *k, *v;
    ASSERT(!dotenv_iter(NULL, &idx, &k, &v), "NULL env");
    ASSERT(!dotenv_iter(env, NULL, &k, &v), "NULL idx");
    ASSERT(!dotenv_iter(env, &idx, NULL, &v), "NULL key ptr");
    ASSERT(!dotenv_iter(env, &idx, &k, NULL), "NULL value ptr");
    dotenv_free(env);
    PASS();
}

static void test_count_empty(void) {
    TEST("count on empty env returns 0");
    env_t *env = dotenv_parse("", NULL);
    ASSERT(dotenv_count(env) == 0, "empty count");
    dotenv_free(env);
    PASS();
}

static void test_count_null(void) {
    TEST("count on NULL returns 0");
    ASSERT(dotenv_count(NULL) == 0, "NULL count");
    PASS();
}

static void test_export(void) {
    TEST("export all entries to process environment");
    env_t *env = dotenv_parse("TEST_DOTENV_EXPORT=exported_value\n"
                              "TEST_DOTENV_ANOTHER=also_exported", NULL);
    ASSERT(dotenv_export(env), "export should succeed");
    const char *val = getenv("TEST_DOTENV_EXPORT");
    ASSERT_STREQ(val, "exported_value", "env var should be set");
    val = getenv("TEST_DOTENV_ANOTHER");
    ASSERT_STREQ(val, "also_exported", "second env var should be set");
    /* Cleanup so we don't pollute */
    unsetenv("TEST_DOTENV_EXPORT");
    unsetenv("TEST_DOTENV_ANOTHER");
    dotenv_free(env);
    PASS();
}

static void test_export_null(void) {
    TEST("export with NULL env returns false");
    ASSERT(!dotenv_export(NULL), "NULL export should return false");
    PASS();
}

static void test_free_null(void) {
    TEST("free NULL env is safe");
    dotenv_free(NULL);
    PASS();
}

static void test_load_nonexistent_file(void) {
    TEST("load nonexistent file returns NULL with error");
    char *err = NULL;
    env_t *env = dotenv_load("/tmp/__nonexistent_env_file__", &err);
    ASSERT(env == NULL, "nonexistent file should return NULL");
    ASSERT(err != NULL, "error_msg should be set");
    ASSERT(strstr(err, "cannot open") != NULL, "error should mention 'cannot open'");
    free(err);
    PASS();
}

static void test_load_file(void) {
    TEST("load from actual file");
    /* Write a temp .env file */
    FILE *f = fopen("/tmp/test_dotenv_load.env", "w");
    ASSERT(f != NULL, "temp file create");
    fprintf(f, "FILE_KEY=file_value\nANOTHER=42\n");
    fclose(f);

    char *err = NULL;
    env_t *env = dotenv_load("/tmp/test_dotenv_load.env", &err);
    ASSERT(env != NULL, "load should succeed");
    ASSERT(err == NULL, "error_msg should be NULL on success");
    ASSERT(dotenv_count(env) == 2, "should have 2 entries");
    ASSERT_STREQ(dotenv_get(env, "FILE_KEY"), "file_value", "FILE_KEY=file_value");
    ASSERT_STREQ(dotenv_get(env, "ANOTHER"), "42", "ANOTHER=42");

    dotenv_free(env);
    unlink("/tmp/test_dotenv_load.env");
    PASS();
}

static void test_load_null_path(void) {
    TEST("load with NULL path returns error");
    char *err = NULL;
    env_t *env = dotenv_load(NULL, &err);
    ASSERT(env == NULL, "NULL path should return NULL");
    ASSERT(err != NULL, "error_msg should be set");
    free(err);
    PASS();
}

static void test_parse_no_equals_skipped(void) {
    TEST("parse line without = is silently skipped");
    env_t *env = dotenv_parse("JUST_A_LINE\nKEY=val", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 1, "only KEY=val should be parsed");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "val", "KEY=val");
    dotenv_free(env);
    PASS();
}

static void test_parse_empty_value(void) {
    TEST("parse KEY= (empty value)");
    env_t *env = dotenv_parse("KEY=", NULL);
    ASSERT(env != NULL, "parse should succeed");
    ASSERT(dotenv_count(env) == 1, "should have 1 entry");
    ASSERT_STREQ(dotenv_get(env, "KEY"), "", "KEY should be empty string");
    dotenv_free(env);
    PASS();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    fprintf(stderr, "=== libdotenv Test Suite ===\n\n");

    test_parse_empty();
    test_parse_single();
    test_parse_multiple();
    test_parse_quoted();
    test_parse_single_quotes();
    test_parse_export_prefix();
    test_parse_comment();
    test_parse_inline_comment();
    test_parse_empty_lines();
    test_parse_whitespace_trimming();
    test_parse_trailing_newline();
    test_parse_value_with_equals();
    test_parse_null_input();
    test_get_nonexistent();
    test_get_null_env();
    test_set_new();
    test_set_override();
    test_set_null_params();
    test_iter();
    test_iter_null_params();
    test_count_empty();
    test_count_null();
    test_export();
    test_export_null();
    test_free_null();
    test_load_nonexistent_file();
    test_load_file();
    test_load_null_path();
    test_parse_no_equals_skipped();
    test_parse_empty_value();

    fprintf(stderr, "\n=== Results: %d/%d passed, %d failed ===\n",
            tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
