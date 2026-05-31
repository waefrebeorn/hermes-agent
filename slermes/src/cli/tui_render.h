#ifndef TUI_RENDER_H
#define TUI_RENDER_H

/*
 * tui_render.h — TUI render engine with virtual screen, double
 * buffering, dirty-rect tracking, and markdown rendering.
 *
 * Provides an abstract render surface that decouples layout from
 * painting. Supports text wrapping, alignment, color/attribute
 * stacks, and optimized incremental redraws.
 *
 * Usage:
 *   tui_render_t *r = tui_render_new(80, 24);
 *   tui_render_set_fg(r, TUI_COLOR_WHITE);
 *   tui_render_write(r, "Hello, world!");
 *   tui_render_newline(r);
 *   tui_render_flush(r, stdscr, 0, 0);
 *   tui_render_free(r);
 */

#include <stdbool.h>
#include <stddef.h>

/* ── Named colors (8 standard + 8 bright) ── */
typedef enum {
    TUI_COLOR_DEFAULT   = -1,
    TUI_COLOR_BLACK     = 0,
    TUI_COLOR_RED,
    TUI_COLOR_GREEN,
    TUI_COLOR_YELLOW,
    TUI_COLOR_BLUE,
    TUI_COLOR_MAGENTA,
    TUI_COLOR_CYAN,
    TUI_COLOR_WHITE,
    /* Bright variants */
    TUI_COLOR_BRIGHT_BLACK,
    TUI_COLOR_BRIGHT_RED,
    TUI_COLOR_BRIGHT_GREEN,
    TUI_COLOR_BRIGHT_YELLOW,
    TUI_COLOR_BRIGHT_BLUE,
    TUI_COLOR_BRIGHT_MAGENTA,
    TUI_COLOR_BRIGHT_CYAN,
    TUI_COLOR_BRIGHT_WHITE,
} tui_color_t;

/* ── Named attributes ── */
typedef enum {
    TUI_ATTR_NONE     = 0,
    TUI_ATTR_BOLD     = 1 << 0,
    TUI_ATTR_DIM      = 1 << 1,
    TUI_ATTR_UNDERLINE = 1 << 2,
    TUI_ATTR_REVERSE  = 1 << 3,
    TUI_ATTR_BLINK    = 1 << 4,
} tui_attr_t;

/* ── Horizontal alignment ── */
typedef enum {
    TUI_ALIGN_LEFT,
    TUI_ALIGN_CENTER,
    TUI_ALIGN_RIGHT,
} tui_align_t;

/* ── Markdown role (for role-colored rendering) ── */
typedef enum {
    TUI_ROLE_DEFAULT,
    TUI_ROLE_USER,
    TUI_ROLE_ASSISTANT,
    TUI_ROLE_SYSTEM,
    TUI_ROLE_TOOL,
    TUI_ROLE_INFO,
    TUI_ROLE_ERROR,
    TUI_ROLE_DEBUG,
    TUI_ROLE_COUNT,
} tui_role_t;

/* ── A single cell on the virtual screen ── */
typedef struct {
    char    ch;          /* Character (0 = empty/no change) */
    int     color_pair;  /* NCURSES color pair index */
    int     attrs;       /* NCURSES attribute flags */ /* Actually tui_attr_t but ncurses uses int */
    bool    dirty;       /* Cell changed since last flush */
} tui_render_cell_t;

/* ── Render surface (virtual screen) ── */
typedef struct {
    int     cols;          /* Width of surface */
    int     rows;          /* Height of surface */
    int     cursor_y;      /* Current write cursor Y */
    int     cursor_x;      /* Current write cursor X */
    int     scroll_y;      /* Scroll offset for buffered content */
    int     color_pair;    /* Current color pair index */
    int     attrs;         /* Current attribute stack */
    tui_align_t align;     /* Current alignment */

    /* Cell buffer (rows × cols) */
    tui_render_cell_t *cells;

    /* Scrollback buffer (rows above visible area) */
    char  **scrollback;
    int     scrollback_rows;
    int     scrollback_cap;
    int     scrollback_pos;

    /* Line tracking for wrapped lines */
    int    *line_starts;   /* Row indices where lines start */
    int     line_count;
    int     line_cap;

    /* Optimisation: bounding box of dirty cells */
    int     dirty_min_y, dirty_max_y;
    int     dirty_min_x, dirty_max_x;
    bool    has_dirty;
} tui_render_t;

/* ── API: Lifecycle ── */

/* Create a new render surface with given dimensions. */
tui_render_t *tui_render_new(int cols, int rows);

/* Free render surface and all resources. */
void tui_render_free(tui_render_t *r);

/* Resize the render surface (preserves content where possible). */
void tui_render_resize(tui_render_t *r, int cols, int rows);

/* Clear the render surface (reset all cells to empty). */
void tui_render_clear(tui_render_t *r);

/* ── API: Drawing ── */

/* Set current color pair index (0-63, or -1 for default). */
void tui_render_set_color(tui_render_t *r, int color_pair);

/* Set attributes (bitmask of tui_attr_t, or 0 for none). */
void tui_render_set_attrs(tui_render_t *r, int attrs);

/* Set alignment for subsequent writes. */
void tui_render_set_align(tui_render_t *r, tui_align_t align);

/* Write a single character at current cursor, advancing. */
void tui_render_putchar(tui_render_t *r, char ch);

/* Write a string at current cursor, wrapping at surface width. */
void tui_render_write(tui_render_t *r, const char *text);

/* Write a formatted string. */
void tui_render_printf(tui_render_t *r, const char *fmt, ...);

/* Advance to next line (scrolls if at bottom). */
void tui_render_newline(tui_render_t *r);

/* Insert blank lines. */
void tui_render_blank_lines(tui_render_t *r, int count);

/* Draw a horizontal rule across the surface. */
void tui_render_hr(tui_render_t *r, char ch);

/* ── API: Markdown rendering ── */

/* Render markdown text with role-based coloring.
 * Supports: **bold**, `code`, # headers, indented code blocks.
 */
void tui_render_markdown(tui_render_t *r, const char *text, tui_role_t role);

/* Get the color pair index for a given role. */
int tui_render_role_color(tui_role_t role);

/* ── API: Flushing ── */

/* Mark entire surface as dirty (force full redraw). */
void tui_render_mark_all_dirty(tui_render_t *r);

/* Flush dirty cells to an ncurses window.
 * Returns the number of cells updated.
 * ncurses_window is a void* to avoid ncurses include dependency
 * in the header. Caller passes a WINDOW* from ncurses.
 */
int tui_render_flush(tui_render_t *r, void *ncurses_window,
                      int offset_y, int offset_x);

/* ── API: Queries ── */

/* Get current cursor row. */
int tui_render_cursor_y(const tui_render_t *r);

/* Get current cursor column. */
int tui_render_cursor_x(const tui_render_t *r);

/* Get surface dimensions. */
int tui_render_cols(const tui_render_t *r);
int tui_render_rows(const tui_render_t *r);

/* Check if surface has any pending changes. */
bool tui_render_has_dirty(const tui_render_t *r);

/* Get content width (for wrapping/external use). */
int tui_render_content_width(const tui_render_t *r);

#endif /* TUI_RENDER_H */
