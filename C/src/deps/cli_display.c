/*
 * cli_display.c — Terminal display for Hermes C.
 * ANSI escape codes only. No ncurses.
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

/* Forward declaration */
static char *xstrdup(const char *s);

/* Internal helpers */
static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* ================================================================
 *  Init
 * ================================================================ */

void display_init(void) {
    is_tty = isatty(STDOUT_FILENO);
}

bool display_has_color(void) {
    const char *term = getenv("TERM");
    if (!term) return false;
    if (strstr(term, "color") || strstr(term, "xterm") || strstr(term, "screen"))
        return true;
    return is_tty;
}

int display_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        return w.ws_col;
    return 80;
}

/* ================================================================
 *  Styles
 * ================================================================ */

void display_set_fg(display_color_t color) {
    if (!is_tty) return;
    printf(ANSI_ESC "%dm", 30 + (int)color);
    fflush(stdout);
}

void display_set_bg(display_color_t color) {
    if (!is_tty) return;
    printf(ANSI_ESC "%dm", 40 + (int)color);
    fflush(stdout);
}

void display_set_style(display_style_t style) {
    if (!is_tty) return;
    if (style & DISPLAY_BOLD)      printf(ANSI_ESC "1m");
    if (style & DISPLAY_DIM)       printf(ANSI_ESC "2m");
    if (style & DISPLAY_ITALIC)    printf(ANSI_ESC "3m");
    if (style & DISPLAY_UNDERLINE) printf(ANSI_ESC "4m");
    fflush(stdout);
}

void display_reset(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "0m");
    fflush(stdout);
}

void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...)
{
    display_set_fg(color);
    display_set_style(style);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    display_reset();
}

/* ================================================================
 *  Cursor
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
    bar->width = display_width() > 20 ? display_width() - 20 : 30;
    bar->current = 0;
    bar->total = total;
    snprintf(bar->label, sizeof(bar->label), "%s", label ? label : "");
}

void display_progress_update(display_progress_t *bar, int current) {
    if (!is_tty) return;
    bar->current = current;
    float pct = bar->total > 0 ? (float)current / (float)bar->total : 0.0f;
    int filled = (int)(pct * bar->width);

    display_save_pos();
    printf("\r%s [", bar->label);
    display_set_fg(DISPLAY_GREEN);
    for (int i = 0; i < bar->width; i++) {
        putchar(i < filled ? '=' : (i == filled ? '>' : ' '));
    }
    display_reset();
    printf("] %3d%%", (int)(pct * 100.0f));
    fflush(stdout);
    display_restore_pos();
}

void display_progress_done(display_progress_t *bar) {
    display_progress_update(bar, bar->total);
    printf("\n");
}

/* ================================================================
 *  Spinner
 * ================================================================ */

static const char *spinner_frames = "|/-\\";

void display_spinner_start(display_spinner_t *sp, const char *label) {
    sp->frame = 0;
    sp->label = label ? xstrdup(label) : NULL;
    sp->active = true;
    if (is_tty) printf("%s %c", label ? label : "", spinner_frames[0]);
}

void display_spinner_tick(display_spinner_t *sp) {
    if (!is_tty || !sp->active) return;
    sp->frame = (sp->frame + 1) % 4;
    printf("\r%s %c", sp->label ? sp->label : "", spinner_frames[sp->frame]);
    fflush(stdout);
}

void display_spinner_stop(display_spinner_t *sp, const char *done_msg) {
    if (!sp->active) return;
    sp->active = false;
    if (is_tty) {
        printf("\r%s %s\n", sp->label ? sp->label : "", done_msg ? done_msg : "done.");
    }
    free(sp->label);
    sp->label = NULL;
}

/* ================================================================
 *  Panel
 * ================================================================ */

void display_panel(const char *title, const char *content, display_color_t color) {
    int w = display_width();
    if (w > 78) w = 78;

    if (is_tty) display_set_fg(color);
    printf("┌─");
    if (title) {
        display_set_style(DISPLAY_BOLD);
        printf(" %s ", title);
        display_set_fg(color);
        display_set_style(DISPLAY_NORMAL);
        int pad = w - 4 - (int)strlen(title) - 2;
        for (int i = 0; i < pad; i++) printf("-");
    } else {
        for (int i = 0; i < w - 2; i++) printf("-");
    }
    printf("┐\n");

    /* Content with side borders */
    if (content) {
        const char *line_start = content;
        const char *nl;
        while ((nl = strchr(line_start, '\n')) != NULL) {
            size_t line_len = (size_t)(nl - line_start);
            printf("│ ");
            size_t print_len = line_len < (size_t)(w - 4) ? line_len : (size_t)(w - 4);
            printf("%.*s", (int)print_len, line_start);
            for (size_t i = print_len; i < (size_t)(w - 4); i++) putchar(' ');
            printf(" │\n");
            line_start = nl + 1;
        }
        /* Last line (no trailing newline) */
        size_t last_len = strlen(line_start);
        printf("│ ");
        size_t print_len = last_len < (size_t)(w - 4) ? last_len : (size_t)(w - 4);
        printf("%.*s", (int)print_len, line_start);
        for (size_t i = print_len; i < (size_t)(w - 4); i++) putchar(' ');
        printf(" │\n");
    }

    printf("└");
    for (int i = 0; i < w - 2; i++) printf("-");
    printf("┘\n");
    if (is_tty) display_reset();
}

void display_hr(display_color_t color) {
    int w = display_width();
    if (is_tty) display_set_fg(color);
    for (int i = 0; i < w; i++) printf("-");
    printf("\n");
    if (is_tty) display_reset();
}
