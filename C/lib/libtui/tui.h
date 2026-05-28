#ifndef LIBTUI_H
#define LIBTUI_H

/*
 * libtui.h — Terminal UI library for C.
 * Replaces Python's prompt_toolkit + rich + tqdm + termcolor.
 *
 * MIT License — WuBu Hermes Project
 *
 * Two subsystems:
 *   tui_input  — line input with history, editing, completion
 *   tui_disp   — styled output, progress, tables, boxes
 *
 * Usage (input):
 *   tui_input_t *in = tui_input_new("$ ");
 *   tui_input_history_add(in, "previous command");
 *   const char *line = tui_input_read(in);
 *   tui_input_free(in);
 *
 * Usage (display):
 *   tui_progress_t *p = tui_progress_new("Loading", 100);
 *   for (int i = 0; i <= 100; i++) tui_progress_update(p, i);
 *   tui_progress_done(p);
 *
 *   tui_box("title", "content here", 40);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  INPUT SUBSYSTEM (replaces prompt_toolkit)
 * ==================================================================== */

typedef struct tui_input_t tui_input_t;

/* Create input handler with given prompt string. */
tui_input_t *tui_input_new(const char *prompt);

/* Read a line of input. Returns malloc'd string (caller free). */
char *tui_input_read(tui_input_t *in);

/* Read with history navigation (up/down arrows). */
char *tui_input_read_history(tui_input_t *in);

/* Add line to history. */
void tui_input_history_add(tui_input_t *in, const char *line);

/* Get history entry by index (0 = oldest). Returns NULL if out of range. */
const char *tui_input_history_get(const tui_input_t *in, size_t idx);

/* Get history size. */
size_t tui_input_history_size(const tui_input_t *in);

/* Enable/disable echo (for password input). */
void tui_input_set_echo(tui_input_t *in, bool echo);

/* Set custom prompt. */
void tui_input_set_prompt(tui_input_t *in, const char *prompt);

/* Free input handler. */
void tui_input_free(tui_input_t *in);

/* ====================================================================
 *  DISPLAY SUBSYSTEM (replaces rich + tqdm + termcolor)
 * ==================================================================== */

/* === ANSI Color Codes === */
typedef enum {
    TUI_BLACK, TUI_RED, TUI_GREEN, TUI_YELLOW,
    TUI_BLUE, TUI_MAGENTA, TUI_CYAN, TUI_WHITE,
    TUI_DEFAULT = 9,
} tui_color_t;

/* Set foreground/background color for subsequent output. */
const char *tui_fg(tui_color_t c);
const char *tui_bg(tui_color_t c);
const char *tui_bold(void);
const char *tui_dim(void);
const char *tui_reset(void);

/* Print colored text. */
void tui_printf(tui_color_t color, const char *fmt, ...);
void tui_printfln(tui_color_t color, const char *fmt, ...);

/* === Progress Bar (replaces tqdm) === */
typedef struct tui_progress_t tui_progress_t;

/* Create progress bar with label and total steps. */
tui_progress_t *tui_progress_new(const char *label, int total);

/* Update progress to current step. Call with 0..total. */
void tui_progress_update(tui_progress_t *p, int current);

/* Finish progress bar (drops to new line). */
void tui_progress_done(tui_progress_t *p);

/* Free progress bar (calls done if not already). */
void tui_progress_free(tui_progress_t *p);

/* === Box/Frame === */
/* Draw a framed box with title around content. width=0 for auto. */
void tui_box(const char *title, const char *content, int width);

/* === Ruler/Separator === */
void tui_ruler(const char *label, tui_color_t color, int width);

/* === Table === */
void tui_table(const char *headers[], const char *rows[], int cols, int rows_count);

/* === Spinner (for long operations) === */
typedef struct tui_spinner_t tui_spinner_t;

tui_spinner_t *tui_spinner_new(const char *label);
void tui_spinner_tick(tui_spinner_t *s);
void tui_spinner_done(tui_spinner_t *s, const char *final_msg);
void tui_spinner_free(tui_spinner_t *s);

#ifdef __cplusplus
}
#endif

#endif /* LIBTUI_H */
