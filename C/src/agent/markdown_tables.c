/* CJK/wide-character-aware re-alignment of model-emitted markdown tables.
 *
 * Python equivalent: agent/markdown_tables.py
 *
 * Models pad markdown tables assuming each character occupies one terminal
 * cell. CJK glyphs and most emoji render as two cells, so the model's
 * spacing collapses into drift the moment a table reaches a real terminal.
 *
 * This module rebuilds row padding using wcwidth (display columns),
 * preserving the table's pipes and dashes.
 */

#define _GNU_SOURCE
#include "hermes_markdown_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#define MIN_COL_WIDTH 3

/* ------------------------------------------------------------------ */
/*  Display width helpers                                             */
/* ------------------------------------------------------------------ */

int hermes_disp_width(const char *s)
{
    if (!s || !*s)
        return 0;

    int total = 0;
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));

    size_t len = strlen(s);
    const char *p = s;
    while (len > 0) {
        wchar_t wc;
        size_t ret = mbrtowc(&wc, p, len, &ps);
        if (ret == (size_t)-1 || ret == (size_t)-2) {
            /* Bad byte sequence — skip it */
            p++;
            len--;
            total += 1; /* assume one cell */
        } else if (ret == 0) {
            break;
        } else {
            int w = wcwidth(wc);
            if (w > 0)
                total += w;
            /* else: w <= 0 means zero-width (combining/control) */
            p += ret;
            len -= ret;
        }
    }
    return total;
}

static char *pad_to_width(const char *s, int target)
{
    int current = hermes_disp_width(s);
    int needed = target - current;
    if (needed <= 0)
        return strdup(s);

    size_t slen = strlen(s);
    char *result = malloc(slen + needed + 1);
    if (!result)
        return NULL;
    memcpy(result, s, slen);
    memset(result + slen, ' ', needed);
    result[slen + needed] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/*  Row splitting                                                      */
/* ------------------------------------------------------------------ */

char **hermes_split_table_row(const char *row, size_t *out_len)
{
    *out_len = 0;
    if (!row || !*row)
        return NULL;

    const char *s = row;
    /* Skip leading whitespace only */
    while (*s == ' ' || *s == '\t') s++;
    /* Skip leading pipe */
    if (*s == '|') s++;

    size_t len = strlen(s);

    /* Count pipes */
    size_t pipe_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '|') pipe_count++;
    }

    /* Allocate up to pipe_count+1 cells */
    char **cells = calloc(pipe_count + 2, sizeof(char *));
    if (!cells) return NULL;

    size_t cell_count = 0;
    const char *start = s;
    size_t remaining = len;
    while (remaining > 0) {
        /* Find next pipe or end */
        const char *pipe = memchr(start, '|', remaining);
        size_t cell_len;
        if (pipe) {
            cell_len = pipe - start;
            remaining -= cell_len + 1;
        } else {
            cell_len = remaining;
            remaining = 0;
        }

        char *cell = malloc(cell_len + 1);
        if (!cell) goto cleanup;
        memcpy(cell, start, cell_len);
        cell[cell_len] = '\0';

        /* Advance start past this segment */
        if (pipe) {
            start = pipe + 1;
        }

        /* Trim trailing whitespace */
        while (cell_len > 0 && (cell[cell_len - 1] == ' ' || cell[cell_len - 1] == '\t'))
            cell[--cell_len] = '\0';
        /* Trim leading whitespace */
        char *trimmed = cell;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (trimmed != cell) {
            char *stripped = strdup(trimmed);
            free(cell);
            if (!stripped) goto cleanup;
            cell = stripped;
        }

        cells[cell_count++] = cell;
    }

    cells[cell_count] = NULL;
    *out_len = cell_count;
    return cells;

cleanup:
    for (size_t i = 0; i < cell_count; i++)
        free(cells[i]);
    free(cells);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Table detection                                                    */
/* ------------------------------------------------------------------ */

bool hermes_is_table_divider(const char *row)
{
    if (!row || !*row)
        return false;

    size_t n = 0;
    char **cells = hermes_split_table_row(row, &n);
    if (!cells || n <= 1) {
        free(cells);
        return false;
    }

    bool result = true;
    for (size_t i = 0; i < n; i++) {
        const char *c = cells[i];
        /* Skip leading whitespace/colon */
        while (*c == ' ' || *c == ':' || *c == '\t') c++;
        if (!*c) { result = false; break; }
        /* Count dashes */
        int dashes = 0;
        while (*c == '-') { dashes++; c++; }
        if (dashes < 3) { result = false; break; }
        /* Allow trailing colon */
        if (*c == ':') c++;
        /* Must be at end */
        while (*c == ' ' || *c == '\t') c++;
        if (*c) { result = false; break; }
    }

    for (size_t i = 0; i < n; i++)
        free(cells[i]);
    free(cells);
    return result;
}

bool hermes_looks_like_table_row(const char *row)
{
    if (!row || !*row)
        return false;
    if (!strchr(row, '|'))
        return false;

    /* Skip leading whitespace */
    const char *s = row;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s)
        return false;

    if (*s == '|')
        return true;

    /* At least 2 pipes */
    int pipes = 0;
    while (*s) {
        if (*s == '|') pipes++;
        s++;
    }
    return pipes >= 2;
}

/* ------------------------------------------------------------------ */
/*  Rendering                                                          */
/* ------------------------------------------------------------------ */

static char **wrap_to_width(const char *text, int width, size_t *out_lines)
{
    *out_lines = 0;
    if (!text || width <= 0) {
        char **r = malloc(sizeof(char *));
        if (!r) return NULL;
        r[0] = strdup(text ? text : "");
        *out_lines = 1;
        return r;
    }

    /* Count words */
    char *copy = strdup(text);
    if (!copy) return NULL;

    size_t max_words = 1;
    for (const char *p = text; *p; p++)
        if (*p == ' ') max_words++;

    char **words = malloc(sizeof(char *) * max_words);
    size_t word_count = 0;
    char *tok = strtok(copy, " ");
    while (tok) {
        words[word_count++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    if (word_count == 0) {
        words[0] = strdup("");
        word_count = 1;
    }

    /* Line assembly */
    size_t cap = word_count + 1;
    char **lines = malloc(sizeof(char *) * cap);
    size_t line_count = 0;

    char *current = NULL;
    int current_w = 0;

    for (size_t i = 0; i < word_count; i++) {
        const char *word = words[i];
        int ww = hermes_disp_width(word);

        if (!current) {
            if (ww <= width) {
                current = strdup(word);
                current_w = ww;
            } else {
                /* Hard-break long word */
                size_t wlen = strlen(word);
                char *buf = malloc(wlen + 1);
                int bw = 0;
                size_t buf_pos = 0;
                for (size_t ci = 0; ci < wlen; ) {
                    wchar_t wc;
                    mbstate_t ps;
                    memset(&ps, 0, sizeof(ps));
                    size_t r = mbrtowc(&wc, word + ci, wlen - ci, &ps);
                    if (r == (size_t)-1) { ci++; continue; }
                    if (r == (size_t)-2) { buf[buf_pos++] = word[ci++]; continue; }
                    if (r == 0) break;

                    int cw = wcwidth(wc);
                    if (cw < 0) cw = 0;
                    if (bw + cw > width && buf_pos > 0) {
                        buf[buf_pos] = '\0';
                        lines[line_count++] = strdup(buf);
                        buf_pos = 0;
                        bw = 0;
                    }
                    memcpy(buf + buf_pos, word + ci, r);
                    buf_pos += r;
                    bw += cw;
                    ci += r;
                }
                if (buf_pos > 0) {
                    buf[buf_pos] = '\0';
                    current = strdup(buf);
                    current_w = bw;
                } else {
                    current = strdup("");
                    current_w = 0;
                }
                free(buf);
            }
        } else if (current_w + 1 + ww <= width) {
            size_t clen = strlen(current);
            size_t wlen = strlen(word);
            char *newc = realloc(current, clen + 1 + wlen + 1);
            if (!newc) goto cleanup_words;
            newc[clen] = ' ';
            memcpy(newc + clen + 1, word, wlen + 1);
            current = newc;
            current_w += 1 + ww;
        } else {
            lines[line_count++] = current;
            if (ww <= width) {
                current = strdup(word);
                current_w = ww;
            } else {
                /* Hard-break */
                size_t wlen = strlen(word);
                char *buf = malloc(wlen + 1);
                int bw = 0;
                size_t buf_pos = 0;
                for (size_t ci = 0; ci < wlen; ) {
                    wchar_t wc;
                    mbstate_t ps;
                    memset(&ps, 0, sizeof(ps));
                    size_t r = mbrtowc(&wc, word + ci, wlen - ci, &ps);
                    if (r == (size_t)-1) { ci++; continue; }
                    if (r == (size_t)-2) { buf[buf_pos++] = word[ci++]; continue; }
                    if (r == 0) break;
                    int cw = wcwidth(wc);
                    if (cw < 0) cw = 0;
                    if (bw + cw > width && buf_pos > 0) {
                        buf[buf_pos] = '\0';
                        lines[line_count++] = strdup(buf);
                        buf_pos = 0;
                        bw = 0;
                    }
                    memcpy(buf + buf_pos, word + ci, r);
                    buf_pos += r;
                    bw += cw;
                    ci += r;
                }
                if (buf_pos > 0) {
                    buf[buf_pos] = '\0';
                    current = strdup(buf);
                    current_w = bw;
                } else {
                    current = strdup("");
                    current_w = 0;
                }
                free(buf);
            }
        }

        if (line_count >= cap) {
            cap *= 2;
            char **newl = realloc(lines, sizeof(char *) * cap);
            if (!newl) goto cleanup_words;
            lines = newl;
        }
    }
    if (current) {
        lines[line_count++] = current;
    } else {
        lines[line_count++] = strdup("");
    }

    *out_lines = line_count;

    /* Cleanup words */
    for (size_t i = 0; i < word_count; i++)
        free(words[i]);
    free(words);
    free(copy);
    return lines;

cleanup_words:
    for (size_t i = 0; i < word_count; i++)
        free(words[i]);
    free(words);
    free(copy);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    return NULL;
}

static char **render_vertical(char ***rows, size_t nrows, size_t ncols,
                               int available_width, size_t *out_lines)
{
    *out_lines = 0;
    size_t cap = 256;
    char **out = malloc(sizeof(char *) * cap);
    size_t count = 0;

    int sep_width = available_width > 0
        ? (available_width - 2 < 20 ? 20 : (available_width - 2 > 40 ? 40 : available_width - 2))
        : 30;

    for (size_t ri = 1; ri < nrows; ri++) {
        if (ri > 1) {
            /* Separator line */
            char *sep = malloc(sep_width + 1);
            if (!sep) { /* leak */ break; }
            memset(sep, 0xC1, sep_width); /* UTF-8 '─' = 0xE2 0x94 0x80 */
            sep[0] = 0xE2; sep[1] = 0x94; sep[2] = 0x80;
            for (int i = 3; i < sep_width; i++)
                sep[i] = sep[2]; /* fill with end byte */
            sep[sep_width] = '\0';
            out[count++] = sep;
        }

        for (size_t ci = 0; ci < ncols; ci++) {
            const char *label = rows[0][ci] ? rows[0][ci] : "";
            const char *value = (ci < nrows && rows[ri][ci]) ? rows[ri][ci] : "";

            int label_w = hermes_disp_width(label);
            int first_budget = (available_width - label_w - 2) < 10 ? 10 : (available_width - label_w - 2);
            int cont_budget = (available_width - 2) < 10 ? 10 : (available_width - 2);

            if (!*value) {
                char *line;
                asprintf(&line, "%s:", label);
                out[count++] = line;
                continue;
            }

            size_t nw;
            char **wrapped = wrap_to_width(value, first_budget, &nw);
            if (!wrapped) continue;

            char *line;
            asprintf(&line, "%s: %s", label, wrapped[0]);
            out[count++] = line;

            for (size_t wi = 1; wi < nw; wi++) {
                if (wrapped[wi] && *wrapped[wi]) {
                    char *cline;
                    asprintf(&cline, "  %s", wrapped[wi]);
                    out[count++] = cline;
                }
            }

            for (size_t wi = 0; wi < nw; wi++)
                free(wrapped[wi]);
            free(wrapped);
        }
    }

    *out_lines = count;
    return out;
}

static char **render_block(char ***rows, size_t nrows, size_t ncols,
                            int available_width, size_t *out_lines)
{
    *out_lines = 0;

    /* Compute column widths */
    int *widths = calloc(ncols, sizeof(int));
    if (!widths) return NULL;

    for (size_t c = 0; c < ncols; c++) {
        widths[c] = MIN_COL_WIDTH;
        for (size_t r = 0; r < nrows; r++) {
            const char *cell = (c < nrows && rows[r][c]) ? rows[r][c] : "";
            int w = hermes_disp_width(cell);
            if (w > widths[c])
                widths[c] = w;
        }
    }

    /* Horizontal width: |` ` + cell + ` ` for each column + final `|` */
    int horizontal_width = 3; /* initial "| " */
    for (size_t c = 0; c < ncols; c++)
        horizontal_width += widths[c] + 3; /* " | " */
    /* Actually: |` ` + cell + ` ` join + final `|`
     * = `| ` + ` | `.join(padded_cells) + ` |`
     * = 2 + (3 * (ncols-1)) + sum(widths) + 2
     * = 2 + 3*ncols - 3 + sum(widths) + 2
     * = 1 + 3*ncols + sum(widths)
     * Let's recompute: each cell contributes: widths[c] + 2 (for " " before and after)
     * Plus ncols-1 junction "|" chars. Plus leading "|" and trailing "|".
     * = 1 (leading |) + sum(widths[c] + 2) + (ncols-1) + 1 (trailing |)
     * = 2 + sum(widths[c] + 2) + ncols - 1
     * = 1 + sum(widths[c] + 2) + ncols
     * = 1 + sum(widths) + 2*ncols + ncols
     * = 1 + sum(widths) + 3*ncols
     */
    /* Simpler: compute actual rendered row length */
    size_t cap = 256;
    char **out = malloc(sizeof(char *) * cap);
    size_t count = 0;

    if (available_width > 0 && horizontal_width > available_width && available_width >= 20) {
        /* Fall back to vertical */
        free(widths);
        char **result = render_vertical(rows, nrows, ncols, available_width, out_lines);
        free(out);
        return result;
    }

    /* Header row */
    {
        size_t hdr_cap = 256;
        char hdr[4096];
        size_t pos = 0;
        pos += snprintf(hdr + pos, sizeof(hdr) - pos, "| ");
        for (size_t c = 0; c < ncols; c++) {
            char *padded = pad_to_width(rows[0][c] ? rows[0][c] : "", widths[c]);
            if (!padded) { free(widths); free(out); return NULL; }
            pos += snprintf(hdr + pos, sizeof(hdr) - pos, "%s |", padded);
            free(padded);
        }
        out[count++] = strdup(hdr);
    }

    /* Divider */
    {
        char div[4096];
        size_t pos = 0;
        div[pos++] = '|';
        for (size_t c = 0; c < ncols; c++) {
            if (c > 0) div[pos++] = '|';
            div[pos++] = '-';
            for (int i = 0; i < widths[c]; i++)
                div[pos++] = '-';
            div[pos++] = '-';
        }
        div[pos] = '\0';
        out[count++] = strdup(div);
    }

    /* Body rows */
    for (size_t r = 1; r < nrows; r++) {
        char buf[4096];
        size_t pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "| ");
        for (size_t c = 0; c < ncols; c++) {
            const char *cell = (c < nrows && rows[r][c]) ? rows[r][c] : "";
            char *padded = pad_to_width(cell, widths[c]);
            if (!padded) { free(widths); /* leak */ break; }
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s |", padded);
            free(padded);
        }
        out[count++] = strdup(buf);
    }

    *out_lines = count;
    free(widths);
    return out;
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                   */
/* ------------------------------------------------------------------ */

char *hermes_realign_markdown_tables(const char *text, int available_width)
{
    if (!text || !strchr(text, '|'))
        return strdup(text ? text : "");

    /* Split into lines */
    char *copy = strdup(text);
    if (!copy) return NULL;

    size_t line_cap = 256;
    size_t line_count = 0;
    char **lines = malloc(sizeof(char *) * line_cap);
    if (!lines) { free(copy); return NULL; }

    char *save;
    char *tok = strtok_r(copy, "\n", &save);
    while (tok) {
        if (line_count >= line_cap) {
            line_cap *= 2;
            char **newl = realloc(lines, sizeof(char *) * line_cap);
            if (!newl) { free(copy); /* leak */ return NULL; }
            lines = newl;
        }
        lines[line_count++] = strdup(tok);
        tok = strtok_r(NULL, "\n", &save);
    }

    /* Build output */
    size_t out_cap = 256;
    char **out = malloc(sizeof(char *) * out_cap);
    size_t out_count = 0;

    for (size_t i = 0; i < line_count; ) {
        char *line = lines[i];
        if (strchr(line, '|') && i + 1 < line_count && hermes_is_table_divider(lines[i + 1])) {
            /* Found table starting at i */
            size_t header_idx = i;
            size_t body_start = i + 2;

            size_t nrows = 1;
            size_t j = body_start;
            while (j < line_count && strchr(lines[j], '|') && lines[j] && *lines[j] && lines[j][0] != '\0') {
                if (hermes_is_table_divider(lines[j])) { j++; continue; }
                nrows++;
                j++;
            }

            /* Parse rows */
            char ***rows = malloc(sizeof(char **) * nrows);
            if (!rows) goto cleanup;

            size_t ncols = 0;
            /* Header */
            rows[0] = hermes_split_table_row(lines[header_idx], &ncols);

            /* Body */
            size_t ri = 1;
            for (size_t k = body_start; k < j && ri < nrows; k++) {
                if (hermes_is_table_divider(lines[k])) continue;
                size_t nc;
                rows[ri] = hermes_split_table_row(lines[k], &nc);
                if (nc > ncols) ncols = nc;
                ri++;
            }
            size_t actual_rows = ri;

            /* Render */
            size_t rendered_count;
            char **rendered = render_block(rows, actual_rows, ncols, available_width, &rendered_count);
            if (rendered) {
                for (size_t r = 0; r < rendered_count; r++) {
                    if (out_count >= out_cap) {
                        out_cap *= 2;
                        char **newl = realloc(out, sizeof(char *) * out_cap);
                        if (!newl) break;
                        out = newl;
                    }
                    out[out_count++] = rendered[r];
                }
                free(rendered);
            }

            /* Cleanup rows */
            for (size_t r = 0; r < actual_rows; r++) {
                if (rows[r]) {
                    size_t ci = 0;
                    while (rows[r][ci]) {
                        free(rows[r][ci]);
                        ci++;
                    }
                    free(rows[r]);
                }
            }
            free(rows);

            i = j;
        } else {
            if (out_count >= out_cap) {
                out_cap *= 2;
                char **newl = realloc(out, sizeof(char *) * out_cap);
                if (!newl) break;
                out = newl;
            }
            out[out_count++] = strdup(line);
            i++;
        }
    }

    /* Join output lines */
    size_t total_len = 0;
    for (size_t i = 0; i < out_count; i++)
        total_len += strlen(out[i]) + 1;

    char *result = malloc(total_len + 1);
    if (!result) goto cleanup;

    size_t pos = 0;
    for (size_t i = 0; i < out_count; i++) {
        size_t l = strlen(out[i]);
        memcpy(result + pos, out[i], l);
        pos += l;
        result[pos++] = '\n';
    }
    if (pos > 0) result[pos - 1] = '\0'; /* remove trailing newline */
    else result[0] = '\0';

    /* Cleanup */
    for (size_t i = 0; i < out_count; i++)
        free(out[i]);
    free(out);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    free(copy);

    return result;

cleanup:
    for (size_t i = 0; i < out_count; i++)
        free(out[i]);
    free(out);
    for (size_t i = 0; i < line_count; i++)
        free(lines[i]);
    free(lines);
    free(copy);
    return NULL;
}
