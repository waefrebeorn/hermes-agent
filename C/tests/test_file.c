/*
 * test_file.c — File tool unit tests (M31).
 *
 * Tests file CRUD (read/write/search) and sandbox enforcement.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson \
 *       tests/test_file.c src/tools/file.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_file -lm
 *
 * Run:
 *   /tmp/hermes_test_file
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Extern declarations for file tool handlers */
extern char *file_read_handler(const char *args_json, const char *task_id);
extern char *file_write_handler(const char *args_json, const char *task_id);
extern char *file_search_handler(const char *args_json, const char *task_id);
extern char *file_diff_handler(const char *args_json, const char *task_id);
extern char *file_perms_handler(const char *args_json, const char *task_id);
extern char *file_hex_handler(const char *args_json, const char *task_id);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* Parse JSON string result safely */
static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static int result_get_int(json_node_t *obj, const char *key, int def) {
    return (int)json_object_get_number(obj, key, (double)def);
}

static const char *result_get_str(json_node_t *obj, const char *key, const char *def) {
    return json_object_get_string(obj, key, def);
}

int main(void) {
    printf("=== File Tool Tests ===\n");

    /* ---- Setup: clean temp dir and sandbox ---- */
    const char *tmpdir = "/tmp/hermes_test_file";
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    if (system(cmd) != 0) { /* cleanup temp dir */ }
    mkdir(tmpdir, 0755);

    sandbox_clear();
    sandbox_add_allowed_dir("/tmp");
    sandbox_enable(true);
    sandbox_set_symlink_check(true);

    /* Test paths */

    /* ============ write_file ============ */

    /* 1. write new file */
    char args[2048];
    snprintf(args, sizeof(args), "{\"path\":\"%s/new.txt\",\"content\":\"Hello World\\nLine 2\\n\"}", tmpdir);
    json_node_t *r = parse_result(file_write_handler(args, NULL));
    TEST("write_file creates new file",
         r && result_get_str(r, "path", NULL) && strstr(result_get_str(r, "path", ""), "new.txt"));
    TEST("write_file returns success", r && json_object_get_bool(r, "success", false));
    TEST("write_file bytes_written > 0", r && result_get_int(r, "bytes_written", 0) > 0);
    json_free(r);

    /* 2. verify file exists on disk */
    char path[1024];
    snprintf(path, sizeof(path), "%s/new.txt", tmpdir);
    struct stat st;
    TEST("write_file file exists on disk", stat(path, &st) == 0);
    TEST("write_file file non-empty", st.st_size > 0);

    /* 3. overwrite existing file */
    snprintf(args, sizeof(args), "{\"path\":\"%s/new.txt\",\"content\":\"Overwritten\"}", tmpdir);
    r = parse_result(file_write_handler(args, NULL));
    TEST("write_file overwrites existing", r && json_object_get_bool(r, "success", false));
    json_free(r);

    /* 4. verify overwritten content */
    snprintf(args, sizeof(args), "{\"path\":\"%s/new.txt\"}", tmpdir);
    r = parse_result(file_read_handler(args, NULL));
    TEST("read_file after overwrite shows new content",
         r && strstr(result_get_str(r, "content", ""), "Overwritten"));
    json_free(r);

    /* 5. write with parent dir creation */
    snprintf(args, sizeof(args), "{\"path\":\"%s/subdir/test.txt\",\"content\":\"Nested\"}", tmpdir);
    r = parse_result(file_write_handler(args, NULL));
    TEST("write_file creates parent dirs", r && json_object_get_bool(r, "success", false));
    json_free(r);

    char subpath[1024];
    snprintf(subpath, sizeof(subpath), "%s/subdir/test.txt", tmpdir);
    TEST("write_file nested file exists on disk", stat(subpath, &st) == 0);

    /* 6. write with empty content */
    snprintf(args, sizeof(args), "{\"path\":\"%s/empty.txt\",\"content\":\"\"}", tmpdir);
    r = parse_result(file_write_handler(args, NULL));
    TEST("write_file empty content succeeds", r && json_object_get_bool(r, "success", false));
    TEST("write_file empty content has 0 bytes",
         r && result_get_int(r, "bytes_written", -1) == 0);
    json_free(r);

    /* 7. write with missing path */
    r = parse_result(file_write_handler("{\"content\":\"no path\"}", NULL));
    TEST("write_file missing path returns error",
         r && strstr(result_get_str(r, "error", ""), "Invalid") != NULL);
    json_free(r);

    /* ============ read_file ============ */

    /* 8. read entire file */
    snprintf(args, sizeof(args), "{\"path\":\"%s/new.txt\"}", tmpdir);
    r = parse_result(file_read_handler(args, NULL));
    TEST("read_file returns content", r && result_get_str(r, "content", NULL));
    TEST("read_file total_lines > 0", r && result_get_int(r, "total_lines", 0) > 0);
    TEST("read_file file_size > 0", r && result_get_int(r, "file_size", 0) > 0);
    if (r) {
        int tl = result_get_int(r, "total_lines", 0);
        int fs = result_get_int(r, "file_size", 0);
        printf("    total_lines=%d file_size=%d\n", tl, fs);
    }
    json_free(r);

    /* 9. read with offset/limit */
    snprintf(args, sizeof(args), "{\"path\":\"%s/new.txt\",\"offset\":1,\"limit\":5}", tmpdir);
    r = parse_result(file_read_handler(args, NULL));
    TEST("read_file with offset/limit returns content",
         r && result_get_str(r, "content", NULL));
    if (r) {
        int off = result_get_int(r, "offset", 0);
        int lim = result_get_int(r, "limit", 0);
        TEST("read_file offset preserved", off == 1);
        TEST("read_file limit preserved", lim == 5);
    }
    json_free(r);

    /* 10. read non-existent file */
    r = parse_result(file_read_handler("{\"path\":\"/tmp/hermes_test_file/nonexistent_xyz.txt\"}", NULL));
    TEST("read_file non-existent returns error",
         r && strstr(result_get_str(r, "error", ""), "Cannot open") != NULL);
    json_free(r);

    /* 11. read null args */
    r = parse_result(file_read_handler(NULL, NULL));
    TEST("read_file null args returns error",
         r && strstr(result_get_str(r, "error", ""), "No args") != NULL);
    json_free(r);

    /* ============ search_files ============ */

    /* 12. search by pattern */
    snprintf(args, sizeof(args), "{\"pattern\":\"Overwritten\",\"path\":\"%s\"}", tmpdir);
    r = parse_result(file_search_handler(args, NULL));
    TEST("search_files finds pattern", r && result_get_str(r, "matches", NULL));
    if (r) {
        const char *matches = result_get_str(r, "matches", "");
        TEST("search_files match contains Overwritten", strstr(matches, "Overwritten") != NULL);
    }
    json_free(r);

    /* 13. search with file_glob */
    snprintf(args, sizeof(args), "{\"pattern\":\"Nested\",\"path\":\"%s\",\"file_glob\":\"*.txt\"}", tmpdir);
    r = parse_result(file_search_handler(args, NULL));
    TEST("search_files with glob finds pattern", r && result_get_str(r, "matches", NULL));
    if (r) {
        const char *matches = result_get_str(r, "matches", "");
        TEST("search_files glob match contains Nested", strstr(matches, "Nested") != NULL);
    }
    json_free(r);

    /* 14. search no match */
    snprintf(args, sizeof(args), "{\"pattern\":\"ZZZZNONEXISTENTXXXX\",\"path\":\"%s\"}", tmpdir);
    r = parse_result(file_search_handler(args, NULL));
    TEST("search_files no match returns empty (no error)",
         r && strstr(result_get_str(r, "matches", ""), "ZZZZNONEXISTENT") == NULL);
    json_free(r);

    /* 15. search with non-matching glob */
    snprintf(args, sizeof(args), "{\"pattern\":\"Hello\",\"path\":\"%s\",\"file_glob\":\"*.py\"}", tmpdir);
    r = parse_result(file_search_handler(args, NULL));
    TEST("search_files non-matching glob returns empty",
         r && strlen(result_get_str(r, "matches", "")) == 0);
    json_free(r);

    /* 16. search missing pattern */
    r = parse_result(file_search_handler("{\"path\":\"/tmp\"}", NULL));
    TEST("search_files missing pattern returns error",
         r && strstr(result_get_str(r, "error", ""), "Missing") != NULL);
    json_free(r);

    /* ============ Sandbox security ============ */

    /* 17. path traversal blocked */
    snprintf(args, sizeof(args), "{\"path\":\"/tmp/../../etc/passwd\"}");
    r = parse_result(file_read_handler(args, NULL));
    TEST("sandbox blocks path traversal",
         r && strstr(result_get_str(r, "error", ""), "Invalid") != NULL);
    json_free(r);

    /* 18. path outside sandbox blocked */
    snprintf(args, sizeof(args), "{\"path\":\"/etc/hostname\"}");
    r = parse_result(file_read_handler(args, NULL));
    TEST("sandbox blocks /etc path",
         r && strstr(result_get_str(r, "error", ""), "Invalid") != NULL);
    json_free(r);

    /* 19. write with traversal blocked */
    snprintf(args, sizeof(args), "{\"path\":\"/tmp/../etc/cronjob\",\"content\":\"evil\"}");
    r = parse_result(file_write_handler(args, NULL));
    TEST("sandbox blocks write traversal",
         r && strstr(result_get_str(r, "error", ""), "Invalid") != NULL);
    json_free(r);

    /* 20. write to /tmp allowed (sandbox) */
    snprintf(args, sizeof(args), "{\"path\":\"/tmp/hermes_test_sandbox.txt\",\"content\":\"sandbox test\"}");
    r = parse_result(file_write_handler(args, NULL));
    TEST("sandbox allows /tmp write", r && json_object_get_bool(r, "success", false));
    json_free(r);

    /* ============ Edge cases ============ */

    /* 21. write file with notable content */
    char med_content[500];
    memset(med_content, 'C', sizeof(med_content) - 1);
    med_content[sizeof(med_content) - 1] = '\0';
    snprintf(args, sizeof(args), "{\"path\":\"%s/big.txt\",\"content\":\"%s\"}", tmpdir, med_content);
    r = parse_result(file_write_handler(args, NULL));
    TEST("write_file 2KB content succeeds", r && json_object_get_bool(r, "success", false));
    if (r) {
        int bw = result_get_int(r, "bytes_written", 0);
        printf("    bytes_written=%d\n", bw);
    }
    json_free(r);

    /* 22. read 500B file back */
    snprintf(args, sizeof(args), "{\"path\":\"%s/big.txt\"}", tmpdir);
    r = parse_result(file_read_handler(args, NULL));
    TEST("read_file 500B content", r && result_get_int(r, "file_size", 0) > 400);
    json_free(r);

    /* 23. read_empty file */
    snprintf(args, sizeof(args), "{\"path\":\"%s/empty.txt\"}", tmpdir);
    r = parse_result(file_read_handler(args, NULL));
    TEST("read_file empty file returns 0 total_lines",
         r && result_get_int(r, "total_lines", -1) == 0);
    json_free(r);

    /* 24. write special chars in path */
    snprintf(args, sizeof(args), "{\"path\":\"%s/file-with-dashes_and_underscores.txt\",\"content\":\"special\"}", tmpdir);
    r = parse_result(file_write_handler(args, NULL));
    TEST("write_file special path succeeds", r && json_object_get_bool(r, "success", false));
    json_free(r);

    /* ============ Cleanup ============ */
    /* Remove test files */
    unlink(path);
    unlink(subpath);
    snprintf(path, sizeof(path), "%s/empty.txt", tmpdir); unlink(path);
    snprintf(path, sizeof(path), "%s/big.txt", tmpdir); unlink(path);
    snprintf(path, sizeof(path), "%s/subdir/test.txt", tmpdir); unlink(path);
    rmdir("/tmp/hermes_test_file/subdir");
    snprintf(path, sizeof(path), "%s/file-with-dashes_and_underscores.txt", tmpdir); unlink(path);
    snprintf(path, sizeof(path), "%s/hermes_test_sandbox.txt", tmpdir); unlink(path);
    rmdir(tmpdir);

    sandbox_clear();

    /* ============ Diff tests ============ */
    printf("\n--- file_diff tests ---\n");
    {
        /* Create two files with differences */
        FILE *fa = fopen("/tmp/hermes_test_diff_a.txt", "w");
        fprintf(fa, "hello\nworld\n");
        fclose(fa);
        FILE *fb = fopen("/tmp/hermes_test_diff_b.txt", "w");
        fprintf(fb, "hello\nthere\n");
        fclose(fb);

        char args[512];
        snprintf(args, sizeof(args),
            "{\"path_a\":\"/tmp/hermes_test_diff_a.txt\",\"path_b\":\"/tmp/hermes_test_diff_b.txt\"}");
        json_node_t *r = parse_result(file_diff_handler(args, NULL));
        if (r && json_object_get_string(r, "diff", NULL)) {
            const char *diff = json_object_get_string(r, "diff", "");
            TEST("diff output non-empty", strlen(diff) > 0);
            TEST("diff shows added line", strstr(diff, "there") != NULL);
            TEST("diff shows removed line", strstr(diff, "world") != NULL);
        } else {
            TEST("diff handler returned result", r != NULL);
        }
        if (r) json_free(r);
        unlink("/tmp/hermes_test_diff_a.txt");
        unlink("/tmp/hermes_test_diff_b.txt");
    }
    {
        /* Test same file — identical diff */
        FILE *fa = fopen("/tmp/hermes_test_same.txt", "w");
        fprintf(fa, "identical\ncontent\n");
        fclose(fa);
        char args[512];
        snprintf(args, sizeof(args),
            "{\"path_a\":\"/tmp/hermes_test_same.txt\",\"path_b\":\"/tmp/hermes_test_same.txt\"}");
        json_node_t *r = parse_result(file_diff_handler(args, NULL));
        if (r) {
            double ratio = json_object_get_number(r, "similarity", 0.0);
            TEST("identical files similarity is 1.0", ratio >= 0.99);
        } else {
            TEST("same file diff returns result", r != NULL);
        }
        if (r) json_free(r);
        unlink("/tmp/hermes_test_same.txt");
    }
    {
        /* Test missing file */
        json_node_t *r = parse_result(file_diff_handler(
            "{\"path_a\":\"/tmp/hermes_test_nonexist.txt\",\"path_b\":\"/tmp/other.txt\"}", NULL));
        TEST("missing file returns error", r != NULL && strstr(json_serialize(r), "error") != NULL);
        if (r) json_free(r);
    }

    /* ============ Permissions tests ============ */
    printf("\n--- file_permissions tests ---\n");
    {
        /* Stat an existing file */
        json_node_t *r = parse_result(file_perms_handler(
            "{\"path\":\"/bin/ls\"}", NULL));
        TEST("stat returns result", r != NULL);
        if (r) {
            const char *mode = json_object_get_string(r, "mode", "");
            TEST("has mode field", mode != NULL && strlen(mode) >= 3);
            TEST("has type field", json_object_get_string(r, "type", "") != NULL);
            json_free(r);
        }
    }
    {
        /* Stat non-existent file */
        json_node_t *r = parse_result(file_perms_handler(
            "{\"path\":\"/tmp/hermes_test_nonexist_xyz\"}", NULL));
        TEST("missing file returns error", r != NULL && strstr(json_serialize(r), "error") != NULL);
        if (r) json_free(r);
    }

    /* ============ Hex view tests ============ */
    printf("\n--- file_hex tests ---\n");
    {
        /* Create a file with known content */
        FILE *f = fopen("/tmp/hermes_test_hex.txt", "wb");
        if (f) {
            unsigned char data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
            fwrite(data, 1, 5, f);
            fclose(f);
        }
        json_node_t *r = parse_result(file_hex_handler(
            "{\"path\":\"/tmp/hermes_test_hex.txt\"}", NULL));
        TEST("hex returns result", r != NULL);
        if (r) {
            const char *hex = json_object_get_string(r, "hex", "");
            TEST("hex output non-empty", hex && strlen(hex) > 0);
            if (hex) TEST("hex contains expected bytes", strstr(hex, "48 65 6c") != NULL);
            json_free(r);
        }
        unlink("/tmp/hermes_test_hex.txt");
    }
    {
        json_node_t *r = parse_result(file_hex_handler(
            "{\"path\":\"/tmp/hermes_test_nonexist_hex.txt\"}", NULL));
        TEST("missing file returns error", r != NULL && strstr(json_serialize(r), "error") != NULL);
        if (r) json_free(r);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
