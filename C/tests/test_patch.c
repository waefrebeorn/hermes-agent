/*
 * test_patch.c — Patch tool unit tests (M42).
 *
 * Tests find-and-replace on temp files: basic replace,
 * replace_all, error paths, and \\t/\\r unescape from upstream @78be45860.
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

/* Helper: build JSON-escaped patch args without needing literal string escaping */
static char args_buf[4096];
static void build_json_args(const char *path, const char *old_str, const char *new_str) {
    memset(args_buf, 0, sizeof(args_buf));
    size_t bp = 0;
    bp += snprintf(args_buf + bp, sizeof(args_buf) - bp - 1, "{\"path\":\"");
    for (const char *s = path; *s && bp < sizeof(args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') args_buf[bp++] = '\\';
        args_buf[bp++] = *s;
    }
    bp += snprintf(args_buf + bp, sizeof(args_buf) - bp - 1, "\",\"old_string\":\"");
    for (const char *s = old_str; *s && bp < sizeof(args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') args_buf[bp++] = '\\';
        args_buf[bp++] = *s;
    }
    bp += snprintf(args_buf + bp, sizeof(args_buf) - bp - 1, "\",\"new_string\":\"");
    for (const char *s = new_str; *s && bp < sizeof(args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') args_buf[bp++] = '\\';
        args_buf[bp++] = *s;
    }
    bp += snprintf(args_buf + bp, sizeof(args_buf) - bp - 1, "\"}");
}

/* Helper: build JSON args with dry_run flag */
static char dry_args_buf[4096];
static void build_json_dry(const char *path, const char *old_str, const char *new_str) {
    memset(dry_args_buf, 0, sizeof(dry_args_buf));
    size_t bp = 0;
    bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1, "{\"path\":\"");
    for (const char *s = path; *s && bp < sizeof(dry_args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') dry_args_buf[bp++] = '\\';
        dry_args_buf[bp++] = *s;
    }
    bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1, "\",\"old_string\":\"");
    for (const char *s = old_str; *s && bp < sizeof(dry_args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') dry_args_buf[bp++] = '\\';
        dry_args_buf[bp++] = *s;
    }
    bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1, "\",\"new_string\":\"");
    for (const char *s = new_str; *s && bp < sizeof(dry_args_buf) - 50; s++) {
        if (*s == '"' || *s == '\\') dry_args_buf[bp++] = '\\';
        dry_args_buf[bp++] = *s;
    }
    bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1, "\",\"dry_run\":true}");
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

    /* 7. old_string not found (exact + fuzzy) */
    write_file(testfile, "some random content is here");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"NONEXISTENT\"}", NULL));
    TEST("patch old_string not found returns error",
         r && strstr(result_get_str(r, "error", ""), "not found") != NULL);
    json_free(r);

    /* 8. fuzzy: line_trimmed — extra whitespace on lines */
    write_file(testfile, "int main(void) {\n    return 0;\n}\n");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"  int main(void) {\\n  return 0;\\n  }\",\"new_string\":\"int main() {\\n  return 1;\\n}\"}", NULL));
    TEST("patch fuzzy line_trimmed returns success",
         r && json_object_get_bool(r, "success", false));
    TEST("patch fuzzy line_trimmed strategy != exact",
         r && strcmp(result_get_str(r, "strategy", ""), "exact") != 0);
    json_free(r);
    content = read_file(testfile);
    TEST("patch fuzzy line_trimmed content correct",
         content && strstr(content, "return 1") != NULL);
    free(content);

    /* 9. fuzzy: whitespace_normalized — extra middle spaces */
    write_file(testfile, "Hello    World   Foo");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"Hello World Foo\",\"new_string\":\"Hi There\"}", NULL));
    TEST("patch fuzzy whitespace_normalized returns success",
         r && json_object_get_bool(r, "success", false));
    json_free(r);
    content = read_file(testfile);
    TEST("patch fuzzy whitespace_normalized content correct",
         content && strstr(content, "Hi There") != NULL);
    free(content);

    /* 10. fuzzy: indentation_flexible — different leading whitespace */
    write_file(testfile, "        leading spaces here\n        more indented");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"  leading spaces here\\n  more indented\",\"new_string\":\"done\"}", NULL));
    TEST("patch fuzzy indentation_flexible returns success",
         r && json_object_get_bool(r, "success", false));
    json_free(r);
    content = read_file(testfile);
    TEST("patch fuzzy indentation_flexible content correct",
         content && strstr(content, "done") != NULL);
    free(content);

    /* 11. exact match still uses exact strategy */
    write_file(testfile, "exact match test");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"exact match\",\"new_string\":\"EXACT MATCH\"}", NULL));
    TEST("patch exact match uses exact strategy",
         r && strcmp(result_get_str(r, "strategy", ""), "exact") == 0);
    json_free(r);

    /* 12. empty old_string */
    write_file(testfile, "content");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"\"}", NULL));
    TEST("patch empty old_string returns error",
         r && strstr(result_get_str(r, "error", ""), "empty") != NULL);
    json_free(r);

    /* 13. replace with empty new_string (delete) */
    write_file(testfile, "Hello World");
    r = parse_result(patch_handler("{\"path\":\"/tmp/hermes_test_patch.txt\",\"old_string\":\"Hello \",\"new_string\":\"\"}", NULL));
    TEST("patch delete (empty new_string) returns success",
         r && json_object_get_bool(r, "success", false));
    json_free(r);

    content = read_file(testfile);
    TEST("patch delete content correct",
         content && strcmp(content, "World") == 0);
    free(content);

    /* 14. JSON parse error */
    r = parse_result(patch_handler("{invalid json}", NULL));
    TEST("patch JSON parse error",
         r && strstr(result_get_str(r, "error", ""), "JSON parse") != NULL);
    json_free(r);

    /* ================================================================
     * Unescape regression tests — ported from upstream @78be45860
     * Tests the region-based \\t/\\r unescape in new_string.
     * ================================================================ */

    /* 15. Tab unescape under exact: file has real tab, old_string has real
     * tab (exact match), new_string has literal \\t. Must unescape. */
    {
        const char *tab_file = "/tmp/hermes_test_unescape_tab.txt";
        const char *tab_content = "before\n\tafter\n";
        write_file(tab_file, tab_content);
        /* old: real tab (0x09), new: two-char \\t followed by "done" */
        build_json_args(tab_file, "\tafter", "\\tdone");
        r = parse_result(patch_handler(args_buf, NULL));
        TEST("unescape tab under exact: success",
             r && json_object_get_bool(r, "success", false));
        TEST("unescape tab under exact: strategy=exact",
             r && strcmp(result_get_str(r, "strategy", ""), "exact") == 0);
        json_free(r);
        char *c = read_file(tab_file);
        TEST("unescape tab under exact: real tab in output",
             c && strstr(c, "\tdone") != NULL);
        TEST("unescape tab under exact: no literal \\\\t",
             c && strstr(c, "\\t") == NULL);
        free(c);
        unlink(tab_file);
    }

    /* 16. \\n NOT unescaped — newline is left as literal two-char sequence */
    {
        const char *nl_file = "/tmp/hermes_test_unescape_nl.txt";
        write_file(nl_file, "line1\nline2\n");
        build_json_args(nl_file, "line1\nline2", "alpha\\nbeta");
        r = parse_result(patch_handler(args_buf, NULL));
        TEST("unescape newline preserved: success",
             r && json_object_get_bool(r, "success", false));
        json_free(r);
        char *c = read_file(nl_file);
        TEST("unescape newline preserved: literal \\\\n in output",
             c && strstr(c, "\\n") != NULL);
        TEST("unescape newline preserved: no real newline from \\\\n",
             c && strstr(c, "alpha\nbeta") == NULL);
        free(c);
        unlink(nl_file);
    }

    /* 17. Literal \\t in Python string literal — NOT unescaped.
     * File has literal two-char \\t (backslash + t) from source code,
     * not a real tab. The matched region has no real tab, so \\t in
     * new_string passes through verbatim. */
    {
        const char *lit_file = "/tmp/hermes_test_unescape_lit.txt";
        write_file(lit_file, "sep = \"\\t\"\n");
        build_json_args(lit_file, "sep = \"\\t\"", "sep = \"\\tab\"");
        r = parse_result(patch_handler(args_buf, NULL));
        TEST("unescape literal preserved: success",
             r && json_object_get_bool(r, "success", false));
        json_free(r);
        char *c = read_file(lit_file);
        TEST("unescape literal preserved: literal \\\\t in output",
             c && strstr(c, "\\t") != NULL);
        TEST("unescape literal preserved: no real tab injected",
             c && strchr(c, '\t') == NULL);
        free(c);
        unlink(lit_file);
    }

    /* 18. No escape sequences — pure passthrough */
    {
        const char *noesc_file = "/tmp/hermes_test_unescape_noesc.txt";
        write_file(noesc_file, "def foo():\n    return 1\n");
        build_json_args(noesc_file, "    return 1", "    return 2");
        r = parse_result(patch_handler(args_buf, NULL));
        TEST("unescape no sequences: success",
             r && json_object_get_bool(r, "success", false));
        json_free(r);
        char *c = read_file(noesc_file);
        TEST("unescape no sequences: content correct",
             c && strstr(c, "return 2") != NULL);
        free(c);
        unlink(noesc_file);
    }

    /* 19. Dry run — patch succeeds but file is NOT modified */
    {
        const char *dry_file = "/tmp/hermes_test_dry_run.txt";
        write_file(dry_file, "original content");
        build_json_dry(dry_file, "original", "REPLACED");
        r = parse_result(patch_handler(dry_args_buf, NULL));
        TEST("dry_run: success returned",
             r && json_object_get_bool(r, "success", false));
        TEST("dry_run: dry_run flag in response",
             r && json_object_get_bool(r, "dry_run", false));
        TEST("dry_run: replacements count > 0",
             r && json_object_get_number(r, "replacements", 0) > 0);
        json_free(r);
        /* File should still contain original content */
        char *c = read_file(dry_file);
        TEST("dry_run: file not modified",
             c && strstr(c, "original") != NULL);
        TEST("dry_run: no REPLACED content in file",
             c && strstr(c, "REPLACED") == NULL);
        free(c);
        unlink(dry_file);
    }

    /* 20. Dry run with replace_all — multiple replacements, no file changes */
    {
        const char *dry_all_file = "/tmp/hermes_test_dry_all.txt";
        write_file(dry_all_file, "aaa bbb aaa ccc aaa");
        memset(dry_args_buf, 0, sizeof(dry_args_buf));
        size_t bp = 0;
        bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1,
            "{\"path\":\"");

#ifdef __linux__
        /* Append path with JSON escaping */
        for (const char *s = dry_all_file; *s && bp < sizeof(dry_args_buf) - 50; s++) {
            if (*s == '"' || *s == '\\') dry_args_buf[bp++] = '\\';
            dry_args_buf[bp++] = *s;
        }
        bp += snprintf(dry_args_buf + bp, sizeof(dry_args_buf) - bp - 1,
            "\",\"old_string\":\"aaa\",\"new_string\":\"ZZZ\","
            "\"replace_all\":true,\"dry_run\":true}");
#endif
        r = parse_result(patch_handler(dry_args_buf, NULL));
        TEST("dry_run replace_all: success",
             r && json_object_get_bool(r, "success", false));
        TEST("dry_run replace_all: dry_run flag",
             r && json_object_get_bool(r, "dry_run", false));
        TEST("dry_run replace_all: 3 replacements",
             r && (int)json_object_get_number(r, "replacements", 0) == 3);
        json_free(r);
        /* File is unchanged */
        char *c = read_file(dry_all_file);
        TEST("dry_run replace_all: original preserved",
             c && strstr(c, "aaa") != NULL);
        free(c);
        unlink(dry_all_file);
    }

    /* Cleanup */
    unlink(testfile);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
