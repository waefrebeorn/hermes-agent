/* test_exec_code.c — Unit tests for exec_code */
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
    printf("All exec_code tests passed.\n");
    return 0;
}
