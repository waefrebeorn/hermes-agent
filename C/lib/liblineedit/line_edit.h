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

/* Line editor state */
typedef struct line_edit_t line_edit_t;

/* Completion callback: given partial word, return NULL-terminated list of matches.
 * Each match is a heap-allocated string; the caller frees them and the array. */
typedef char **(*line_edit_completion_cb)(const char *partial, void *user_data);

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

#endif /* HERMES_LINE_EDIT_H */
