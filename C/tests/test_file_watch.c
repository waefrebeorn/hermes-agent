/* test_file_watch.c — test file_watch tool error paths */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Forward declarations */
extern char *file_watch_handler(const char *args_json, const char *task_id);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

static void test_null_args(void) {
    TEST("file_watch_handler(NULL args) returns error");
    char *result = file_watch_handler(NULL, "test");
    assert(result != NULL);
    assert(strstr(result, "No args") != NULL);
    free(result);
    PASS();
}

static void test_bad_json(void) {
    TEST("file_watch_handler(bad JSON) returns error");
    char *result = file_watch_handler("{bad json}", "test");
    assert(result != NULL);
    assert(strstr(result, "JSON parse") != NULL);
    free(result);
    PASS();
}

static void test_missing_path(void) {
    TEST("file_watch_handler(missing path) returns error");
    char *result = file_watch_handler("{}", "test");
    assert(result != NULL);
    assert(strstr(result, "Missing") != NULL || strstr(result, "error") != NULL);
    free(result);
    PASS();
}

int main(void) {
    printf("file_watch tests:\n");
    test_null_args();
    test_bad_json();
    test_missing_path();
    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
