/*
 * ansi.h — ANSI terminal escape codes (J22: Python colorama/rich port).
 *
 * Provides ANSI escape constants for terminal colors, styles,
 * cursor control, and screen manipulation.
 */
#ifndef HERMES_ANSI_H
#define HERMES_ANSI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Reset
 * ================================================================ */
#define ANSI_RESET      "\033[0m"

/* ================================================================
 *  Text styles
 * ================================================================ */
#define ANSI_BOLD       "\033[1m"
#define ANSI_DIM        "\033[2m"
#define ANSI_ITALIC     "\033[3m"
#define ANSI_UNDERLINE  "\033[4m"
#define ANSI_BLINK      "\033[5m"
#define ANSI_REVERSE    "\033[7m"
#define ANSI_HIDDEN     "\033[8m"
#define ANSI_STRIKE     "\033[9m"

/* ================================================================
 *  Foreground colors (4-bit standard)
 * ================================================================ */
#define ANSI_BLACK      "\033[30m"
#define ANSI_RED        "\033[31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_BLUE       "\033[34m"
#define ANSI_MAGENTA    "\033[35m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_WHITE      "\033[37m"
#define ANSI_DEFAULT    "\033[39m"

/* ================================================================
 *  Bright foreground colors
 * ================================================================ */
#define ANSI_BRIGHT_BLACK   "\033[90m"
#define ANSI_BRIGHT_RED     "\033[91m"
#define ANSI_BRIGHT_GREEN   "\033[92m"
#define ANSI_BRIGHT_YELLOW  "\033[93m"
#define ANSI_BRIGHT_BLUE    "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN    "\033[96m"
#define ANSI_BRIGHT_WHITE   "\033[97m"

/* ================================================================
 *  Background colors (4-bit standard)
 * ================================================================ */
#define ANSI_BG_BLACK       "\033[40m"
#define ANSI_BG_RED         "\033[41m"
#define ANSI_BG_GREEN       "\033[42m"
#define ANSI_BG_YELLOW      "\033[43m"
#define ANSI_BG_BLUE        "\033[44m"
#define ANSI_BG_MAGENTA     "\033[45m"
#define ANSI_BG_CYAN        "\033[46m"
#define ANSI_BG_WHITE       "\033[47m"
#define ANSI_BG_DEFAULT     "\033[49m"

/* ================================================================
 *  Bright background colors
 * ================================================================ */
#define ANSI_BG_BRIGHT_BLACK   "\033[100m"
#define ANSI_BG_BRIGHT_RED     "\033[101m"
#define ANSI_BG_BRIGHT_GREEN   "\033[102m"
#define ANSI_BG_BRIGHT_YELLOW  "\033[103m"
#define ANSI_BG_BRIGHT_BLUE    "\033[104m"
#define ANSI_BG_BRIGHT_MAGENTA "\033[105m"
#define ANSI_BG_BRIGHT_CYAN    "\033[106m"
#define ANSI_BG_BRIGHT_WHITE   "\033[107m"

/* ================================================================
 *  Cursor control
 * ================================================================ */
#define ANSI_CURSOR_HOME    "\033[H"
#define ANSI_CURSOR_UP(n)   "\033[" #n "A"
#define ANSI_CURSOR_DOWN(n) "\033[" #n "B"
#define ANSI_CURSOR_RIGHT(n) "\033[" #n "C"
#define ANSI_CURSOR_LEFT(n) "\033[" #n "D"
#define ANSI_CURSOR_SAVE    "\033[s"
#define ANSI_CURSOR_RESTORE "\033[u"
#define ANSI_CURSOR_HIDE    "\033[?25l"
#define ANSI_CURSOR_SHOW    "\033[?25h"

/* ================================================================
 *  Screen clearing
 * ================================================================ */
#define ANSI_CLEAR_SCREEN   "\033[2J"
#define ANSI_CLEAR_LINE     "\033[2K"
#define ANSI_CLEAR_EOL      "\033[K"

/* ================================================================
 *  Helper functions
 * ================================================================ */

/* Wrap text in ANSI codes. Caller must free the returned string.
 * Example: ansi_wrap("hello", ANSI_RED, ANSI_BOLD) = "\033[31m\033[1mhello\033[0m" */
char *ansi_wrap(const char *text, const char *fg, const char *style);

/* Strip ANSI escape sequences from a string, returning plain text.
 * Caller must free the returned string. */
char *ansi_strip(const char *text);

/* Check if the terminal supports ANSI codes (checks TERM env var). */
bool ansi_supported(void);

/* Get terminal width (uses TIOCGWINSZ ioctl). Returns 80 on failure. */
int ansi_term_width(void);

/* Get terminal height. Returns 24 on failure. */
int ansi_term_height(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ANSI_H */
