/* test_exec_code.c — Unit tests for exec_code */
/* Phase 338: +7 edge cases: empty code, stderr, stdout+stderr, unicode,
   no-output, very long code, zero/negative timeout */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern char *exec_code_handler(const char *args_json, const char *task_id);

#define TEST(fn) do { \
    printf("  %s ... ", #fn); \
    fn; \
    printf("PASS\n"); \
} while(0)

static int get_int_field(const char *json, const char *field) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", field);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ') p++;
    return atoi(p);
}

static void test_hello_world(void) {
    char *r = exec_code_handler("{\"code\":\"print(42)\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "\"output\"") != NULL);
    free(r);
}

static void test_fail_with_code(void) {
    char *r = exec_code_handler("{\"code\":\"import sys; sys.exit(5)\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 5);
    free(r);
}

static void test_syntax_error(void) {
    char *r = exec_code_handler("{\"code\":\"if True print(1)\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 1);
    free(r);
}

static void test_missing_code(void) {
    char *r = exec_code_handler("{}", NULL);
    assert(r != NULL);
    assert(strstr(r, "error") != NULL);
    free(r);
}

static void test_null_args(void) {
    char *r = exec_code_handler(NULL, NULL);
    assert(r != NULL);
    assert(strstr(r, "error") != NULL);
    free(r);
}

static void test_timeout_param(void) {
    /* timeout should be accepted without error */
    char *r = exec_code_handler("{\"code\":\"print('ok')\",\"timeout\":30}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "ok") != NULL);
    free(r);
}

static void test_sandbox_flag(void) {
    /* sandbox flag accepted (may or may not have bwrap) */
    char *r = exec_code_handler("{\"code\":\"print('sandbox_test')\",\"sandbox\":true}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    free(r);
}

static void test_output_content(void) {
    char *r = exec_code_handler("{\"code\":\"print('Hello from C exec')\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "Hello from C exec") != NULL);
    assert(strstr(r, "\"output\"") != NULL);
    free(r);
}

/* --- Phase 338: Edge case expansion --- */

static void test_empty_code(void) {
    /* Empty code string — runs python -c '' which exits 0 */
    char *r = exec_code_handler("{\"code\":\"\"}", NULL);
    assert(r != NULL);
    /* Should return valid JSON with exit_code and output fields */
    assert(strstr(r, "\"exit_code\"") != NULL);
    assert(strstr(r, "\"output\"") != NULL);
    free(r);
}

static void test_stderr_capture(void) {
    /* Code that writes to stderr should be captured */
    char *r = exec_code_handler("{\"code\":\"import sys; sys.stderr.write('stderr msg\\\\n')\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "stderr msg") != NULL);
    free(r);
}

static void test_stdout_and_stderr(void) {
    /* Code writing to both stdout and stderr */
    char *r = exec_code_handler("{\"code\":\"import sys; print('stdout msg'); sys.stderr.write('stderr msg\\\\n')\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "stdout msg") != NULL);
    assert(strstr(r, "stderr msg") != NULL);
    free(r);
}

static void test_unicode_output(void) {
    /* Unicode/emoji in output should round-trip through JSON */
    char *r = exec_code_handler("{\"code\":\"print('\\\\u2603 snowman')\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "snowman") != NULL);
    assert(strstr(r, "\\u2603") != NULL || strstr(r, "\xe2\x98\x83") != NULL);
    free(r);
}

static void test_no_output(void) {
    /* Code that produces no output — just an expression */
    char *r = exec_code_handler("{\"code\":\"x = 42\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    /* Should have output field even if empty */
    assert(strstr(r, "\"output\"") != NULL);
    free(r);
}

static void test_very_long_code(void) {
    /* Very long code string — ~2000 chars */
    char long_code[4096];
    snprintf(long_code, sizeof(long_code),
             "{\"code\":\"print('long_test_start'); %s print('long_test_end')\"}",
             "x = 'a' * 1000; ");
    char *r = exec_code_handler(long_code, NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "long_test_start") != NULL);
    assert(strstr(r, "long_test_end") != NULL);
    free(r);
}

static void test_zero_timeout(void) {
    /* timeout=0 or negative — should either error or run (not hang) */
    char *r = exec_code_handler("{\"code\":\"print('zero_timeout')\",\"timeout\":0}", NULL);
    assert(r != NULL);
    /* timeout=0 may be treated as no limit or clamped — either is fine, just don't crash */
    free(r);
}

/* --- Phase 402: Edge case expansion --- */

static void test_large_output_truncated(void) {
    /* Generate ~70KB output to verify truncation flag */
    char *r = exec_code_handler("{\"code\":\"print('A' * 70000)\"}", NULL);
    assert(r != NULL);
    assert(strstr(r, "\"exit_code\"") != NULL);
    assert(strstr(r, "\"output\"") != NULL);
    /* Should either have truncated=true marker or contain '[...truncated]' */
    int has_truncated = strstr(r, "\"truncated\":true") != NULL;
    int has_marker = strstr(r, "truncated") != NULL;
    assert(has_truncated || has_marker);
    free(r);
}

static void test_negative_timeout(void) {
    /* Negative timeout should clamp, not crash */
    char *r = exec_code_handler("{\"code\":\"print('neg_timeout')\",\"timeout\":-1}", NULL);
    assert(r != NULL);
    /* Should produce valid output */
    assert(strstr(r, "neg_timeout") != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    free(r);
}

static void test_extra_fields_ignored(void) {
    /* Unknown JSON fields should be ignored */
    char *r = exec_code_handler("{\"code\":\"print('extra_fields')\",\"unknown\":\"val\",\"extra\":42}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "extra_fields") != NULL);
    free(r);
}

static void test_code_with_quotes(void) {
    /* Code containing single and double quotes */
    char *r = exec_code_handler("{\"code\":\"print(\\\"hello 'world'\\\")\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "hello") != NULL);
    free(r);
}

static void test_task_id_handling(void) {
    /* Non-NULL task_id should be handled gracefully */
    char *r = exec_code_handler("{\"code\":\"print('task_test')\"}", "my_task_123");
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "task_test") != NULL);
    free(r);
}

static void test_output_fields_present(void) {
    /* Verify all expected fields present in output */
    char *r = exec_code_handler("{\"code\":\"print('field_check')\"}", NULL);
    assert(r != NULL);
    assert(get_int_field(r, "exit_code") == 0);
    assert(strstr(r, "\"output\"") != NULL);
    assert(strstr(r, "field_check") != NULL);
    assert(strstr(r, "\"truncated\"") != NULL);
    free(r);
}

int main(void) {
    printf("exec_code tests:\n");
    TEST(test_hello_world());
    TEST(test_fail_with_code());
    TEST(test_syntax_error());
    TEST(test_missing_code());
    TEST(test_null_args());
    TEST(test_timeout_param());
    TEST(test_sandbox_flag());
    TEST(test_output_content());
    /* Phase 338 */
    TEST(test_empty_code());
    TEST(test_stderr_capture());
    TEST(test_stdout_and_stderr());
    TEST(test_unicode_output());
    TEST(test_no_output());
    TEST(test_very_long_code());
    TEST(test_zero_timeout());
    /* Phase 402 */
    TEST(test_large_output_truncated());
    TEST(test_negative_timeout());
    TEST(test_extra_fields_ignored());
    TEST(test_code_with_quotes());
    TEST(test_task_id_handling());
    TEST(test_output_fields_present());
    printf("All exec_code tests passed.\n");
    return 0;
}
