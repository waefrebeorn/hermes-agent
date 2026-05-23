/*
 * test_tool_output.c — Tests for libtooloutput (tool output limits).
 */

#include "tool_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

static void test_defaults(void)
{
    TEST("default max bytes", tool_output_get_max_bytes() == 50000);
    TEST("default max lines", tool_output_get_max_lines() == 2000);
    TEST("default max line length", tool_output_get_max_line_length() == 2000);
}

static void test_limits(void)
{
    TEST("exceeds byte limit 0", !tool_output_exceeds_byte_limit(0));
    TEST("exceeds byte limit under", !tool_output_exceeds_byte_limit(49999));
    TEST("exceeds byte limit at", !tool_output_exceeds_byte_limit(50000));
    TEST("exceeds byte limit over", tool_output_exceeds_byte_limit(50001));

    TEST("exceeds line limit 0", !tool_output_exceeds_line_limit(0));
    TEST("exceeds line limit under", !tool_output_exceeds_line_limit(1999));
    TEST("exceeds line limit at", !tool_output_exceeds_line_limit(2000));
    TEST("exceeds line limit over", tool_output_exceeds_line_limit(2001));
}

static void test_env_override(void)
{
    /* Set env vars */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "100000", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINES", "5000", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "500", 1);

    TEST("env max bytes", tool_output_get_max_bytes() == 100000);
    TEST("env max lines", tool_output_get_max_lines() == 5000);
    TEST("env max line length", tool_output_get_max_line_length() == 500);

    TEST("env exceeds bytes over", tool_output_exceeds_byte_limit(100001));
    TEST("env exceeds bytes at", !tool_output_exceeds_byte_limit(100000));
    TEST("env exceeds lines over", tool_output_exceeds_line_limit(5001));

    /* Clean up */
    unsetenv("HERMES_TOOL_OUTPUT_MAX_BYTES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
}

static void test_env_invalid(void)
{
    /* Invalid values should fall back to defaults */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "not-a-number", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINES", "-100", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "0", 1);

    TEST("invalid bytes falls back", tool_output_get_max_bytes() == 50000);
    TEST("negative lines falls back", tool_output_get_max_lines() == 2000);
    TEST("zero line length falls back", tool_output_get_max_line_length() == 2000);

    unsetenv("HERMES_TOOL_OUTPUT_MAX_BYTES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
}

int main(void)
{
    printf("=== Tool Output Limits Library Tests ===\n");

    test_defaults();
    test_limits();
    test_env_override();
    test_env_invalid();

    printf("\nResults: %d passed, %d failed, %d total\n", passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
