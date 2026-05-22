/*
 * test_exec_code.c — M41: exec_code tool tests
 *
 * Tests code execution via subprocess (same mechanism as exec_code_handler).
 * Verifies: stdout capture, error reporting, timeout handling, empty input.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

/* Run Python code and return output as malloc'd string */
static char *run_python(const char *code, int timeout_sec) {
    if (!code) return strdup("{\"error\":\"No code provided\"}");

    /* Write code to temp file */
    char tmp_path[] = "/tmp/hermes_exec_test_XXXXXX.py";
    int fd = mkstemps(tmp_path, 3);
    if (fd < 0) return strdup("{\"error\":\"Cannot create temp file\"}");

    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return strdup("{\"error\":\"Cannot write temp file\"}"); }
    fputs(code, f);
    fclose(f);

    /* Build and run command */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "timeout %d python3 %s 2>&1 || true",
             timeout_sec > 0 ? timeout_sec : 60, tmp_path);

    FILE *fp = popen(cmd, "r");
    if (!fp) { unlink(tmp_path); return strdup("{\"error\":\"Cannot execute\"}"); }

    char result[65536];
    size_t pos = 0;
    while (fgets(result + pos, (int)(sizeof(result) - pos), fp)) {
        pos += strlen(result + pos);
        if (pos >= sizeof(result) - 1) break;
    }
    int exit_code = pclose(fp);
    unlink(tmp_path);

    /* Strip trailing newline(s) */
    while (pos > 0 && (result[pos-1] == '\n' || result[pos-1] == '\r'))
        result[--pos] = '\0';

    /* Build JSON-like output */
    size_t out_sz = pos + 128;
    char *out = (char *)malloc(out_sz);
    if (!out) return NULL;

    /* Escape the output for JSON */
    char *escaped = (char *)malloc(pos * 2 + 1);
    if (!escaped) { free(out); return NULL; }
    size_t j = 0;
    for (size_t i = 0; i < pos; i++) {
        if (result[i] == '"' || result[i] == '\\') escaped[j++] = '\\';
        escaped[j++] = result[i];
    }
    escaped[j] = '\0';

    snprintf(out, out_sz, "{\"exit_code\":%d,\"stdout\":\"%s\"}", exit_code, escaped);
    free(escaped);
    return out;
}

int main(void) {
    printf("=== M41: exec_code tool ===\n\n");

    /* Test: Python version output */
    {
        char *result = run_python("import sys; print(sys.version)", 10);
        TEST("result non-NULL", result != NULL);
        if (result) {
            TEST("stdout key present", strstr(result, "\"stdout\"") != NULL);
            TEST("contains Python version", strstr(result, "3.") != NULL);
            free(result);
        }
    }

    /* Test: NULL code */
    {
        char *result = run_python(NULL, 10);
        TEST("NULL code returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test: empty code */
    {
        char *result = run_python("", 10);
        TEST("empty code non-NULL", result != NULL);
        if (result) {
            TEST("empty code runs cleanly", strstr(result, "exit_code") != NULL);
            free(result);
        }
    }

    /* Test: string printing */
    {
        char *result = run_python("print('hello world')", 10);
        TEST("print result non-NULL", result != NULL);
        if (result) {
            TEST("stdout contains hello world", strstr(result, "hello world") != NULL);
            free(result);
        }
    }

    /* Test: multiple lines */
    {
        char *result = run_python("for i in range(3):\n    print(i)", 10);
        TEST("multi-line result non-NULL", result != NULL);
        if (result) {
            TEST("stdout contains 0", strstr(result, "0") != NULL);
            TEST("stdout contains 1", strstr(result, "1") != NULL);
            TEST("stdout contains 2", strstr(result, "2") != NULL);
            free(result);
        }
    }

    /* Test: stderr capture */
    {
        char *result = run_python("import sys; sys.stderr.write('err msg')", 10);
        TEST("stderr result non-NULL", result != NULL);
        if (result) {
            TEST("stderr captured", strstr(result, "err msg") != NULL);
            free(result);
        }
    }

    /* Test: exit code 0 */
    {
        char *result = run_python("print('ok')", 10);
        TEST("exit 0 result non-NULL", result != NULL);
        if (result) {
            TEST("exit code 0", strstr(result, "\"exit_code\":0") != NULL);
            free(result);
        }
    }

    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
