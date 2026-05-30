/*
 * tui.c — Terminal UI library for C (input + display).
 * MIT License — WuBu Hermes Project
 */

#define _GNU_SOURCE
#include "tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

/* ====================================================================
 *  HISTORY
 * ==================================================================== */

#define HISTORY_MAX 1024

typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
} history_t;

static history_t *history_new(void) {
    history_t *h = (history_t *)calloc(1, sizeof(history_t));
    if (!h) return NULL;
    h->capacity = 64;
    h->lines = (char **)calloc(h->capacity, sizeof(char *));
    if (!h->lines) { free(h); return NULL; }
    return h;
}

static void history_add(history_t *h, const char *line) {
    if (!h || !line || !*line) return;
    /* Don't add duplicate of last entry */
    if (h->count > 0 && strcmp(h->lines[h->count - 1], line) == 0) return;
    if (h->count >= h->capacity) {
        h->capacity *= 2;
        h->lines = (char **)realloc(h->lines, h->capacity * sizeof(char *));
        if (!h->lines) return;
    }
    if (h->count >= HISTORY_MAX) {
        free(h->lines[0]);
        memmove(h->lines, h->lines + 1, (h->count - 1) * sizeof(char *));
        h->count--;
    }
    h->lines[h->count++] = strdup(line);
}

/* ====================================================================
 *  INPUT (replaces prompt_toolkit)
 * ==================================================================== */

struct tui_input_t {
    char   prompt[256];
    bool   echo;
    history_t *history;
    struct termios orig_term;
    bool   term_saved;
};

tui_input_t *tui_input_new(const char *prompt) {
    tui_input_t *in = (tui_input_t *)calloc(1, sizeof(tui_input_t));
    if (!in) return NULL;
    if (prompt) strncpy(in->prompt, prompt, sizeof(in->prompt) - 1);
    in->echo = true;
    in->history = history_new();
    return in;
}

/* Set terminal to raw mode */
static bool raw_mode(tui_input_t *in) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &in->orig_term) < 0) return false;
    in->term_saved = true;
    raw = in->orig_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
}

/* Restore terminal */
static void restore_mode(tui_input_t *in) {
    if (in->term_saved)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &in->orig_term);
    in->term_saved = false;
}

char *tui_input_read_history(tui_input_t *in) {
    if (!in) return NULL;

    printf("%s", in->prompt);
    fflush(stdout);

    if (!raw_mode(in)) {
        /* Fallback: simple line input */
        char *buf = NULL;
        size_t n = 0;
        ssize_t len = getline(&buf, &n, stdin);
        if (len < 0) { free(buf); return NULL; }
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';

        if (in->echo) {
            history_add(in->history, buf);
        }
        return buf;
    }

    /* Line editor buffer */
    size_t bufsz = 256, pos = 0;
    char *buf = (char *)calloc(bufsz, 1);
    if (!buf) { restore_mode(in); return NULL; }

    int hist_idx = (int)in->history->count;  /* current = end (new) */

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        if (c == '\n' || c == '\r') {
            printf("\n");
            break;
        }

        if (c == 127 || c == '\b') { /* backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (c == 27) { /* escape sequence */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') { /* Up arrow — history back */
                    if (hist_idx > 0) {
                        /* Erase current line */
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx--;
                        strncpy(buf, in->history->lines[hist_idx], bufsz - 1);
                        pos = strlen(buf);
                        printf("%s", buf);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { /* Down arrow — history forward */
                    if (hist_idx < (int)in->history->count - 1) {
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx++;
                        strncpy(buf, in->history->lines[hist_idx], bufsz - 1);
                        pos = strlen(buf);
                        printf("%s", buf);
                        fflush(stdout);
                    } else if (hist_idx == (int)in->history->count - 1) {
                        /* At end — clear */
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx = (int)in->history->count;
                        buf[0] = '\0';
                        pos = 0;
                    }
                }
            }
            continue;
        }

        /* Regular character */
        if (pos + 1 >= bufsz) {
            bufsz *= 2;
            char *nb = (char *)realloc(buf, bufsz);
            if (!nb) break;
            buf = nb;
        }
        buf[pos++] = c;
        buf[pos] = '\0';
        if (in->echo) {
            putchar(c);
            fflush(stdout);
        }
    }

    restore_mode(in);
    if (pos == 0) { free(buf); return NULL; }
    if (in->echo) history_add(in->history, buf);
    return buf;
}

char *tui_input_read(tui_input_t *in) {
    return tui_input_read_history(in);
}

void tui_input_history_add(tui_input_t *in, const char *line) {
    if (in) history_add(in->history, line);
}

const char *tui_input_history_get(const tui_input_t *in, size_t idx) {
    if (!in || !in->history || idx >= in->history->count) return NULL;
    return in->history->lines[idx];
}

size_t tui_input_history_size(const tui_input_t *in) {
    return in && in->history ? in->history->count : 0;
}

void tui_input_set_echo(tui_input_t *in, bool echo) { if (in) in->echo = echo; }
void tui_input_set_prompt(tui_input_t *in, const char *prompt) {
    if (in && prompt) strncpy(in->prompt, prompt, sizeof(in->prompt) - 1);
}

void tui_input_free(tui_input_t *in) {
    if (!in) return;
    if (in->history) {
        for (size_t i = 0; i < in->history->count; i++)
            free(in->history->lines[i]);
        free(in->history->lines);
        free(in->history);
    }
    if (in->term_saved) restore_mode(in);
    free(in);
}

/* ====================================================================
 *  DISPLAY
 * ==================================================================== */

/* ANSI codes */
const char *tui_fg(tui_color_t c) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "\033[%dm", 30 + (int)c);
    return buf;
}
const char *tui_bg(tui_color_t c) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "\033[%dm", 40 + (int)c);
    return buf;
}
const char *tui_bold(void) { return "\033[1m"; }
const char *tui_dim(void) { return "\033[2m"; }
const char *tui_reset(void) { return "\033[0m"; }

void tui_printf(tui_color_t color, const char *fmt, ...) {
    printf("%s", tui_fg(color));
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("%s", tui_reset());
}

void tui_printfln(tui_color_t color, const char *fmt, ...) {
    printf("%s", tui_fg(color));
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("%s\n", tui_reset());
}

/* ====================================================================
 *  SPINNER
 * ==================================================================== */

struct tui_spinner_t {
    char label[128];
    int frame;
    bool done;
};

tui_spinner_t *tui_spinner_new(const char *label) {
    tui_spinner_t *s = (tui_spinner_t *)calloc(1, sizeof(tui_spinner_t));
    if (!s) return NULL;
    if (label) strncpy(s->label, label, sizeof(s->label) - 1);
    s->frame = 0;
    s->done = false;
    return s;
}

void tui_spinner_tick(tui_spinner_t *s) {
    if (!s || s->done) return;
    static const char *frames = "|/-\\";
    printf("\r%s %c", s->label, frames[s->frame]);
    fflush(stdout);
    s->frame = (s->frame + 1) % 4;
}

void tui_spinner_done(tui_spinner_t *s, const char *final_msg) {
    if (!s) return;
    s->done = true;
    printf("\r\x1b[K"); /* clear line */
    if (final_msg) printf("%s\n", final_msg);
    else printf("%s done.\n", s->label);
}

void tui_spinner_free(tui_spinner_t *s) {
    if (!s) return;
    if (!s->done) tui_spinner_done(s, NULL);
    free(s);
}

/* ====================================================================
 *  PROGRESS BAR (replaces tqdm)
 * ==================================================================== */

struct tui_progress_t {
    char label[128];
    int total;
    int current;
    bool done;
    struct timeval start;
};

tui_progress_t *tui_progress_new(const char *label, int total) {
    tui_progress_t *p = (tui_progress_t *)calloc(1, sizeof(tui_progress_t));
    if (!p) return NULL;
    if (label) strncpy(p->label, label, sizeof(p->label) - 1);
    p->total = total > 0 ? total : 1;
    p->current = 0;
    p->done = false;
    gettimeofday(&p->start, NULL);
    tui_progress_update(p, 0);
    return p;
}

void tui_progress_update(tui_progress_t *p, int current) {
    if (!p || p->done) return;
    p->current = current;

    int bar_width = 30;
    int pct = current * 100 / p->total;
    int filled = current * bar_width / p->total;

    /* Elapsed time */
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - p->start.tv_sec)
                   + (now.tv_usec - p->start.tv_usec) / 1e6;

    printf("\r%s: [", p->label);
    for (int i = 0; i < bar_width; i++)
        putchar(i < filled ? '=' : (i == filled ? '>' : ' '));
    printf("] %3d%%  %6.1fs", pct, elapsed);
    fflush(stdout);
}

void tui_progress_done(tui_progress_t *p) {
    if (!p || p->done) return;
    p->done = true;
    tui_progress_update(p, p->total);
    printf("\n");
}

void tui_progress_free(tui_progress_t *p) {
    if (!p) return;
    if (!p->done) tui_progress_done(p);
    free(p);
}

/* ====================================================================
 *  BOX
 * ==================================================================== */

void tui_box(const char *title, const char *content, int width) {
    if (width <= 0) width = 50;

    /* Top line */
    printf("+");
    if (title && *title) {
        printf(" %s ", title);
        int remain = width - (int)strlen(title) - 3;
        for (int i = 0; i < remain; i++) printf("-");
    } else {
        for (int i = 0; i < width; i++) printf("-");
    }
    printf("+\n");

    /* Content — split by newlines */
    const char *p = content;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        printf("| %.*s|\n", (int)(nl - p), p);
        p = nl + 1;
    }
    if (*p) printf("| %s|\n", p);

    /* Bottom line */
    printf("+");
    for (int i = 0; i < width; i++) printf("-");
    printf("+\n");
}

/* ====================================================================
 *  RULER
 * ==================================================================== */

void tui_ruler(const char *label, tui_color_t color, int width) {
    if (width <= 0) width = 60;
    printf("%s", tui_fg(color));
    if (label && *label) {
        int lbl_len = (int)strlen(label) + 2;
        int side = (width - lbl_len) / 2;
        for (int i = 0; i < side; i++) printf("-");
        printf(" %s ", label);
        for (int i = width - side - lbl_len; i > 0; i--) printf("-");
    } else {
        for (int i = 0; i < width; i++) printf("-");
    }
    printf("%s\n", tui_reset());
}

/* ====================================================================
 *  TABLE
 * ==================================================================== */

void tui_table(const char *headers[], const char *rows[],
               int cols, int rows_count) {
    if (!headers || !rows || cols <= 0) return;

    /* Calculate column widths */
    int *widths = (int *)calloc((size_t)cols, sizeof(int));
    if (!widths) return;
    for (int c = 0; c < cols; c++)
        if (headers[c]) widths[c] = (int)strlen(headers[c]);

    for (int r = 0; r < rows_count; r++) {
        for (int c = 0; c < cols; c++) {
            int len = (int)strlen(rows[r * cols + c]);
            if (len > widths[c]) widths[c] = len;
        }
    }

    /* Header separator */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    /* Headers */
    printf("|");
    for (int c = 0; c < cols; c++) {
        printf(" %-*s |", widths[c], headers[c] ? headers[c] : "");
    }
    printf("\n");

    /* Separator */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    /* Rows */
    for (int r = 0; r < rows_count; r++) {
        printf("|");
        for (int c = 0; c < cols; c++) {
            const char *val = rows[r * cols + c] ? rows[r * cols + c] : "";
            printf(" %-*s |", widths[c], val);
        }
        printf("\n");
    }

    /* Bottom */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    free(widths);
}
