/*
 * line_edit.c — Minimal line editor with history and tab completion.
 * Uses raw terminal mode (termios) for single-character input.
 * No external dependencies (no readline, no libedit).
 *
 * Features:
 * - Raw terminal input with line editing (backspace, left/right arrows)
 * - History (up/down arrows, up to 100 entries)
 * - Tab completion via callback
 * - In-memory history with file save/load
 */

#include "line_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

#define MAX_HISTORY 100
#define TAB_BUF_SIZE 8192

/* Escape sequences */
#define KEY_UP      'A'
#define KEY_DOWN    'B'
#define KEY_RIGHT   'C'
#define KEY_LEFT    'D'
#define KEY_HOME    'H'
#define KEY_END     'F'
#define KEY_DELETE  '3'
#define KEY_TAB      9
#define KEY_BACKSPACE 127
#define KEY_ENTER     '\n'
#define KEY_CTRL_C     3
#define KEY_CTRL_D     4
#define KEY_CTRL_R    18
#define KEY_CTRL_U    21
#define KEY_CTRL_W    23

/* ------------------------------------------------------------------ */
/*  Line buffer                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    size_t cursor;    /* cursor position in buf (0 = start) */
} line_buf_t;

static line_buf_t *line_buf_create(void) {
    line_buf_t *lb = calloc(1, sizeof(line_buf_t));
    if (!lb) return NULL;
    lb->cap = 256;
    lb->buf = malloc(lb->cap);
    if (!lb->buf) { free(lb); return NULL; }
    lb->buf[0] = '\0';
    lb->len = 0;
    lb->cursor = 0;
    return lb;
}

static void line_buf_free(line_buf_t *lb) {
    if (!lb) return;
    free(lb->buf);
    free(lb);
}

static bool line_buf_insert(line_buf_t *lb, char c) {
    if (lb->len + 2 >= lb->cap) {
        size_t new_cap = lb->cap * 2;
        if (new_cap > LINE_EDIT_MAX_LINE) new_cap = LINE_EDIT_MAX_LINE;
        char *new_buf = realloc(lb->buf, new_cap);
        if (!new_buf) return false;
        lb->buf = new_buf;
        lb->cap = new_cap;
    }
    /* Shift chars right from cursor */
    memmove(lb->buf + lb->cursor + 1, lb->buf + lb->cursor, lb->len - lb->cursor);
    lb->buf[lb->cursor] = c;
    lb->len++;
    lb->buf[lb->len] = '\0';
    lb->cursor++;
    return true;
}

static void line_buf_delete(line_buf_t *lb) {
    if (lb->cursor == 0) return;
    memmove(lb->buf + lb->cursor - 1, lb->buf + lb->cursor, lb->len - lb->cursor);
    lb->len--;
    lb->buf[lb->len] = '\0';
    lb->cursor--;
}

static void line_buf_delete_forward(line_buf_t *lb) {
    if (lb->cursor >= lb->len) return;
    memmove(lb->buf + lb->cursor, lb->buf + lb->cursor + 1, lb->len - lb->cursor);
    lb->len--;
    lb->buf[lb->len] = '\0';
}

static void line_buf_clear(line_buf_t *lb) {
    lb->buf[0] = '\0';
    lb->len = 0;
    lb->cursor = 0;
}

static void line_buf_set(line_buf_t *lb, const char *s) {
    size_t slen = strlen(s);
    if (slen >= lb->cap) {
        size_t new_cap = slen + 256;
        if (new_cap > LINE_EDIT_MAX_LINE) new_cap = LINE_EDIT_MAX_LINE;
        char *new_buf = realloc(lb->buf, new_cap);
        if (!new_buf) return;
        lb->buf = new_buf;
        lb->cap = new_cap;
    }
    memcpy(lb->buf, s, slen + 1);
    lb->len = slen;
    lb->cursor = slen;
}

/* ------------------------------------------------------------------ */
/*  Terminal raw mode                                                 */
/* ------------------------------------------------------------------ */

static struct termios g_old_term;
static bool g_raw_mode = false;

static void term_enter_raw(void) {
    if (g_raw_mode) return;
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &g_old_term);
    struct termios raw = g_old_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = true;
    /* Enable bracketed paste mode */
    printf("\x1B[?2004h");
    fflush(stdout);
}

static void term_exit_raw(void) {
    if (!g_raw_mode) return;
    /* Disable bracketed paste mode */
    printf("\x1B[?2004l");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_old_term);
    g_raw_mode = false;
}

/* Redraw current line after cursor position */
static void term_redraw_line(const line_buf_t *lb) {
    /* Move to start of line, print buffer, clear to end */
    printf("\r\033[K%s", lb->buf);
    /* Move cursor back to position */
    size_t move_back = lb->len - lb->cursor;
    if (move_back > 0)
        printf("\033[%zuD", move_back);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/*  History                                                            */
/* ------------------------------------------------------------------ */

typedef struct hist_entry {
    char *text;
    struct hist_entry *prev;
    struct hist_entry *next;
} hist_entry_t;

typedef struct {
    hist_entry_t *head;
    hist_entry_t *tail;
    hist_entry_t *current;  /* for navigation */
    int count;
} history_t;

static history_t *history_create(void) {
    history_t *h = calloc(1, sizeof(history_t));
    return h;
}

static void history_free(history_t *h) {
    if (!h) return;
    hist_entry_t *e = h->head;
    while (e) {
        hist_entry_t *next = e->next;
        free(e->text);
        free(e);
        e = next;
    }
    free(h);
}

static void history_add(history_t *h, const char *text) {
    if (!h || !text || !*text) return;
    /* Don't add duplicate of last entry */
    if (h->tail && strcmp(h->tail->text, text) == 0) return;

    hist_entry_t *e = calloc(1, sizeof(hist_entry_t));
    if (!e) return;
    e->text = strdup(text);
    if (!e->text) { free(e); return; }

    if (h->tail) {
        h->tail->next = e;
        e->prev = h->tail;
        h->tail = e;
    } else {
        h->head = h->tail = e;
    }
    h->count++;
    h->current = h->tail;

    /* Trim if over limit */
    while (h->count > MAX_HISTORY && h->head) {
        hist_entry_t *old = h->head;
        h->head = old->next;
        if (h->head) h->head->prev = NULL;
        if (h->current == old) h->current = h->head;
        free(old->text);
        free(old);
        h->count--;
    }
}

static const char *history_prev(history_t *h) {
    if (!h || !h->current) return NULL;
    const char *text = h->current->text;
    if (h->current->prev) h->current = h->current->prev;
    return text;
}

static const char *history_next(history_t *h) {
    if (!h || !h->current) return NULL;
    if (h->current->next) {
        h->current = h->current->next;
        return h->current->text;
    }
    return NULL;  /* At end of history */
}

static void history_reset_pos(history_t *h) {
    if (h) h->current = h->tail;
}

/* ------------------------------------------------------------------ */
/*  Line editor state                                                 */
/* ------------------------------------------------------------------ */

struct line_edit_t {
    line_buf_t *buf;
    history_t *history;
    line_edit_completion_cb complete;
    void *user_data;
    char saved_line[LINE_EDIT_MAX_LINE];  /* for history navigation */
};

line_edit_t *line_edit_create(line_edit_completion_cb complete, void *user_data) {
    line_edit_t *le = calloc(1, sizeof(line_edit_t));
    if (!le) return NULL;
    le->buf = line_buf_create();
    if (!le->buf) { free(le); return NULL; }
    le->history = history_create();
    if (!le->history) { line_buf_free(le->buf); free(le); return NULL; }
    le->complete = complete;
    le->user_data = user_data;
    le->saved_line[0] = '\0';
    return le;
}

void line_edit_free(line_edit_t *le) {
    if (!le) return;
    term_exit_raw();
    line_buf_free(le->buf);
    history_free(le->history);
    free(le);
}

/* ------------------------------------------------------------------ */
/*  Tab completion                                                     */
/* ------------------------------------------------------------------ */

static void do_tab_complete(line_edit_t *le) {
    if (!le->complete) return;

    /* Find the current word (from last space or start) */
    const char *buf = le->buf->buf;
    size_t cursor = le->buf->cursor;

    /* Find start of current word */
    size_t word_start = cursor;
    while (word_start > 0 && !isspace((unsigned char)buf[word_start - 1]))
        word_start--;

    /* Extract partial word */
    size_t word_len = cursor - word_start;
    if (word_len > 512) return;

    char partial[512];
    memcpy(partial, buf + word_start, word_len);
    partial[word_len] = '\0';

    char **matches = le->complete(partial, le->user_data);
    if (!matches) return;

    /* Count matches */
    int count = 0;
    while (matches[count]) count++;

    if (count == 0) {
        /* No matches — beep */
        printf("\a");
        fflush(stdout);
    } else if (count == 1) {
        /* Single match — complete */
        const char *completion = matches[0];
        size_t clen = strlen(completion);
        /* Remove the partial word and insert completion */
        le->buf->cursor = word_start;
        /* Delete chars from cursor to end of word */
        size_t delete_count = word_len;
        if (delete_count > 0) {
            memmove(le->buf->buf + le->buf->cursor,
                    le->buf->buf + le->buf->cursor + delete_count,
                    le->buf->len - le->buf->cursor - delete_count + 1);
            le->buf->len -= delete_count;
        }
        /* Insert completion */
        for (size_t i = 0; i < clen; i++)
            line_buf_insert(le->buf, completion[i]);
        /* Add space */
        line_buf_insert(le->buf, ' ');
        term_redraw_line(le->buf);
    } else {
        /* Multiple matches — show them */
        printf("\n");
        int col = 0;
        for (int i = 0; i < count; i++) {
            printf("  %-20s", matches[i]);
            if (++col >= 3) { printf("\n"); col = 0; }
        }
        if (col > 0) printf("\n");
        term_redraw_line(le->buf);
    }

    /* Free matches */
    for (int i = 0; matches[i]; i++)
        free(matches[i]);
    free(matches);
}

/* ------------------------------------------------------------------ */
/*  Read a line                                                       */
/* ------------------------------------------------------------------ */

char *line_edit_read(line_edit_t *le, const char *prompt) {
    if (!le) return NULL;

    line_buf_clear(le->buf);
    term_enter_raw();

    /* Print prompt */
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            term_exit_raw();
            return NULL;  /* EOF */
        }

        if (c == KEY_ENTER) {
            /* Done */
            break;
        }

        if (c == KEY_TAB) {
            do_tab_complete(le);
            continue;
        }

        if (c == KEY_CTRL_C) {
            /* Cancel, return empty */
            term_exit_raw();
            printf("^C\n");
            return NULL;
        }

        if (c == KEY_CTRL_D) {
            /* EOF on empty line */
            if (le->buf->len == 0) {
                term_exit_raw();
                printf("\n");
                return NULL;
            }
            continue;
        }

        if (c == KEY_BACKSPACE) {
            line_buf_delete(le->buf);
            term_redraw_line(le->buf);
            continue;
        }

        if (c == 0x1b) {
            /* Escape sequence */
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

            if (seq[0] == '[') {
                /* Check for bracketed paste \e[200~ (start) / \e[201~ (end) */
                if (seq[1] == '2' || seq[1] == '2') {
                    char rest[4];
                    ssize_t rn = read(STDIN_FILENO, rest, 4);
                    if (rn == 4 && rest[0] == '0' && rest[1] == '0' && rest[2] == '~') {
                        /* \e[200~ — paste start: collect until \e[201~ */
                        while (1) {
                            char pc;
                            ssize_t pr = read(STDIN_FILENO, &pc, 1);
                            if (pr <= 0) break;
                            if (pc == 0x1b) {
                                char pseq[5];
                                if (read(STDIN_FILENO, pseq, 5) > 0 &&
                                    pseq[0] == '[' && pseq[1] == '2' &&
                                    pseq[2] == '0' && pseq[3] == '1' && pseq[4] == '~')
                                    break; /* \e[201~ — paste end */
                                /* Not paste end — re-insert ESC + sequence as literal */
                                line_buf_insert(le->buf, 0x1b);
                                for (int pi = 0; pi < 5; pi++)
                                    line_buf_insert(le->buf, (unsigned char)pseq[pi]);
                                continue;
                            }
                            if (pc == '\n' || pc == '\r') {
                                /* Insert literal newline in buffer */
                                line_buf_insert(le->buf, '\n');
                            } else if (pc >= 32 && pc < 127) {
                                line_buf_insert(le->buf, pc);
                            }
                        }
                        /* Print bracketed paste indication and redraw */
                        if (le->buf->len > 80)
                            printf("\r\033[K[pasted %zu chars]", le->buf->len);
                        else
                            printf("\r\033[K[pasted %zu chars] %s", le->buf->len, le->buf->buf);
                        fflush(stdout);
                        continue;
                    }
                    if (rn == 4 && rest[0] == '0' && rest[1] == '1' && rest[2] == '~') {
                        /* \e[201~ — paste end without start (shouldn't happen) */
                        continue;
                    }
                }
                switch (seq[1]) {
                    case KEY_UP: {
                        /* Save current line before navigating */
                        if (le->history->current == le->history->tail)
                            strncpy(le->saved_line, le->buf->buf, LINE_EDIT_MAX_LINE - 1);
                        const char *hist = history_prev(le->history);
                        if (hist) {
                            line_buf_set(le->buf, hist);
                            term_redraw_line(le->buf);
                        }
                        break;
                    }
                    case KEY_DOWN: {
                        const char *hist = history_next(le->history);
                        if (hist) {
                            line_buf_set(le->buf, hist);
                        } else if (le->saved_line[0]) {
                            line_buf_set(le->buf, le->saved_line);
                        }
                        term_redraw_line(le->buf);
                        break;
                    }
                    case KEY_RIGHT:
                        if (le->buf->cursor < le->buf->len) {
                            le->buf->cursor++;
                            term_redraw_line(le->buf);
                        }
                        break;
                    case KEY_LEFT:
                        if (le->buf->cursor > 0) {
                            le->buf->cursor--;
                            term_redraw_line(le->buf);
                        }
                        break;
                    case KEY_HOME:
                        le->buf->cursor = 0;
                        term_redraw_line(le->buf);
                        break;
                    case KEY_END:
                        le->buf->cursor = le->buf->len;
                        term_redraw_line(le->buf);
                        break;
                    case KEY_DELETE:
                        line_buf_delete_forward(le->buf);
                        term_redraw_line(le->buf);
                        break;
                }
            }
            continue;
        }

        /* Regular character — Ctrl+U (kill line), Ctrl+W (kill word) */
        if (c == KEY_CTRL_U) {
            line_buf_clear(le->buf);
            printf("\r\033[K");
            if (prompt) printf("%s", prompt);
            fflush(stdout);
            continue;
        }

        if (c == KEY_CTRL_W) {
            /* Delete word backward */
            while (le->buf->cursor > 0 && isspace((unsigned char)le->buf->buf[le->buf->cursor - 1]))
                line_buf_delete(le->buf);
            while (le->buf->cursor > 0 && !isspace((unsigned char)le->buf->buf[le->buf->cursor - 1]))
                line_buf_delete(le->buf);
            term_redraw_line(le->buf);
            continue;
        }

        /* Printable characters */
        if (c >= 32 && c <= 126) {
            line_buf_insert(le->buf, c);
            term_redraw_line(le->buf);
        }
    }

    term_exit_raw();
    printf("\n");

    if (le->buf->len == 0) {
        return strdup("");
    }

    /* Add to history */
    history_add(le->history, le->buf->buf);
    history_reset_pos(le->history);
    le->saved_line[0] = '\0';

    return strdup(le->buf->buf);
}

/* ------------------------------------------------------------------ */
/*  History persistence                                                */
/* ------------------------------------------------------------------ */

bool line_edit_save_history(line_edit_t *le, const char *path) {
    if (!le || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    hist_entry_t *e = le->history->head;
    while (e) {
        fprintf(f, "%s\n", e->text);
        e = e->next;
    }
    fclose(f);
    return true;
}

bool line_edit_load_history(line_edit_t *le, const char *path) {
    if (!le || !path) return false;
    FILE *f = fopen(path, "r");
    if (!f) return true;  /* File doesn't exist = not an error */

    char line[LINE_EDIT_MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0)
            history_add(le->history, line);
    }
    fclose(f);
    return true;
}
