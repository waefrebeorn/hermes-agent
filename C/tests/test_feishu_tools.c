/* test_feishu_tools.c — Tests for Feishu doc/drive tool.
 * Tests error paths and argument validation without network access.
 *
 * Compile with:
 *   gcc -O2 -Wall -Wextra -I../include -I../lib/libjson \
 *       test_feishu_tools.c ../src/tools/feishu_tools.c \
 *       ../lib/libjson/json.c ../lib/libhttp/http.c
 *       -o /tmp/hermes_test_feishu -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Declare the handlers (non-static in feishu_tools.c) */
extern char *handle_feishu_doc_read(const char *args_json, const char *task_id);
extern char *handle_feishu_drive_list(const char *args_json, const char *task_id);

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

static void test_doc_read_null_args(void) {
    /* Null args should return error JSON */
    char *result = handle_feishu_doc_read(NULL, NULL);
    TEST("doc_read null args: result not null", result != NULL);
    TEST("doc_read null args: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("doc_read null args: is valid JSON", j != NULL);
    if (j) json_free(j);
    free(result);
}

static void test_doc_read_empty_doc_id(void) {
    /* Missing doc_id should return error */
    const char *args = "{}";
    char *result = handle_feishu_doc_read(args, NULL);
    TEST("doc_read empty args: result not null", result != NULL);
    TEST("doc_read empty args: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("doc_read empty args: valid JSON", j != NULL);
    if (j) {
        const char *err = json_get_str(j, "error", "");
        TEST("doc_read empty args: error mentions doc_id",
             strstr(err, "doc_id") != NULL);
        json_free(j);
    }
    free(result);
}

static void test_doc_read_invalid_json(void) {
    /* Malformed JSON should return error */
    const char *args = "{bad json here";
    char *result = handle_feishu_doc_read(args, NULL);
    TEST("doc_read bad json: result not null", result != NULL);
    TEST("doc_read bad json: contains error", strstr(result, "error") != NULL);
    free(result);
}

static void test_drive_list_null_args(void) {
    /* Null args should return error JSON */
    char *result = handle_feishu_drive_list(NULL, NULL);
    TEST("drive_list null args: result not null", result != NULL);
    TEST("drive_list null args: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("drive_list null args: valid JSON", j != NULL);
    if (j) json_free(j);
    free(result);
}

static void test_drive_list_no_auth_env(void) {
    /* With valid args but no FEISHU_APP_ID/FEISHU_APP_SECRET set:
     * feishu_get_token returns NULL, handler returns auth error. */
    unsetenv("FEISHU_APP_ID");
    unsetenv("FEISHU_APP_SECRET");

    const char *args = "{\"folder_token\":\"fake_token\"}";
    char *result = handle_feishu_drive_list(args, NULL);
    TEST("drive_list no auth: result not null", result != NULL);
    TEST("drive_list no auth: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("drive_list no auth: valid JSON", j != NULL);
    if (j) {
        const char *err = json_get_str(j, "error", "");
        TEST("drive_list no auth: error mentions auth",
             strstr(err, "auth") != NULL);
        json_free(j);
    }
    free(result);
}

static void test_doc_read_no_auth_env(void) {
    /* Valid doc_id but no Feishu credentials → early auth error */
    const char *args = "{\"doc_id\":\"test_doc_123\"}";
    char *result = handle_feishu_doc_read(args, NULL);
    TEST("doc_read no auth: result not null", result != NULL);
    TEST("doc_read no auth: contains error", strstr(result, "error") != NULL);
    json_t *j = json_parse(result, NULL);
    TEST("doc_read no auth: valid JSON", j != NULL);
    if (j) {
        const char *err = json_get_str(j, "error", "");
        TEST("doc_read no auth: error mentions auth",
             strstr(err, "auth") != NULL || strstr(err, "set") != NULL);
        json_free(j);
    }
    free(result);
}

int main(void) {
    test_doc_read_null_args();
    test_doc_read_empty_doc_id();
    test_doc_read_invalid_json();
    test_drive_list_null_args();
    test_drive_list_no_auth_env();
    test_doc_read_no_auth_env();

    fprintf(stderr, "feishu_tools: %d/%d pass\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
