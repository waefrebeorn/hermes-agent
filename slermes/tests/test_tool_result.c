/*
 * test_tool_result.c — Tests for tool result classification.
 * Port of Python agent/tool_result_classification.py.
 *
 * Tests: NULL/empty inputs, non-mutation tools, write_file success/failure,
 * patch success/failure, error field handling, JSON edge cases.
 */

#include "hermes_tool_result.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static void test_null_empty_inputs(void) {
    printf("\n--- NULL/empty inputs ---\n");
    TEST("NULL tool_name", !tool_result_file_mutation_landed(NULL, "{}"));
    TEST("NULL result", !tool_result_file_mutation_landed("write_file", NULL));
    TEST("empty result", !tool_result_file_mutation_landed("write_file", ""));
    TEST("both NULL", !tool_result_file_mutation_landed(NULL, NULL));
}

static void test_non_mutation_tools(void) {
    printf("\n--- Non-mutation tools ---\n");
    TEST("read_file returns false",
        !tool_result_file_mutation_landed("read_file", "{\"bytes_read\":100}"));
    TEST("search_files returns false",
        !tool_result_file_mutation_landed("search_files", "{\"matches\":[]}"));
    TEST("terminal returns false",
        !tool_result_file_mutation_landed("terminal", "{\"output\":\"ok\"}"));
    TEST("empty tool name",
        !tool_result_file_mutation_landed("", "{}"));
    TEST("unknown tool",
        !tool_result_file_mutation_landed("unknown_tool", "{}"));
}

static void test_write_file_success(void) {
    printf("\n--- write_file success ---\n");
    TEST("bytes_written > 0",
        tool_result_file_mutation_landed("write_file", "{\"bytes_written\":100}"));
    TEST("bytes_written = 1",
        tool_result_file_mutation_landed("write_file", "{\"bytes_written\":1}"));
    TEST("bytes_written large",
        tool_result_file_mutation_landed("write_file", "{\"bytes_written\":99999}"));
    TEST("bytes_written float",
        tool_result_file_mutation_landed("write_file", "{\"bytes_written\":42.5}"));
}

static void test_write_file_failure(void) {
    printf("\n--- write_file failure ---\n");
    TEST("bytes_written = 0",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":0}"));
    TEST("error field present",
        !tool_result_file_mutation_landed("write_file",
            "{\"error\":\"permission denied\"}"));
    TEST("error + bytes_written 0",
        !tool_result_file_mutation_landed("write_file",
            "{\"error\":\"disk full\",\"bytes_written\":0}"));
    TEST("no bytes_written field",
        !tool_result_file_mutation_landed("write_file", "{\"success\":true}"));
    TEST("bytes_written = -1",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":-1}"));
}

static void test_patch_success(void) {
    printf("\n--- patch success ---\n");
    TEST("success true",
        tool_result_file_mutation_landed("patch", "{\"success\":true}"));
    TEST("success true + extra fields",
        tool_result_file_mutation_landed("patch",
            "{\"success\":true,\"matches\":5}"));
}

static void test_patch_failure(void) {
    printf("\n--- patch failure ---\n");
    TEST("success false",
        !tool_result_file_mutation_landed("patch", "{\"success\":false}"));
    TEST("error field present",
        !tool_result_file_mutation_landed("patch",
            "{\"error\":\"no match found\"}"));
    TEST("error + success false",
        !tool_result_file_mutation_landed("patch",
            "{\"error\":\"match failed\",\"success\":false}"));
    TEST("no success field",
        !tool_result_file_mutation_landed("patch", "{\"matches\":0}"));
}

static void test_json_edge_cases(void) {
    printf("\n--- JSON edge cases ---\n");
    TEST("invalid JSON",
        !tool_result_file_mutation_landed("write_file", "{invalid json}"));
    TEST("null result JSON",
        !tool_result_file_mutation_landed("write_file", "null"));
    TEST("number result, not object",
        !tool_result_file_mutation_landed("write_file", "42"));
    TEST("array result, not object",
        !tool_result_file_mutation_landed("write_file", "[1,2,3]"));
    TEST("string result",
        !tool_result_file_mutation_landed("write_file", "\"ok\""));
    TEST("error is null (not an error)",
        tool_result_file_mutation_landed("write_file",
            "{\"bytes_written\":10,\"error\":null}"));
}

static void test_additional_edge_cases(void) {
    printf("\n--- Additional edge cases ---\n");

    /* Case sensitivity */
    TEST("Write_File (PascalCase) returns false",
        !tool_result_file_mutation_landed("Write_File", "{\"bytes_written\":10}"));
    TEST("WRITE_FILE (SCREAMING) returns false",
        !tool_result_file_mutation_landed("WRITE_FILE", "{\"bytes_written\":10}"));

    /* Other mutation tools not in the list */
    TEST("file_merge returns false",
        !tool_result_file_mutation_landed("file_merge", "{\"success\":true}"));
    TEST("exec_code returns false",
        !tool_result_file_mutation_landed("exec_code", "{\"success\":true}"));
    TEST("image_gen returns false",
        !tool_result_file_mutation_landed("image_gen", "{\"success\":true}"));
    TEST("session_crud returns false",
        !tool_result_file_mutation_landed("session_crud", "{\"success\":true}"));

    /* bytes_written edge cases */
    TEST("bytes_written = 0.0",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":0.0}"));
    TEST("bytes_written = -0.5",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":-0.5}"));
    TEST("bytes_written as string '5'",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":\"5\"}"));
    TEST("bytes_written as bool true",
        !tool_result_file_mutation_landed("write_file", "{\"bytes_written\":true}"));

    /* success edge cases */
    TEST("success = 1 (number, not bool)",
        !tool_result_file_mutation_landed("patch", "{\"success\":1}"));
    TEST("success = \"true\" (string, not bool)",
        !tool_result_file_mutation_landed("patch", "{\"success\":\"true\"}"));

    /* Error field edge cases */
    TEST("error = empty string treated as error",
        !tool_result_file_mutation_landed("write_file",
            "{\"bytes_written\":10,\"error\":\"\"}"));
    TEST("error = false treated as error",
        !tool_result_file_mutation_landed("write_file",
            "{\"bytes_written\":10,\"error\":false}"));
    TEST("error = 0 treated as error",
        !tool_result_file_mutation_landed("write_file",
            "{\"bytes_written\":10,\"error\":0}"));

    /* Deeply nested bytes_written */
    TEST("nested bytes_written not detected at top level",
        !tool_result_file_mutation_landed("write_file",
            "{\"result\":{\"bytes_written\":10}}"));
    TEST("bytes_written at root in bigger JSON",
        tool_result_file_mutation_landed("write_file",
            "{\"bytes_written\":42,\"path\":\"/tmp/x\",\"checksum\":\"abc123\"}"));

    /* Very long result JSON */
    {
        char long_result[4096];
        char padding[3800];
        memset(padding, 'x', 3798);
        padding[3798] = '\0';
        snprintf(long_result, sizeof(long_result),
            "{\"bytes_written\":1,\"padding\":\"%s\"}", padding);
        TEST("very long JSON result handled gracefully",
            tool_result_file_mutation_landed("write_file", long_result));
    }
}

int main(void) {
    printf("=== Tool Result Tests ===\n");

    test_null_empty_inputs();
    test_non_mutation_tools();
    test_write_file_success();
    test_write_file_failure();
    test_patch_success();
    test_patch_failure();
    test_json_edge_cases();
    test_additional_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
