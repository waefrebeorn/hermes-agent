/*
 * test_hermes_error.c — Tests for the typed error hierarchy (K01-K05).
 */

#include "hermes_error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int failures = 0;
static int tests = 0;

#define TEST(name) do { \
    tests++; \
    printf("  %-50s ", name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    failures++; \
} while(0)

/* K01: ValueError — invalid argument */
static void test_value_error(void) {
    TEST("K01: ValueError creation");
    hermes_error_t err;
    hermes_err_value(&err, "invalid port: %d", -1);
    if (err.type != HERMES_ERR_VALUE) { FAIL("wrong type"); return; }
    if (strstr(err.message, "invalid port") == NULL) { FAIL("wrong message"); return; }
    if (err.line <= 0) { FAIL("no line"); return; }
    PASS();
}

/* K02: TypeError — type mismatch */
static void test_type_error(void) {
    TEST("K02: TypeError creation");
    hermes_error_t err;
    hermes_err_type(&err, "expected string, got %s", "int");
    if (err.type != HERMES_ERR_TYPE) { FAIL("wrong type"); return; }
    if (strstr(err.message, "expected string") == NULL) { FAIL("wrong message"); return; }
    PASS();
}

/* K03: RuntimeError — general failure */
static void test_runtime_error(void) {
    TEST("K03: RuntimeError creation");
    hermes_error_t err;
    hermes_err_runtime(&err, "connection failed after %d retries", 3);
    if (err.type != HERMES_ERR_RUNTIME) { FAIL("wrong type"); return; }
    if (strstr(err.message, "connection failed") == NULL) { FAIL("wrong message"); return; }
    PASS();
}

/* K04: OSError from current errno */
static void test_os_error(void) {
    TEST("K04: OSError from errno");
    errno = ENOENT;
    hermes_error_t err;
    hermes_errno(&err, "open config");
    if (err.type != HERMES_ERR_OS) { FAIL("wrong type"); return; }
    if (err.code != ENOENT) { FAIL("wrong errno"); return; }
    if (strstr(err.message, "open config") == NULL) { FAIL("no context"); return; }
    PASS();
}

/* K04: OSError with custom message */
static void test_os_error_custom(void) {
    TEST("K04: OSError from format");
    hermes_error_t err;
    hermes_err_os(&err, "file not found: %s", "/nonexistent/path");
    if (err.type != HERMES_ERR_OS) { FAIL("wrong type"); return; }
    if (strstr(err.message, "nonexistent") == NULL) { FAIL("wrong message"); return; }
    PASS();
}

/* K05: TimeoutError */
static void test_timeout_error(void) {
    TEST("K05: TimeoutError creation");
    hermes_error_t err;
    hermes_err_timeout(&err, "request timed out after %dms", 30000);
    if (err.type != HERMES_ERR_TIMEOUT) { FAIL("wrong type"); return; }
    if (strstr(err.message, "timed out") == NULL) { FAIL("wrong message"); return; }
    PASS();
}

/* hermes_error_type_name */
static void test_type_names(void) {
    TEST("Type names correct");
    if (strcmp(hermes_error_type_name(HERMES_ERR_VALUE), "ValueError") != 0) { FAIL("ValueError name"); return; }
    if (strcmp(hermes_error_type_name(HERMES_ERR_TYPE), "TypeError") != 0) { FAIL("TypeError name"); return; }
    if (strcmp(hermes_error_type_name(HERMES_ERR_RUNTIME), "RuntimeError") != 0) { FAIL("RuntimeError name"); return; }
    if (strcmp(hermes_error_type_name(HERMES_ERR_OS), "OSError") != 0) { FAIL("OSError name"); return; }
    if (strcmp(hermes_error_type_name(HERMES_ERR_TIMEOUT), "TimeoutError") != 0) { FAIL("TimeoutError name"); return; }
    if (strcmp(hermes_error_type_name(HERMES_OK), "Ok") != 0) { FAIL("Ok name"); return; }
    if (strcmp(hermes_error_type_name((hermes_error_type_t)99), "UnknownError") != 0) { FAIL("unknown name"); return; }
    PASS();
}

/* hermes_error_string formatting */
static void test_error_string(void) {
    TEST("Error string formatting");
    hermes_error_t err;
    hermes_err_value(&err, "invalid argument");
    char buf[1024];
    hermes_error_string(&err, buf, sizeof(buf));
    if (strstr(buf, "ValueError") == NULL) { FAIL("no type name"); return; }
    if (strstr(buf, "hermes_error.c") == NULL) { FAIL("no file"); return; }
    PASS();
}

/* hermes_is_error / hermes_error_is_type */
static void test_query_functions(void) {
    TEST("Query: hermes_is_error / hermes_error_is_type");
    hermes_error_t ok = { HERMES_OK, 0, "", "", 0 };
    if (hermes_is_error(&ok)) { FAIL("OK is not error"); return; }
    hermes_error_t err;
    hermes_err_value(&err, "test");
    if (!hermes_is_error(&err)) { FAIL("should be error"); return; }
    if (!hermes_error_is_type(&err, HERMES_ERR_VALUE)) { FAIL("wrong type check"); return; }
    PASS();
}

/* Thread-local last error */
static void test_last_error(void) {
    TEST("Thread-local last error");
    hermes_clear_last_error();
    if (hermes_get_last_error() != NULL) { FAIL("should be NULL after clear"); return; }

    hermes_set_error(HERMES_ERR_RUNTIME, "something broke");
    hermes_error_t *le = hermes_get_last_error();
    if (!le) { FAIL("should have error"); return; }
    if (le->type != HERMES_ERR_RUNTIME) { FAIL("wrong type"); return; }

    hermes_clear_last_error();
    if (hermes_get_last_error() != NULL) { FAIL("should be NULL after clear"); return; }
    PASS();
}

/* Thread safety: two threads set/get last error independently */
#include <pthread.h>

typedef struct {
    hermes_error_type_t type;
    const char *msg;
    bool ok;
} thread_test_t;

static void *thread_func(void *arg) {
    thread_test_t *tt = (thread_test_t *)arg;
    hermes_set_error(tt->type, "%s", tt->msg);
    hermes_error_t *le = hermes_get_last_error();
    tt->ok = le && le->type == tt->type;
    return NULL;
}

static void test_thread_safety(void) {
    TEST("Thread safety of last-error");
    pthread_t t1, t2;
    thread_test_t tt1 = { HERMES_ERR_VALUE, "thread1 error", false };
    thread_test_t tt2 = { HERMES_ERR_TIMEOUT, "thread2 error", false };

    pthread_create(&t1, NULL, thread_func, &tt1);
    pthread_create(&t2, NULL, thread_func, &tt2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    if (!tt1.ok) { FAIL("thread1 not ok"); return; }
    if (!tt2.ok) { FAIL("thread2 not ok"); return; }
    PASS();
}

int main(void) {
    printf("Hermes Error System Tests (K01-K05)\n");
    printf("====================================\n\n");

    test_value_error();
    test_type_error();
    test_runtime_error();
    test_os_error();
    test_os_error_custom();
    test_timeout_error();
    test_type_names();
    test_error_string();
    test_query_functions();
    test_last_error();
    test_thread_safety();

    printf("\n%d/%d tests passed, %d failed\n", tests - failures, tests, failures);
    return failures > 0 ? 1 : 0;
}
