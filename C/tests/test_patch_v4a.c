/* test_patch_v4a.c — V4A patch mode tests */

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

static void test_v4a_update_single_hunk(void) {
    TEST("V4A update single hunk");

    /* Create temp file */
    const char *tmp_path = "/tmp/test_v4a_update.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("line1\nline2\nline3\nline4\n", f);
    fclose(f);

    char patch[] = "*** Begin Patch\n"
                   "*** Update File: /tmp/test_v4a_update.txt\n"
                   "@@ context @@\n"
                   " line1\n"
                   "-line2\n"
                   "+line2_modified\n"
                   " line3\n"
                   "*** End Patch";

    char args[4096];
    snprintf(args, sizeof(args), "{\"mode\":\"patch\",\"patch\":\"%s\"}", patch);
    /* Escape newlines in patch for JSON */
    /* Actually, let's just serialize properly via C */
    char *escaped = NULL;
    size_t elen = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') {
            elen += 2;
        } else if (*p == '"') {
            elen += 2;
        } else {
            elen++;
        }
    }
    escaped = malloc(elen + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') {
            escaped[pos++] = '\\';
            escaped[pos++] = 'n';
        } else if (*p == '"') {
            escaped[pos++] = '\\';
            escaped[pos++] = '"';
        } else {
            escaped[pos++] = *p;
        }
    }
    escaped[pos] = '\0';

    char json_args[8192];
    snprintf(json_args, sizeof(json_args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(json_args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        const char *err = json_object_get_string(rj, "error", "?");
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", err);
        FAIL(buf);
        json_free(rj);
        free(result);
        return;
    }

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

    json_free(rj);
    free(result);
    remove(tmp_path);
}

static void test_v4a_add_file(void) {
    TEST("V4A add file");

    const char *tmp_path = "/tmp/test_v4a_new.txt";
    remove(tmp_path); /* ensure clean */

    char patch[] = "*** Begin Patch\n"
                   "*** Add File: /tmp/test_v4a_new.txt\n"
                   "+hello world\n"
                   "+second line\n"
                   "*** End Patch";

    char args[8192];
    char *escaped = malloc(strlen(patch) * 2 + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') { escaped[pos++] = '\\'; escaped[pos++] = 'n'; }
        else if (*p == '"') { escaped[pos++] = '\\'; escaped[pos++] = '"'; }
        else { escaped[pos++] = *p; }
    }
    escaped[pos] = '\0';
    snprintf(args, sizeof(args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        const char *err = json_object_get_string(rj, "error", "?");
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", err);
        FAIL(buf);
        json_free(rj);
        free(result);
        return;
    }

    /* Verify file exists and has content */
    FILE *f = fopen(tmp_path, "r");
    if (!f) { FAIL("file not created"); json_free(rj); free(result); return; }
    char buf[256];
    size_t br = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[br] = '\0';

    if (strstr(buf, "hello world") && strstr(buf, "second line")) {
        PASS();
    } else {
        FAIL("content mismatch");
    }

    json_free(rj);
    free(result);
    remove(tmp_path);
}

static void test_v4a_delete_file(void) {
    TEST("V4A delete file");

    const char *tmp_path = "/tmp/test_v4a_delete.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("delete me\n", f);
    fclose(f);

    char patch[] = "*** Begin Patch\n"
                   "*** Delete File: /tmp/test_v4a_delete.txt\n"
                   "*** End Patch";

    char args[8192];
    char *escaped = malloc(strlen(patch) * 2 + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') { escaped[pos++] = '\\'; escaped[pos++] = 'n'; }
        else if (*p == '"') { escaped[pos++] = '\\'; escaped[pos++] = '"'; }
        else { escaped[pos++] = *p; }
    }
    escaped[pos] = '\0';
    snprintf(args, sizeof(args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        const char *err = json_object_get_string(rj, "error", "?");
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", err);
        FAIL(buf);
        json_free(rj);
        free(result);
        return;
    }

    /* Verify file is gone */
    if (access(tmp_path, F_OK) != 0) {
        PASS();
    } else {
        FAIL("file still exists");
    }

    json_free(rj);
    free(result);
}

static void test_v4a_update_with_fuzzy(void) {
    TEST("V4A update with fuzzy whitespace");

    const char *tmp_path = "/tmp/test_v4a_fuzzy.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("  indented line\n    more indented\n  last line\n", f);
    fclose(f);

    /* Patch with exact whitespace mismatch — should use fuzzy */
    char patch[] = "*** Begin Patch\n"
                   "*** Update File: /tmp/test_v4a_fuzzy.txt\n"
                   "@@ fuzzy test @@\n"
                   " indented line\n"
                   "-    more indented\n"
                   "+  changed\n"
                   " last line\n"
                   "*** End Patch";

    char args[8192];
    char *escaped = malloc(strlen(patch) * 2 + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') { escaped[pos++] = '\\'; escaped[pos++] = 'n'; }
        else if (*p == '"') { escaped[pos++] = '\\'; escaped[pos++] = '"'; }
        else { escaped[pos++] = *p; }
    }
    escaped[pos] = '\0';
    snprintf(args, sizeof(args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        const char *err = json_object_get_string(rj, "error", "?");
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", err);
        FAIL(buf);
        json_free(rj);
        free(result);
        return;
    }

    /* Check file has 'changed' */
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

    json_free(rj);
    free(result);
    remove(tmp_path);
}

static void test_v4a_missing_begin_end(void) {
    TEST("V4A without explicit Begin/End markers");

    const char *tmp_path = "/tmp/test_v4a_no_begin.txt";
    FILE *f = fopen(tmp_path, "w");
    fputs("alpha\nbeta\ngamma\n", f);
    fclose(f);

    /* No Begin/End markers — should still parse */
    char patch[] = "*** Update File: /tmp/test_v4a_no_begin.txt\n"
                   " alpha\n"
                   "-beta\n"
                   "+omega\n"
                   " gamma";

    char args[8192];
    char *escaped = malloc(strlen(patch) * 2 + 1);
    size_t pos = 0;
    for (const char *p = patch; *p; p++) {
        if (*p == '\n') { escaped[pos++] = '\\'; escaped[pos++] = 'n'; }
        else if (*p == '"') { escaped[pos++] = '\\'; escaped[pos++] = '"'; }
        else { escaped[pos++] = *p; }
    }
    escaped[pos] = '\0';
    snprintf(args, sizeof(args),
             "{\"mode\":\"patch\",\"patch\":\"%s\"}", escaped);
    free(escaped);

    char *result = call_patch(args);
    if (!result) { FAIL("null result"); return; }

    json_node_t *rj = json_parse(result, NULL);
    if (!rj) { FAIL("result not JSON"); free(result); return; }

    bool success = json_object_get_bool(rj, "success", false);
    if (!success) {
        const char *err = json_object_get_string(rj, "error", "?");
        char buf[256]; snprintf(buf, sizeof(buf), "not success: %s", err);
        FAIL(buf);
        json_free(rj);
        free(result);
        return;
    }

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

    json_free(rj);
    free(result);
    remove(tmp_path);
}

int main(void) {
    printf("=== Patch V4A Mode Tests ===\n");

    /* Init just the patch tool registrations if needed */
    /* patch_handler is already linked — call directly */

    test_v4a_update_single_hunk();
    test_v4a_add_file();
    test_v4a_delete_file();
    test_v4a_update_with_fuzzy();
    test_v4a_missing_begin_end();

    printf("\nResults: %d failed\n", failures);
    return failures > 0 ? 1 : 0;
}
