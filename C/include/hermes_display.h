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
    char *label;
    bool  active;
} display_spinner_t;

void display_spinner_start(display_spinner_t *sp, const char *label);
void display_spinner_tick(display_spinner_t *sp);
void display_spinner_stop(display_spinner_t *sp, const char *done_msg);

/* Print a box/panel around text */
void display_panel(const char *title, const char *content, display_color_t color);

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
