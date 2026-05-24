/* test_file_batch.c — Tests for batch file operations. */

#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* Declare the handler */
char *file_batch_handler(const char *args_json, const char *task_id);

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static void setup_test_files(void) {
    system("rm -rf /tmp/hermes_test_batch && mkdir -p /tmp/hermes_test_batch/src");
    FILE *f = fopen("/tmp/hermes_test_batch/src/a.txt", "w");
    if (f) { fprintf(f, "file a content\\n"); fclose(f); }
    f = fopen("/tmp/hermes_test_batch/src/b.txt", "w");
    if (f) { fprintf(f, "file b content\\n"); fclose(f); }
}

static void test_batch_delete(void) {
    setup_test_files();
    const char *args = "{"
        "\\\"action\\\":\\\"delete\\\","
        "\\\"files\\\":[\\\"/tmp/hermes_test_batch/src/a.txt\\\"]"
    "}";
    char *result = file_batch_handler(args, NULL);
    TEST("delete: result not null", result != NULL);
    TEST("delete: contains success", strstr(result, "success") != NULL);
    /* File should be gone */
    TEST("delete: file removed", access("/tmp/hermes_test_batch/src/a.txt", F_OK) != 0);
    free(result);
}

static void test_batch_copy(void) {
    setup_test_files();
    const char *args = "{"
        "\\\"action\\\":\\\"copy\\\","
        "\\\"files\\\":[\\\"/tmp/hermes_test_batch/src/b.txt\\\"],"
        "\\\"dest\\\":\\\"/tmp/hermes_test_batch/b_copy.txt\\\""
    "}";
    char *result = file_batch_handler(args, NULL);
    TEST("copy: result not null", result != NULL);
    TEST("copy: dest exists", access("/tmp/hermes_test_batch/b_copy.txt", F_OK) == 0);
    free(result);
}

static void test_batch_move(void) {
    setup_test_files();
    const char *args = "{"
        "\\\"action\\\":\\\"move\\\","
        "\\\"files\\\":[\\\"/tmp/hermes_test_batch/src/b.txt\\\"],"
        "\\\"dest\\\":\\\"/tmp/hermes_test_batch/b_moved.txt\\\""
    "}";
    char *result = file_batch_handler(args, NULL);
    TEST("move: result not null", result != NULL);
    TEST("move: dest exists", access("/tmp/hermes_test_batch/b_moved.txt", F_OK) == 0);
    TEST("move: src gone", access("/tmp/hermes_test_batch/src/b.txt", F_OK) != 0);
    free(result);
}

int main(void) {
    test_batch_delete();
    test_batch_copy();
    test_batch_move();

    /* Cleanup */
    system("rm -rf /tmp/hermes_test_batch");

    fprintf(stderr, "file_batch: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
