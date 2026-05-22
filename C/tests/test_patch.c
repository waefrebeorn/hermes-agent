/*
 * test_patch.c — Patch tool unit tests (M42).
 *
 * Tests find-and-replace on temp files: basic replace,
 * replace_all, error paths.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_patch.c src/tools/patch.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_patch -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_patch
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

extern char *patch_handler(const char *args_json, const char *task_id);

static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static const char *result_get_str(json_node_t *obj, const char *key, const char *def) {
    return json_object_get_string(obj, key, def);
}

/* Helper: write file content */
static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

/* Helper: read file content */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

int main(void) {
    printf("=== Patch Tool Tests ===\n");

    const char *testfile = "/tmp/hermes_test_patch.txt";
    json_node_t *r;

    /* 1. null args */
    r = parse_result(patch_handler(NULL, NULL));
    TEST("patch null args returns error",
         r && strstr(result_get_str(r, "error", ""), "No arguments") != NULL);
    json_free(r);

    /* 2. missing path */
    r = parse_result(patch_handler("{\"old_string\":\"foo\"}", NULL));
    TEST("patch missing path returns error",
         r && strstr(result_get_str(r, "error", ""), "Missing path") != NULL);
    json_free(r);

    /* 3. non-existent file */
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_nonexist.txt\",\"old_string\":\"x\"}", NULL));
    TEST("patch non-existent file returns error",
         r && strstr(result_get_str(r, "error", ""), "Cannot open") != NULL);
    json_free(r);

    /* Setup: create test file */
    write_file(testfile, "Hello World\nLine 2\nHello again\n");
    char *content = read_file(testfile);
    TEST("setup: test file exists", content != NULL);
    free(content);

    /* 4. basic replace */
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"World\",\"new_string\":\"There\"}", NULL));
    TEST("patch basic replace returns success",
         r && json_object_get_bool(r, "success", false));
    TEST("patch basic replace replacements=1",
         r && json_object_get_number(r, "replacements", -1) == 1.0);
    TEST("patch basic replace has diff",
         r && result_get_str(r, "diff", NULL) && strlen(result_get_str(r, "diff", "")) > 0);
    json_free(r);

    /* Verify content */
    content = read_file(testfile);
    TEST("patch basic replace content correct",
         content && strstr(content, "There") != NULL &&
         strstr(content, "World") == NULL);
    free(content);

    /* 5. replace multiple (without replace_all) */
    write_file(testfile, "abc def abc ghi abc");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"abc\",\"new_string\":\"XYZ\"}", NULL));
    TEST("patch without replace_all errors on multiple matches",
         r && strstr(result_get_str(r, "error", ""), "multiple times") != NULL);
    json_free(r);

    /* 6. replace_all multiple */
    write_file(testfile, "abc def abc ghi abc");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"abc\",\"new_string\":\"XYZ\",\"replace_all\":true}", NULL));
    TEST("patch replace_all returns success",
         r && json_object_get_bool(r, "success", false));
    TEST("patch replace_all replacements=3",
         r && json_object_get_number(r, "replacements", -1) == 3.0);
    json_free(r);

    content = read_file(testfile);
    TEST("patch replace_all content correct",
         content && strstr(content, "XYZ") != NULL &&
         strstr(content, "abc") == NULL);
    free(content);

    /* 7. old_string not found */
    write_file(testfile, "some random content");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"NONEXISTENT\"}", NULL));
    TEST("patch old_string not found returns error",
         r && strstr(result_get_str(r, "error", ""), "not found") != NULL);
    json_free(r);

    /* 8. empty old_string */
    write_file(testfile, "content");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"\"}", NULL));
    TEST("patch empty old_string returns error",
         r && strstr(result_get_str(r, "error", ""), "empty") != NULL);
    json_free(r);

    /* 9. replace with empty new_string (delete) */
    write_file(testfile, "Hello World");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"Hello \",\"new_string\":\"\"}", NULL));
    TEST("patch delete (empty new_string) returns success",
         r && json_object_get_bool(r, "success", false));
    json_free(r);

    content = read_file(testfile);
    TEST("patch delete content correct",
         content && strcmp(content, "World") == 0);
    free(content);

    /* 10. JSON parse error */
    r = parse_result(patch_handler("{invalid json}", NULL));
    TEST("patch JSON parse error",
         r && strstr(result_get_str(r, "error", ""), "JSON parse") != NULL);
    json_free(r);

    /* Cleanup */
    unlink(testfile);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
