/*
 * difflib.c — C diff library (J15: Python difflib port).
 *
 * Implements unified diff generation and similarity ratio via LCS.
 */

#include "difflib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Line splitting
 * ================================================================ */

#define MAX_LINES 2048

/* Split text into lines. Returns count. Lines point into malloc'd copy. */
static int split_lines(const char *text, char **lines, int max) {
    if (!text || !text[0]) return 0;
    char *copy = strdup(text);
    if (!copy) return 0;

    int count = 0;
    char *save = NULL;
    char *line = strtok_r(copy, "\n", &save);
    while (line && count < max) {
        lines[count] = strdup(line);
        count++;
        line = strtok_r(NULL, "\n", &save);
    }
    free(copy);
    return count;
}

static void free_lines(char **lines, int count) {
    for (int i = 0; i < count; i++) free(lines[i]);
}

/* ================================================================
 *  Longest Common Subsequence (for ratio and diff)
 * ================================================================ */

/* LCS table size */
#define LCS_MAX 256

/* Compute LCS table for two line arrays */
static int lcs_length(char **a, int na, char **b, int nb) {
    /* Use DP with O(min(n,m)) space */
    int m = na, n = nb;
    if (m > LCS_MAX) m = LCS_MAX;
    if (n > LCS_MAX) n = LCS_MAX;

    /* Simple 2-row DP */
    int *prev = (int *)calloc((size_t)(n + 1), sizeof(int));
    int *curr = (int *)calloc((size_t)(n + 1), sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); return 0; }

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            if (strcmp(a[i], b[j]) == 0)
                curr[j + 1] = prev[j] + 1;
            else
                curr[j + 1] = (prev[j + 1] > curr[j]) ? prev[j + 1] : curr[j];
        }
        int *tmp = prev; prev = curr; curr = tmp;
    }

    int result = prev[n];
    free(prev);
    free(curr);
    return result;
}

/* ================================================================
 *  Public API
 * ================================================================ */

double difflib_ratio(const char *a, const char *b) {
    if (!a && !b) return 1.0;
    if (!a || !b) return 0.0;
    if (strcmp(a, b) == 0) return 1.0;

    /* If single-line, compare character-by-character */
    if (!strchr(a, '\n') && !strchr(b, '\n')) {
        int na = (int)strlen(a);
        int nb = (int)strlen(b);
        if (na == 0 && nb == 0) return 1.0;
        if (na == 0 || nb == 0) return 0.0;

        /* Simple LCS for character sequences */
        int m = na > LCS_MAX ? LCS_MAX : na;
        int n = nb > LCS_MAX ? LCS_MAX : nb;
        int *prev = (int *)calloc((size_t)(n + 1), sizeof(int));
        int *curr = (int *)calloc((size_t)(n + 1), sizeof(int));
        if (!prev || !curr) { free(prev); free(curr); return 0.0; }

        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                if (a[i] == b[j])
                    curr[j + 1] = prev[j] + 1;
                else
                    curr[j + 1] = (prev[j + 1] > curr[j]) ? prev[j + 1] : curr[j];
            }
            int *tmp = prev; prev = curr; curr = tmp;
        }

        int lcs = prev[n];
        free(prev);
        free(curr);
        return (na + nb > 0) ? (2.0 * lcs) / (na + nb) : 1.0;
    }

    /* Multi-line: compare line-by-line */
    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    /* Handle empty cases */
    if (na == 0 && nb == 0) { free_lines(la, na); free_lines(lb, nb); return 1.0; }
    if (na == 0 || nb == 0) { free_lines(la, na); free_lines(lb, nb); return 0.0; }

    int lcs = lcs_length(la, na, lb, nb);
    double ratio = (na + nb > 0) ? (2.0 * lcs) / (na + nb) : 1.0;

    free_lines(la, na);
    free_lines(lb, nb);
    return ratio;
}

char *difflib_unified_diff(const char *a, const char *b, int context_lines) {
    if (context_lines < 0) context_lines = 3;

    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    /* Build result with dynamic buffer */
    size_t buf_size = 65536;
    char *result = (char *)calloc(buf_size, 1);
    if (!result) { free_lines(la, na); free_lines(lb, nb); return strdup(""); }
    int pos = 0;

    /* Header */
    pos += snprintf(result + pos, buf_size - (size_t)pos,
                    "--- original\n+++ modified\n");

    /* Simple line-by-line diff (no LCS-based grouping for simplicity) */
    int max_lines = (na > nb) ? na : nb;
    int changed = 0, start = -1;

    for (int i = 0; i < max_lines; i++) {
        if (i < na && i < nb && strcmp(la[i], lb[i]) == 0) {
            /* Same */
            if (changed) {
                /* End of a changed block */
                int chunk_start = (start - context_lines < 0) ? 0 : start - context_lines;
                int chunk_end = (i + context_lines > max_lines) ? max_lines : i + context_lines;

                pos += snprintf(result + pos, buf_size - (size_t)pos,
                                "@@ -%d,%d +%d,%d @@\n",
                                start + 1, i - start,
                                start + 1, i - start);

                for (int j = chunk_start; j < chunk_end; j++) {
                    if (j >= start && j < i) {
                        /* In changed region */
                        if (j < na) pos += snprintf(result + pos, buf_size - (size_t)pos, "-%s\n", la[j]);
                        if (j < nb) pos += snprintf(result + pos, buf_size - (size_t)pos, "+%s\n", lb[j]);
                    } else if (j < na && j < nb) {
                        pos += snprintf(result + pos, buf_size - (size_t)pos, " %s\n", la[j]);
                    }
                }
                changed = 0;
            }
        } else {
            if (!changed) {
                start = i;
                changed = 1;
            }
        }
    }

    /* Handle trailing changed block */
    if (changed) {
        int chunk_start = (start - context_lines < 0) ? 0 : start - context_lines;
        int chunk_end = max_lines;

        pos += snprintf(result + pos, buf_size - (size_t)pos,
                        "@@ -%d,%d +%d,%d @@\n",
                        start + 1, max_lines - start,
                        start + 1, max_lines - start);

        for (int j = chunk_start; j < chunk_end; j++) {
            if (j >= start) {
                if (j < na) pos += snprintf(result + pos, buf_size - (size_t)pos, "-%s\n", la[j]);
                if (j < nb) pos += snprintf(result + pos, buf_size - (size_t)pos, "+%s\n", lb[j]);
            } else if (j < na && j < nb) {
                pos += snprintf(result + pos, buf_size - (size_t)pos, " %s\n", la[j]);
            }
        }
    }

    free_lines(la, na);
    free_lines(lb, nb);

    return result;
}

char *difflib_simple_diff(const char *a, const char *b) {
    if (!a && !b) return strdup("");
    if (!a) return strdup(b);
    if (!b) return strdup(a);

    char *la[MAX_LINES], *lb[MAX_LINES];
    int na = split_lines(a, la, MAX_LINES);
    int nb = split_lines(b, lb, MAX_LINES);

    size_t buf_size = 65536;
    char *result = (char *)calloc(buf_size, 1);
    if (!result) { free_lines(la, na); free_lines(lb, nb); return strdup(""); }
    int pos = 0;

    int max_lines = (na > nb) ? na : nb;
    for (int i = 0; i < max_lines; i++) {
        char *line_a = (i < na) ? la[i] : "";
        char *line_b = (i < nb) ? lb[i] : "";

        if (strcmp(line_a, line_b) == 0) {
            pos += snprintf(result + pos, buf_size - (size_t)pos, " %s\n", line_a);
        } else {
            if (line_a[0]) pos += snprintf(result + pos, buf_size - (size_t)pos, "-%s\n", line_a);
            if (line_b[0]) pos += snprintf(result + pos, buf_size - (size_t)pos, "+%s\n", line_b);
        }
    }

    free_lines(la, na);
    free_lines(lb, nb);
    return result;
}
