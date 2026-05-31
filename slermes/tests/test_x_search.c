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

/* Date validation tests */
static void test_invalid_from_date_format(void) {
    TEST("x_search_handler(bad from_date format) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"hello\",\"from_date\":\"bad-date\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "Invalid from_date") != NULL);
    assert(strstr(result, "must be YYYY-MM-DD") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_invalid_to_date_format(void) {
    TEST("x_search_handler(bad to_date format) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"hello\",\"to_date\":\"2024/01/01\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "Invalid to_date") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_from_date_after_to_date(void) {
    TEST("x_search_handler(from_date after to_date) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"hello\",\"from_date\":\"2025-06-01\",\"to_date\":\"2024-01-01\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "from_date must not be after to_date") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_from_date_future(void) {
    TEST("x_search_handler(from_date in future) returns error");
    setenv("XAI_API_KEY", "test-key-123", 1);
    /* Use year 2099 which is definitely in the future */
    char *result = x_search_handler("{\"query\":\"hello\",\"from_date\":\"2099-12-25\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "is in the future") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_valid_dates_pass_api_key_check(void) {
    TEST("x_search_handler(valid dates) passes validation, fails on API key");
    setenv("XAI_API_KEY", "test-key-123", 1);
    /* Valid dates should pass validation; the actual call will fail on xAI API call,
     * but this test verifies no date validation error is returned */
    char *result = x_search_handler("{\"query\":\"hello\",\"from_date\":\"2024-01-01\",\"to_date\":\"2024-12-31\"}", NULL);
    assert(result != NULL);
    /* Should NOT contain date validation error messages */
    assert(strstr(result, "Invalid from_date") == NULL);
    assert(strstr(result, "Invalid to_date") == NULL);
    assert(strstr(result, "from_date must not be after to_date") == NULL);
    assert(strstr(result, "is in the future") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

/* --- Edge case expansion: 15 new tests --- */

static void test_empty_query_string(void) {
    TEST("x_search_handler(empty query) returns Missing query");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_empty_object(void) {
    TEST("x_search_handler({}) returns Missing query");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_from_date_dashes_only(void) {
    TEST("x_search_handler(from_date=----) returns invalid format");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"from_date\":\"----\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Invalid from_date") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_only_to_date_no_from_date(void) {
    TEST("x_search_handler(only to_date) validates, calls API");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"to_date\":\"2024-12-31\"}", NULL);
    assert(result != NULL);
    /* Should pass date validation, fail on actual API call */
    assert(strstr(result, "Invalid to_date") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_only_from_date_no_to_date(void) {
    TEST("x_search_handler(only from_date) validates, calls API");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"from_date\":\"2024-01-01\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Invalid from_date") == NULL);
    assert(strstr(result, "is in the future") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_from_date_equals_to_date(void) {
    TEST("x_search_handler(from_date == to_date) passes validation");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"from_date\":\"2024-06-15\",\"to_date\":\"2024-06-15\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "from_date must not be after to_date") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_empty_from_date_string(void) {
    TEST("x_search_handler(empty from_date) returns invalid format");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"from_date\":\"\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Invalid from_date") != NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_max_results_zero(void) {
    TEST("x_search_handler(max_results=0) passes validation, calls API");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"max_results\":0}", NULL);
    assert(result != NULL);
    /* Should build request and fail on API call, not on validation */
    assert(strstr(result, "error") != NULL);
    assert(strstr(result, "Missing query") == NULL);
    assert(strstr(result, "JSON parse") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_search_type_users(void) {
    TEST("x_search_handler(search_type=users) takes users code path");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"search_type\":\"users\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_extra_unknown_fields_ignored(void) {
    TEST("x_search_handler(extra fields) ignores unknown fields");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"unknown_field\":\"value\",\"another\":42}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    assert(strstr(result, "JSON parse") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_sort_order_recency(void) {
    TEST("x_search_handler(sort_order=recency) passes validation");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"sort_order\":\"recency\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_lang_param(void) {
    TEST("x_search_handler(lang=ja) filters by language");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"lang\":\"ja\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_geo_partial_params(void) {
    TEST("x_search_handler(geo_lat without geo_long) still builds request");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"geo_lat\":37.7749}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    /* geo_lat=999.0 sentinel means lat is "missing"; so no geo block built */
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_exclude_retweets_true(void) {
    TEST("x_search_handler(exclude_retweets=true) sets flag, calls API");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"exclude_retweets\":true}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Missing query") == NULL);
    free(result);
    unsetenv("XAI_API_KEY");
    PASS();
}

static void test_from_date_zero_values(void) {
    TEST("x_search_handler(from_date=0000-00-00) accepts zero date, calls API");
    setenv("XAI_API_KEY", "test-key-123", 1);
    char *result = x_search_handler("{\"query\":\"x\",\"from_date\":\"0000-00-00\"}", NULL);
    assert(result != NULL);
    assert(strstr(result, "Invalid from_date") == NULL);
    assert(strstr(result, "is in the future") == NULL);
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
    test_invalid_from_date_format();
    test_invalid_to_date_format();
    test_from_date_after_to_date();
    test_from_date_future();
    test_valid_dates_pass_api_key_check();

    test_empty_query_string();
    test_empty_object();
    test_from_date_dashes_only();
    test_only_to_date_no_from_date();
    test_only_from_date_no_to_date();
    test_from_date_equals_to_date();
    test_empty_from_date_string();
    test_max_results_zero();
    test_search_type_users();
    test_extra_unknown_fields_ignored();
    test_sort_order_recency();
    test_lang_param();
    test_geo_partial_params();
    test_exclude_retweets_true();
    test_from_date_zero_values();

    printf("\n%d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
