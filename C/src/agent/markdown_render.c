/*
 * markdown_render.c — Render markdown text to ANSI-escaped terminal output.
 *
 * Handles: headers, bold, italic, inline code, fenced code blocks,
 * links, lists, blockquotes, horizontal rules.
 * Tables pass through (handled separately by markdown_tables.c).
 *
 * ANSI color scheme (matches Python Rich's markdown theme):
 *   Headers: bold + color gradient (h1=bright yellow, h2=cyan, h3=green, h4=blue)
 *   Bold: ANSI bold (\\x1B[1m)
 *   Italic: ANSI italic (\\x1B[3m)
 *   Inline code: dark background + bright foreground (\\x1B[48;5;236;38;5;215m)
 *   Code blocks: dark background (\\x1B[48;5;235m) with border
 *   Links: underline + blue (\\x1B[4;38;5;33m)
 *   Lists: bullet with indent
 *   Blockquotes: dim foreground (\\x1B[2;38;5;244m) with │ prefix
 *   HR: dim line
 */

#include "hermes_markdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 *  ANSI escape constants
 * ────────────────────────────────────────────── */

#define RST   "\x1B[0m"
#define BOLD  "\x1B[1m"
#define DIM   "\x1B[2m"
#define ITAL  "\x1B[3m"
#define ULIN  "\x1B[4m"

/* Colors for headers */
#define H1_CLR "\x1B[1;38;2;255;215;0m"   /* bright yellow */
#define H2_CLR "\x1B[1;38;2;0;215;255m"    /* cyan */
#define H3_CLR "\x1B[1;38;2;0;255;128m"    /* green */
#define H4_CLR "\x1B[1;38;2;100;149;237m"  /* cornflower blue */
#define H5_CLR "\x1B[1;38;2;180;130;255m"  /* purple */
#define H6_CLR "\x1B[1;38;2;255;100;100m"  /* dim red */

#define CODE_BG  "\x1B[48;5;236;38;5;215m" /* inline code: dark bg + orange fg */
#define CODE_BG_BLOCK "\x1B[48;5;235m"     /* code block: very dark bg */
#define LINK_CLR "\x1B[4;38;5;33m"         /* links: underline + blue */
#define BQ_CLR   "\x1B[2;38;5;244m"        /* blockquotes: dim gray */
#define HR_CLR   "\x1B[2;38;5;240m"        /* HR: dim */

/* ──────────────────────────────────────────────
 *  Dynamic string builder
 * ────────────────────────────────────────────── */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sbuf_t;

static sbuf_t *sbuf_new(void) {
    sbuf_t *s = (sbuf_t *)calloc(1, sizeof(sbuf_t));
    if (!s) return NULL;
    s->cap = 1024;
    s->buf = (char *)malloc(s->cap);
    if (!s->buf) { free(s); return NULL; }
    s->buf[0] = '\0';
    return s;
}

static void sbuf_free(sbuf_t *s) {
    if (s) { free(s->buf); free(s); }
}

static bool sbuf_grow(sbuf_t *s, size_t need) {
    if (s->len + need < s->cap) return true;
    size_t new_cap = s->cap * 2;
    while (s->len + need >= new_cap) new_cap *= 2;
    char *nb = (char *)realloc(s->buf, new_cap);
    if (!nb) return false;
    s->buf = nb;
    s->cap = new_cap;
    return true;
}

#define sbuf_add(s, str) do { \
    size_t _l = strlen(str); \
    if (sbuf_grow(s, _l + 1)) { \
        memcpy(s->buf + s->len, str, _l); \
        s->len += _l; \
        s->buf[s->len] = '\0'; \
    } \
} while(0)

#define sbuf_addc(s, ch) do { \
    if (sbuf_grow(s, 2)) { \
        s->buf[s->len++] = (ch); \
        s->buf[s->len] = '\0'; \
    } \
} while(0)

/* ──────────────────────────────────────────────
 *  Line-type detection
 * ────────────────────────────────────────────── */

static bool is_header(const char *line, int *level) {
    int n = 0;
    while (line[n] == '#') n++;
    if (n < 1 || n > 6) return false;
    if (line[n] != ' ' && line[n] != '\t') return false;
    *level = n;
    return true;
}

static bool is_code_fence(const char *line) {
    const char *p = line;
    while (*p == ' ') p++;
    return (strncmp(p, "```", 3) == 0);
}

static bool is_hr(const char *line) {
    const char *p = line;
    while (*p == ' ') p++;
    if (strncmp(p, "---", 3) == 0 || strncmp(p, "***", 3) == 0 || strncmp(p, "___", 3) == 0)
        return true;
    return false;
}

static bool is_bq(const char *line) {
    const char *p = line;
    while (*p == ' ') p++;
    return (*p == '>');
}

static bool is_list(const char *line) {
    const char *p = line;
    while (*p == ' ') p++;
    return (*p == '-' || *p == '*' || *p == '+');
}

/* ──────────────────────────────────────────────
 *  Inline markdown processing
 *  Processes bold, italic, code, links on one line
 * ────────────────────────────────────────────── */

static void render_inline(sbuf_t *out, const char *line) {
    const char *p = line;
    int in_bold = 0, in_italic = 0, in_code = 0;

    while (*p) {
        if (in_code) {
            /* Inside inline code — find closing backtick */
            if (*p == '`') {
                sbuf_add(out, RST);
                in_code = 0;
                p++;
                continue;
            }
            sbuf_addc(out, *p);
            p++;
            continue;
        }

        /* Check for inline code first (highest precedence) */
        if (*p == '`') {
            sbuf_add(out, CODE_BG);
            in_code = 1;
            p++;
            continue;
        }

        /* Bold: **text** */
        if (p[0] == '*' && p[1] == '*') {
            if (in_bold) {
                sbuf_add(out, RST);
                in_bold = 0;
            } else {
                sbuf_add(out, BOLD);
                in_bold = 1;
            }
            p += 2;
            continue;
        }

        /* Italic: *text* (single asterisk, not part of **) */
        if (*p == '*' && p[1] != '*') {
            if (in_italic) {
                sbuf_add(out, RST);
                in_italic = 0;
            } else {
                sbuf_add(out, ITAL);
                in_italic = 1;
            }
            p++;
            continue;
        }

        /* Link: [text](url) */
        if (*p == '[') {
            const char *close_b = strchr(p + 1, ']');
            if (close_b && close_b[1] == '(') {
                const char *close_p = strchr(close_b + 2, ')');
                if (close_p) {
                    /* Extract link text */
                    size_t text_len = close_b - p - 1;
                    sbuf_add(out, LINK_CLR);
                    sbuf_grow(out, text_len + 1);
                    memcpy(out->buf + out->len, p + 1, text_len);
                    out->len += text_len;
                    out->buf[out->len] = '\0';
                    sbuf_add(out, RST);
                    p = close_p + 1;
                    continue;
                }
            }
        }

        /* Regular character — escape literal backtick? No, just emit. */
        if (*p == '\\' && p[1] == '`') {
            sbuf_addc(out, '`');
            p += 2;
            continue;
        }

        sbuf_addc(out, *p);
        p++;
    }

    /* Close any open formatting */
    if (in_bold) sbuf_add(out, RST);
    if (in_italic) sbuf_add(out, RST);
}

/* ──────────────────────────────────────────────
 *  Main render
 * ────────────────────────────────────────────── */

char *hermes_markdown_render(const char *md, int term_width) {
    if (!md) return NULL;
    (void)term_width; /* Not used yet — could wrap long lines */

    sbuf_t *out = sbuf_new();
    if (!out) return NULL;

    const char *p = md;
    char line[4096];
    int in_code_block = 0;

    while (*p) {
        /* Read one line */
        int li = 0;
        while (*p && *p != '\n' && li < (int)sizeof(line) - 1) {
            line[li++] = *p++;
        }
        line[li] = '\0';
        if (*p == '\n') p++;

        if (in_code_block) {
            /* Check for closing fence */
            if (is_code_fence(line)) {
                /* Close code block */
                sbuf_add(out, RST);
                sbuf_add(out, "\n");
                in_code_block = 0;
                continue;
            }
            /* Emit line with code background */
            sbuf_add(out, CODE_BG_BLOCK);
            sbuf_add(out, " ");
            /* Escape ANSI inside code (just print raw) */
            sbuf_add(out, line);
            sbuf_add(out, RST);
            sbuf_add(out, "\n");
            continue;
        }

        /* Check for opening fence */
        if (is_code_fence(line)) {
            in_code_block = 1;
            /* Emit a faint header for the code block */
            sbuf_add(out, CODE_BG_BLOCK);
            sbuf_add(out, " ");
            sbuf_add(out, DIM);
            /* Extract language if present */
            const char *lang = line + 3;
            while (*lang == ' ') lang++;
            if (*lang) {
                sbuf_add(out, lang);
            }
            sbuf_add(out, RST);
            sbuf_add(out, "\n");
            continue;
        }

        /* Header */
        int hlevel;
        if (is_header(line, &hlevel)) {
            const char *text = line;
            while (*text == '#' || *text == ' ') text++;
            const char *hcolors[] = {H1_CLR, H2_CLR, H3_CLR, H4_CLR, H5_CLR, H6_CLR};
            const char *hc = hcolors[(hlevel - 1) % 6];
            sbuf_add(out, hc);
            sbuf_add(out, BOLD);
            render_inline(out, text);
            sbuf_add(out, RST);
            sbuf_add(out, "\n");
            continue;
        }

        /* Horizontal rule */
        if (is_hr(line)) {
            sbuf_add(out, HR_CLR);
            sbuf_add(out, "────────────────────────────────────────\n");
            sbuf_add(out, RST);
            continue;
        }

        /* Blockquote */
        if (is_bq(line)) {
            const char *text = line;
            while (*text == ' ') text++;
            if (*text == '>') {
                text++;
                if (*text == ' ') text++;
            }
            sbuf_add(out, BQ_CLR);
            sbuf_add(out, " │ ");
            render_inline(out, text);
            sbuf_add(out, RST);
            sbuf_add(out, "\n");
            continue;
        }

        /* List */
        if (is_list(line)) {
            const char *text = line;
            int indent = 0;
            while (*text == ' ') { indent++; text++; }
            /* Skip - * or + */
            if (*text == '-' || *text == '*' || *text == '+') text++;
            while (*text == ' ') text++;
            for (int i = 0; i < indent; i++) sbuf_addc(out, ' ');
            sbuf_add(out, " • ");
            render_inline(out, text);
            sbuf_add(out, "\n");
            continue;
        }

        /* Empty line */
        if (line[0] == '\0') {
            sbuf_add(out, "\n");
            continue;
        }

        /* Regular paragraph line */
        render_inline(out, line);
        /* Preserve paragraph break only if next line is also non-empty? */
        if (p[0] == '\n' || p[0] == '\0') {
            sbuf_add(out, "\n\n"); /* paragraph break */
        } else {
            sbuf_add(out, "\n");
        }
    }

    char *result = out->buf;
    free(out);  /* free struct only — buf is the result */
    return result;
}

/* ──────────────────────────────────────────────
 *  Strip markdown (plain text)
 * ────────────────────────────────────────────── */

char *hermes_markdown_strip(const char *md) {
    if (!md) return NULL;

    sbuf_t *out = sbuf_new();
    if (!out) return NULL;

    const char *p = md;
    int in_code_block = 0;
    int in_inline_code = 0;

    while (*p) {
        char ch = *p++;

        if (in_code_block) {
            if (ch == '`' && *p == '`' && p[1] == '`') {
                p += 2;
                in_code_block = 0;
                sbuf_add(out, "\n");
                continue;
            }
            sbuf_addc(out, ch);
            continue;
        }

        if (ch == '`') {
            if (*p == '`' && p[1] == '`') {
                p += 2;
                in_code_block = 1;
                sbuf_add(out, "\n");
                continue;
            }
            in_inline_code = !in_inline_code;
            continue;
        }

        if (in_inline_code) {
            sbuf_addc(out, ch);
            continue;
        }

        /* Skip markdown formatting */
        if ((ch == '*' || ch == '_') && (*p == '*' || *p == '_')) {
            p++; /* skip paired formatting */
            continue;
        }
        if (ch == '*' || ch == '_') continue;

        /* Skip link syntax but keep text */
        if (ch == '[') {
            const char *close = strchr(p, ']');
            if (close && close[1] == '(') {
                const char *end = strchr(close + 2, ')');
                if (end) {
                    size_t txtlen = close - p;
                    sbuf_grow(out, txtlen + 1);
                    memcpy(out->buf + out->len, p, txtlen);
                    out->len += txtlen;
                    p = end + 1;
                    continue;
                }
            }
        }

        if (ch == ']' || ch == '(' || ch == ')') continue;

        /* Skip image syntax */
        if (ch == '!') continue;

        /* Skip heading markers */
        if (ch == '#' && (p[0] == ' ' || p[0] == '#')) continue;

        sbuf_addc(out, ch);
    }

    char *result = out->buf;
    free(out);
    return result;
}