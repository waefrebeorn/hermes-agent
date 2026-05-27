/* test_tool_output.c -- Tests for libtooloutput module.
 * Tests: config limits, env-override, exceed checks.
 * Compile with tool_output.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tool_output.h"

static int pass = 0, fail = 0;
#define T(n, e) do { if(e) { pass++; printf("  OK %s\n", n); } else { fail++; printf("  FAIL %s\n", n); } } while(0)

static void test_defaults(void) {
    unsetenv("HERMES_TOOL_OUTPUT_MAX_BYTES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINES");
    unsetenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH");
    T("default max bytes", tool_output_get_max_bytes() == 50000);
    T("default max lines", tool_output_get_max_lines() == 2000);
    T("default max line length", tool_output_get_max_line_length() == 2000);
}

static void test_env_overrides(void) {
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "10000", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINES", "500", 1);
    setenv("HERMES_TOOL_OUTPUT_MAX_LINE_LENGTH", "100", 1);
    T("env overrides max bytes", tool_output_get_max_bytes() == 10000);
    T("env overrides max lines", tool_output_get_max_lines() == 500);
    T("env overrides line length", tool_output_get_max_line_length() == 100);
}

static void test_exceed_checks(void) {
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "1000", 1);
    T("500 bytes not exceeded", !tool_output_exceeds_byte_limit(500));
    T("1500 bytes exceeded", tool_output_exceeds_byte_limit(1500));
    T("1000 bytes not exceeded (equal)", !tool_output_exceeds_byte_limit(1000));
    T("0 bytes not exceeded", !tool_output_exceeds_byte_limit(0));

    setenv("HERMES_TOOL_OUTPUT_MAX_LINES", "50", 1);
    T("25 lines not exceeded", !tool_output_exceeds_line_limit(25));
    T("75 lines exceeded", tool_output_exceeds_line_limit(75));
    T("50 lines not exceeded (equal)", !tool_output_exceeds_line_limit(50));
}

int main(void) {
    printf("=== Tool Output Tests ===\n\n");
    printf("--- Defaults ---\n"); test_defaults();
    printf("\n--- Env Overrides ---\n"); test_env_overrides();
    printf("\n--- Exceed Checks ---\n"); test_exceed_checks();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0;
}
