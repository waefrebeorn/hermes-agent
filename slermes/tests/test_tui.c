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

    /* ── Edge case expansion ── */
    printf("\n--- Edge cases ---\n");

    /* ANSI color out of range */
    {
        const char *fg_invalid = tui_fg(-1);
        TEST("tui_fg(-1) non-NULL", fg_invalid != NULL);
        const char *fg_high = tui_fg(256);
        TEST("tui_fg(256) non-NULL", fg_high != NULL);
        const char *bg_invalid = tui_bg(-1);
        TEST("tui_bg(-1) non-NULL", bg_invalid != NULL);
    }

    /* Input history edge cases */
    {
        tui_input_t *h = tui_input_new("> ");
        TEST("history handler non-NULL", h != NULL);
        if (h) {
            /* Empty string is skipped by history_add (!*line check) */
            tui_input_history_add(h, "");
            TEST("empty line not added", tui_input_history_size(h) == 0);

            /* Duplicate entry is deduped (checks last entry) */
            tui_input_history_add(h, "repeat");
            tui_input_history_add(h, "repeat");
            TEST("duplicate skipped, size=1", tui_input_history_size(h) == 1);
            const char *rep = tui_input_history_get(h, 0);
            TEST("repeat stored correctly", rep && strcmp(rep, "repeat") == 0);

            /* Very long command */
            char long_cmd[2048];
            memset(long_cmd, 'x', 2000);
            long_cmd[2000] = '\0';
            tui_input_history_add(h, long_cmd);
            TEST("long cmd stored, size=2", tui_input_history_size(h) == 2);
            const char *long_entry = tui_input_history_get(h, 1);
            TEST("long entry non-NULL", long_entry != NULL);
            if (long_entry) TEST("long entry correct len", strlen(long_entry) == 2000);

            /* Negative index (size_t wraps, returns NULL) */
            const char *neg = tui_input_history_get(h, (size_t)-1);
            TEST("SIZE_MAX index returns NULL", neg == NULL);

            /* Index past end */
            const char *past = tui_input_history_get(h, 999);
            TEST("index 999 returns NULL", past == NULL);

            /* NULL handler safety */
            TEST("history_size(NULL) == 0", tui_input_history_size(NULL) == 0);
            TEST("history_get(NULL, 0) == NULL", tui_input_history_get(NULL, 0) == NULL);

            tui_input_free(h);
        }
    }

    /* Input creation with variant prompts */
    {
        tui_input_t *h = tui_input_new("");
        TEST("empty prompt non-NULL", h != NULL);
        if (h) {
            tui_input_history_add(h, "cmd");
            TEST("history works with empty prompt", tui_input_history_size(h) == 1);
            tui_input_free(h);
        }
    }

    /* Progress bar edge cases */
    {
        /* Total = 0 (potential division by zero) */
        tui_progress_t *p = tui_progress_new("zero", 0);
        TEST("progress total=0 non-NULL", p != NULL);
        if (p) {
            tui_progress_update(p, 0);
            tui_progress_update(p, 50);
            tui_progress_done(p);
            tui_progress_free(p);
            TEST("progress total=0 no crash", true);
        }

        /* Negative total */
        p = tui_progress_new("negative", -10);
        TEST("progress total=-10 non-NULL", p != NULL);
        if (p) {
            tui_progress_update(p, 0);
            tui_progress_update(p, 5);
            tui_progress_done(p);
            tui_progress_free(p);
            TEST("progress total=-10 no crash", true);
        }

        /* Update past 100% */
        p = tui_progress_new("past", 100);
        TEST("progress past-100% non-NULL", p != NULL);
        if (p) {
            tui_progress_update(p, 50);
            tui_progress_update(p, 200);
            tui_progress_done(p);
            tui_progress_free(p);
            TEST("progress past 100% no crash", true);
        }
    }

    /* Display edge cases */
    {
        tui_ruler(NULL, TUI_GREEN, 40);
        TEST("tui_ruler(NULL) no crash", true);
        tui_ruler("", TUI_GREEN, 40);
        TEST("tui_ruler(\"\") no crash", true);
        tui_ruler("long", TUI_GREEN, 0);
        TEST("tui_ruler width=0 no crash", true);
        tui_box(NULL, "content", 30);
        TEST("tui_box(NULL title) no crash", true);
        tui_box("title", "content", 0);
        TEST("tui_box width=0 no crash", true);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All TUI tests PASSED");
    return failures ? 1 : 0;
}
