#ifndef TUI_LAYOUT_H
#define TUI_LAYOUT_H

/*
 * tui_layout.h — App layout + chrome engine for the TUI.
 *
 * Provides configurable pane layout management with chrome decorations
 * (headers, borders, separators), responsive breakpoints, and pane
 * constraints. Replaces hardcoded inline layout math in tui_fullscreen.c.
 *
 * Usage:
 *   tui_layout_t layout;
 *   tui_layout_init(&layout, 80, 24);
 *   tui_layout_add_pane(&layout, "history", TUI_PANE_FILL, ...);
 *   tui_layout_set_mode(&layout, TUI_LAYOUT_NORMAL);
 *   tui_layout_calculate(&layout);
 *   for (int i = 0; i < layout.pane_count; i++)
 *       mvwin(layout.panes[i].win, ...);
 */

#include <stdbool.h>
#include <stddef.h>

/* ── Maximum panes ── */
#define TUI_LAYOUT_MAX_PANES 8

/* ── Layout modes ── */
typedef enum {
    TUI_LAYOUT_NORMAL,    /* Full: history left/center, tool feed right, input bottom */
    TUI_LAYOUT_MOBILE,    /* Thin: stacked vertically, tool feed compact */
    TUI_LAYOUT_COMPACT,   /* Minimal: history + input only, no tool feed */
    TUI_LAYOUT_WIDE,      /* Extra-wide: history left, tool feed right, input bottom */
} tui_layout_mode_t;

/* ── Pane sizing policy ── */
typedef enum {
    TUI_PANE_FILL,        /* Pane fills remaining space */
    TUI_PANE_FIXED,       /* Fixed number of rows/cols */
    TUI_PANE_RATIO,       /* Ratio of total terminal size */
    TUI_PANE_AUTO,        /* Automatically sized based on content */
} tui_pane_size_t;

/* ── Pane position (side of parent) ── */
typedef enum {
    TUI_PANE_TOP,
    TUI_PANE_BOTTOM,
    TUI_PANE_LEFT,
    TUI_PANE_RIGHT,
    TUI_PANE_CENTER,
} tui_pane_side_t;

/* ── Chrome decoration flags ── */
typedef enum {
    TUI_CHROME_NONE     = 0,
    TUI_CHROME_HEADER   = 1 << 0,   /* Title bar at top */
    TUI_CHROME_BORDER   = 1 << 1,   /* Box border around pane */
    TUI_CHROME_SEPARATOR = 1 << 2,  /* Line/separator adjacent to pane */
    TUI_CHROME_SCROLL   = 1 << 3,   /* Scroll indicator */
    TUI_CHROME_FOOTER   = 1 << 4,   /* Status footer at bottom */
    TUI_CHROME_ALL      = 0xFF,
} tui_chrome_flags_t;

/* ── Pane definition ── */
typedef struct {
    char            name[32];       /* Human-readable pane name */
    tui_pane_side_t side;           /* Which side of parent */
    tui_pane_size_t sizing;         /* Sizing policy */
    double          ratio;          /* Ratio (0.0-1.0, for RATIO sizing) */
    int             fixed_size;     /* Fixed rows/cols (for FIXED sizing) */
    int             min_size;       /* Minimum rows/cols */
    int             max_size;       /* Maximum rows/cols */
    tui_chrome_flags_t chrome;      /* Chrome decoration flags */
    char            title[64];      /* Header title (if CHROME_HEADER) */

    /* Calculated position (filled by tui_layout_calculate) */
    int             y, x;
    int             rows, cols;
    bool            visible;
} tui_layout_pane_t;

/* ── Layout instance ── */
typedef struct {
    tui_layout_mode_t mode;         /* Current layout mode */
    int               term_rows;    /* Terminal dimensions */
    int               term_cols;
    int               pane_count;   /* Number of registered panes */
    tui_layout_pane_t panes[TUI_LAYOUT_MAX_PANES];
} tui_layout_t;

/* ── API ── */

/* Initialize layout with terminal dimensions. */
void tui_layout_init(tui_layout_t *l, int term_rows, int term_cols);

/* Set current layout mode. */
void tui_layout_set_mode(tui_layout_t *l, tui_layout_mode_t mode);

/* Get layout mode name. */
const char *tui_layout_mode_name(tui_layout_mode_t mode);

/* Add a pane to the layout.
 * Returns index (0-based), or -1 on error (max panes reached).
 */
int tui_layout_add_pane(tui_layout_t *l,
                         const char *name,
                         tui_pane_side_t side,
                         tui_pane_size_t sizing,
                         double ratio,
                         int fixed_size,
                         int min_size,
                         int max_size,
                         tui_chrome_flags_t chrome,
                         const char *title);

/* Set chrome flags for a pane by index. */
void tui_layout_set_chrome(tui_layout_t *l, int pane_idx, tui_chrome_flags_t chrome);

/* Set pane title. */
void tui_layout_set_title(tui_layout_t *l, int pane_idx, const char *title);

/* Recalculate all pane positions based on terminal dimensions and mode.
 * Call after changing mode, terminal size, or pane config.
 */
void tui_layout_calculate(tui_layout_t *l);

/* Update terminal dimensions and recalculate. */
void tui_layout_resize(tui_layout_t *l, int term_rows, int term_cols);

/* Find pane index by name. Returns -1 if not found. */
int tui_layout_find_pane(const tui_layout_t *l, const char *name);

/* Check if a pane has visible area. */
bool tui_layout_pane_visible(const tui_layout_t *l, int pane_idx);

/* Get total area consumed by chrome decorations for a pane.
 * Returns: total rows/cols consumed by header + border + footer.
 */
int tui_layout_chrome_rows(const tui_layout_t *l, int pane_idx);
int tui_layout_chrome_cols(const tui_layout_t *l, int pane_idx);

/* Get pane by index (or NULL if out of range). */
const tui_layout_pane_t *tui_layout_get_pane(const tui_layout_t *l, int pane_idx);

/* Get number of panes. */
int tui_layout_pane_count(const tui_layout_t *l);

#endif /* TUI_LAYOUT_H */
