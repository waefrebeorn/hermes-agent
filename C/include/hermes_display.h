/**
 * @defgroup hermes_display Display
 * @brief Terminal output formatting.
 *
 *
ANSI color, spinner/progress display, structured output.
Used by CLI and TUI for status messages and activity
feedback.
 *
 * @{
 */
#ifndef HERMES_DISPLAY_H
#define HERMES_DISPLAY_H

/*
 * hermes_display.h — Terminal display for Hermes C.
 * Uses ANSI escape codes. No ncurses dependency.
 * Supports: colors, progress bars, spinners, panels.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Terminal color codes */
typedef enum {
    DISPLAY_BLACK   = 0,
    DISPLAY_RED     = 1,
    DISPLAY_GREEN   = 2,
    DISPLAY_YELLOW  = 3,
    DISPLAY_BLUE    = 4,
    DISPLAY_MAGENTA = 5,
    DISPLAY_CYAN    = 6,
    DISPLAY_WHITE   = 7,
    DISPLAY_DEFAULT = 9,
} display_color_t;

/* Style flags */
typedef enum {
    DISPLAY_NORMAL      = 0,
    DISPLAY_BOLD        = 1 << 0,
    DISPLAY_DIM         = 1 << 1,
    DISPLAY_ITALIC      = 1 << 2,
    DISPLAY_UNDERLINE   = 1 << 3,
} display_style_t;

/* Initialize terminal (check if output is a TTY) */
void display_init(void);

/* Set text color and style */
void display_set_fg(display_color_t color);
void display_set_bg(display_color_t color);
void display_set_style(display_style_t style);
void display_reset(void);

/* Set truecolor (24-bit) foreground/background */
void display_set_fg_rgb(int r, int g, int b);
void display_set_bg_rgb(int r, int g, int b);

/* Print formatted text with truecolor foreground */
void display_printf_hex(const char *hex_fg, display_style_t style,
                        const char *fmt, ...);

/* Print colored text */
void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...);

/* Clear screen */
void display_clear(void);

/* Move cursor */
void display_goto(int row, int col);
void display_save_pos(void);
void display_restore_pos(void);

/* Progress bar */
typedef struct {
    int    width;
    int    current;
    int    total;
    char   label[64];
} display_progress_t;

void display_progress_init(display_progress_t *bar, const char *label, int total);
void display_progress_update(display_progress_t *bar, int current);
void display_progress_done(display_progress_t *bar);

/* Spinner */
typedef struct {
    int   frame;
    int   frame_count;
    char *label;
    char *face;           /* current kawaii face string (malloc'd) */
    bool  active;
} display_spinner_t;

void display_spinner_start(display_spinner_t *sp, const char *label);
void display_spinner_tick(display_spinner_t *sp);
void display_spinner_stop(display_spinner_t *sp, const char *done_msg);

/* Kawaii spinner with animated face cycles + verb + wings */
typedef struct {
    int    frame;
    char  *label;
    char   face[64];       /* current face emoji/kaomoji */
    char   verb[64];       /* current thinking verb (empty for waiting) */
    char   wing_left[16];  /* left wing decoration */
    char   wing_right[16]; /* right wing decoration */
    bool   active;
    bool   thinking;       /* true=thinking faces, false=waiting faces */
    double start_time;     /* seconds since epoch */
} display_kawaii_t;

void display_kawaii_start(display_kawaii_t *sp, const char *label, bool thinking);
void display_kawaii_tick(display_kawaii_t *sp);
void display_kawaii_stop(display_kawaii_t *sp, const char *done_msg);

/* Set the display skin for skin-driven styling. Pass NULL to unset. */
void display_set_skin(void *skin);

/* Tool activity feed — build one-line preview from tool name + args JSON */
/* Returns malloc'd string (caller free) or NULL if no preview possible. */
char *display_tool_preview(const char *tool_name, const char *args_json);

/* Render a unified diff with ANSI color for inline display */
/* Returns malloc'd string (caller free). */
char *display_inline_diff(const char *diff_text);

/* Print a tool activity line with ┊ prefix, emoji, tool name, preview */
void display_tool_activity(const char *tool_name, const char *preview,
                           display_color_t color);

/* Print a box/panel around text (with word-wrap) */
void display_panel(const char *title, const char *content, display_color_t color);

/* Print a panel with TrueColor hex border color */
void display_panel_hex(const char *title, const char *content, const char *border_hex);

/* Horizontal rule with TrueColor hex */
void display_hr_hex(const char *hex_fg);

/* ================================================================
 *  Status Bar
 * ================================================================ */

/* Display a status bar line showing model, session, and context info.
 * Uses skin colors (status_bar_bg, status_bar_text, status_bar_dim)
 * when a skin is active via display_set_skin(). Pass NULL fg_bg to use
 * terminal defaults. */
void display_statusbar(const char *model, const char *session_id,
                       int turn_count, int context_pct);

/* Print an ASCII table with headers and aligned columns.
 * columns: number of columns
 * headers: array of column header strings (NULL for no header)
 * rows: array of strings, each containing tab-separated column values
 * num_rows: number of rows
 * color: color for borders and headers
 */
void display_table(int columns, const char **headers,
                   const char **rows, int num_rows,
                   display_color_t color);

/* Word-wrap text to max_width columns. Returns malloc'd string (caller free). */
char *display_word_wrap(const char *text, int max_width);

/* Print a horizontal rule */
void display_hr(display_color_t color);

/* Check if terminal supports colors */
bool display_has_color(void);

/* Get terminal width */
int display_width(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_display group */
#endif /* HERMES_DISPLAY_H */
