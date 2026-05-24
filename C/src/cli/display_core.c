/*
 * display_core.c — Terminal display for Hermes C.
 * ANSI escape codes only. No ncurses dependency.
 * Provides: display_init, display_reset, display_printf, progress, spinner, panel.
 */

#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ioctl.h>

/* ================================================================
 *  State
 * ================================================================ */

static bool is_tty = false;

#define ANSI_ESC "\x1B["

/* ================================================================
 *  Initialization
 * ================================================================ */

void display_init(void) {
    is_tty = isatty(STDOUT_FILENO);
    if (is_tty) {
        /* Enable alternate screen? No, keep it simple */
    }
}

void display_reset(void) {
    if (is_tty) {
        printf(ANSI_ESC "0m");
        fflush(stdout);
    }
}

/* ================================================================
 *  Color + Style
 * ================================================================ */

void display_set_fg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "39m");
    else
        printf(ANSI_ESC "9%dm", (int)color);
    fflush(stdout);
}

void display_set_bg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "49m");
    else
        printf(ANSI_ESC "10%dm", (int)color);
    fflush(stdout);
}

/* Truecolor (24-bit) support */
void display_set_fg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "38;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

void display_set_bg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "48;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

void display_set_style(display_style_t style) {
    if (!is_tty) return;
    if (style & DISPLAY_BOLD)
        printf(ANSI_ESC "1m");
    if (style & DISPLAY_DIM)
        printf(ANSI_ESC "2m");
    if (style & DISPLAY_ITALIC)
        printf(ANSI_ESC "3m");
    if (style & DISPLAY_UNDERLINE)
        printf(ANSI_ESC "4m");
    fflush(stdout);
}

/* ================================================================
 *  Printf with color
 * ================================================================ */

void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...)
{
    if (!is_tty) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }

    display_set_fg(color);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* Print with 24-bit truecolor foreground */
void display_printf_hex(const char *hex_fg, display_style_t style,
                         const char *fmt, ...)
{
    if (!is_tty || !hex_fg) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    int r, g, b;
    if (!ansi_parse_hex(hex_fg, &r, &g, &b)) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    display_set_fg_rgb(r, g, b);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* ================================================================
 *  Cursor control
 * ================================================================ */

void display_clear(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "2J" ANSI_ESC "H");
    fflush(stdout);
}

void display_goto(int row, int col) {
    if (!is_tty) return;
    printf(ANSI_ESC "%d;%dH", row, col);
    fflush(stdout);
}

void display_save_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "s");
    fflush(stdout);
}

void display_restore_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "u");
    fflush(stdout);
}

/* ================================================================
 *  Progress bar
 * ================================================================ */

void display_progress_init(display_progress_t *bar, const char *label, int total) {
    if (!bar) return;
    bar->current = 0;
    bar->total = total > 0 ? total : 100;
    bar->width = 40;
    snprintf(bar->label, sizeof(bar->label), "%s", label ? label : "");
    if (is_tty) {
        display_printf(DISPLAY_WHITE, DISPLAY_DIM, "%s: ", bar->label);
        fflush(stdout);
    }
}

void display_progress_update(display_progress_t *bar, int current) {
    if (!bar || !is_tty) return;
    bar->current = current;
    int pct = (bar->total > 0) ? (current * 100 / bar->total) : 0;
    int filled = (bar->width * current) / bar->total;
    printf("\r%s: [", bar->label);
    for (int i = 0; i < bar->width; i++) {
        if (i < filled) putchar('=');
        else if (i == filled) putchar('>');
        else putchar(' ');
    }
    printf("] %d%%", pct);
    fflush(stdout);
}

void display_progress_done(display_progress_t *bar) {
    if (!bar) return;
    if (is_tty) {
        display_progress_update(bar, bar->total);
        printf("\n");
    }
    fflush(stdout);
}

/* ================================================================
 *  Spinner
 * ================================================================ */

static const char SPINNER_CHARS[] = {'|', '/', '-', '\\'};

void display_spinner_start(display_spinner_t *sp, const char *label) {
    if (!sp) return;
    sp->frame = 0;
    sp->active = true;
    if (label) {
        sp->label = strdup(label);
    } else {
        sp->label = NULL;
    }
}

void display_spinner_tick(display_spinner_t *sp) {
    if (!sp || !sp->active || !is_tty) return;
    printf("\r%c %s", SPINNER_CHARS[sp->frame % 4],
           sp->label ? sp->label : "");
    sp->frame++;
    fflush(stdout);
}

void display_spinner_stop(display_spinner_t *sp, const char *done_msg) {
    if (!sp) return;
    sp->active = false;
    if (is_tty) {
        if (done_msg)
            printf("\r\xE2\x9C\x93 %s\n", done_msg);
        else
            printf("\r\xE2\x9C\x93 %s\n", sp->label ? sp->label : "done");
    }
    free(sp->label);
    sp->label = NULL;
    fflush(stdout);
}

/* ================================================================
 *  Panel / Box
 * ================================================================ */

void display_panel(const char *title, const char *content, display_color_t color) {
    if (!content) return;
    int term_width = display_width();

    /* Calculate inner width (80% of terminal, min 40, max 80) */
    int inner = term_width > 80 ? 80 : (term_width < 40 ? 40 : term_width);
    inner = (inner * 8) / 10;

    /* Top border */
    if (title && title[0]) {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        printf(" %s ", title);
        int remaining = inner - (int)strlen(title) - 3;
        for (int i = 0; i < remaining && i < 60; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    } else {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        for (int i = 0; i < inner; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    }

    /* Content */
    display_printf(color, DISPLAY_NORMAL, "%s", content);
    if (content[strlen(content) - 1] != '\n')
        printf("\n");

    /* Bottom border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x94");
    for (int i = 0; i < inner; i++)
        printf("\xE2\x94\x80");
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x98\n\n");
}

void display_hr(display_color_t color) {
    int w = display_width();
    if (w > 80) w = 80;
    display_printf(color, DISPLAY_DIM, "");
    for (int i = 0; i < w; i++)
        printf("\xE2\x94\x80");
    printf("\n");
    fflush(stdout);
}

/* ================================================================
 *  Utility
 * ================================================================ */

bool display_has_color(void) {
    return is_tty;
}

int display_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}
