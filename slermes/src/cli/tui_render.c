/*
 * tui_render.c — TUI render engine with virtual screen, double
 * buffering, dirty-rect tracking, and markdown rendering.
 *
 * MIT License — WuBu Hermes Project
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <curses.h>

#include "tui_render.h"

/* ── Role color table (color pair indices for each role) ── */
static const int s_role_colors[TUI_ROLE_COUNT] = {
    1,   /* DEFAULT   → white on default */
    2,   /* USER      → green */
    3,   /* ASSISTANT → cyan */
    4,   /* SYSTEM    → yellow */
    5,   /* TOOL      → magenta */
    6,   /* INFO      → blue */
    7,   /* ERROR     → red */
    8,   /* DEBUG     → bright black (dim) */
};

/* ── API: Role color ── */
int tui_render_role_color(tui_role_t role) {
    if ((unsigned)role >= TUI_ROLE_COUNT) return s_role_colors[0];
    return s_role_colors[role];
}

/* ── Internal: mark cell dirty ── */
static void mark_dirty(tui_render_t *r, int y, int x) {
    if (y < 0 || y >= r->rows || x < 0 || x >= r->cols) return;
    r->cells[y * r->cols + x].dirty = true;
    /* Expand dirty bounding box */
    if (!r->has_dirty) {
        r->dirty_min_y = r->dirty_max_y = y;
        r->dirty_min_x = r->dirty_max_x = x;
        r->has_dirty = true;
    } else {
        if (y < r->dirty_min_y) r->dirty_min_y = y;
        if (y > r->dirty_max_y) r->dirty_max_y = y;
        if (x < r->dirty_min_x) r->dirty_min_x = x;
        if (x > r->dirty_max_x) r->dirty_max_x = x;
    }
}

/* ── Internal: set cell content ── */
static void set_cell(tui_render_t *r, int y, int x, char ch) {
    if (y < 0 || y >= r->rows || x < 0 || x >= r->cols) return;
    tui_render_cell_t *cell = &r->cells[y * r->cols + x];
    if (cell->ch != ch || cell->color_pair != r->color_pair || cell->attrs != r->attrs) {
        cell->ch = ch;
        cell->color_pair = r->color_pair;
        cell->attrs = r->attrs;
        mark_dirty(r, y, x);
    }
}

/* ── Internal: scroll content up by one line ── */
static void scroll_up(tui_render_t *r) {
    /* Move all rows up by 1 */
    int row_bytes = r->cols * sizeof(tui_render_cell_t);
    memmove(r->cells, r->cells + r->cols, (r->rows - 1) * row_bytes);
    /* Clear last row */
    memset(r->cells + (r->rows - 1) * r->cols, 0, row_bytes);
    /* Mark all cells as dirty */
    for (int x = 0; x < r->cols; x++)
        mark_dirty(r, r->rows - 1, x);
}

/* ── API: Create ── */
tui_render_t *tui_render_new(int cols, int rows) {
    if (cols < 1) cols = 80;
    if (rows < 1) rows = 24;

    tui_render_t *r = (tui_render_t *)calloc(1, sizeof(tui_render_t));
    if (!r) return NULL;

    r->cols = cols;
    r->rows = rows;
    r->cursor_y = 0;
    r->cursor_x = 0;
    r->scroll_y = 0;
    r->color_pair = -1;
    r->attrs = 0;
    r->align = TUI_ALIGN_LEFT;

    /* Cell buffer */
    r->cells = (tui_render_cell_t *)calloc((size_t)(cols * rows), sizeof(tui_render_cell_t));
    if (!r->cells) { free(r); return NULL; }

    return r;
}

/* ── API: Free ── */
void tui_render_free(tui_render_t *r) {
    if (!r) return;
    free(r->cells);
    for (int i = 0; i < r->scrollback_rows; i++)
        free(r->scrollback[i]);
    free(r->scrollback);
    free(r->line_starts);
    free(r);
}

/* ── API: Resize ── */
void tui_render_resize(tui_render_t *r, int cols, int rows) {
    if (!r || cols < 1 || rows < 1) return;

    tui_render_cell_t *new_cells = (tui_render_cell_t *)calloc(
        (size_t)(cols * rows), sizeof(tui_render_cell_t));
    if (!new_cells) return;

    /* Copy old content where it fits */
    int copy_rows = rows < r->rows ? rows : r->rows;
    int copy_cols = cols < r->cols ? cols : r->cols;
    for (int y = 0; y < copy_rows; y++) {
        for (int x = 0; x < copy_cols; x++) {
            new_cells[y * cols + x] = r->cells[y * r->cols + x];
        }
    }

    free(r->cells);
    r->cells = new_cells;
    r->cols = cols;
    r->rows = rows;

    /* Clamp cursor */
    if (r->cursor_y >= rows) r->cursor_y = rows - 1;
    if (r->cursor_x >= cols) r->cursor_x = cols - 1;

    r->has_dirty = true;
    r->dirty_min_y = 0;
    r->dirty_max_y = rows - 1;
    r->dirty_min_x = 0;
    r->dirty_max_x = cols - 1;
}

/* ── API: Clear ── */
void tui_render_clear(tui_render_t *r) {
    if (!r) return;
    memset(r->cells, 0, (size_t)(r->cols * r->rows) * sizeof(tui_render_cell_t));
    r->cursor_y = 0;
    r->cursor_x = 0;
    r->has_dirty = true;
    r->dirty_min_y = 0;
    r->dirty_max_y = r->rows - 1;
    r->dirty_min_x = 0;
    r->dirty_max_x = r->cols - 1;
}

/* ── API: Set color ── */
void tui_render_set_color(tui_render_t *r, int color_pair) {
    if (r) r->color_pair = color_pair;
}

/* ── API: Set attrs ── */
void tui_render_set_attrs(tui_render_t *r, int attrs) {
    if (r) r->attrs = attrs;
}

/* ── API: Set align ── */
void tui_render_set_align(tui_render_t *r, tui_align_t align) {
    if (r) r->align = align;
}

/* ── API: Put character ── */
void tui_render_putchar(tui_render_t *r, char ch) {
    if (!r) return;

    if (ch == '\n') {
        tui_render_newline(r);
        return;
    }

    if (ch == '\r') {
        r->cursor_x = 0;
        return;
    }

    /* Handle wrapping */
    if (r->cursor_x >= r->cols) {
        r->cursor_x = 0;
        r->cursor_y++;
    }

    /* Scroll if needed */
    while (r->cursor_y >= r->rows) {
        scroll_up(r);
        r->cursor_y = r->rows - 1;
    }

    set_cell(r, r->cursor_y, r->cursor_x, ch);
    r->cursor_x++;
}

/* ── API: Write string ── */
void tui_render_write(tui_render_t *r, const char *text) {
    if (!r || !text) return;
    for (const char *p = text; *p; p++) {
        tui_render_putchar(r, *p);
    }
}

/* ── API: Printf ── */
void tui_render_printf(tui_render_t *r, const char *fmt, ...) {
    if (!r || !fmt) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    tui_render_write(r, buf);
}

/* ── API: Newline ── */
void tui_render_newline(tui_render_t *r) {
    if (!r) return;
    r->cursor_x = 0;
    r->cursor_y++;
    while (r->cursor_y >= r->rows) {
        scroll_up(r);
        r->cursor_y = r->rows - 1;
    }
}

/* ── API: Blank lines ── */
void tui_render_blank_lines(tui_render_t *r, int count) {
    for (int i = 0; i < count; i++)
        tui_render_newline(r);
}

/* ── API: Horizontal rule ── */
void tui_render_hr(tui_render_t *r, char ch) {
    if (!r) return;
    int old_attrs = r->attrs;
    r->attrs = r->attrs | TUI_ATTR_DIM;
    for (int x = 0; x < r->cols; x++)
        tui_render_putchar(r, ch);
    r->attrs = old_attrs;
    tui_render_newline(r);
}

/* ── API: Markdown render ── */
void tui_render_markdown(tui_render_t *r, const char *text, tui_role_t role) {
    if (!r || !text) return;

    int base_pair = tui_render_role_color(role);
    int hl_pair = 9;    /* highlight pair */
    int dim_pair = 10;  /* dim pair */

    bool in_bold = false;
    bool in_code = false;
    bool in_header = false;
    int col = 0;

    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            /* End formatting at newline */
            if (in_bold) { r->attrs &= ~TUI_ATTR_BOLD; in_bold = false; }
            if (in_header) { r->attrs &= ~TUI_ATTR_BOLD; in_header = false; }
            tui_render_newline(r);
            col = 0;
            if (p[1] == '#') in_header = true;
            continue;
        }

        /* Detect markdown patterns */
        if (col == 0 && *p == '#' && role != TUI_ROLE_ERROR) {
            in_header = true;
            r->color_pair = base_pair;
            r->attrs |= TUI_ATTR_BOLD;
            continue;
        }
        if (in_header && *p == '#') continue;
        if (in_header && *p == ' ') continue;

        if (*p == '`' && !in_bold) {
            if (in_code) {
                r->color_pair = base_pair;
                in_code = false;
            } else {
                r->color_pair = hl_pair;
                in_code = true;
            }
            continue;
        }

        /* Bold: **text** */
        if (*p == '*' && *(p+1) == '*' && col > 0) {
            if (in_bold) {
                r->attrs &= ~TUI_ATTR_BOLD;
                in_bold = false;
            } else {
                r->attrs |= TUI_ATTR_BOLD;
                in_bold = true;
            }
            p++;
            continue;
        }

        /* Code block lines (indented) */
        if (col == 0 && *p == ' ' && role != TUI_ROLE_ERROR) {
            r->color_pair = dim_pair;
            tui_render_putchar(r, *p);
            r->color_pair = base_pair;
            col++;
            continue;
        }

        /* Write character */
        if (in_header) {
            r->color_pair = base_pair;
            r->attrs |= TUI_ATTR_BOLD;
            tui_render_putchar(r, *p);
            r->attrs &= ~TUI_ATTR_BOLD;
        } else if (in_code) {
            r->color_pair = hl_pair;
            tui_render_putchar(r, *p);
            r->color_pair = base_pair;
        } else {
            r->color_pair = base_pair;
            tui_render_putchar(r, *p);
        }
        col++;
    }

    /* End formatting */
    r->attrs &= ~TUI_ATTR_BOLD;
    r->color_pair = base_pair;
}

/* ── API: Mark all dirty ── */
void tui_render_mark_all_dirty(tui_render_t *r) {
    if (!r) return;
    if (r->has_dirty) return; /* Already fully dirty */
    r->has_dirty = true;
    r->dirty_min_y = 0;
    r->dirty_max_y = r->rows - 1;
    r->dirty_min_x = 0;
    r->dirty_max_x = r->cols - 1;
}

/* ── API: Flush to ncurses window ── */
int tui_render_flush(tui_render_t *r, void *ncurses_window,
                      int offset_y, int offset_x) {
    if (!r || !ncurses_window) return 0;

    WINDOW *win = (WINDOW *)ncurses_window;
    int updated = 0;

    if (!r->has_dirty) return 0;

    /* Flush only dirty region */
    for (int y = r->dirty_min_y; y <= r->dirty_max_y && y < r->rows; y++) {
        /* Find first dirty cell in row */
        bool row_has_dirty = false;

        for (int x = r->dirty_min_x; x <= r->dirty_max_x && x < r->cols; x++) {
            tui_render_cell_t *cell = &r->cells[y * r->cols + x];
            if (!cell->dirty) continue;

            if (!row_has_dirty) {
                row_has_dirty = true;
                wmove(win, offset_y + y, offset_x + x);
            }

            /* Apply attributes */
            if (cell->color_pair > 0)
                wattron(win, COLOR_PAIR(cell->color_pair));
            if (cell->attrs & TUI_ATTR_BOLD)
                wattron(win, A_BOLD);
            if (cell->attrs & TUI_ATTR_DIM)
                wattron(win, A_DIM);
            if (cell->attrs & TUI_ATTR_UNDERLINE)
                wattron(win, A_UNDERLINE);
            if (cell->attrs & TUI_ATTR_REVERSE)
                wattron(win, A_REVERSE);

            waddch(win, cell->ch ? cell->ch : ' ');

            /* Clean up attributes */
            if (cell->color_pair > 0)
                wattroff(win, COLOR_PAIR(cell->color_pair));
            wattroff(win, A_BOLD | A_DIM | A_UNDERLINE | A_REVERSE);

            cell->dirty = false;
            updated++;
        }
    }

    r->has_dirty = false;
    return updated;
}

/* ── API: Cursor queries ── */
int tui_render_cursor_y(const tui_render_t *r) { return r ? r->cursor_y : 0; }
int tui_render_cursor_x(const tui_render_t *r) { return r ? r->cursor_x : 0; }
int tui_render_cols(const tui_render_t *r) { return r ? r->cols : 0; }
int tui_render_rows(const tui_render_t *r) { return r ? r->rows : 0; }
bool tui_render_has_dirty(const tui_render_t *r) { return r ? r->has_dirty : false; }
int tui_render_content_width(const tui_render_t *r) { return r ? r->cols : 0; }
