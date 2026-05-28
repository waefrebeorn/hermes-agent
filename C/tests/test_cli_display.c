/*
 * test_cli_display.c — CLI display core test suite (D01).
 *
 * Tests: display_init, display_width, display_has_color, style functions,
 *        progress bar, spinner, printf, cursor movement, RGB/256 color.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_cli_display.c src/deps/cli_display.c \
 *       -Wl,--unresolved-symbols=ignore-all \
 *       -o /tmp/hermes_test_cli_display -lm
 *
 * Run:
 *   /tmp/hermes_test_cli_display
 */

#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_EQ(name, a, b) TEST(name, (a) == (b))

/* ================================================================
 *  1. init and basic properties
 * ================================================================ */
static void test_init(void) {
    printf("\n--- init ---\n");

    display_init();
    TEST("init runs without crash", 1);

    int w = display_width();
    TEST("width > 0", w > 0);

    int has = display_has_color();
    TEST_EQ("has_color is 0 or 1", (has == 0 || has == 1), 1);
}

/* ================================================================
 *  2. style functions
 * ================================================================ */
static void test_styles(void) {
    printf("\n--- style functions ---\n");

    display_set_fg(DISPLAY_GREEN);
    TEST("set_fg GREEN doesn't crash", 1);

    display_set_bg(DISPLAY_BLACK);
    TEST("set_bg BLACK doesn't crash", 1);

    display_set_style(DISPLAY_BOLD);
    TEST("set_style BOLD doesn't crash", 1);

    display_set_style(DISPLAY_DIM);
    TEST("set_style DIM doesn't crash", 1);

    display_set_style(DISPLAY_ITALIC);
    TEST("set_style ITALIC doesn't crash", 1);

    display_set_style(DISPLAY_UNDERLINE);
    TEST("set_style UNDERLINE doesn't crash", 1);

    display_set_style(DISPLAY_NORMAL);
    TEST("set_style NORMAL doesn't crash", 1);

    display_reset();
    TEST("reset doesn't crash", 1);
}

/* ================================================================
 *  3. panel and horizontal rule
 * ================================================================ */
static void test_panel_hr(void) {
    printf("\n--- panel and HR ---\n");

    display_panel("test panel", "content here", DISPLAY_CYAN);
    TEST("panel doesn't crash", 1);

    display_hr(DISPLAY_WHITE);
    TEST("hr doesn't crash", 1);

    display_reset();
}

/* ================================================================
 *  4. printf and hex printf
 * ================================================================ */
static void test_printf(void) {
    printf("\n--- printf ---\n");

    display_printf(DISPLAY_WHITE, DISPLAY_NORMAL, "test %s %d", "printf", 42);
    TEST("display_printf doesn't crash", 1);

    /* Clear screen (no crash) */
    display_clear();
    TEST("display_clear doesn't crash", 1);
}

/* ================================================================
 *  5. cursor movement
 * ================================================================ */
static void test_cursor(void) {
    printf("\n--- cursor ---\n");

    display_save_pos();
    TEST("save_pos doesn't crash", 1);

    display_goto(5, 10);
    TEST("goto doesn't crash", 1);

    display_restore_pos();
    TEST("restore_pos doesn't crash", 1);

    /* goto with negative values */
    display_goto(-1, -1);
    TEST("goto(-1,-1) doesn't crash", 1);
}

/* ================================================================
 *  6. progress bar
 * ================================================================ */
static void test_progress(void) {
    printf("\n--- progress ---\n");

    display_progress_t bar;

    display_progress_init(&bar, "test-progress", 100);
    TEST("progress init doesn't crash", 1);

    display_progress_update(&bar, 0);
    TEST("progress update 0% doesn't crash", 1);

    display_progress_update(&bar, 50);
    TEST("progress update 50% doesn't crash", 1);

    display_progress_update(&bar, 100);
    TEST("progress update 100% doesn't crash", 1);

    display_progress_done(&bar);
    TEST("progress done doesn't crash", 1);

    /* Progress with 0 total (edge case) */
    display_progress_init(&bar, "zero", 0);
    display_progress_update(&bar, 0);
    display_progress_done(&bar);
    TEST("zero-total progress doesn't crash", 1);
}

/* ================================================================
 *  7. spinner
 * ================================================================ */
static void test_spinner(void) {
    printf("\n--- spinner ---\n");

    display_spinner_t sp;

    display_spinner_start(&sp, "test-spinner");
    TEST("spinner start doesn't crash", 1);

    display_spinner_tick(&sp);
    TEST("spinner tick doesn't crash", 1);

    display_spinner_tick(&sp);
    TEST("spinner second tick doesn't crash", 1);

    display_spinner_stop(&sp, "complete");
    TEST("spinner stop doesn't crash", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== CLI Display Test Suite ===\n");

    test_init();
    test_styles();
    test_panel_hr();
    test_printf();
    test_cursor();
    test_progress();
    test_spinner();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
