/*
 * fuzzy_match.c — Multi-strategy fuzzy matching for find-and-replace.
 *
 * Port of Python tools/fuzzy_match.py (703 lines).
 *
 * Implements an 8-strategy matching chain to robustly find and replace
 * text, accommodating variations in whitespace, indentation, and escaping
 * common in LLM-generated code.
 */

#include "fuzzy_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Constants ──────────────────────────────────────────────────────── */

#define MAX_LINES 4096
#define MAX_LINE_LEN 4096
#define BLOCK_ANCHOR_RATIO 0.6
#define CONTEXT_RATIO 0.5

/* ─── Helpers ────────────────────────────────────────────────────────── */

/* Safe strdup */
static char *sstrdup(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/* Count lines in a string */
static int count_lines(const char *s)
{
    if (!s || !*s) return 0;
    int n = 1;
    while (*s) { if (*s++ == '\n') n++; }
    return n;
}

/* Split a string into lines (into pre-allocated array of MAX_LINES). Returns count. */
static int split_lines(const char *text, const char **lines, int max)
{
    if (!text || !*text) return 0;
    int count = 0;
    const char *start = text;
    while (*start && count < max) {
        const char *end = strchr(start, '\n');
        if (!end) {
            lines[count++] = start;
            break;
        }
        /* Include the newline in the line */
        lines[count++] = start;
        start = end + 1;
    }
    return count;
}

/* Join an array of strings (with separators) into a malloc'd string */
static char *join_strings(const char **parts, int count, const char *sep)
{
    if (count <= 0 || !parts) return sstrdup("");
    size_t total = 0;
    size_t seplen = sep ? strlen(sep) : 0;
    for (int i = 0; i < count; i++)
        total += parts[i] ? strlen(parts[i]) : 0;
    total += (size_t)(count - 1) * seplen + 1;

    char *buf = (char *)malloc(total);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0 && seplen) strcat(buf, sep);
        if (parts[i]) strcat(buf, parts[i]);
    }
    return buf;
}

/* Trim leading and trailing whitespace from a string (returns pointer into original) */
static const char *trim_str(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    /* Return a malloc'd substring */
    size_t len = (size_t)(end - s + 1);
    char *trimmed = (char *)malloc(len + 1);
    if (!trimmed) return s;
    memcpy(trimmed, s, len);
    trimmed[len] = '\0';
    return trimmed;
}

/* Trim leading whitespace only (returns pointer into original) */
static const char *trim_left(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Trim trailing whitespace only — returns malloc'd copy */
static char *trim_right(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) end--;
    size_t len = (size_t)(end - s + 1);
    char *result = (char *)malloc(len + 1);
    if (!result) return sstrdup(s);
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

/* Check if string contains substring */
static bool contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

/* Replace first occurrence of 'from' with 'to' in 'str' (in-place, str must be malloc'd).
 * Returns new malloc'd string. */
static char *str_replace_first(const char *str, const char *from, const char *to)
{
    if (!str || !from || !*from) return sstrdup(str);
    const char *pos = strstr(str, from);
    if (!pos) return sstrdup(str);

    size_t prefix_len = (size_t)(pos - str);
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    size_t total = prefix_len + to_len + strlen(pos + from_len) + 1;

    char *result = (char *)malloc(total);
    if (!result) return NULL;
    memcpy(result, str, prefix_len);
    memcpy(result + prefix_len, to, to_len);
    strcpy(result + prefix_len + to_len, pos + from_len);
    return result;
}

/* Replace all occurrences — returns malloc'd string */
static char *str_replace_all(const char *str, const char *from, const char *to)
{
    if (!str || !from || !*from) return sstrdup(str);

    /* Count occurrences */
    int count = 0;
    const char *p = str;
    size_t from_len = strlen(from);
    while ((p = strstr(p, from)) != NULL) {
        count++;
        p += from_len;
    }
    if (count == 0) return sstrdup(str);

    /* Build result */
    size_t to_len = strlen(to);
    size_t total = strlen(str) + (size_t)count * (to_len - from_len) + 1;
    char *result = (char *)malloc(total);
    if (!result) return NULL;

    char *dst = result;
    const char *src = str;
    for (int i = 0; i < count; i++) {
        const char *pos = strstr(src, from);
        size_t chunk = (size_t)(pos - src);
        memcpy(dst, src, chunk);
        dst += chunk;
        memcpy(dst, to, to_len);
        dst += to_len;
        src = pos + from_len;
    }
    strcpy(dst, src);
    return result;
}

/* Normalize whitespace: collapse multiple spaces/tabs to single space */
static char *normalize_ws(const char *s)
{
    if (!s || !*s) return sstrdup("");
    char *out = (char *)malloc(strlen(s) + 1);
    if (!out) return NULL;
    size_t j = 0;
    bool was_space = false;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == ' ' || s[i] == '\t') {
            if (!was_space) {
                out[j++] = ' ';
                was_space = true;
            }
        } else {
            /* Keep newlines as-is */
            out[j++] = s[i];
            was_space = false;
        }
    }
    out[j] = '\0';
    return out;
}

/* Strip indentation per line (remove leading whitespace) */
static char *strip_indent(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    char *stripped[MAX_LINES];
    for (int i = 0; i < lc; i++) {
        stripped[i] = sstrdup(trim_left(lines[i]));
    }
    char *result = join_strings((const char **)stripped, lc, "");
    for (int i = 0; i < lc; i++) free(stripped[i]);
    return result;
}

/* Trim each line */
static char *trim_each_line(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    char *trimmed[MAX_LINES];
    for (int i = 0; i < lc; i++) {
        const char *t = trim_str(lines[i]);
        trimmed[i] = sstrdup(t ? t : lines[i]);
    }
    /* Rejoin without separator (each line already includes its newline) */
    char *result = join_strings((const char **)trimmed, lc, "");
    for (int i = 0; i < lc; i++) free(trimmed[i]);
    return result;
}

/* Trim first and last line boundaries only */
static char *trim_boundary(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    char *trimmed[MAX_LINES];
    for (int i = 0; i < lc; i++) {
        if (i == 0 || i == lc - 1) {
            const char *t = trim_str(lines[i]);
            trimmed[i] = sstrdup(t ? t : lines[i]);
        } else {
            trimmed[i] = sstrdup(lines[i]);
        }
    }
    char *result = join_strings((const char **)trimmed, lc, "");
    for (int i = 0; i < lc; i++) free(trimmed[i]);
    return result;
}

/* Escape-normalize: convert literal '\n' to actual newlines */
static char *normalize_escapes(const char *s)
{
    if (!s || !*s) return sstrdup("");
    return str_replace_all(s, "\\n", "\n");
}

/* ─── Similarity ratio (line-based LCS) ─────────────────────────────── */

double fuzzy_ratio(const char *a, const char *b)
{
    if (!a && !b) return 1.0;
    if (!a || !b) return 0.0;

    const char *lines_a[MAX_LINES], *lines_b[MAX_LINES];
    int lc_a = split_lines(a, lines_a, MAX_LINES);
    int lc_b = split_lines(b, lines_b, MAX_LINES);

    if (lc_a == 0 && lc_b == 0) return 1.0;
    if (lc_a == 0 || lc_b == 0) return 0.0;

    /* LCS on line level */
    int **dp = (int **)calloc((size_t)(lc_a + 1), sizeof(int *));
    if (!dp) return 0.0;
    for (int i = 0; i <= lc_a; i++) {
        dp[i] = (int *)calloc((size_t)(lc_b + 1), sizeof(int));
        if (!dp[i]) {
            for (int j = 0; j < i; j++) free(dp[j]);
            free(dp);
            return 0.0;
        }
    }

    /* LCS on line level — need to compare per-line, not full substrings */
    /* Extract each line as a proper null-terminated string */
    char line_a[4096], line_b[4096];

    for (int i = 1; i <= lc_a; i++) {
        for (int j = 1; j <= lc_b; j++) {
            /* Extract first line from each (up to newline or end) */
            const char *pa = lines_a[i-1];
            const char *pb = lines_b[j-1];
            int la = 0, lb = 0;
            while (pa && pa[la] && pa[la] != '\n' && la < 4095) {
                line_a[la] = pa[la]; la++;
            }
            line_a[la] = '\0';
            while (pb && pb[lb] && pb[lb] != '\n' && lb < 4095) {
                line_b[lb] = pb[lb]; lb++;
            }
            line_b[lb] = '\0';

            bool match = (la == lb) && memcmp(line_a, line_b, la) == 0;
            if (match)
                dp[i][j] = dp[i-1][j-1] + 1;
            else
                dp[i][j] = (dp[i-1][j] > dp[i][j-1]) ? dp[i-1][j] : dp[i][j-1];
        }
    }

    int lcs = dp[lc_a][lc_b];
    for (int i = 0; i <= lc_a; i++) free(dp[i]);
    free(dp);

    int max_lines = (lc_a > lc_b) ? lc_a : lc_b;
    return (double)lcs / (double)max_lines;
}

/* ─── Strategy implementations ──────────────────────────────────────── */

/*
 * Each strategy function takes: content, old_string, new_string, replace_all.
 * Returns: fuzzy_result_t (caller must free content + error).
 *
 * Returns NULL if this strategy doesn't match (try next).
 */

/* Strategy 1: Exact match */
static fuzzy_result_t *strategy_exact(const char *content, const char *old_string,
                                       const char *new_string, bool replace_all)
{
    if (!content || !old_string || !new_string) return NULL;
    if (!contains(content, old_string)) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) return NULL;

    if (replace_all)
        r->content = str_replace_all(content, old_string, new_string);
    else
        r->content = str_replace_first(content, old_string, new_string);

    if (!r->content) { free(r); return NULL; }

    /* Count matches (safely handle empty old_string) */
    int count = 0;
    if (strlen(old_string) > 0) {
        const char *p = content;
        size_t olen = strlen(old_string);
        while ((p = strstr(p, old_string)) != NULL) { count++; p += olen; }
    }
    r->match_count = count;
    r->strategy = 1;
    return r;
}

/* Strategy 2: Line-trimmed */
static fuzzy_result_t *strategy_line_trimmed(const char *content,
                                              const char *old_string,
                                              const char *new_string,
                                              bool replace_all)
{
    char *trimmed_content = trim_each_line(content);
    char *trimmed_old = trim_each_line(old_string);
    if (!trimmed_content || !trimmed_old) {
        free(trimmed_content); free(trimmed_old);
        return NULL;
    }

    if (!contains(trimmed_content, trimmed_old)) {
        free(trimmed_content); free(trimmed_old);
        return NULL;
    }

    /* Replace in trimmed content */
    char *replaced;
    if (replace_all)
        replaced = str_replace_all(trimmed_content, trimmed_old, new_string);
    else
        replaced = str_replace_first(trimmed_content, trimmed_old, new_string);

    free(trimmed_content);
    free(trimmed_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 2;
    return r;
}

/* Strategy 3: Whitespace normalized */
static fuzzy_result_t *strategy_ws_normalized(const char *content,
                                               const char *old_string,
                                               const char *new_string,
                                               bool replace_all)
{
    char *norm_content = normalize_ws(content);
    char *norm_old = normalize_ws(old_string);
    if (!norm_content || !norm_old) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    if (!contains(norm_content, norm_old)) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(norm_content, norm_old, new_string);
    else
        replaced = str_replace_first(norm_content, norm_old, new_string);

    free(norm_content);
    free(norm_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 3;
    return r;
}

/* Strategy 4: Indentation flexible */
static fuzzy_result_t *strategy_indent_free(const char *content,
                                             const char *old_string,
                                             const char *new_string,
                                             bool replace_all)
{
    char *strip_content = strip_indent(content);
    char *strip_old = strip_indent(old_string);
    if (!strip_content || !strip_old) {
        free(strip_content); free(strip_old);
        return NULL;
    }

    if (!contains(strip_content, strip_old)) {
        free(strip_content); free(strip_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(strip_content, strip_old, new_string);
    else
        replaced = str_replace_first(strip_content, strip_old, new_string);

    free(strip_content);
    free(strip_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 4;
    return r;
}

/* Strategy 5: Escape normalized */
static fuzzy_result_t *strategy_escape_norm(const char *content,
                                             const char *old_string,
                                             const char *new_string,
                                             bool replace_all)
{
    char *norm_content = normalize_escapes(content);
    char *norm_old = normalize_escapes(old_string);
    if (!norm_content || !norm_old) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    if (!contains(norm_content, norm_old)) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(norm_content, norm_old, new_string);
    else
        replaced = str_replace_first(norm_content, norm_old, new_string);

    free(norm_content);
    free(norm_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 5;
    return r;
}

/* Strategy 6: Trimmed boundary */
static fuzzy_result_t *strategy_trim_boundary(const char *content,
                                               const char *old_string,
                                               const char *new_string,
                                               bool replace_all)
{
    char *trim_content = trim_boundary(content);
    char *trim_old = trim_boundary(old_string);
    if (!trim_content || !trim_old) {
        free(trim_content); free(trim_old);
        return NULL;
    }

    if (!contains(trim_content, trim_old)) {
        free(trim_content); free(trim_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(trim_content, trim_old, new_string);
    else
        replaced = str_replace_first(trim_content, trim_old, new_string);

    free(trim_content);
    free(trim_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 6;
    return r;
}

/* Strategy 7: Block anchor — match first+last line, ratio check on middle */
static fuzzy_result_t *strategy_block_anchor(const char *content,
                                              const char *old_string,
                                              const char *new_string,
                                              bool replace_all)
{
    (void)replace_all; /* Block anchor only does first match */

    if (!content || !old_string || !new_string) return NULL;

    const char *c_lines[MAX_LINES], *o_lines[MAX_LINES];
    int c_lc = split_lines(content, c_lines, MAX_LINES);
    int o_lc = split_lines(old_string, o_lines, MAX_LINES);

    if (o_lc < 3) return NULL; /* Need at least 3 lines for anchor matching */

    const char *old_first = o_lines[0];
    const char *old_last = o_lines[o_lc - 1];

    /* Scan content for first-line match */
    char *first = sstrdup(trim_str(old_first));
    char *last = sstrdup(trim_str(old_last));
    if (!first || !last) { free(first); free(last); return NULL; }

    /* Find first occurrence of trimmed first line */
    const char *match_start = strstr(content, first);
    free(first);

    if (!match_start) { free(last); return NULL; }

    /* Check if trimmed last line appears after match_start */
    const char *tail = match_start + strlen(old_first);
    const char *match_end = strstr(tail, last);
    free(last);

    if (!match_end) return NULL;

    /* Extract the block between start and end */
    size_t block_len = (size_t)(match_end + strlen(old_last) - match_start);
    char *block = (char *)malloc(block_len + 1);
    if (!block) return NULL;
    memcpy(block, match_start, block_len);
    block[block_len] = '\0';

    /* Check similarity ratio between extracted block and old_string */
    double ratio = fuzzy_ratio(block, old_string);
    free(block);

    if (ratio < BLOCK_ANCHOR_RATIO) return NULL;

    /* Match — do replacement */
    char *before = (char *)malloc((size_t)(match_start - content) + 1);
    if (!before) return NULL;
    memcpy(before, content, (size_t)(match_start - content));
    before[match_start - content] = '\0';

    size_t total = strlen(before) + strlen(new_string) + strlen(match_end + strlen(old_last)) + 1;
    char *result = (char *)malloc(total);
    if (!result) { free(before); return NULL; }
    strcpy(result, before);
    strcat(result, new_string);
    strcat(result, match_end + strlen(old_last));
    free(before);

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(result); return NULL; }
    r->content = result;
    r->match_count = 1;
    r->strategy = 7;
    return r;
}

/* Strategy 8: Context-aware — 50% line similarity threshold */
static fuzzy_result_t *strategy_context(const char *content,
                                         const char *old_string,
                                         const char *new_string,
                                         bool replace_all)
{
    (void)replace_all; /* Only first match for context */

    if (!content || !old_string || !new_string) return NULL;

    const char *c_lines[MAX_LINES], *o_lines[MAX_LINES];
    int c_lc = split_lines(content, c_lines, MAX_LINES);
    int o_lc = split_lines(old_string, o_lines, MAX_LINES);

    if (o_lc < 1 || c_lc < o_lc) return NULL;

    /* Slide window over content lines */
    for (int start = 0; start <= c_lc - o_lc; start++) {
        /* Count matching lines */
        int matching = 0;
        for (int j = 0; j < o_lc; j++) {
            const char *ct = trim_str(c_lines[start + j]);
            const char *ot = trim_str(o_lines[j]);
            if (ct && ot && strcmp(ct, ot) == 0)
                matching++;
        }
        if ((double)matching / (double)o_lc >= CONTEXT_RATIO) {
            /* Build replacement */
            char *before = (char *)malloc((size_t)(c_lines[start] - content) + 1);
            if (!before) return NULL;
            memcpy(before, content, (size_t)(c_lines[start] - content));
            before[c_lines[start] - content] = '\0';

            const char *block_end = c_lines[start + o_lc - 1] + strlen(c_lines[start + o_lc - 1]);
            size_t total = strlen(before) + strlen(new_string) + strlen(block_end) + 1;
            char *result = (char *)malloc(total);
            if (!result) { free(before); return NULL; }
            strcpy(result, before);
            strcat(result, new_string);
            strcat(result, block_end);
            free(before);

            fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
            if (!r) { free(result); return NULL; }
            r->content = result;
            r->match_count = 1;
            r->strategy = 8;
            return r;
        }
    }
    return NULL;
}

/* ─── Strategy function pointer table ────────────────────────────────── */

typedef fuzzy_result_t *(*strategy_fn)(const char *, const char *,
                                        const char *, bool);

static const strategy_fn STRATEGIES[] = {
    strategy_exact,
    strategy_line_trimmed,
    strategy_ws_normalized,
    strategy_indent_free,
    strategy_escape_norm,
    strategy_trim_boundary,
    strategy_block_anchor,
    strategy_context,
};

#define NUM_STRATEGIES (sizeof(STRATEGIES) / sizeof(STRATEGIES[0]))

/* ─── Public API ─────────────────────────────────────────────────────── */

fuzzy_result_t fuzzy_find_and_replace(const char *content,
                                       const char *old_string,
                                       const char *new_string,
                                       bool replace_all)
{
    fuzzy_result_t null_result = {NULL, 0, 0, NULL};

    if (!content || !old_string || !new_string) {
        null_result.error = sstrdup("NULL argument");
        return null_result;
    }

    for (size_t i = 0; i < NUM_STRATEGIES; i++) {
        fuzzy_result_t *r = STRATEGIES[i](content, old_string, new_string, replace_all);
        if (r) {
            fuzzy_result_t result = *r;
            free(r);
            return result;
        }
    }

    null_result.error = sstrdup("no match found");
    return null_result;
}

void fuzzy_result_free(fuzzy_result_t *result)
{
    if (!result) return;
    free(result->content);
    free(result->error);
    result->content = NULL;
    result->error = NULL;
    result->match_count = 0;
    result->strategy = 0;
}
