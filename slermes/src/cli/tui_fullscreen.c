/*
 * tui_fullscreen.c — Full-screen ncurses TUI for Hermes C.
 * Replaces the raw-terminal CLI with a split-screen interface:
 *   - Output panel (top): scrollable conversation display
 *   - Input line (bottom): command prompt
 *   - Status bar: version, mode info
 */

#include "slermes.h"
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

/* ================================================================
 *  TUI State
 * ================================================================ */

typedef struct {
    WINDOW *output_win;       /* scrollable output area */
    WINDOW *input_win;        /* single-line input area  */
    WINDOW *status_win;       /* status bar              */
    int     rows, cols;       /* terminal dimensions     */
    int     input_row;        /* row where input starts  */
    int     status_row;       /* row where status is     */
    int     scroll_pos;       /* output scroll position  */
    int     line_count;       /* total lines in output   */
    bool    running;          /* main loop active        */
    char   *input_buf;        /* current input buffer    */
    int     input_pos;        /* cursor position in buf  */
} tui_state_t;

static tui_state_t tui;

/* Forward declarations */
void tui_fullscreen_print(const char *fmt, ...);
void tui_fullscreen_error(const char *fmt, ...);

/* ================================================================
 *  Signal handler — terminal resize
 * ================================================================ */

static void handle_winch(int sig) {
    (void)sig;
    endwin();
    refresh();
    clear();

    getmaxyx(stdscr, tui.rows, tui.cols);
    tui.status_row = tui.rows - 1;
    tui.input_row  = tui.rows - 2;

    /* Resize windows */
    wresize(tui.output_win, tui.input_row - 1, tui.cols);
    mvwin(tui.input_win, tui.input_row, 0);
    wresize(tui.input_win, 1, tui.cols);
    mvwin(tui.status_win, tui.status_row, 0);
    wresize(tui.status_win, 1, tui.cols);

    /* Redraw */
    wrefresh(tui.status_win);
    wnoutrefresh(tui.input_win);
    wnoutrefresh(tui.output_win);
    doupdate();
}

/* ================================================================
 *  Init / Cleanup
 * ================================================================ */

bool tui_fullscreen_init(void) {
    memset(&tui, 0, sizeof(tui));

    initscr();                  /* start ncurses */
    cbreak();                   /* line buffering off */
    noecho();                   /* don't echo input */
    keypad(stdscr, TRUE);       /* enable F-keys, arrows */
    curs_set(1);                /* visible cursor */
    start_color();              /* enable colors */

    /* Define color pairs */
    init_pair(1, COLOR_CYAN,   COLOR_BLACK);   /* status bar */
    init_pair(2, COLOR_GREEN,  COLOR_BLACK);   /* prompt */
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);   /* warnings */
    init_pair(4, COLOR_RED,    COLOR_BLACK);   /* errors */

    getmaxyx(stdscr, tui.rows, tui.cols);
    if (tui.rows < 5 || tui.cols < 20) {
        endwin();
        fprintf(stderr, "Terminal too small (%dx%d), need at least 20x5\n",
                tui.cols, tui.rows);
        return false;
    }

    tui.status_row = tui.rows - 1;
    tui.input_row  = tui.rows - 2;

    /* Create windows */
    tui.output_win = newwin(tui.input_row - 1, tui.cols, 0, 0);
    tui.input_win  = newwin(1, tui.cols, tui.input_row, 0);
    tui.status_win = newwin(1, tui.cols, tui.status_row, 0);

    if (!tui.output_win || !tui.input_win || !tui.status_win) {
        endwin();
        return false;
    }

    /* Enable scrolling and keypad on output window */
    scrollok(tui.output_win, TRUE);
    keypad(tui.input_win, TRUE);

    /* Setup signal handler for resize */
    signal(SIGWINCH, handle_winch);

    /* Status bar */
    wattron(tui.status_win, A_REVERSE | COLOR_PAIR(1));
    mvwprintw(tui.status_win, 0, 0, " Hermes v%s | Ctrl+C: quit | Tab: focus | PgUp/PgDn: scroll ",
              SLERMES_VERSION);
    wattroff(tui.status_win, A_REVERSE | COLOR_PAIR(1));
    wrefresh(tui.status_win);

    /* Prompt */
    wattron(tui.input_win, COLOR_PAIR(2));
    mvwprintw(tui.input_win, 0, 0, "> ");
    wattroff(tui.input_win, COLOR_PAIR(2));
    wrefresh(tui.input_win);

    /* Welcome message */
    tui_fullscreen_print("Welcome to WuBu Slermes C v%s", SLERMES_VERSION);
    tui_fullscreen_print("Type /help for commands. Press Ctrl+C to quit.");
    tui_fullscreen_print("");

    tui.running = true;
    return true;
}

void tui_fullscreen_cleanup(void) {
    if (tui.output_win) delwin(tui.output_win);
    if (tui.input_win)  delwin(tui.input_win);
    if (tui.status_win) delwin(tui.status_win);
    free(tui.input_buf);
    tui.input_buf = NULL;
    endwin();
}

/* ================================================================
 *  Output display
 * ================================================================ */

void tui_fullscreen_print(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Write to output window */
    wprintw(tui.output_win, "%s\n", buf);
    tui.line_count++;

    /* Auto-scroll to bottom unless user scrolled up */
    if (tui.scroll_pos <= 0) {
        wrefresh(tui.output_win);
    } else {
        wnoutrefresh(tui.output_win);
    }

    /* Redraw input line (may have been overwritten by scroll) */
    wnoutrefresh(tui.input_win);
    doupdate();
}

void tui_fullscreen_error(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    wattron(tui.output_win, COLOR_PAIR(4));
    wprintw(tui.output_win, "Error: %s\n", buf);
    wattroff(tui.output_win, COLOR_PAIR(4));
    tui.line_count++;

    if (tui.scroll_pos <= 0) wrefresh(tui.output_win);
    else wnoutrefresh(tui.output_win);
    wnoutrefresh(tui.input_win);
    doupdate();
}

void tui_fullscreen_warn(const char *fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    wattron(tui.output_win, COLOR_PAIR(3));
    wprintw(tui.output_win, "Warning: %s\n", buf);
    wattroff(tui.output_win, COLOR_PAIR(3));
    tui.line_count++;

    if (tui.scroll_pos <= 0) wrefresh(tui.output_win);
    else wnoutrefresh(tui.output_win);
    wnoutrefresh(tui.input_win);
    doupdate();
}

void tui_fullscreen_stream_token(const char *token) {
    /* Stream a token to the output window without newline */
    wprintw(tui.output_win, "%s", token);
    wrefresh(tui.output_win);
    wnoutrefresh(tui.input_win);
    doupdate();
}

void tui_fullscreen_stream_done(void) {
    wprintw(tui.output_win, "\n");
    tui.line_count++;
    wrefresh(tui.output_win);
    wnoutrefresh(tui.input_win);
    doupdate();
}

/* ================================================================
 *  Input handling
 * ================================================================ */

void tui_fullscreen_set_prompt(const char *text) {
    werase(tui.input_win);
    wattron(tui.input_win, COLOR_PAIR(2));
    mvwprintw(tui.input_win, 0, 0, "%s", text);
    wattroff(tui.input_win, COLOR_PAIR(2));
    wrefresh(tui.input_win);
}

/* Read a line of input. Returns allocated string or NULL on quit. */
char *tui_fullscreen_read_line(void) {
    int buf_cap = 4096;
    char *line = (char *)calloc(buf_cap, 1);
    if (!line) return NULL;
    int pos = 0;

    /* Save reference for redraw */
    free(tui.input_buf);
    tui.input_buf = NULL;
    tui.input_pos = 0;

    curs_set(1);

    while (tui.running) {
        int ch = wgetch(tui.input_win);

        switch (ch) {
        case '\n':           /* Enter */
        case '\r':
            if (pos == 0) break; /* ignore empty */
            line[pos] = '\0';
            curs_set(0);
            /* Echo input to output */
            wattron(tui.output_win, A_BOLD);
            wprintw(tui.output_win, "> %s\n", line);
            wattroff(tui.output_win, A_BOLD);
            tui.line_count++;
            wrefresh(tui.output_win);
            /* Clear input line */
            werase(tui.input_win);
            wattron(tui.input_win, COLOR_PAIR(2));
            mvwprintw(tui.input_win, 0, 0, "> ");
            wattroff(tui.input_win, COLOR_PAIR(2));
            wrefresh(tui.input_win);
            return line;

        case KEY_BACKSPACE:
        case 127:            /* DEL */
            if (pos > 0) {
                pos--;
                line[pos] = '\0';
                mvwprintw(tui.input_win, 0, 2 + pos, " ");
                wmove(tui.input_win, 0, 2 + pos);
                wrefresh(tui.input_win);
            }
            break;

        case KEY_DC:         /* Delete */
            if (pos > 0 && line[pos] != '\0') {
                memmove(&line[pos], &line[pos + 1], strlen(&line[pos + 1]) + 1);
                mvwprintw(tui.input_win, 0, 2, "%s ", line);
                wmove(tui.input_win, 0, 2 + pos);
                wrefresh(tui.input_win);
            }
            break;

        case KEY_LEFT:
            if (pos > 0) {
                pos--;
                wmove(tui.input_win, 0, 2 + pos);
                wrefresh(tui.input_win);
            }
            break;

        case KEY_RIGHT:
            if (pos < (int)strlen(line)) {
                wmove(tui.input_win, 0, 2 + pos);
                wrefresh(tui.input_win);
                pos++;
            }
            break;

        case KEY_HOME:
            pos = 0;
            wmove(tui.input_win, 0, 2);
            wrefresh(tui.input_win);
            break;

        case KEY_END:
            pos = (int)strlen(line);
            wmove(tui.input_win, 0, 2 + pos);
            wrefresh(tui.input_win);
            break;

        case KEY_PPAGE:      /* Page Up */
            tui.scroll_pos++;
            wscrl(tui.output_win, -1);
            wrefresh(tui.output_win);
            break;

        case KEY_NPAGE:      /* Page Down */
            if (tui.scroll_pos > 0) {
                tui.scroll_pos--;
                wscrl(tui.output_win, 1);
                wrefresh(tui.output_win);
            }
            break;

        case 3:              /* Ctrl+C */
            tui.running = false;
            free(line);
            return NULL;

        default:
            if (ch >= 32 && ch <= 126 && pos < buf_cap - 1) {
                line[pos] = (char)ch;
                line[pos + 1] = '\0';
                mvwaddch(tui.input_win, 0, 2 + pos, ch);
                pos++;
                wmove(tui.input_win, 0, 2 + pos);
                wrefresh(tui.input_win);
            }
            break;
        }
    }

    free(line);
    return NULL;
}

/* ================================================================
 *  Main Loop
 * ================================================================ */

int tui_fullscreen_run(void) {
    if (!tui_fullscreen_init())
        return 1;

    while (tui.running) {
        char *line = tui_fullscreen_read_line();
        if (!line) break;

        /* Process the line — caller replaces this with actual dispatch */
        /* For now, demonstrate the interface */
        if (strcmp(line, "/quit") == 0 || strcmp(line, "/exit") == 0) {
            free(line);
            break;
        }

        /* Placeholder: call CLI process_command here */
        tui_fullscreen_print("(echo) %s", line);
        free(line);
    }

    tui_fullscreen_cleanup();
    return 0;
}
