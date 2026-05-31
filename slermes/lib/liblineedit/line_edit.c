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
 * - Emacs-style keybindings: Ctrl-A/E (line start/end), Ctrl-K/Y (kill/yank),
 *   Ctrl-L (clear screen), Ctrl-T (transpose), Alt-F/B/D (word nav/kill)
 */

#include "line_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */


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
#define KEY_CTRL_A     1
#define KEY_CTRL_B     2
#define KEY_CTRL_C     3
#define KEY_CTRL_D     4
#define KEY_CTRL_E     5
#define KEY_CTRL_F     6
#define KEY_CTRL_K    11
#define KEY_CTRL_L    12
#define KEY_CTRL_T    20
#define KEY_CTRL_U    21
#define KEY_CTRL_W    23
#define KEY_CTRL_Y    25
#define KEY_ESC       27
#define KEY_CTRL_R    18

/* ------------------------------------------------------------------ */
/*  Line buffer — static helpers                                      */
/* ------------------------------------------------------------------ */

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

/* Set buffer content (used by history navigation) */
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

/* Get current editing mode */
line_edit_mode_t line_edit_get_mode(const line_edit_t *le) {
    return le ? le->vi_mode : LINE_EDIT_MODE_INSERT;
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

/* Redraw current line after cursor position — with horizontal scrolling */
static void term_redraw_line(const line_edit_t *le) {
    const line_buf_t *lb = le->buf;
    /* Get terminal width */
    struct winsize ws;
    int term_w = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 10)
        term_w = ws.ws_col;

    /* Determine visual selection bounds */
    int sel_start = -1, sel_end = -1;
    if (le->vi_visual_active) {
        size_t a = le->vi_visual_start;
        size_t b = le->buf->cursor;
        sel_start = (int)(a < b ? a : b);
        sel_end   = (int)(a < b ? b : a);
    }

    /* Reserve margin for scroll indicator */
    int margin = 3;  /* "> " prefix when scrolled */

    /* If buffer fits, simple redraw */
    if ((int)lb->len + margin <= term_w) {
        printf("\r\033[K");
        for (int i = 0; i < (int)lb->len; i++) {
            if (i >= sel_start && i < sel_end)
                printf("\033[7m%c\033[27m", lb->buf[i]);
            else
                putchar(lb->buf[i]);
        }
        size_t move_back = lb->len - lb->cursor;
        if (move_back > 0)
            printf("\033[%zuD", move_back);
        fflush(stdout);
        return;
    }

    /* Buffer wider than terminal - horizontal scroll */
    int view_w = term_w - margin;
    int cursor_vis = (int)lb->cursor;
    int scroll_offset = cursor_vis - view_w / 2;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset + view_w > (int)lb->len)
        scroll_offset = (int)lb->len - view_w;
    if (scroll_offset < 0) scroll_offset = 0;

    printf("\r\033[K> ");
    for (int i = 0; i < view_w && scroll_offset + i < (int)lb->len; i++) {
        int idx = scroll_offset + i;
        if (idx >= sel_start && idx < sel_end)
            printf("\033[7m%c\033[27m", lb->buf[idx]);
        else
            putchar(lb->buf[idx]);
    }
    int vis_cursor = cursor_vis - scroll_offset;
    int trailing = view_w - vis_cursor;
    if (trailing > 1)
        printf("\033[%dD", trailing);
    else if (trailing == 1)
        printf("\033[1D");
    fflush(stdout);
}

/* Clear screen and redraw prompt + current buffer */
static void term_clear_screen(const char *prompt, const line_buf_t *lb) {
    /* Clear screen and move cursor home */
    printf("\033[2J\033[H");
    if (prompt) printf("%s", prompt);
    if (lb && lb->len > 0) {
        printf("%s", lb->buf);
        size_t move_back = lb->len - lb->cursor;
        if (move_back > 0)
            printf("\033[%zuD", move_back);
    }
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

typedef struct history_t {
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
    le->kill_ring[0] = '\0';
    le->kill_ring_len = 0;
    le->vi_mode = LINE_EDIT_MODE_INSERT;
    le->vi_saved = false;
    le->vi_saved_line[0] = '\0';
    le->vi_last_find_char = 0;
    le->vi_last_find_forward = false;
    le->vi_last_find_till = false;
    le->vi_last_change_op = 0;
    le->vi_last_change_param = 0;
    return le;
}

void line_edit_free(line_edit_t *le) {
    if (!le) return;
    term_exit_raw();
    line_buf_free(le->buf);
    history_free(le->history);
    free(le);
}

/* Set buffer text for pre-populating input (e.g. from type-ahead) */
void line_edit_set_text(line_edit_t *le, const char *text) {
    if (!le || !text) return;
    line_buf_set(le->buf, text);
}

/* ------------------------------------------------------------------ */
/*  Word motion helpers                                               */
/* ------------------------------------------------------------------ */

/* Move cursor forward to end of next word */
void line_edit_cursor_word_forward(line_edit_t *le) {
    if (!le) return;
    line_buf_t *lb = le->buf;
    if (lb->cursor >= lb->len) return;
    /* Skip current word */
    while (lb->cursor < lb->len && !isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor++;
    /* Skip whitespace */
    while (lb->cursor < lb->len && isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor++;
}

/* Move cursor backward to start of current/previous word */
void line_edit_cursor_word_backward(line_edit_t *le) {
    if (!le) return;
    line_buf_t *lb = le->buf;
    if (lb->cursor == 0) return;
    /* Skip whitespace before cursor */
    lb->cursor--;
    while (lb->cursor > 0 && isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor--;
    /* Skip word chars */
    while (lb->cursor > 0 && !isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor--;
    /* If we stopped on a non-space, cursor is at start of word */
    if (lb->cursor > 0 && isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor++;
}

/* Move cursor to end of current/next word */
void line_edit_cursor_word_end(line_edit_t *le) {
    if (!le) return;
    line_buf_t *lb = le->buf;
    if (lb->cursor >= lb->len) return;
    /* Skip current word */
    while (lb->cursor < lb->len && !isspace((unsigned char)lb->buf[lb->cursor]))
        lb->cursor++;
    /* If we landed on a space, step back to end of word */
    if (lb->cursor > 0) lb->cursor--;
}

/* ------------------------------------------------------------------ */
/*  Emacs-style kill/yank                                             */
/* ------------------------------------------------------------------ */

/* Kill from cursor to end of line, saving killed text */
void line_edit_kill_line(line_edit_t *le) {
    if (!le) return;
    if (le->buf->cursor >= le->buf->len) return;
    size_t remain = le->buf->len - le->buf->cursor;
    if (remain >= MAX_KILL_RING) remain = MAX_KILL_RING - 1;
    memcpy(le->kill_ring, le->buf->buf + le->buf->cursor, remain);
    le->kill_ring[remain] = '\0';
    le->kill_ring_len = remain;
    /* Truncate buffer at cursor */
    le->buf->buf[le->buf->cursor] = '\0';
    le->buf->len = le->buf->cursor;
}

/* Yank (paste) killed text at cursor */
void line_edit_yank(line_edit_t *le) {
    if (!le) return;
    if (le->kill_ring_len == 0) return;
    for (size_t i = 0; i < le->kill_ring_len; i++)
        if (!line_buf_insert(le->buf, le->kill_ring[i]))
            break;
}

/* Yank entire line into kill ring (yy / Y) */
void line_edit_yank_line(line_edit_t *le) {
    if (!le) return;
    if (le->buf->len == 0) return;
    size_t to_copy = le->buf->len < MAX_KILL_RING - 1 ? le->buf->len : MAX_KILL_RING - 1;
    memcpy(le->kill_ring, le->buf->buf, to_copy);
    le->kill_ring[to_copy] = '\0';
    le->kill_ring_len = to_copy;
}

/* Kill from cursor to end of current/next word */
void line_edit_kill_word_forward(line_edit_t *le) {
    if (!le) return;
    if (le->buf->cursor >= le->buf->len) return;

    /* Find end of word */
    size_t end = le->buf->cursor;
    /* Skip whitespace */
    while (end < le->buf->len && isspace((unsigned char)le->buf->buf[end]))
        end++;
    /* Find end of word chars */
    while (end < le->buf->len && !isspace((unsigned char)le->buf->buf[end]))
        end++;

    size_t kill_len = end - le->buf->cursor;
    if (kill_len == 0) return;
    if (kill_len >= MAX_KILL_RING) kill_len = MAX_KILL_RING - 1;

    memcpy(le->kill_ring, le->buf->buf + le->buf->cursor, kill_len);
    le->kill_ring[kill_len] = '\0';
    le->kill_ring_len = kill_len;

    /* Remove the text */
    memmove(le->buf->buf + le->buf->cursor,
            le->buf->buf + le->buf->cursor + kill_len,
            le->buf->len - le->buf->cursor - kill_len + 1);
    le->buf->len -= kill_len;
}

/* Transpose characters at cursor */
void line_edit_transpose_chars(line_edit_t *le) {
    if (!le) return;
    if (le->buf->len < 2) return;
    size_t p1, p2;
    if (le->buf->cursor == 0 && le->buf->len >= 2) {
        /* At start: swap first two chars */
        p1 = 0; p2 = 1;
    } else if (le->buf->cursor == le->buf->len) {
        /* At end: swap last two chars, move cursor to end */
        p1 = le->buf->len - 2;
        p2 = le->buf->len - 1;
    } else {
        /* Middle: swap cursor-1 and cursor, advance cursor */
        p1 = le->buf->cursor - 1;
        p2 = le->buf->cursor;
    }
    char tmp = le->buf->buf[p1];
    le->buf->buf[p1] = le->buf->buf[p2];
    le->buf->buf[p2] = tmp;
    if (le->buf->cursor > 0)
        le->buf->cursor = p2 + 1;
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
        term_redraw_line(le);
    } else {
        /* Multiple matches — show them */
        printf("\n");
        int col = 0;
        for (int i = 0; i < count; i++) {
            printf("  %-20s", matches[i]);
            if (++col >= 3) { printf("\n"); col = 0; }
        }
        if (col > 0) printf("\n");
        term_redraw_line(le);
    }

    /* Free matches */
    for (int i = 0; matches[i]; i++)
        free(matches[i]);
    free(matches);
}

/* ------------------------------------------------------------------ */
/*  Vi search helper                                                   */
/* ------------------------------------------------------------------ */

/* Searches for pattern in buffer starting from cursor.
 * forward=true: forward from cursor+1, wraps to start.
 * forward=false: backward from cursor-1, wraps to end.
 * Moves cursor to match start if found. */
void line_edit_search_internal(line_edit_t *le, const char *pattern,
                                       size_t pat_len, bool forward) {
    if (!le || !pattern || pat_len == 0) return;
    line_buf_t *lb = le->buf;
    if (lb->len == 0) return;

    if (forward) {
        /* Search from cursor+1 to end */
        if (le->buf->cursor + 1 < le->buf->len) {
            char *found = strstr(lb->buf + le->buf->cursor + 1, pattern);
            if (found) {
                le->buf->cursor = (size_t)(found - lb->buf);
                return;
            }
        }
        /* Wrap: search from start up to cursor */
        size_t limit = le->buf->cursor < lb->len ? le->buf->cursor + pat_len : lb->len;
        size_t scan_end = limit > pat_len ? limit - pat_len : 0;
        for (size_t i = 0; i + pat_len <= scan_end; i++) {
            if (strncmp(lb->buf + i, pattern, pat_len) == 0) {
                le->buf->cursor = i;
                return;
            }
        }
    } else {
        /* Backward: scan from cursor-1 down to start */
        size_t start = le->buf->cursor;
        for (size_t i = start; i > 0; i--) {
            size_t check_start = i - 1;
            if (check_start + pat_len <= lb->len &&
                strncmp(lb->buf + check_start, pattern, pat_len) == 0) {
                le->buf->cursor = check_start;
                return;
            }
        }
        /* Wrap: scan from end back to cursor */
        for (size_t i = lb->len; i > start + 1; i--) {
            size_t check_start = i - pat_len;
            if (check_start < lb->len &&
                strncmp(lb->buf + check_start, pattern, pat_len) == 0 &&
                check_start + pat_len <= lb->len) {
                le->buf->cursor = check_start;
                return;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Read a line                                                       */
/* ------------------------------------------------------------------ */

char *line_edit_read(line_edit_t *le, const char *prompt) {
    if (!le) return NULL;

    line_buf_clear(le->buf);
    term_enter_raw();

    /* Print prompt with vi mode indicator */
    if (prompt) {
        if (le->vi_mode == LINE_EDIT_MODE_NORMAL) {
            /* Show mode indicator before prompt in NORMAL mode */
            printf("%s %s", le->vi_visual_active ? "[VISUAL]" : "[NORMAL]", prompt);
        } else {
            printf("%s", prompt);
        }
        fflush(stdout);
    }

    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            term_exit_raw();
            return NULL;
        }
        char c = (char)ch;

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

        if (c == KEY_CTRL_R) {
            /* Reverse history search (Ctrl-R) */
            char search_query[1024] = "";
            int query_len = 0;
            char saved_search[LINE_EDIT_MAX_LINE];
            strncpy(saved_search, le->buf->buf, LINE_EDIT_MAX_LINE - 1);
            saved_search[LINE_EDIT_MAX_LINE - 1] = '\0';
            hist_entry_t *search_entry = le->history->tail;
            bool first_search = true;

            while (1) {
                /* Search: find first match from tail (most recent to oldest) */
                if (first_search || query_len == 0) {
                    search_entry = le->history->tail;
                    first_search = false;
                } else {
                    if (search_entry) search_entry = search_entry->prev;
                }
                const char *match = NULL;
                if (query_len > 0) {
                    while (search_entry) {
                        if (strstr(search_entry->text, search_query)) {
                            match = search_entry->text;
                            break;
                        }
                        search_entry = search_entry->prev;
                    }
                }

                printf("\r\033[K");
                if (query_len > 0 && match) {
                    printf("(reverse-i-search)`%s': %s", search_query, match);
                } else if (query_len > 0) {
                    printf("(reverse-i-search)`%s': (failing)", search_query);
                } else {
                    printf("(reverse-i-search)`': ");
                }
                fflush(stdout);

                char sc;
                ssize_t sn = read(STDIN_FILENO, &sc, 1);
                if (sn <= 0) break;

                if (sc == KEY_ENTER) {
                    if (match) {
                        line_buf_set(le->buf, match);
                        term_redraw_line(le);
                    } else {
                        line_buf_set(le->buf, saved_search);
                        term_redraw_line(le);
                    }
                    break;
                }

                if (sc == KEY_CTRL_R) {
                    if (search_entry && query_len > 0)
                        search_entry = search_entry->prev;
                    continue;
                }

                if (sc == KEY_ESC || sc == 7) {
                    line_buf_set(le->buf, saved_search);
                    term_redraw_line(le);
                    break;
                }

                if (sc == KEY_BACKSPACE) {
                    if (query_len > 0) {
                        query_len--;
                        search_query[query_len] = '\0';
                    }
                    first_search = true;
                    continue;
                }

                if (sc >= 32 && sc <= 126) {
                    if (query_len < (int)sizeof(search_query) - 2) {
                        search_query[query_len++] = sc;
                        search_query[query_len] = '\0';
                    }
                    first_search = true;
                    continue;
                }
            }
            continue;
        }

        /* --- Emacs-style keybindings --- */

        /* Ctrl-A: beginning of line */
        if (c == KEY_CTRL_A) {
            le->buf->cursor = 0;
            term_redraw_line(le);
            continue;
        }

        /* Ctrl-B: backward one character */
        if (c == KEY_CTRL_B) {
            if (le->buf->cursor > 0) {
                le->buf->cursor--;
                term_redraw_line(le);
            }
            continue;
        }

        /* Ctrl-E: end of line */
        if (c == KEY_CTRL_E) {
            le->buf->cursor = le->buf->len;
            term_redraw_line(le);
            continue;
        }

        /* Ctrl-F: forward one character */
        if (c == KEY_CTRL_F) {
            if (le->buf->cursor < le->buf->len) {
                le->buf->cursor++;
                term_redraw_line(le);
            }
            continue;
        }

        /* Ctrl-K: kill to end of line */
        if (c == KEY_CTRL_K) {
            line_edit_kill_line(le);
            term_redraw_line(le);
            continue;
        }

        /* Ctrl-L: clear screen / redraw */
        if (c == KEY_CTRL_L) {
            term_clear_screen(prompt, le->buf);
            continue;
        }

        /* Ctrl-T: transpose characters */
        if (c == KEY_CTRL_T) {
            line_edit_transpose_chars(le);
            term_redraw_line(le);
            continue;
        }

        /* Ctrl-Y: yank (paste killed text) */
        if (c == KEY_CTRL_Y) {
            line_edit_yank(le);
            term_redraw_line(le);
            continue;
        }

        if (c == KEY_BACKSPACE) {
            line_buf_delete(le->buf);
            term_redraw_line(le);
            continue;
        }

        if (c == KEY_ESC) {
            /* Always read escape sequence first, regardless of mode.
             * Arrow keys (UP/DOWN/RIGHT/LEFT) and other ESC-prefixed sequences
             * must be consumed BEFORE mode handling, otherwise the \x1b from
             * an arrow key press toggles INSERT->NORMAL and the trailing bytes
             * get interpreted as vi commands (e.g. 'A' in "ESC[A" = append).
             *
             * CRITICAL: use getchar() not read(STDIN_FILENO) because getchar()
             * uses stdio buffered I/O. When the terminal sends \x1b[A as 3 bytes,
             * getchar() may read all 3 into the stdio buffer but only return \x1b.
             * Reading from the raw fd would miss [A stranded in the stdio buffer. */
            char seq[3];
            int next_ch = getchar();
            if (next_ch == EOF) continue;
            seq[0] = (char)next_ch;
            next_ch = getchar();
            if (next_ch == EOF) continue;
            seq[1] = (char)next_ch;

            if (seq[0] == '[') {
                /* CSI sequence */
                /* Check for bracketed paste \e[200~ (start) / \e[201~ (end) */
                if (seq[1] == '2') {
                    /* Read rest of \e[200~ or \e[201~ via getchar (same buffer as seq) */
                    char rest[4];
                    int ri;
                    for (ri = 0; ri < 4; ri++) {
                        int rc = getchar();
                        if (rc == EOF) break;
                        rest[ri] = (char)rc;
                    }
                    if (ri == 4 && rest[0] == '0' && rest[1] == '0' && rest[2] == '~') {
                        /* \e[200~ — paste start: collect until \e[201~ */
                        while (1) {
                            int pc_ch = getchar();
                            if (pc_ch == EOF) break;
                            char pc = (char)pc_ch;
                            if (pc == KEY_ESC) {
                                char pseq[5];
                                int pi;
                                for (pi = 0; pi < 5; pi++) {
                                    int rc = getchar();
                                    if (rc == EOF) break;
                                    pseq[pi] = (char)rc;
                                }
                                if (pi == 5 &&
                                    pseq[0] == '[' && pseq[1] == '2' &&
                                    pseq[2] == '0' && pseq[3] == '1' && pseq[4] == '~')
                                    break; /* \e[201~ — paste end */
                                /* Not paste end — re-insert ESC + sequence as literal */
                                line_buf_insert(le->buf, KEY_ESC);
                                for (int pj = 0; pj < pi; pj++)
                                    line_buf_insert(le->buf, (unsigned char)pseq[pj]);
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
                    if (ri == 4 && rest[0] == '0' && rest[1] == '1' && rest[2] == '~') {
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
                            term_redraw_line(le);
                        }
                        continue; /* skip "Standalone ESC" fallthrough below */
                    }
                    case KEY_DOWN: {
                        const char *hist = history_next(le->history);
                        if (hist) {
                            line_buf_set(le->buf, hist);
                        } else if (le->saved_line[0]) {
                            line_buf_set(le->buf, le->saved_line);
                        }
                        term_redraw_line(le);
                        continue; /* skip "Standalone ESC" fallthrough below */
                    }
                    case KEY_RIGHT:
                        if (le->buf->cursor < le->buf->len) {
                            le->buf->cursor++;
                            term_redraw_line(le);
                        }
                        continue;
                    case KEY_LEFT:
                        if (le->buf->cursor > 0) {
                            le->buf->cursor--;
                            term_redraw_line(le);
                        }
                        continue;
                    case KEY_HOME:
                        le->buf->cursor = 0;
                        term_redraw_line(le);
                        continue;
                    case KEY_END:
                        le->buf->cursor = le->buf->len;
                        term_redraw_line(le);
                        continue;
                    case KEY_DELETE:
                        line_buf_delete_forward(le->buf);
                        term_redraw_line(le);
                        continue;
                }
            }

            /* Alt-letter / Meta keybindings: ESC followed by a char */

            /* Alt+F (forward word) */
            if (seq[0] == 'f' && seq[1] < 32) {
                line_edit_cursor_word_forward(le);
                term_redraw_line(le);
            }

            /* Alt+B (backward word) */
            if (seq[0] == 'b' && seq[1] < 32) {
                line_edit_cursor_word_backward(le);
                term_redraw_line(le);
            }

            /* Alt+D (kill word forward) */
            if (seq[0] == 'd' && seq[1] < 32) {
                line_edit_kill_word_forward(le);
                term_redraw_line(le);
            }

            /* Alt+Enter (ESC \r) — insert literal newline */
            if (seq[0] == '\r') {
                line_buf_insert(le->buf, '\n');
                term_redraw_line(le);
            }

            /* Standalone ESC (not an arrow/Alt sequence): switch to NORMAL mode */
            if (le->vi_mode == LINE_EDIT_MODE_INSERT) {
                strncpy(le->vi_saved_line, le->buf->buf, LINE_EDIT_MAX_LINE - 1);
                le->vi_saved_line[LINE_EDIT_MAX_LINE - 1] = '\0';
                le->vi_saved = true;
                le->vi_mode = LINE_EDIT_MODE_NORMAL;
                printf("\r\033[K%s", le->vi_visual_active ? "[VISUAL]" : "[NORMAL]");
                if (prompt) printf("%s", prompt);
                term_redraw_line(le);
                fflush(stdout);
            }
            continue;
        }

        /* Ctrl-U (kill line / clear to start) */
        if (c == KEY_CTRL_U) {
            line_buf_clear(le->buf);
            printf("\r\033[K");
            if (prompt) printf("%s", prompt);
            fflush(stdout);
            continue;
        }

        /* Ctrl-W (kill word backward) */
        if (c == KEY_CTRL_W) {
            /* Delete word backward */
            while (le->buf->cursor > 0 && isspace((unsigned char)le->buf->buf[le->buf->cursor - 1]))
                line_buf_delete(le->buf);
            while (le->buf->cursor > 0 && !isspace((unsigned char)le->buf->buf[le->buf->cursor - 1]))
                line_buf_delete(le->buf);
            term_redraw_line(le);
            continue;
        }

        /* Ctrl-P: previous history (like up arrow) */
        if (c == 16) {
            if (le->history->current == le->history->tail)
                strncpy(le->saved_line, le->buf->buf, LINE_EDIT_MAX_LINE - 1);
            const char *hist = history_prev(le->history);
            if (hist) {
                line_buf_set(le->buf, hist);
                term_redraw_line(le);
            }
            continue;
        }

        /* Ctrl-N: next history (like down arrow) */
        if (c == 14) {
            const char *hist = history_next(le->history);
            if (hist) {
                line_buf_set(le->buf, hist);
            } else if (le->saved_line[0]) {
                line_buf_set(le->buf, le->saved_line);
            }
            term_redraw_line(le);
            continue;
        }

        /* Vi NORMAL mode keybindings */
        if (le->vi_mode == LINE_EDIT_MODE_NORMAL) {
            bool handled = true;

            /* Count prefix: digits 1-9 accumulate as repeat count */
            if (c >= '1' && c <= '9') {
                le->vi_count = le->vi_count * 10 + (c - '0');
                continue;
            }

            /* Visual mode: intercept v/V/ESC/x/d/y before NORMAL dispatch */
            if (le->vi_visual_active) {
                if (c == 'v' || c == 'V' || c == 27) {
                    /* Exit visual mode */
                    le->vi_visual_active = false;
                } else if (c == 'x' || c == 'd') {
                    /* Delete selection into kill ring */
                    size_t a = le->vi_visual_start;
                    size_t b = le->buf->cursor;
                    size_t sel_s = a < b ? a : b;
                    size_t sel_e = a < b ? b : a;
                    size_t sel_len = sel_e - sel_s;
                    if (sel_len > 0 && sel_len < MAX_KILL_RING - 1) {
                        memcpy(le->kill_ring, le->buf->buf + sel_s, sel_len);
                        le->kill_ring[sel_len] = '\0';
                        le->kill_ring_len = sel_len;
                    }
                    /* Delete selection from buffer */
                    if (sel_len > 0) {
                        memmove(le->buf->buf + sel_s, le->buf->buf + sel_e,
                                le->buf->len - sel_e + 1);
                        le->buf->len -= sel_len;
                        le->buf->cursor = sel_s;
                    }
                    le->vi_visual_active = false;
                } else if (c == 'y') {
                    /* Yank selection into kill ring */
                    size_t a = le->vi_visual_start;
                    size_t b = le->buf->cursor;
                    size_t sel_s = a < b ? a : b;
                    size_t sel_e = a < b ? b : a;
                    size_t sel_len = sel_e - sel_s;
                    if (sel_len > 0 && sel_len < MAX_KILL_RING - 1) {
                        memcpy(le->kill_ring, le->buf->buf + sel_s, sel_len);
                        le->kill_ring[sel_len] = '\0';
                        le->kill_ring_len = sel_len;
                    }
                    le->vi_visual_active = false;
                }
                /* For all other keys (movement), fall through to switch below */
            }

            switch (c) {
                case 'h': /* left */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep && le->buf->cursor > 0; _i++)
                        le->buf->cursor--;
                    break;
                }
                case 'l': /* right */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep && le->buf->cursor < le->buf->len; _i++)
                        le->buf->cursor++;
                    break;
                }
                case 'j': /* down (next history) */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    if (rep > 1) { /* skip first if count > 1 */
                        for (int _i = 1; _i < rep && history_next(le->history); _i++);
                    }
                    const char *hist = history_next(le->history);
                    if (hist) {
                        line_buf_set(le->buf, hist);
                    } else if (le->saved_line[0]) {
                        line_buf_set(le->buf, le->saved_line);
                    }
                    break;
                }
                case 'k': /* up (prev history) */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++) {
                        if (le->history->current == le->history->tail)
                            strncpy(le->saved_line, le->buf->buf, LINE_EDIT_MAX_LINE - 1);
                        const char *hist = history_prev(le->history);
                        if (!hist) break;
                        if (_i == rep - 1 && hist)
                            line_buf_set(le->buf, hist);
                    }
                    break;
                }
                case '0': /* go to line start */
                    le->buf->cursor = 0;
                    break;
                case '$': /* go to line end */
                    le->buf->cursor = le->buf->len;
                    break;
                case '^': /* first non-whitespace */
                {
                    size_t pos = 0;
                    while (pos < le->buf->len && isspace((unsigned char)le->buf->buf[pos]))
                        pos++;
                    le->buf->cursor = pos;
                    break;
                }
                case '_': /* last non-whitespace */
                {
                    size_t pos = le->buf->len;
                    while (pos > 0) {
                        pos--;
                        if (!isspace((unsigned char)le->buf->buf[pos])) {
                            pos++;
                            break;
                        }
                    }
                    le->buf->cursor = pos;
                    break;
                }
                case 'x': /* delete char under cursor */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_buf_delete_forward(le->buf);
                    le->vi_last_change_op = 'x';
                    break;
                }
                case 'X': /* delete char before cursor */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_buf_delete(le->buf);
                    le->vi_last_change_op = 'X';
                    break;
                }
                case 'i': /* insert before cursor */
                case 'I': /* insert at line start */
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    if (c == 'I') le->buf->cursor = 0;
                    break;
                case 'a': /* append after cursor */
                case 'A': /* append at line end */
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    if (c == 'A') le->buf->cursor = le->buf->len;
                    else if (le->buf->cursor < le->buf->len) le->buf->cursor++;
                    break;
                case 'u': /* undo */
                    if (le->vi_saved) {
                        line_buf_set(le->buf, le->vi_saved_line);
                        le->vi_saved = false;
                    }
                    break;
                case 'v': /* visual mode (character-wise) */
                case 'V': /* visual mode (line-wise, same behavior for now) */
                    le->vi_visual_active = true;
                    le->vi_visual_start = le->buf->cursor;
                    break;
                case 'd': /* dd — delete line */
                {
                    /* Check for second d (dd sequence) */
                    char next = 'd';
                    if (read(STDIN_FILENO, &next, 1) <= 0 || next != 'd') {
                        /* Not dd — do nothing */
                        if (next > 0 && next != 'd') {
                            /* Put back via ungetc or just ignore */
                        }
                    } else {
                        /* dd — delete entire line into kill ring */
                        size_t orig_len = le->buf->len;
                        if (orig_len > 0) {
                            size_t to_copy = orig_len < MAX_KILL_RING - 1 ? orig_len : MAX_KILL_RING - 1;
                            memcpy(le->kill_ring, le->buf->buf, to_copy);
                            le->kill_ring[to_copy] = '\0';
                            le->kill_ring_len = to_copy;
                        }
                        line_buf_clear(le->buf);
                        le->vi_last_change_op = 'd';
                    }
                    break;
                }
                case 'y': /* yy — yank whole line */
                {
                    char next;
                    if (read(STDIN_FILENO, &next, 1) <= 0 || next != 'y') {
                        if (next > 0 && next != 'y') { /* ignore non-yy */ }
                    } else {
                        line_edit_yank_line(le);
                    }
                    break;
                }
                case 'Y': /* yank whole line */
                    line_edit_yank_line(le);
                    break;
                case 'p': /* paste after cursor */
                    if (le->kill_ring_len > 0) {
                        /* Insert killed text at current position */
                        for (size_t ki = 0; ki < le->kill_ring_len; ki++)
                            line_buf_insert(le->buf, le->kill_ring[ki]);
                    }
                    break;
                case 'P': /* paste before cursor */
                    if (le->kill_ring_len > 0) {
                        /* Insert killed text at current position (same as 'p' for our model) */
                        for (size_t ki = 0; ki < le->kill_ring_len; ki++)
                            line_buf_insert(le->buf, le->kill_ring[ki]);
                    }
                    break;
                case 'w': /* word forward */
                case 'W': {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_edit_cursor_word_forward(le);
                    break;
                }
                case 'b': /* word backward */
                case 'B': {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_edit_cursor_word_backward(le);
                    break;
                }
                case 'e': /* word end */
                case 'E': {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_edit_cursor_word_end(le);
                    break;
                }
                case 'D': /* delete to end of line */
                    line_edit_kill_line(le);
                    le->vi_last_change_op = 'D';
                    break;
                case 'C': /* change to end of line */
                    line_edit_kill_line(le);
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    le->vi_last_change_op = 'C';
                    break;
                case 's': /* substitute char (delete + insert) */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep; _i++)
                        line_buf_delete_forward(le->buf);
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    le->vi_last_change_op = 's';
                    break;
                }
                case 'o': /* open line below, enter INSERT */
                {
                    le->buf->cursor = le->buf->len;
                    line_buf_insert(le->buf, '\n');
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    break;
                }
                case 'O': /* open line above, enter INSERT */
                {
                    le->buf->cursor = 0;
                    line_buf_insert(le->buf, '\n');
                    le->buf->cursor = 0;
                    le->vi_mode = LINE_EDIT_MODE_INSERT;
                    break;
                }
                case '%': /* jump to matching bracket () [] {} */
                {
                    if (le->buf->cursor < le->buf->len) {
                        char c = le->buf->buf[le->buf->cursor];
                        char open_c, close_c;
                        int dir;
                        if (c == '(') { open_c = '('; close_c = ')'; dir = 1; }
                        else if (c == ')') { open_c = '('; close_c = ')'; dir = -1; }
                        else if (c == '[') { open_c = '['; close_c = ']'; dir = 1; }
                        else if (c == ']') { open_c = '['; close_c = ']'; dir = -1; }
                        else if (c == '{') { open_c = '{'; close_c = '}'; dir = 1; }
                        else if (c == '}') { open_c = '{'; close_c = '}'; dir = -1; }
                        else break;
                        int depth = 1;
                        size_t pos = le->buf->cursor;
                        while (depth > 0) {
                            if (dir > 0) {
                                pos++;
                                if (pos >= le->buf->len) break;
                                if (le->buf->buf[pos] == open_c) depth++;
                                else if (le->buf->buf[pos] == close_c) depth--;
                            } else {
                                if (pos == 0) break;
                                pos--;
                                if (le->buf->buf[pos] == open_c) depth--;
                                else if (le->buf->buf[pos] == close_c) depth++;
                            }
                        }
                        if (depth == 0) le->buf->cursor = pos;
                    }
                    break;
                }
                case '.': /* repeat last change */
                {
                    le->vi_count = 0;
                    if (le->vi_last_change_op) {
                        switch (le->vi_last_change_op) {
                            case 'x': line_buf_delete_forward(le->buf); break;
                            case 'X': line_buf_delete(le->buf); break;
                            case '~':
                                if (le->buf->cursor < le->buf->len) {
                                    unsigned char uc = (unsigned char)le->buf->buf[le->buf->cursor];
                                    if (uc >= 'a' && uc <= 'z')
                                        le->buf->buf[le->buf->cursor] = uc - 32;
                                    else if (uc >= 'A' && uc <= 'Z')
                                        le->buf->buf[le->buf->cursor] = uc + 32;
                                }
                                break;
                            case 'r':
                                if (le->buf->cursor < le->buf->len && le->vi_last_change_param >= 32)
                                    le->buf->buf[le->buf->cursor] = le->vi_last_change_param;
                                break;
                            case 's':
                                line_buf_delete_forward(le->buf);
                                le->vi_mode = LINE_EDIT_MODE_INSERT;
                                break;
                            case 'D': line_edit_kill_line(le); break;
                            case 'C': line_edit_kill_line(le); le->vi_mode = LINE_EDIT_MODE_INSERT; break;
                            case 'd': /* dd — delete whole line */
                            {
                                size_t orig_len = le->buf->len;
                                if (orig_len > 0) {
                                    size_t to_copy = orig_len < MAX_KILL_RING - 1 ? orig_len : MAX_KILL_RING - 1;
                                    memcpy(le->kill_ring, le->buf->buf, to_copy);
                                    le->kill_ring[to_copy] = '\0';
                                    le->kill_ring_len = to_copy;
                                }
                                line_buf_clear(le->buf);
                                break;
                            }
                        }
                        term_redraw_line(le);
                    }
                    break;
                }
                case 'r': /* replace char under cursor */
                {
                    char next;
                    if (read(STDIN_FILENO, &next, 1) > 0 && next >= 32 && next <= 126) {
                        int rep = le->vi_count > 0 ? le->vi_count : 1;
                        le->vi_count = 0;
                        for (int _i = 0; _i < rep && le->buf->cursor + _i < le->buf->len; _i++)
                            le->buf->buf[le->buf->cursor + _i] = next;
                        le->vi_last_change_op = 'r';
                        le->vi_last_change_param = next;
                        term_redraw_line(le);
                    }
                    break;
                }
                case '~': /* toggle case of chars */ {
                    int rep = le->vi_count > 0 ? le->vi_count : 1;
                    le->vi_count = 0;
                    for (int _i = 0; _i < rep && le->buf->cursor < le->buf->len; _i++) {
                        unsigned char uc = (unsigned char)le->buf->buf[le->buf->cursor];
                        if (uc >= 'a' && uc <= 'z')
                            le->buf->buf[le->buf->cursor] = uc - 32;
                        else if (uc >= 'A' && uc <= 'Z')
                            le->buf->buf[le->buf->cursor] = uc + 32;
                        if (le->buf->cursor < le->buf->len)
                            le->buf->cursor++;
                    }
                    le->vi_last_change_op = '~';
                    term_redraw_line(le);
                    break;
                }
                case 'f': /* find char forward */
                case 'F': /* find char backward */
                case 't': /* till char forward (stops before) */
                case 'T': /* till char backward (stops after) */
                {
                    char target;
                    if (read(STDIN_FILENO, &target, 1) <= 0) break;
                    le->vi_last_find_char = target;
                    le->vi_last_find_forward = (c == 'f' || c == 't');
                    le->vi_last_find_till = (c == 't' || c == 'T');
                    /* Search for target */
                    line_buf_t *lb = le->buf;
                    if (c == 'f' || c == 't') {
                        /* Forward search */
                        size_t start = le->buf->cursor + 1;
                        for (size_t i = start; i < lb->len; i++) {
                            if (lb->buf[i] == (unsigned char)target) {
                                le->buf->cursor = (c == 't') ? (i - 1) : i;
                                break;
                            }
                        }
                    } else {
                        /* Backward search (F/T) */
                        size_t end = le->buf->cursor;
                        for (size_t i = end; i > 0; i--) {
                            if (lb->buf[i - 1] == (unsigned char)target) {
                                le->buf->cursor = (c == 'T') ? i : (i - 1);
                                break;
                            }
                        }
                    }
                    break;
                }
                case ';': /* repeat last f/F/t/T */
                {
                    if (le->vi_last_find_char && le->vi_last_find_char) {
                        line_buf_t *lb = le->buf;
                        if (le->vi_last_find_forward) {
                            size_t start = le->buf->cursor + 1;
                            for (size_t i = start; i < lb->len; i++) {
                                if (lb->buf[i] == (unsigned char)le->vi_last_find_char) {
                                    le->buf->cursor = le->vi_last_find_till ? (i - 1) : i;
                                    break;
                                }
                            }
                        } else {
                            size_t end = le->buf->cursor;
                            for (size_t i = end; i > 0; i--) {
                                if (lb->buf[i - 1] == (unsigned char)le->vi_last_find_char) {
                                    le->buf->cursor = le->vi_last_find_till ? i : (i - 1);
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
                case ',': /* repeat last f/F/t/T in opposite direction */
                {
                    if (le->vi_last_find_char) {
                        line_buf_t *lb = le->buf;
                        if (!le->vi_last_find_forward) {
                            size_t start = le->buf->cursor + 1;
                            for (size_t i = start; i < lb->len; i++) {
                                if (lb->buf[i] == (unsigned char)le->vi_last_find_char) {
                                    le->buf->cursor = le->vi_last_find_till ? (i - 1) : i;
                                    break;
                                }
                            }
                        } else {
                            size_t end = le->buf->cursor;
                            for (size_t i = end; i > 0; i--) {
                                if (lb->buf[i - 1] == (unsigned char)le->vi_last_find_char) {
                                    le->buf->cursor = le->vi_last_find_till ? i : (i - 1);
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
                case '/':  /* search forward */
                case '?':  /* search backward */
                {
                    bool forward = (c == '/');
                    /* Show search prompt */
                    printf("\r\033[K%s", forward ? "/" : "?");
                    fflush(stdout);

                    /* Read search pattern character by character */
                    char pattern[LINE_EDIT_MAX_LINE] = {0};
                    size_t pat_len = 0;
                    char ch;
                    while (read(STDIN_FILENO, &ch, 1) > 0) {
                        if (ch == '\n' || ch == '\r') {
                            break;  /* Execute search */
                        }
                        if (ch == 27) { /* ESC */
                            char esc_seq[2];
                            /* Check for escape sequence (arrow keys etc.) */
                            if (read(STDIN_FILENO, esc_seq, 1) == 1 && esc_seq[0] == '[') {
                                /* Arrow key — cancel and consume rest */
                                read(STDIN_FILENO, esc_seq, 1);
                                pattern[0] = '\0';
                                pat_len = 0;
                            } else {
                                /* Bare ESC — cancel search */
                                pattern[0] = '\0';
                                pat_len = 0;
                            }
                            break;
                        }
                        if (ch == KEY_BACKSPACE || ch == 8) {
                            if (pat_len > 0) {
                                pat_len--;
                                pattern[pat_len] = '\0';
                                printf("\b \b");
                                fflush(stdout);
                            }
                            continue;
                        }
                        if (ch >= 32 && ch <= 126 && pat_len < LINE_EDIT_MAX_LINE - 1) {
                            pattern[pat_len++] = ch;
                            pattern[pat_len] = '\0';
                            putchar(ch);
                            fflush(stdout);
                        }
                    }

                    /* Store pattern and execute search */
                    if (pat_len > 0) {
                        memcpy(le->vi_search_pattern, pattern, pat_len + 1);
                        le->vi_search_pattern_len = pat_len;
                        le->vi_last_search_forward = forward;
                        line_edit_search_internal(le, pattern, pat_len, forward);
                    }
                    break;
                }
                case 'n': /* repeat last search in same direction */
                {
                    if (le->vi_search_pattern_len > 0) {
                        line_edit_search_internal(le, le->vi_search_pattern,
                            le->vi_search_pattern_len, le->vi_last_search_forward);
                    }
                    break;
                }
                case 'N': /* repeat last search in opposite direction */
                {
                    if (le->vi_search_pattern_len > 0) {
                        line_edit_search_internal(le, le->vi_search_pattern,
                            le->vi_search_pattern_len, !le->vi_last_search_forward);
                    }
                    break;
                }
                default:
                    handled = false;
                    le->vi_count = 0;  /* reset count on unknown commands */
                    break;
            }
            if (handled) {
                /* Redraw with mode indicator */
            printf("\r\033[K%s", le->vi_visual_active ? "[VISUAL]" : "[NORMAL]");
                if (prompt) printf("%s", prompt);
                term_redraw_line(le);
                fflush(stdout);
                continue;
            }
        }

        /* Printable characters */
        if (c >= 32 && c <= 126) {
            line_buf_insert(le->buf, c);
            term_redraw_line(le);
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
