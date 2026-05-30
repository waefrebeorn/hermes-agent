/*
 * ansi.c — ANSI terminal escape code helpers (J22).
 * Wraps/strips ANSI codes, detects terminal support.
 */
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

char *ansi_wrap(const char *text, const char *fg, const char *style) {
    if (!text) return NULL;
    size_t tlen = strlen(text);
    size_t fg_len = fg ? strlen(fg) : 0;
    size_t st_len = style ? strlen(style) : 0;
    size_t reset_len = strlen(ANSI_RESET);
    size_t total = fg_len + st_len + tlen + reset_len + 1;
    char *out = malloc(total);
    if (!out) return NULL;
    size_t pos = 0;
    if (fg)  { memcpy(out + pos, fg, fg_len);  pos += fg_len; }
    if (style) { memcpy(out + pos, style, st_len); pos += st_len; }
    memcpy(out + pos, text, tlen); pos += tlen;
    memcpy(out + pos, ANSI_RESET, reset_len); pos += reset_len;
    out[pos] = '\0';
    return out;
}

bool ansi_supported(void) {
    const char *term = getenv("TERM");
    if (!term || !term[0]) return false;
    /* Common non-ANSI terminals */
    if (strcmp(term, "dumb") == 0 || strcmp(term, "cons25") == 0)
        return false;
    if (!isatty(STDOUT_FILENO)) return false;
    return true;
}

int ansi_term_width(void) {
#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
#endif
    const char *cols = getenv("COLUMNS");
    if (cols) {
        int v = atoi(cols);
        if (v > 0) return v;
    }
    return 80;
}

int ansi_term_height(void) {
#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
        return (int)ws.ws_row;
#endif
    const char *lines = getenv("LINES");
    if (lines) {
        int v = atoi(lines);
        if (v > 0) return v;
    }
    return 24;
}

/* ── Truecolor helpers ──────────────────────────────────────── */

bool ansi_parse_hex(const char *hex, int *r, int *g, int *b) {
    if (!hex || !r || !g || !b) return false;
    /* Skip leading # if present */
    if (hex[0] == '#') hex++;
    if (strlen(hex) != 6) return false;
    char buf[3];
    buf[0] = hex[0]; buf[1] = hex[1]; buf[2] = '\0';
    char *end = NULL;
    long rv = strtol(buf, &end, 16);
    if (end && *end) return false;
    buf[0] = hex[2]; buf[1] = hex[3];
    long gv = strtol(buf, &end, 16);
    if (end && *end) return false;
    buf[0] = hex[4]; buf[1] = hex[5];
    long bv = strtol(buf, &end, 16);
    if (end && *end) return false;
    *r = (int)rv; *g = (int)gv; *b = (int)bv;
    return true;
}

char *ansi_fg_hex(const char *hex) {
    int r, g, b;
    if (!ansi_parse_hex(hex, &r, &g, &b)) return NULL;
    char *out = (char *)malloc(24); /* "\033[38;2;255;255;255m" max */
    if (!out) return NULL;
    snprintf(out, 24, "\033[38;2;%d;%d;%dm", r, g, b);
    return out;
}

char *ansi_bg_hex(const char *hex) {
    int r, g, b;
    if (!ansi_parse_hex(hex, &r, &g, &b)) return NULL;
    char *out = (char *)malloc(24);
    if (!out) return NULL;
    snprintf(out, 24, "\033[48;2;%d;%d;%dm", r, g, b);
    return out;
}
