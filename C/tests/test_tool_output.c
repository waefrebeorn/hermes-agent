/* test_tool_output.c -- Tests for libtooloutput module.
 * Tests: config limits, env-override, exceed checks, edge cases.
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

/* ── Edge cases ── */

static void test_invalid_env_values(void) {
    /* Non-numeric string → fallback to default */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "not-a-number", 1);
    T("non-numeric max_bytes falls back to default",
      tool_output_get_max_bytes() == 50000);

    /* Empty string → fallback to default */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "", 1);
    T("empty max_bytes falls back to default",
      tool_output_get_max_bytes() == 50000);

    /* Negative value → fallback to default (n <= 0 check) */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "-100", 1);
    T("negative max_bytes falls back to default",
      tool_output_get_max_bytes() == 50000);

    /* Zero value → fallback to default (n <= 0 check) */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "0", 1);
    T("zero max_bytes falls back to default",
      tool_output_get_max_bytes() == 50000);

    /* Very large value → clamped (> 100000000 check) */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "999999999", 1);
    T("excessive max_bytes falls back to default",
      tool_output_get_max_bytes() == 50000);

    /* Whitespace prefix — strtol handles */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "  25000", 1);
    T("whitespace prefix parsed correctly",
      tool_output_get_max_bytes() == 25000);
}

static void test_edge_exceed_checks(void) {
    /* size_t edge: SIZE_MAX should exceed any reasonable limit */
    setenv("HERMES_TOOL_OUTPUT_MAX_BYTES", "1000", 1);
    T("SIZE_MAX exceeds byte limit",
      tool_output_exceeds_byte_limit((size_t)-1));

    /* Negative line count passed as int: coerces to large unsigned when
     * compared, so -1 > 50 → true (exceeded). Depending on implementation
     * this may differ, but we test the observed behavior. */
    setenv("HERMES_TOOL_OUTPUT_MAX_LINES", "50", 1);
    /* tool_output_exceeds_line_limit takes int, so -1 stays -1.
     * Comparison against max_lines (50): -1 > 50 is false.
     * A negative line count is a caller bug, not something the function
     * needs to guard against. This test documents the behavior. */
    T("-1 line count does not exceed (caller's responsibility)",
      !tool_output_exceeds_line_limit(-1));
}

int main(void) {
    printf("=== Tool Output Tests ===\n\n");
    printf("--- Defaults ---\n"); test_defaults();
    printf("\n--- Env Overrides ---\n"); test_env_overrides();
    printf("\n--- Exceed Checks ---\n"); test_exceed_checks();
    printf("\n--- Invalid Env Values ---\n"); test_invalid_env_values();
    printf("\n--- Edge Exceed Checks ---\n"); test_edge_exceed_checks();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
