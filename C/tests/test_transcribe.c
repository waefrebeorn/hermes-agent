/*
 * test_transcribe.c — Tests for transcribe tool.
 */

#include "transcribe.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

/* Forward declare the handler from transcribe.c */
extern char *transcribe_handler(const char *args_json, const char *task_id);

static void check(const char *label, const char *args, bool expect_success,
                  const char *expect_key) {
    char *result = transcribe_handler(args, NULL);
    if (!result) { FAIL("null result"); return; }

    char *err = NULL;
    json_t *j = json_parse(result, &err);
    if (!j) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: JSON parse: %s", label, err ? err : "?");
        free(err);
        FAIL(buf);
        free(result);
        return;
    }

    bool success = json_get_bool(j, "success", false);
    if (success != expect_success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: success=%d, expected=%d", label, success, expect_success);
        FAIL(buf);
        json_free(j);
        free(result);
        return;
    }

    /* Check for expected key */
    const char *val = NULL;
    if (expect_success) {
        val = json_get_str(j, "transcript", NULL);
    } else {
        val = json_get_str(j, "error", NULL);
    }
    if (!val && expect_key) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: missing key '%s'", label, expect_key);
        FAIL(buf);
    } else {
        PASS();
    }

    json_free(j);
    free(result);
}

int main(void) {
    printf("=== Transcribe Tool Tests ===\n\n");

    TEST("null args");
    check("null", NULL, false, "error");

    TEST("empty JSON");
    check("empty", "{}", false, "error");

    TEST("file_path required");
    check("no path", "{\"model\":\"groq\"}", false, "error");

    TEST("nonexistent file");
    check("missing", "{\"file_path\":\"/tmp/nonexistent_audio_xyz.wav\"}", false, "error");

    TEST("wrong extension");
    check("bad ext", "{\"file_path\":\"/tmp/test.txt\"}", false, "error");

    TEST("invalid JSON");
    check("bad json", "{invalid}", false, "error");

    TEST("empty file path");
    check("empty path", "{\"file_path\":\"\"}", false, "error");

    TEST("unsupported audio extension");
    check("unsupported", "{\"file_path\":\"/tmp/test.aiff\"}", false, "error");

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
