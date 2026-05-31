/*
 * test_tui_layout.c — Test suite for TUI app layout + chrome engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/cli/tui_layout.h"

void test_init(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    assert(l.term_rows == 24);
    assert(l.term_cols == 80);
    assert(l.mode == TUI_LAYOUT_NORMAL);
    assert(l.pane_count == 0);
    printf("  PASS init\n");
}

void test_init_defaults(void) {
    tui_layout_t l;
    tui_layout_init(&l, 0, 0);
    assert(l.term_rows == 24);
    assert(l.term_cols == 80);
    printf("  PASS init_defaults\n");
}

void test_add_pane(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    int idx = tui_layout_add_pane(&l, "history", TUI_PANE_CENTER,
                                   TUI_PANE_FILL, 0, 0, 0, 0,
                                   TUI_CHROME_HEADER, "Messages");
    assert(idx == 0);
    assert(l.pane_count == 1);
    assert(strcmp(l.panes[0].name, "history") == 0);
    assert(l.panes[0].chrome == TUI_CHROME_HEADER);
    printf("  PASS add_pane\n");
}

void test_max_panes(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    for (int i = 0; i < TUI_LAYOUT_MAX_PANES; i++) {
        int idx = tui_layout_add_pane(&l, "p", TUI_PANE_CENTER,
                                       TUI_PANE_FILL, 0, 0, 0, 0,
                                       TUI_CHROME_NONE, NULL);
        assert(idx == i);
    }
    int idx = tui_layout_add_pane(&l, "overflow", TUI_PANE_CENTER,
                                   TUI_PANE_FILL, 0, 0, 0, 0,
                                   TUI_CHROME_NONE, NULL);
    assert(idx == -1);
    printf("  PASS max_panes\n");
}

void test_calculate_center(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_calculate(&l);
    assert(l.panes[0].y == 0);
    assert(l.panes[0].x == 0);
    assert(l.panes[0].rows == 24);
    assert(l.panes[0].cols == 80);
    assert(l.panes[0].visible == true);
    printf("  PASS calculate_center\n");
}

void test_calculate_top_bottom(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "header", TUI_PANE_TOP,
                         TUI_PANE_FIXED, 0, 3, 1, 10,
                         TUI_CHROME_NONE, NULL);
    tui_layout_add_pane(&l, "footer", TUI_PANE_BOTTOM,
                         TUI_PANE_FIXED, 0, 1, 1, 5,
                         TUI_CHROME_NONE, NULL);
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_calculate(&l);
    assert(l.panes[0].y == 0);
    assert(l.panes[0].rows == 3);
    assert(l.panes[1].y == 23);
    assert(l.panes[1].rows == 1);
    assert(l.panes[2].y == 3);
    assert(l.panes[2].rows == 20);
    printf("  PASS calculate_top_bottom\n");
}

void test_calculate_left_right(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "sidebar", TUI_PANE_LEFT,
                         TUI_PANE_FIXED, 0, 20, 10, 40,
                         TUI_CHROME_BORDER, "Files");
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_calculate(&l);
    assert(l.panes[0].x == 0);
    assert(l.panes[0].cols == 20);
    assert(l.panes[0].rows == 24);
    assert(l.panes[1].x == 20);
    assert(l.panes[1].cols == 60);
    printf("  PASS calculate_left_right\n");
}

void test_mode_names(void) {
    assert(strcmp(tui_layout_mode_name(TUI_LAYOUT_NORMAL), "normal") == 0);
    assert(strcmp(tui_layout_mode_name(TUI_LAYOUT_MOBILE), "mobile") == 0);
    assert(strcmp(tui_layout_mode_name(TUI_LAYOUT_COMPACT), "compact") == 0);
    assert(strcmp(tui_layout_mode_name(TUI_LAYOUT_WIDE), "wide") == 0);
    printf("  PASS mode_names\n");
}

void test_set_mode(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_set_mode(&l, TUI_LAYOUT_MOBILE);
    assert(l.mode == TUI_LAYOUT_MOBILE);
    tui_layout_set_mode(&l, TUI_LAYOUT_COMPACT);
    assert(l.mode == TUI_LAYOUT_COMPACT);
    printf("  PASS set_mode\n");
}

void test_resize(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_resize(&l, 40, 100);
    assert(l.term_rows == 40);
    assert(l.term_cols == 100);
    assert(l.panes[0].rows == 40);
    assert(l.panes[0].cols == 100);
    printf("  PASS resize\n");
}

void test_find_pane(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "history", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_HEADER, "Messages");
    assert(tui_layout_find_pane(&l, "history") == 0);
    assert(tui_layout_find_pane(&l, "nonexistent") == -1);
    printf("  PASS find_pane\n");
}

void test_set_chrome_title(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_set_chrome(&l, 0, TUI_CHROME_HEADER | TUI_CHROME_BORDER);
    assert(l.panes[0].chrome == (TUI_CHROME_HEADER | TUI_CHROME_BORDER));
    tui_layout_set_title(&l, 0, "Main Window");
    assert(strcmp(l.panes[0].title, "Main Window") == 0);
    printf("  PASS set_chrome_title\n");
}

void test_pane_count_get(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    assert(tui_layout_pane_count(&l) == 0);
    assert(tui_layout_get_pane(&l, 0) == NULL);
    tui_layout_add_pane(&l, "a", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    assert(tui_layout_pane_count(&l) == 1);
    const tui_layout_pane_t *p = tui_layout_get_pane(&l, 0);
    assert(p != NULL);
    assert(strcmp(p->name, "a") == 0);
    assert(tui_layout_get_pane(&l, 5) == NULL);
    printf("  PASS pane_count_get\n");
}

void test_zero_height_visibility(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "hidden", TUI_PANE_BOTTOM,
                         TUI_PANE_FIXED, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_calculate(&l);
    assert(l.panes[0].visible == false);
    assert(l.panes[1].rows == 24);
    printf("  PASS zero_height_visibility\n");
}

void test_ratio_sizing(void) {
    tui_layout_t l;
    tui_layout_init(&l, 24, 80);
    tui_layout_add_pane(&l, "left", TUI_PANE_LEFT,
                         TUI_PANE_RATIO, 0.25, 0, 5, 0,
                         TUI_CHROME_BORDER, "Nav");
    tui_layout_add_pane(&l, "main", TUI_PANE_CENTER,
                         TUI_PANE_FILL, 0, 0, 0, 0,
                         TUI_CHROME_NONE, NULL);
    tui_layout_calculate(&l);
    assert(l.panes[0].cols == 20);
    assert(l.panes[1].cols == 60);
    printf("  PASS ratio_sizing\n");
}

int main(void) {
    printf("test_tui_layout:\n");
    test_init();
    test_init_defaults();
    test_add_pane();
    test_max_panes();
    test_calculate_center();
    test_calculate_top_bottom();
    test_calculate_left_right();
    test_mode_names();
    test_set_mode();
    test_resize();
    test_find_pane();
    test_set_chrome_title();
    test_pane_count_get();
    test_zero_height_visibility();
    test_ratio_sizing();
    printf("  ALL PASSED\n");
    return 0;
}
