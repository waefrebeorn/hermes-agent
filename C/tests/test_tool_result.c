/*
 * test_tool_result.c — Tests for tool_result_classification
 */

#include "hermes_tool_result.h"
#include "hermes_json.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s\n", name); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

static void check(const char *label, bool expected, const char *tool, const char *result) {
    bool got = tool_result_file_mutation_landed(tool, result);
    if (got == expected) PASS();
    else {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s: expected %d, got %d", label, expected, got);
        FAIL(buf);
    }
}

int main(void) {
    printf("=== Tool Result Classification Tests ===\n\n");

    TEST("write_file: bytes_written present");
    check("write_file success", true, "write_file",
          "{\"bytes_written\": 1024, \"path\": \"/tmp/test\"}");

    TEST("write_file: no bytes_written");
    check("write_file no bytes", false, "write_file",
          "{\"path\": \"/tmp/test\"}");

    TEST("write_file: zero bytes");
    check("write_file zero", false, "write_file",
          "{\"bytes_written\": 0}");

    TEST("write_file: with error");
    check("write_file error", false, "write_file",
          "{\"error\": \"Permission denied\"}");

    TEST("patch: success true");
    check("patch success", true, "patch",
          "{\"success\": true, \"diff\": \"...\"}");

    TEST("patch: success false");
    check("patch fail", false, "patch",
          "{\"success\": false}");

    TEST("patch: with error");
    check("patch error", false, "patch",
          "{\"error\": \"No match found\"}");

    TEST("empty result");
    check("empty", false, "write_file", "");

    TEST("null input");
    check("null tool", false, "write_file", NULL);

    TEST("wrong tool name");
    check("wrong tool", false, "terminal", "{\"exit_code\": 0}");

    TEST("null result");
    check("null result", false, "patch", NULL);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
