/*
 * test_tui.c — Smoke test for terminal UI library.
 * Tests ANSI codes, progress bar, and input history.
 */
#include "tui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: ANSI escape codes */
    const char *fg = tui_fg(TUI_GREEN);
    TEST("tui_fg non-NULL", fg != NULL);
    if (fg) TEST("tui_fg starts with \\x1b", fg[0] == 0x1b);

    const char *bg = tui_bg(TUI_BLUE);
    TEST("tui_bg non-NULL", bg != NULL);
    if (bg) TEST("tui_bg starts with \\x1b", bg[0] == 0x1b);

    const char *bold = tui_bold();
    TEST("tui_bold non-NULL", bold != NULL);
    if (bold) TEST("tui_bold starts with \\x1b", bold[0] == 0x1b);

    const char *dim = tui_dim();
    TEST("tui_dim non-NULL", dim != NULL);
    if (dim) TEST("tui_dim starts with \\x1b", dim[0] == 0x1b);

    const char *reset = tui_reset();
    TEST("tui_reset non-NULL", reset != NULL);
    if (reset) TEST("tui_reset starts with \\x1b", reset[0] == 0x1b);

    /* Test 2: input history */
    tui_input_t *in = tui_input_new("> ");
    TEST("tui_input_new non-NULL", in != NULL);

    if (in) {
        TEST("tui_input_history_size initially 0", tui_input_history_size(in) == 0);

        tui_input_history_add(in, "first command");
        TEST("tui_input_history_size after add", tui_input_history_size(in) == 1);

        tui_input_history_add(in, "second command");
        tui_input_history_add(in, "third command");
        TEST("tui_input_history_size after 3 adds", tui_input_history_size(in) == 3);

        const char *first = tui_input_history_get(in, 0);
        TEST("tui_input_history_get[0]", first && strcmp(first, "first command") == 0);

        const char *second = tui_input_history_get(in, 1);
        TEST("tui_input_history_get[1]", second && strcmp(second, "second command") == 0);

        const char *third = tui_input_history_get(in, 2);
        TEST("tui_input_history_get[2]", third && strcmp(third, "third command") == 0);

        const char *out_of_range = tui_input_history_get(in, 99);
        TEST("tui_input_history_get out of range", out_of_range == NULL);

        /* Test echo toggle */
        tui_input_set_echo(in, false);
        tui_input_set_prompt(in, "password> ");
        TEST("set_echo and set_prompt no crash", true);

        tui_input_free(in);
    }

    /* Test 3: progress bar construction */
    tui_progress_t *p = tui_progress_new("Test", 100);
    TEST("tui_progress_new non-NULL", p != NULL);
    if (p) {
        /* Update progress bar at various points */
        tui_progress_update(p, 0);
        tui_progress_update(p, 50);
        tui_progress_update(p, 100);
        tui_progress_done(p);
        tui_progress_free(p);
        TEST("progress bar lifecycle no crash", true);
    }

    /* Test 4: display functions (verify they don't crash) */
    tui_printf(TUI_CYAN, "color test");
    tui_printfln(TUI_YELLOW, "color test line");
    tui_ruler("section", TUI_GREEN, 40);
    tui_box("title", "content", 30);
    TEST("display functions no crash", true);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All TUI tests PASSED");
    return failures ? 1 : 0;
}
