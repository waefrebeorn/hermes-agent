/* test_file_merge.c — test file_merge tool error paths */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

/* Forward declarations of functions we test */
extern char *file_merge_handler(const char *args_json, const char *task_id);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

static void test_null_args(void) {
    TEST("file_merge_handler(NULL args) returns error");
    char *result = file_merge_handler(NULL, "test");
    assert(result != NULL);
    assert(strstr(result, "No args") != NULL);
    free(result);
    PASS();
}

static void test_bad_json(void) {
    TEST("file_merge_handler(bad JSON) returns error");
    char *result = file_merge_handler("{bad json}", "test");
    assert(result != NULL);
    assert(strstr(result, "JSON parse") != NULL);
    free(result);
    PASS();
}

static void test_missing_params(void) {
    TEST("file_merge_handler(missing params) returns error");
    /* JSON with only base_path, missing modified_path and output_path */
    char *result = file_merge_handler("{\"base_path\":\"/tmp/x\"}", "test");
    assert(result != NULL);
    assert(strstr(result, "Missing") != NULL);
    free(result);
    PASS();
}

static void test_missing_base(void) {
    TEST("file_merge_handler(missing base_path) returns error");
    char *result = file_merge_handler("{\"modified_path\":\"a\",\"output_path\":\"b\"}", "test");
    assert(result != NULL);
    assert(strstr(result, "Missing") != NULL);
    free(result);
    PASS();
}

int main(void) {
    printf("file_merge tests:\n");
    test_null_args();
    test_bad_json();
    test_missing_params();
    test_missing_base();
    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
