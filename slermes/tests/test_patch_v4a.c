/* test_patch_v4a.c — V4A patch mode tests */
/* Phase 334: +7 edge cases: nonexistent file (update/delete), multi-file patch,
   multi-hunk update, addition-only hunk, deletion-only hunk, empty/malformed patch */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define TEST(name) do { printf("  %s ... ", name); fflush(stdout); } while(0)
#define PASS() do { printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failures++; } while(0)

/* Forward declaration of patch_handler from patch.c */
extern char *patch_handler(const char *args_json, const char *task_id);

/* Forward declaration */
extern void registry_init_patch(void);

static char *call_patch(const char *args_json) {
    return patch_handler(args_json, "test_task");
}

static char *escape_patch_for_json(const char *patch) {
    size_t elen = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') elen += 2;
        else if (*p == '"') elen += 2;
        else elen++;
    }
    char *escaped = malloc(elen + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') { escaped[pos++] = '\\'; escaped[pos++] = 'n'; }
        else if (*p == '"') { escaped[pos++] = '\\'; escaped[pos++] = '"'; }
        else { escaped[pos++] = *p; }
    }
    escaped[pos] = '\0';
    return escaped;
}

static void do_v4a_test(const char *name, const char *patch, int expect_success) {
    TEST(name);
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool got_success = json_object_get_bool(rj, "success", false);
    if (expect_success) {
        if (!got_success) {
            json_node_t *arr = json_object_get(rj, "results");
            const char *detail = "?";
            if (arr && json_array_count(arr) > 0) {
                json_node_t *first = json_array_get(arr, 0);
                const char *e = json_object_get_string(first, "error", NULL);
                if (e) detail = e;
            }
            char buf[512]; snprintf(buf, sizeof(buf), "not success: %s", detail);
            FAIL(buf);
        } else {
            PASS();
        }
    } else {
        if (got_success) {
            FAIL("expected error but got success");
        } else {
            PASS();
        }
    }
    json_free(rj);
    free(result);
}

static void test_v4a_update_single_hunk(void) {
    const char *tmp_path = "/tmp/test_v4a_update.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("line1\nline2\nline3\nline4\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ context @@\n"
             " line1\n"
             "-line2\n"
             "+line2_modified\n"
             " line3\n"
             "*** End Patch", tmp_path);

    do_v4a_test("V4A update single hunk", patch, 1);

    /* Verify file content */
    f = fopen(tmp_path, "r");
    char buf[1024];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "line2_modified") == NULL) {
        FAIL("line2_modified not found in output");
    } else {
        PASS();
    }

    remove(tmp_path);
}

static void test_v4a_add_file(void) {
    const char *tmp_path = "/tmp/test_v4a_new.txt";
    remove(tmp_path); /* ensure clean */

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Add File: %s\n"
             "+hello world\n"
             "+second line\n"
             "*** End Patch", tmp_path);

    do_v4a_test("V4A add file", patch, 1);

    FILE *f = fopen(tmp_path, "r");
    if (!f) { FAIL("file not created"); return; }
    char buf[256];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "hello world") && strstr(buf, "second line")) {
        PASS();
    } else {
        FAIL("content mismatch");
    }

    remove(tmp_path);
}

static void test_v4a_delete_file(void) {
    const char *tmp_path = "/tmp/test_v4a_delete.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("delete me\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Delete File: %s\n"
             "*** End Patch", tmp_path);

    do_v4a_test("V4A delete file", patch, 1);

    if (access(tmp_path, F_OK) != 0) {
        PASS();
    } else {
        FAIL("file still exists");
    }
}

static void test_v4a_update_with_fuzzy(void) {
    const char *tmp_path = "/tmp/test_v4a_fuzzy.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("  indented line\n    more indented\n  last line\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ fuzzy test @@\n"
             " indented line\n"
             "-    more indented\n"
             "+  changed\n"
             " last line\n"
             "*** End Patch", tmp_path);

    do_v4a_test("V4A update with fuzzy whitespace", patch, 1);

    f = fopen(tmp_path, "r");
    char buf[512];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "changed") != NULL) {
        PASS();
    } else {
        FAIL("changed not found");
    }

    remove(tmp_path);
}

static void test_v4a_missing_begin_end(void) {
    const char *tmp_path = "/tmp/test_v4a_no_begin.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("alpha\nbeta\ngamma\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Update File: %s\n"
             " alpha\n"
             "-beta\n"
             "+omega\n"
             " gamma", tmp_path);

    do_v4a_test("V4A without explicit Begin/End markers", patch, 1);

    f = fopen(tmp_path, "r");
    char buf[512];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "omega") != NULL) {
        PASS();
    } else {
        FAIL("omega not found");
    }

    remove(tmp_path);
}

/* --- Phase 334: New edge case tests --- */

static void test_v4a_update_nonexistent_file(void) {
    const char *tmp_path = "/tmp/test_v4a_noexist_update.txt";
    remove(tmp_path); /* guarantee it doesn't exist */

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ context @@\n"
             " alpha\n"
             "-beta\n"
             "+omega\n"
             "*** End Patch", tmp_path);

    /* Should fail with "Cannot open file for reading" */
    TEST("V4A update nonexistent file");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool got_success = json_object_get_bool(rj, "success", false);
    /* Should not have overall success — partial failure */
    if (got_success) {
        FAIL("expected error but got success");
        json_free(rj); free(result);
        return;
    }

    /* Should have partial=true and results[0].error */
    bool partial = json_object_get_bool(rj, "partial", false);
    if (!partial) {
        FAIL("expected partial=true");
        json_free(rj); free(result);
        return;
    }

    json_node_t *arr = json_object_get(rj, "results");
    if (arr && json_array_count(arr) > 0) {
        json_node_t *first = json_array_get(arr, 0);
        const char *err = json_object_get_string(first, "error", "");
        if (strlen(err) > 0) {
            PASS();
        } else {
            FAIL("results[0].error is empty");
        }
    } else {
        FAIL("no results array");
    }

    json_free(rj);
    free(result);
}

static void test_v4a_delete_nonexistent_file(void) {
    const char *tmp_path = "/tmp/test_v4a_noexist_delete.txt";
    remove(tmp_path); /* guarantee it doesn't exist */

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Delete File: %s\n"
             "*** End Patch", tmp_path);

    /* Should fail with "Cannot delete file" */
    TEST("V4A delete nonexistent file");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool got_success = json_object_get_bool(rj, "success", false);
    if (got_success) {
        FAIL("expected error but got success");
        json_free(rj); free(result);
        return;
    }

    bool partial = json_object_get_bool(rj, "partial", false);
    if (!partial) {
        FAIL("expected partial=true");
        json_free(rj); free(result);
        return;
    }

    json_node_t *arr = json_object_get(rj, "results");
    if (arr && json_array_count(arr) > 0) {
        json_node_t *first = json_array_get(arr, 0);
        const char *err = json_object_get_string(first, "error", "");
        if (strlen(err) > 0) {
            PASS();
        } else {
            FAIL("results[0].error is empty");
        }
    } else {
        FAIL("no results array");
    }

    json_free(rj);
    free(result);
}

static void test_v4a_multi_file_patch(void) {
    const char *path_a = "/tmp/test_v4a_multi_a.txt";
    const char *path_b = "/tmp/test_v4a_multi_b.txt";
    const char *path_c = "/tmp/test_v4a_multi_c.txt";

    /* Create A and C; B will be added */
    FILE *f = fopen(path_a, "w");
    fputs("apple\nbanana\ncherry\n", f);
    fclose(f);
    f = fopen(path_c, "w");
    fputs("to be deleted\n", f);
    fclose(f);

    char patch[8192];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ fruit @@\n"
             " apple\n"
             "-banana\n"
             "+blueberry\n"
             " cherry\n"
             "*** Add File: %s\n"
             "+new file b\n"
             "+second line\n"
             "*** Delete File: %s\n"
             "*** End Patch",
             path_a, path_b, path_c);

    /* All three should succeed */
    TEST("V4A multi-file patch");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        FAIL("expected overall success");
        json_free(rj); free(result);
        return;
    }

    int nops = (int)json_object_get_number(rj, "operations", 0);
    if (nops < 3) {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected 3 ops, got %d", nops);
        FAIL(buf);
        json_free(rj); free(result);
        return;
    }

    /* Verify file A updated */
    f = fopen(path_a, "r");
    char buf[512];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';
    if (!strstr(buf, "blueberry")) {
        FAIL("path_a missing blueberry");
        json_free(rj); free(result);
        return;
    }

    /* Verify file B created */
    if (access(path_b, F_OK) != 0) {
        FAIL("path_b not created");
        json_free(rj); free(result);
        return;
    }

    /* Verify file C deleted */
    if (access(path_c, F_OK) == 0) {
        FAIL("path_c not deleted");
        json_free(rj); free(result);
        return;
    }

    PASS();

    json_free(rj);
    free(result);
    remove(path_a);
    remove(path_b);
    remove(path_c);
}

static void test_v4a_deletion_hunk(void) {
    /* Hunk with only - lines (no + replacement) — pure deletion */
    const char *tmp_path = "/tmp/test_v4a_delete_hunk.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("keep1\ndeleteme\nkeep2\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ delete @@\n"
             " keep1\n"
             "-deleteme\n"
             " keep2\n"
             "*** End Patch", tmp_path);

    TEST("V4A deletion-only hunk");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        json_node_t *arr = json_object_get(rj, "results");
        const char *detail = "?";
        if (arr && json_array_count(arr) > 0) {
            json_node_t *first = json_array_get(arr, 0);
            const char *e = json_object_get_string(first, "error", NULL);
            if (e) detail = e;
        }
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", detail);
        FAIL(buf);
        json_free(rj); free(result);
        return;
    }
    json_free(rj);
    free(result);

    /* Verify file content */
    f = fopen(tmp_path, "r");
    char buf[1024];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "deleteme") != NULL) {
        FAIL("\"deleteme\" still present after deletion");
    } else if (strstr(buf, "keep1") == NULL || strstr(buf, "keep2") == NULL) {
        FAIL("context lines missing after deletion");
    } else {
        PASS();
    }

    remove(tmp_path);
}

static void test_v4a_addition_hunk(void) {
    /* Hunk with only context + + lines — adds lines after context */
    const char *tmp_path = "/tmp/test_v4a_add_hunk.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("line1\nline2\nline3\n", f);
    fclose(f);

    char patch[4096];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ add @@\n"
             " line1\n"
             " line2\n"
             "+inserted line\n"
             "+another new line\n"
             " line3\n"
             "*** End Patch", tmp_path);

    TEST("V4A addition-only hunk");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        json_node_t *arr = json_object_get(rj, "results");
        const char *detail = "?";
        if (arr && json_array_count(arr) > 0) {
            json_node_t *first = json_array_get(arr, 0);
            const char *e = json_object_get_string(first, "error", NULL);
            if (e) detail = e;
        }
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", detail);
        FAIL(buf);
        json_free(rj); free(result);
        return;
    }
    json_free(rj);
    free(result);

    /* Verify file content */
    f = fopen(tmp_path, "r");
    char buf[1024];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "inserted line") == NULL) {
        FAIL("inserted line not found");
    } else if (strstr(buf, "another new line") == NULL) {
        FAIL("another new line not found");
    } else {
        PASS();
    }

    remove(tmp_path);
}

static void test_v4a_multi_hunk_update(void) {
    /* Two hunks updating different parts of the same file */
    const char *tmp_path = "/tmp/test_v4a_multi_hunk.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("section1_a\nsection1_b\nsection1_c\nmiddle\nsection2_a\nsection2_b\nsection2_c\n", f);
    fclose(f);

    char patch[8192];
    snprintf(patch, sizeof(patch),
             "*** Begin Patch\n"
             "*** Update File: %s\n"
             "@@ section1 @@\n"
             " section1_a\n"
             "-section1_b\n"
             "+section1_b_modified\n"
             " section1_c\n"
             "@@ section2 @@\n"
             " section2_a\n"
             "-section2_b\n"
             "+section2_b_modified\n"
             " section2_c\n"
             "*** End Patch", tmp_path);

    TEST("V4A multi-hunk update");
    char *escaped = escape_patch_for_json(patch);
    char json_args[16384];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        json_node_t *arr = json_object_get(rj, "results");
        const char *detail = "?";
        if (arr && json_array_count(arr) > 0) {
            json_node_t *first = json_array_get(arr, 0);
            const char *e = json_object_get_string(first, "error", NULL);
            if (e) detail = e;
        }
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", detail);
        FAIL(buf);
        json_free(rj); free(result);
        return;
    }

    /* Check hunks_applied in result */
    json_node_t *arr = json_object_get(rj, "results");
    if (arr && json_array_count(arr) > 0) {
        json_node_t *first = json_array_get(arr, 0);
        double ha = json_object_get_number(first, "hunks_applied", 0);
        if (ha < 2) {
            char buf[128];
            snprintf(buf, sizeof(buf), "expected 2+ hunks_applied, got %.0f", ha);
            FAIL(buf);
            json_free(rj); free(result);
            remove(tmp_path);
            return;
        }
    }

    json_free(rj);
    free(result);

    /* Verify both changes applied */
    f = fopen(tmp_path, "r");
    char buf[1024];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "section1_b_modified") == NULL) {
        FAIL("section1_b_modified not found");
    } else if (strstr(buf, "section2_b_modified") == NULL) {
        FAIL("section2_b_modified not found");
    } else {
        PASS();
    }

    remove(tmp_path);
}

static void test_v4a_empty_patch(void) {
    /* Empty patch — no directives, 0 ops, should succeed with 0 operations */
    TEST("V4A empty/malformed patch");
    char *result = call_patch("{\"mode\":\"patch\",\"patch\":\"\"}");
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    int nops = (int)json_object_get_number(rj, "operations", -1);
    int changes = (int)json_object_get_number(rj, "total_changes", -1);
    bool success = json_object_get_bool(rj, "success", false);

    if (success && nops == 0 && changes == 0) {
        PASS();
    } else if (!success) {
        FAIL("expected success with 0 ops");
    } else if (nops != 0) {
        char buf[128]; snprintf(buf, sizeof(buf), "expected 0 ops, got %d", nops);
        FAIL(buf);
    } else {
        FAIL("unexpected state");
    }

    json_free(rj);
    free(result);
}

int main(void) {
    printf("=== Patch V4A Mode Tests ===\n");

    test_v4a_update_single_hunk();
    test_v4a_add_file();
    test_v4a_delete_file();
    test_v4a_update_with_fuzzy();
    test_v4a_missing_begin_end();

    /* Phase 334: edge cases */
    test_v4a_update_nonexistent_file();
    test_v4a_delete_nonexistent_file();
    test_v4a_multi_file_patch();
    test_v4a_deletion_hunk();
    test_v4a_addition_hunk();
    test_v4a_multi_hunk_update();
    test_v4a_empty_patch();

    printf("\nResults: %d failed\n", failures);
    return failures > 0 ? 1 : 0;
}
