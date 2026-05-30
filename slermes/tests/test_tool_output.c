/*
 * test_tool_output.c — Tests for libtooloutput tool-output truncation limits.
 * Tests env-var overrides, default values, and limit-checking functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Include the source under test (it includes tool_output.h internally) */
#include "../lib/libtooloutput/tool_output.c"

static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [L%d]: %s\n", __LINE__, msg); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

/* Set env var safely */
static void set_env(const char *name, const char *val) {
#ifdef _WIN32
    _putenv_s(name, val);
#else
    setenv(name, val, 1);
#endif
}

static void unset_env(const char *name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

int main(void) {
    printf("=== Tool Output Limits Tests ===\n");

    /* Test 1: Default values when no env vars set */
    unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
    unset_env("HERMES_TOOL_OUTPUT_MAX_LINES");
    unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
    {
        ASSERT(tool_output_get_max_bytes() == 50000,
               "default max_bytes is 50000");
        ASSERT(tool_output_get_max_lines() == 2000,
               "default max_lines is 2000");
        ASSERT(tool_output_get_max_line_length() == 2000,
               "default max_line_length is 2000");
    }

    /* Test 2: Env var overrides */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "1000");
        set_env("HERMES_TOOL_OUTPUT_MAX_LINES", "50");
        set_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "100");
        ASSERT(tool_output_get_max_bytes() == 1000,
               "max_bytes override 1000");
        ASSERT(tool_output_get_max_lines() == 50,
               "max_lines override 50");
        ASSERT(tool_output_get_max_line_length() == 100,
               "max_line_length override 100");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINES");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
    }

    /* Test 3: Invalid env values fall back to defaults */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "not_a_number");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "invalid max_bytes falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");

        set_env("HERMES_TOOL_OUTPUT_MAX_LINES", "-5");
        ASSERT(tool_output_get_max_lines() == 2000,
               "negative max_lines falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINES");

        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "0");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "zero max_bytes falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");

        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "99999999999");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "huge max_bytes (>100M) falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
    }

    /* Test 4: Empty env var falls back to default */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "empty max_bytes falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
    }

    /* Test 5: max_line_length getter default */
    {
        ASSERT(tool_output_get_max_line_length() == 2000,
               "default max_line_length is 2000");
    }

    /* Test 6: max_line_length env override */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "500");
        ASSERT(tool_output_get_max_line_length() == 500,
               "max_line_length override 500");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");

        set_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "0");
        ASSERT(tool_output_get_max_line_length() == 2000,
               "zero max_line_length falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");

        set_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "-100");
        ASSERT(tool_output_get_max_line_length() == 2000,
               "negative max_line_length falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
    }

    /* Test 7: All three env vars set simultaneously */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "1000");
        set_env("HERMES_TOOL_OUTPUT_MAX_LINES", "50");
        set_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "100");
        ASSERT(tool_output_get_max_bytes() == 1000,
               "all-three: max_bytes 1000");
        ASSERT(tool_output_get_max_lines() == 50,
               "all-three: max_lines 50");
        ASSERT(tool_output_get_max_line_length() == 100,
               "all-three: max_line_length 100");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINES");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
    }

    /* Test 8: strtol quirks — trailing garbage / whitespace rejected */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "100abc");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "strtol: trailing garbage '100abc' falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");

        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "  500  ");
        ASSERT(tool_output_get_max_bytes() == 50000,
               "strtol: trailing whitespace '  500  ' falls back to default");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
    }

    /* Test 9: Simultaneous byte AND line exceeds */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "50");
        set_env("HERMES_TOOL_OUTPUT_MAX_LINES", "5");
        ASSERT(tool_output_exceeds_byte_limit(49) == false,
               "mixed: 49 bytes < 50 limit");
        ASSERT(tool_output_exceeds_byte_limit(50) == false,
               "mixed: 50 bytes == 50 limit (not exceeding)");
        ASSERT(tool_output_exceeds_byte_limit(51) == true,
               "mixed: 51 bytes > 50 limit");
        ASSERT(tool_output_exceeds_line_limit(4) == false,
               "mixed: 4 lines < 5 limit");
        ASSERT(tool_output_exceeds_line_limit(5) == false,
               "mixed: 5 lines == 5 limit (not exceeding)");
        ASSERT(tool_output_exceeds_line_limit(6) == true,
               "mixed: 6 lines > 5 limit");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
        unset_env("HERMES_TOOL_OUTPUT_MAX_LINES");
    }

    /* Test 10: exceeds_byte_limit */
    {
        ASSERT(tool_output_exceeds_byte_limit(0) == false,
               "0 bytes does not exceed limit");
        ASSERT(tool_output_exceeds_byte_limit(49999) == false,
               "49999 bytes does not exceed limit");
        ASSERT(tool_output_exceeds_byte_limit(50000) == false,
               "50000 bytes does not exceed limit (equal)");
        ASSERT(tool_output_exceeds_byte_limit(50001) == true,
               "50001 bytes exceeds limit");
        ASSERT(tool_output_exceeds_byte_limit(100000) == true,
               "100000 bytes exceeds limit");
    }

    /* Test 11: exceeds_line_limit with default */
    {
        ASSERT(tool_output_exceeds_line_limit(0) == false,
               "0 lines does not exceed limit");
        ASSERT(tool_output_exceeds_line_limit(1999) == false,
               "1999 lines does not exceed limit");
        ASSERT(tool_output_exceeds_line_limit(2000) == false,
               "2000 lines does not exceed limit (equal)");
        ASSERT(tool_output_exceeds_line_limit(2001) == true,
               "2001 lines exceeds limit");
    }

    /* Test 12: exceeds_* with custom env override */
    {
        set_env("HERMES_TOOL_OUTPUT_MAX_BYTES", "100");
        ASSERT(tool_output_exceeds_byte_limit(99) == false,
               "99 bytes < 100 limit");
        ASSERT(tool_output_exceeds_byte_limit(100) == false,
               "100 bytes == 100 limit (not exceeding)");
        ASSERT(tool_output_exceeds_byte_limit(101) == true,
               "101 bytes > 100 limit");
        unset_env("HERMES_TOOL_OUTPUT_MAX_BYTES");
    }

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
