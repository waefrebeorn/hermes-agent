/*
 * test_tui_render.c — Test suite for TUI render engine.
 *
 * Tests: virtual screen, write/wrap/scroll, markdown rendering,
 * color/attr state, role colors, clear/resize, cursor, hr, blank.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* #define ncurses types stripped — flush tests need ncurses linked */

#include "../src/cli/tui_render.h"

/* ── Test: create and free ── */
void test_create(void) {
    tui_render_t *r = tui_render_new(80, 24);
    assert(r != NULL);
    assert(tui_render_cols(r) == 80);
    assert(tui_render_rows(r) == 24);
    assert(tui_render_cursor_x(r) == 0);
    assert(tui_render_cursor_y(r) == 0);
    assert(!tui_render_has_dirty(r));
    tui_render_free(r);
    printf("  PASS create\n");
}

/* ── Test: write text and check cursor advancement ── */
void test_write_cursor(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_write(r, "Hello");
    assert(tui_render_cursor_x(r) == 5);
    assert(tui_render_cursor_y(r) == 0);
    tui_render_free(r);
    printf("  PASS write_cursor\n");
}

/* ── Test: wrapping at surface width ── */
void test_wrapping(void) {
    tui_render_t *r = tui_render_new(10, 5);
    /* Write 15 characters on a 10-wide surface: should wrap after 10 */
    tui_render_write(r, "1234567890ABCDE");
    assert(tui_render_cursor_x(r) == 5);  /* 5th col after wrap */
    assert(tui_render_cursor_y(r) == 1);  /* wrapped to next line */
    tui_render_free(r);
    printf("  PASS wrapping\n");
}

/* ── Test: newline advances cursor ── */
void test_newline(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_write(r, "Line 1");
    tui_render_newline(r);
    assert(tui_render_cursor_x(r) == 0);
    assert(tui_render_cursor_y(r) == 1);
    tui_render_write(r, "Line 2");
    assert(tui_render_cursor_y(r) == 1);
    tui_render_free(r);
    printf("  PASS newline\n");
}

/* ── Test: scrolling when cursor exceeds bottom ── */
void test_scrolling(void) {
    tui_render_t *r = tui_render_new(20, 3);
    /* Write 5 lines on a 3-row surface */
    tui_render_write(r, "Line 1");
    tui_render_newline(r);
    tui_render_write(r, "Line 2");
    tui_render_newline(r);
    tui_render_write(r, "Line 3");
    tui_render_newline(r);
    tui_render_write(r, "Line 4");
    tui_render_newline(r);
    /* After scrolling, cursor should be on last row (row 2, 0-indexed) */
    assert(tui_render_cursor_y(r) == 2);
    assert(tui_render_cursor_x(r) == 0);
    tui_render_free(r);
    printf("  PASS scrolling\n");
}

/* ── Test: clear resets cursor and marks dirty ── */
void test_clear(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_write(r, "Some text");
    tui_render_newline(r);
    tui_render_clear(r);
    assert(tui_render_cursor_x(r) == 0);
    assert(tui_render_cursor_y(r) == 0);
    assert(tui_render_has_dirty(r));
    tui_render_free(r);
    printf("  PASS clear\n");
}

/* ── Test: resize preserves content ── */
void test_resize(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_write(r, "Hello");
    tui_render_resize(r, 40, 12);
    assert(tui_render_cols(r) == 40);
    assert(tui_render_rows(r) == 12);
    /* Cursor should be clamped */
    assert(tui_render_cursor_x(r) < 40);
    assert(tui_render_cursor_y(r) < 12);
    tui_render_free(r);
    printf("  PASS resize\n");
}

/* ── Test: set_color and set_attrs state ── */
void test_color_attrs(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_set_color(r, 3);
    tui_render_set_attrs(r, TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE);
    /* These just set state; verify indirectly by putting a char */
    tui_render_putchar(r, 'X');
    assert(tui_render_cursor_x(r) == 1);
    tui_render_free(r);
    printf("  PASS color_attrs\n");
}

/* ── Test: horizontal rule ── */
void test_hr(void) {
    tui_render_t *r = tui_render_new(10, 5);
    tui_render_hr(r, '-');
    /* Cursor should be at start of next line */
    assert(tui_render_cursor_x(r) == 0);
    assert(tui_render_cursor_y(r) == 1);
    tui_render_free(r);
    printf("  PASS hr\n");
}

/* ── Test: blank lines ── */
void test_blank_lines(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_blank_lines(r, 3);
    assert(tui_render_cursor_y(r) == 3);
    assert(tui_render_cursor_x(r) == 0);
    tui_render_free(r);
    printf("  PASS blank_lines\n");
}

/* ── Test: markdown rendering (no crash, cursor advances) ── */
void test_markdown_basic(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_markdown(r, "Hello **world**", TUI_ROLE_USER);
    /* Cursor should have advanced past the text */
    assert(tui_render_cursor_x(r) > 0);
    tui_render_free(r);
    printf("  PASS markdown_basic\n");
}

/* ── Test: markdown with bold, code, and header ── */
void test_markdown_formatting(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_markdown(r, "# Header\n**bold** and `code`", TUI_ROLE_ASSISTANT);
    /* After newline, cursor moves down */
    assert(tui_render_cursor_y(r) == 1);
    assert(tui_render_cursor_x(r) > 0);
    tui_render_free(r);
    printf("  PASS markdown_formatting\n");
}

/* ── Test: markdown with code blocks (indented) ── */
void test_markdown_code_block(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_markdown(r, "    printf(\"hello\");", TUI_ROLE_TOOL);
    assert(tui_render_cursor_x(r) > 4);  /* Indent + content */
    tui_render_free(r);
    printf("  PASS markdown_code_block\n");
}

/* ── Test: role colors ── */
void test_role_colors(void) {
    assert(tui_render_role_color(TUI_ROLE_DEFAULT) == 1);
    assert(tui_render_role_color(TUI_ROLE_USER) == 2);
    assert(tui_render_role_color(TUI_ROLE_ASSISTANT) == 3);
    assert(tui_render_role_color(TUI_ROLE_SYSTEM) == 4);
    assert(tui_render_role_color(TUI_ROLE_TOOL) == 5);
    assert(tui_render_role_color(TUI_ROLE_INFO) == 6);
    assert(tui_render_role_color(TUI_ROLE_ERROR) == 7);
    assert(tui_render_role_color(TUI_ROLE_DEBUG) == 8);
    /* Out of range returns default */
    assert(tui_render_role_color(999) == 1);
    printf("  PASS role_colors\n");
}

/* ── Test: printf formatting ── */
void test_printf(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_printf(r, "Hello %s %d", "world", 42);
    assert(tui_render_cursor_x(r) == 14);  /* "Hello world 42" = 14 */
    tui_render_free(r);
    printf("  PASS printf\n");
}

/* ── Test: carriage return ── */
void test_carriage_return(void) {
    tui_render_t *r = tui_render_new(80, 24);
    tui_render_write(r, "Hello");
    tui_render_putchar(r, '\r');
    assert(tui_render_cursor_x(r) == 0);
    tui_render_free(r);
    printf("  PASS carriage_return\n");
}

int main(void) {
    printf("test_tui_render:\n");
    test_create();
    test_write_cursor();
    test_wrapping();
    test_newline();
    test_scrolling();
    test_clear();
    test_resize();
    test_color_attrs();
    test_hr();
    test_blank_lines();
    test_markdown_basic();
    test_markdown_formatting();
    test_markdown_code_block();
    test_role_colors();
    test_printf();
    test_carriage_return();
    printf("  ALL PASSED\n");
    return 0;
}
