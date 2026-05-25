/*
 * test_x_search.c — xAI X/Twitter search tool tests.
 * Tests error paths that don't require HTTP calls:
 * missing API key, bad args, missing query.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Forward declaration of handler */
char *x_search_handler(const char *args_json, const char *task_id);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

static void test_missing_api_key(void) {
    TEST("x_search_handler without API key returns error");
    /* Unset the env var */
    unsetenv("XAI_API_KEY");
    char *result = x_search_handler("{\"query\":\"hello\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "XAI_API_KEY") != NULL);
    free(result);
    PASS();
}

static void test_null_args(void) {
    TEST("x_search_handler(NULL args) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler(NULL, NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "No args") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_bad_json(void) {
    TEST("x_search_handler(bad JSON) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("not json", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "JSON parse") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_missing_query(void) {
    TEST("x_search_handler(no query) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"other\":\"data\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "Missing query") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

int main(void) {
    printf("=== xAI Search Tool Tests ===\n");

    test_missing_api_key();
    test_null_args();
    test_bad_json();
    test_missing_query();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
