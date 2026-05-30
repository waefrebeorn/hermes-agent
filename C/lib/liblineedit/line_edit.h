/*
 * line_edit.h — Minimal line editor with history and tab completion.
 * Self-contained, no external dependencies (uses termios).
 * Provides readline()-like interface for Hermes C CLI.
 */

#ifndef HERMES_LINE_EDIT_H
#define HERMES_LINE_EDIT_H

#include <stdbool.h>
#include <stddef.h>

/* Maximum input line length */
#define LINE_EDIT_MAX_LINE 65536
#define MAX_KILL_RING 65536
#define MAX_HISTORY 100

/* Editing mode */
typedef enum {
    LINE_EDIT_MODE_INSERT = 0,
    LINE_EDIT_MODE_NORMAL = 1
} line_edit_mode_t;

/* Forward declarations */
typedef struct history_t history_t;

/* Line buffer — internal but exposed for unit testing */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    size_t cursor;    /* cursor position in buf (0 = start) */
} line_buf_t;

/* Completion callback: given partial word, return NULL-terminated list of matches.
 * Each match is a heap-allocated string; the caller frees them and the array. */
typedef char **(*line_edit_completion_cb)(const char *partial, void *user_data);

/* Line editor state */
typedef struct line_edit_t {
    line_buf_t *buf;
    history_t *history;
    line_edit_completion_cb complete;
    void *user_data;
    char saved_line[LINE_EDIT_MAX_LINE];  /* for history navigation */
    char kill_ring[MAX_KILL_RING];        /* for Ctrl-K/Ctrl-Y kill/yank */
    size_t kill_ring_len;
    line_edit_mode_t vi_mode;             /* INSERT or NORMAL (vi mode) */
    char vi_saved_line[LINE_EDIT_MAX_LINE]; /* for vi undo */
    bool vi_saved;                        /* whether vi_saved_line is valid */
    char vi_last_find_char;               /* last f/F/t/T target char */
    bool vi_last_find_forward;            /* last find direction (true=forward) */
    bool vi_last_find_till;               /* last find was till (t/T), not find (f/F) */
} line_edit_t;

/* Create a line editor instance */
line_edit_t *line_edit_create(line_edit_completion_cb complete, void *user_data);

/* Free line editor and all state */
void line_edit_free(line_edit_t *le);

/* Read a line of input from stdin. Returns heap-allocated string (caller frees)
 * or NULL on EOF. Prompt is printed before reading. */
char *line_edit_read(line_edit_t *le, const char *prompt);

/* Save history to file */
bool line_edit_save_history(line_edit_t *le, const char *path);

/* Load history from file */
bool line_edit_load_history(line_edit_t *le, const char *path);

/* Set buffer text (for pre-populating input, e.g. from type-ahead).
 * Copies text into the editor buffer. Safe to call before line_edit_read(). */
void line_edit_set_text(line_edit_t *le, const char *text);

/* Get current editing mode. Exposed for testing. */
line_edit_mode_t line_edit_get_mode(const line_edit_t *le);

/* Emacs-style editing helpers — exposed for testing */
void line_edit_kill_line(line_edit_t *le);
void line_edit_yank(line_edit_t *le);
void line_edit_yank_line(line_edit_t *le);   /* yy/Y — yank entire line into kill ring */
void line_edit_kill_word_forward(line_edit_t *le);
void line_edit_transpose_chars(line_edit_t *le);
void line_edit_cursor_word_forward(line_edit_t *le);
void line_edit_cursor_word_backward(line_edit_t *le);
void line_edit_cursor_word_end(line_edit_t *le);

#endif /* HERMES_LINE_EDIT_H */
