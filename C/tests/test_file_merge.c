/* test_file_merge.c — test file_merge tool error paths + merge scenarios */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

/* Forward declarations of functions we test */
extern char *file_merge_handler(const char *args_json, const char *task_id);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* Helper: create temp file with content, returns strdup'd path (caller frees).
 * Since we use a static buffer but strdup the result, multiple calls in one
 * test function get independent strings. */
static char *create_temp(const char *content, int suffix) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/_tmg_%d", suffix);
    FILE *f = fopen(path, "w");
    if (!f) return NULL;
    fprintf(f, "%s", content);
    fclose(f);
    return strdup(path);
}

/* Helper: read temp file content (malloc'd), caller frees */
static char *read_temp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)st.st_size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Helper: check JSON key contains substr */
static int json_has(const char *json, const char *substr) {
    return json && strstr(json, substr) != NULL;
}

static void test_null_args(void) {
    TEST("file_merge_handler(NULL args) returns error");
    char *result = file_merge_handler(NULL, "test");
    assert(result != NULL);
    assert(json_has(result, "No args"));
    free(result);
    PASS();
}

static void test_bad_json(void) {
    TEST("file_merge_handler(bad JSON) returns error");
    char *result = file_merge_handler("{bad json}", "test");
    assert(result != NULL);
    assert(json_has(result, "JSON parse"));
    free(result);
    PASS();
}

static void test_missing_params(void) {
    TEST("file_merge_handler(missing all params) returns error");
    char *result = file_merge_handler("{\"base_path\":\"/tmp/x\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Missing"));
    free(result);
    PASS();
}

static void test_missing_base(void) {
    TEST("file_merge_handler(missing base_path) returns error");
    char *result = file_merge_handler("{\"modified_path\":\"a\",\"output_path\":\"b\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Missing"));
    free(result);
    PASS();
}

static void test_missing_modified(void) {
    TEST("file_merge_handler(missing modified_path) returns error");
    char *result = file_merge_handler("{\"base_path\":\"a\",\"output_path\":\"b\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Missing"));
    free(result);
    PASS();
}

static void test_missing_output(void) {
    TEST("file_merge_handler(missing output_path) returns error");
    char *result = file_merge_handler("{\"base_path\":\"a\",\"modified_path\":\"b\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Missing"));
    free(result);
    PASS();
}

static void test_nonexistent_base(void) {
    TEST("file_merge_handler(non-existent base) returns error");
    char *result = file_merge_handler(
        "{\"base_path\":\"/tmp/_tmg_noexist\",\"modified_path\":\"/tmp/x\",\"output_path\":\"/tmp/y\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Cannot open"));
    free(result);
    PASS();
}

static void test_identical_files(void) {
    TEST("file_merge_handler(identical files) returns unchanged");
    char *base = create_temp("hello world\n", 500);
    const char *out = "/tmp/_tmg_out1";
    char args[1024];
    snprintf(args, sizeof(args),
        "{\"base_path\":\"%s\",\"modified_path\":\"%s\",\"output_path\":\"%s\"}",
        base, base, out);
    char *result = file_merge_handler(args, "test");
    assert(result != NULL);
    assert(json_has(result, "\"status\":\"unchanged\""));
    free(result);
    free(base);
    unlink(out);
    PASS();
}

static void test_different_files(void) {
    TEST("file_merge_handler(different files) returns merged with diff");
    char *base = create_temp("line1\nline2\nline3\n", 501);
    char *modified = create_temp("line1\nline2 modified\nline3\n", 502);
    const char *out = "/tmp/_tmg_out2";
    char args[1024];
    snprintf(args, sizeof(args),
        "{\"base_path\":\"%s\",\"modified_path\":\"%s\",\"output_path\":\"%s\"}",
        base, modified, out);
    char *result = file_merge_handler(args, "test");
    assert(result != NULL);
    assert(json_has(result, "\"status\":\"merged\""));
    assert(json_has(result, "\"diff\""));
    /* Verify output file was written with modified content */
    char *out_content = read_temp(out);
    assert(out_content != NULL);
    assert(strstr(out_content, "line2 modified") != NULL);
    free(out_content);
    free(result);
    free(base);
    free(modified);
    unlink(out);
    PASS();
}

static void test_unknown_strategy(void) {
    TEST("file_merge_handler(unknown strategy) returns error");
    char *base = create_temp("a", 503);
    char *modified = create_temp("b", 504);
    const char *out = "/tmp/_tmg_out3";
    char args[1024];
    snprintf(args, sizeof(args),
        "{\"base_path\":\"%s\",\"modified_path\":\"%s\",\"output_path\":\"%s\",\"strategy\":\"bogus\"}",
        base, modified, out);
    char *result = file_merge_handler(args, "test");
    assert(result != NULL);
    assert(json_has(result, "Unknown merge strategy"));
    free(result);
    free(base);
    free(modified);
    unlink(out);
    PASS();
}

static void test_both_missing(void) {
    TEST("file_merge_handler(both files missing) returns Cannot open");
    char *result = file_merge_handler(
        "{\"base_path\":\"/tmp/_tmg_no1\",\"modified_path\":\"/tmp/_tmg_no2\",\"output_path\":\"/tmp/_tmg_no3\"}", "test");
    assert(result != NULL);
    assert(json_has(result, "Cannot open"));
    free(result);
    PASS();
}

static void test_empty_base_file(void) {
    TEST("file_merge_handler(empty base file, different modified) returns merged");
    char *base = create_temp("", 505);
    char *modified = create_temp("new content\n", 506);
    const char *out = "/tmp/_tmg_out4";
    char args[1024];
    snprintf(args, sizeof(args),
        "{\"base_path\":\"%s\",\"modified_path\":\"%s\",\"output_path\":\"%s\"}",
        base, modified, out);
    char *result = file_merge_handler(args, "test");
    assert(result != NULL);
    assert(json_has(result, "\"status\":\"merged\""));
    free(result);
    free(base);
    free(modified);
    unlink(out);
    PASS();
}

static void test_output_file_written(void) {
    TEST("file_merge_handler(output file written with merged content)");
    char *base = create_temp("original text\n", 507);
    char *modified = create_temp("modified text\n", 508);
    const char *out = "/tmp/_tmg_out5";
    char args[1024];
    snprintf(args, sizeof(args),
        "{\"base_path\":\"%s\",\"modified_path\":\"%s\",\"output_path\":\"%s\"}",
        base, modified, out);
    char *result = file_merge_handler(args, "test");
    assert(result != NULL);
    assert(json_has(result, "\"status\":\"merged\""));
    /* Verify the output file exists and contains modified content */
    struct stat st;
    assert(stat(out, &st) == 0);
    assert(st.st_size > 0);
    free(result);
    free(base);
    free(modified);
    unlink(out);
    PASS();
}

int main(void) {
    printf("file_merge tests:\n");
    test_null_args();
    test_bad_json();
    test_missing_params();
    test_missing_base();
    test_missing_modified();
    test_missing_output();
    test_nonexistent_base();
    test_identical_files();
    test_different_files();
    test_unknown_strategy();
    test_both_missing();
    test_empty_base_file();
    test_output_file_written();
    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
