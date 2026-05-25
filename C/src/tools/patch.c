/*
 * patch.c — Patch/find-replace tool for Hermes C.
 * Reads file, finds unique old_string, replaces with new_string.
 * Supports: replace_all mode, fuzzy matching (basic), diff output.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Schema */
static const char *SCHEMA_PATCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"File path to edit\"},"
      "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find (must be unique unless replace_all=true)\"},"
      "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"},"
      "\"replace_all\":{\"type\":\"boolean\",\"description\":\"Replace all occurrences\",\"default\":false}"
    "},"
    "\"required\":[\"path\",\"old_string\"]"
"}";

/* ================================================================
 *  Fuzzy matching strategies (ported from Python fuzzy_match.py)
 * ================================================================ */

/* Helper: count lines in a string */
static int _count_lines(const char *s) {
    int n = 1;
    while (*s) { if (*s++ == '\n') n++; }
    return n;
}

/* Helper: duplicate a string and replace one char with another */
static char *_str_replace_char(const char *s, char from, char to) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) return NULL;
    for (char *p = dup; *p; p++) {
        if (*p == from) *p = to;
    }
    return dup;
}

/* Strategy 1: Exact match (returns 1 if found, 0 if not).
 * Sets *match_start = position in content, *match_len = pattern length. */
static int _fuzzy_exact(const char *content, const char *pattern,
                         size_t *match_start, size_t *match_len)
{
    const char *p = strstr(content, pattern);
    if (!p) return 0;
    *match_start = (size_t)(p - content);
    *match_len = strlen(pattern);
    return 1;
}

/* Strategy 2: Line-trimmed — strip leading/trailing whitespace per line,
 * then find as sequence of trimmed lines in trimmed content. */
static int _fuzzy_line_trimmed(const char *content, const char *pattern,
                                size_t *match_start, size_t *match_len)
{
    int plines = _count_lines(pattern);
    int clines = _count_lines(content);
    if (plines > clines) return 0;

    /* Split content and pattern into lines (tokenized) */
    /* Use strtok_r — need mutable copies */
    char *content_copy = strdup(content);
    char *pattern_copy = strdup(pattern);
    if (!content_copy || !pattern_copy) { free(content_copy); free(pattern_copy); return 0; }

    /* Build trimmed pattern lines array */
    char *pat_lines[256];
    int npat = 0;
    char *save1 = NULL;
    char *tok = strtok_r(pattern_copy, "\n", &save1);
    while (tok && npat < 256) {
        /* Strip leading/trailing whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        pat_lines[npat++] = tok;
        tok = strtok_r(NULL, "\n", &save1);
    }

    /* Build trimmed content lines array */
    char *con_lines[4096];
    int ncon = 0;
    char *save2 = NULL;
    char *ctok = strtok_r(content_copy, "\n", &save2);
    while (ctok && ncon < 4096) {
        while (*ctok == ' ' || *ctok == '\t') ctok++;
        char *end = ctok + strlen(ctok);
        while (end > ctok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        con_lines[ncon++] = ctok;
        ctok = strtok_r(NULL, "\n", &save2);
    }

    /* Find the pattern as a contiguous subsequence in content */
    int found = 0;
    int start_line = 0;
    for (int i = 0; i <= ncon - npat; i++) {
        int match = 1;
        for (int j = 0; j < npat; j++) {
            if (strcmp(con_lines[i + j], pat_lines[j]) != 0) { match = 0; break; }
        }
        if (match) { found = 1; start_line = i; break; }
    }

    free(content_copy);
    free(pattern_copy);

    if (!found) return 0;

    /* Calculate byte positions in original content */
    /* Walk through original content to find the N-th newline */
    const char *ptr = content;
    for (int i = 0; i < start_line && *ptr; i++) {
        ptr = strchr(ptr, '\n');
        if (!ptr) return 0;
        ptr++; /* skip newline */
    }
    *match_start = (size_t)(ptr - content);

    /* Find end: skip npat-1 newlines from start of match */
    const char *end_ptr = content + *match_start;
    for (int i = 0; i < npat; i++) {
        const char *nl = strchr(end_ptr, '\n');
        if (!nl) { end_ptr = content + strlen(content); break; }
        end_ptr = nl + 1;
    }
    *match_len = (size_t)(end_ptr - (content + *match_start));

    return 1;
}

/* Strategy 3: Whitespace normalized — collapse multiple spaces/tabs to single space */
static int _fuzzy_whitespace_normalized(const char *content, const char *pattern,
                                         size_t *match_start, size_t *match_len)
{
    /* Normalize pattern: collapse spaces, tabs to single space, trim */
    char *pat_norm = _str_replace_char(pattern, '\t', ' ');
    if (!pat_norm) return 0;

    /* Collapse multiple spaces in pattern */
    char *pat_collapsed = strdup(pat_norm);
    free(pat_norm);
    if (!pat_collapsed) return 0;
    int w = 0;
    for (int r = 0; pat_collapsed[r]; r++) {
        if (pat_collapsed[r] == ' ' && r > 0 && pat_collapsed[r-1] == ' ') continue;
        pat_collapsed[w++] = pat_collapsed[r];
    }
    pat_collapsed[w] = '\0';

    /* Normalize content: collapse spaces, tabs to single space */
    char *con_norm = _str_replace_char(content, '\t', ' ');
    if (!con_norm) { free(pat_collapsed); return 0; }
    char *con_collapsed = strdup(con_norm);
    free(con_norm);
    if (!con_collapsed) { free(pat_collapsed); return 0; }
    w = 0;
    for (int r = 0; con_collapsed[r]; r++) {
        if (con_collapsed[r] == ' ' && r > 0 && con_collapsed[r-1] == ' ') continue;
        con_collapsed[w++] = con_collapsed[r];
    }
    con_collapsed[w] = '\0';

    /* Build position map: for each byte in con_collapsed, which byte in original content? */
    /* We need to map bytes by walking both in parallel */
    size_t con_len = strlen(content);
    size_t *pos_map = (size_t *)malloc((strlen(con_collapsed) + 1) * sizeof(size_t));
    if (!pos_map) { free(pat_collapsed); free(con_collapsed); return 0; }

    size_t ci = 0, ni = 0;
    while (ci < con_len && ni < strlen(con_collapsed)) {
        pos_map[ni] = ci;
        /* The normalized byte at ni came from content[ci] */
        if (content[ci] == '\t') {
            /* Tab → space: consume 1 tab, emit 1 space */
            ci++; ni++;
        } else if (content[ci] == ' ') {
            /* Consume all consecutive spaces as one */
            ci++;
            while (ci < con_len && (content[ci] == ' ' || content[ci] == '\t')) ci++;
            ni++;
        } else {
            ci++; ni++;
        }
    }
    if (ni > 0) ni--;
    pos_map[ni] = ci; /* fix last */

    /* Try exact match in normalized content */
    const char *np = strstr(con_collapsed, pat_collapsed);
    int result = 0;
    if (np) {
        size_t norm_start = (size_t)(np - con_collapsed);
        size_t norm_end = norm_start + strlen(pat_collapsed);
        *match_start = pos_map[norm_start];
        *match_len = (norm_end <= ni) ? (pos_map[norm_end] - pos_map[norm_start])
                     : (con_len - pos_map[norm_start]);
        result = 1;
    }

    free(pos_map);
    free(pat_collapsed);
    free(con_collapsed);
    return result;
}

/* Strategy 4: Indentation flexible — ignore leading whitespace */
static int _fuzzy_indentation_flexible(const char *content, const char *pattern,
                                        size_t *match_start, size_t *match_len)
{
    /* Same as line_trimmed but only left-strip, not right-strip */
    int plines = _count_lines(pattern);
    int clines = _count_lines(content);
    if (plines > clines) return 0;

    char *content_copy = strdup(content);
    char *pattern_copy = strdup(pattern);
    if (!content_copy || !pattern_copy) { free(content_copy); free(pattern_copy); return 0; }

    char *pat_lines[256]; int npat = 0;
    char *save1 = NULL;
    char *tok = strtok_r(pattern_copy, "\n", &save1);
    while (tok && npat < 256) {
        while (*tok == ' ' || *tok == '\t') tok++;
        pat_lines[npat++] = tok;
        tok = strtok_r(NULL, "\n", &save1);
    }

    char *con_lines[4096]; int ncon = 0;
    char *save2 = NULL;
    char *ctok = strtok_r(content_copy, "\n", &save2);
    while (ctok && ncon < 4096) {
        char *stripped = ctok;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        con_lines[ncon++] = stripped;
        ctok = strtok_r(NULL, "\n", &save2);
    }

    int found = 0, start_line = 0;
    for (int i = 0; i <= ncon - npat; i++) {
        int match = 1;
        for (int j = 0; j < npat; j++) {
            if (strcmp(con_lines[i + j], pat_lines[j]) != 0) { match = 0; break; }
        }
        if (match) { found = 1; start_line = i; break; }
    }

    free(content_copy);
    free(pattern_copy);

    if (!found) return 0;

    const char *ptr = content;
    for (int i = 0; i < start_line && *ptr; i++) {
        ptr = strchr(ptr, '\n');
        if (!ptr) return 0;
        ptr++;
    }
    *match_start = (size_t)(ptr - content);

    const char *end_ptr = content + *match_start;
    for (int i = 0; i < npat; i++) {
        const char *nl = strchr(end_ptr, '\n');
        if (!nl) { end_ptr = content + strlen(content); break; }
        end_ptr = nl + 1;
    }
    *match_len = (size_t)(end_ptr - (content + *match_start));

    return 1;
}

/* Fuzzy matching chain — tries strategies in order, returns 1 if any matched */
static int _fuzzy_find(const char *content, const char *pattern,
                       size_t *match_start, size_t *match_len, const char **strategy_out)
{
    typedef int (*fuzzy_fn)(const char *, const char *, size_t *, size_t *);
    typedef struct { const char *name; fuzzy_fn fn; } strategy_entry;

    strategy_entry chain[] = {
        {"exact", _fuzzy_exact},
        {"line_trimmed", _fuzzy_line_trimmed},
        {"indentation_flexible", _fuzzy_indentation_flexible},
        {"whitespace_normalized", _fuzzy_whitespace_normalized},
    };
    int n = sizeof(chain) / sizeof(chain[0]);

    for (int i = 0; i < n; i++) {
        if (chain[i].fn(content, pattern, match_start, match_len)) {
            *strategy_out = chain[i].name;
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 *  Core patch logic
 * ================================================================ */

static char *apply_patch(const char *path, const char *old_str,
                          const char *new_str, bool replace_all)
{
    if (!path || !old_str) return strdup("{\"error\":\"Missing path or old_string\"}");

    /* Read entire file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        return strdup("{\"error\":\"Cannot open file for reading\"}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize > 10 * 1024 * 1024) { /* > 10MB */
        fclose(f);
        return strdup("{\"error\":\"File too large (>10MB)\"}");
    }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return strdup("{\"error\":\"OOM\"}"); }
    size_t bytes_read = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[bytes_read] = '\0';

    size_t old_len = strlen(old_str);
    if (old_len == 0) {
        free(content);
        return strdup("{\"error\":\"old_string cannot be empty\"}");
    }

    /* Count occurrences using exact match */
    int count = 0;
    const char *p = content;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    /* Strategy used for matching (reported in output) */
    const char *strategy_used = "exact";
    size_t match_offset = 0, match_length = old_len;

    if (count == 0) {
        /* Try fuzzy matching strategies */
        if (_fuzzy_find(content, old_str, &match_offset, &match_length, &strategy_used)) {
            count = 1;
            /* Update old_len to match_length for correct result_size calculation */
            old_len = match_length;
        } else {
            free(content);
            return strdup("{\"error\":\"old_string not found in file (tried exact + 3 fuzzy strategies)\"}");
        }
    }

    if (!replace_all && count > 1) {
        free(content);
        return strdup("{\"error\":\"old_string found multiple times (use replace_all=true)\"}");
    }

    /* Calculate new content size */
    size_t new_len = strlen(new_str ? new_str : "");
    size_t result_size = bytes_read + (size_t)count * (new_len - old_len) + 1;
    char *result = (char *)malloc(result_size);
    if (!result) { free(content); return strdup("{\"error\":\"OOM\"}"); }

    /* Build result */
    size_t pos = 0;
    const char *src = content;
    int replacements = 0;

    while (replacements < count) {
        const char *match;
        if (replacements == 0 && count == 1 && strategy_used[0] != 'e') {
            /* Fuzzy match — use pre-computed position */
            match = content + match_offset;
        } else {
            match = strstr(src, old_str);
            if (!match) break;
        }

        /* Copy before match */
        size_t before = (size_t)(match - src);
        memcpy(result + pos, src, before);
        pos += before;

        /* Copy new string */
        if (new_len > 0) {
            memcpy(result + pos, new_str, new_len);
            pos += new_len;
        }

        if (replacements == 0 && count == 1 && strategy_used[0] != 'e') {
            src = match + match_length;
        } else {
            src = match + old_len;
        }
        replacements++;

        if (!replace_all) break;
    }

    /* Copy remaining */
    size_t remaining = strlen(src);
    memcpy(result + pos, src, remaining);
    pos += remaining;
    result[pos] = '\0';

    /* Write back */
    f = fopen(path, "w");
    if (!f) {
        free(content);
        free(result);
        return strdup("{\"error\":\"Cannot open file for writing\"}");
    }
    size_t written = fwrite(result, 1, pos, f);
    fclose(f);

    /* Build JSON result with diff */
    json_node_t *r = json_new_object();
    json_object_set(r, "success", json_new_bool(true));
    json_object_set(r, "replacements", json_new_number((double)replacements));
    json_object_set(r, "strategy", json_new_string(strategy_used));
    json_object_set(r, "bytes_written", json_new_number((double)written));

    /* Show unified diff (simple: show first 200 chars of old/new) */
    char diff_buf[1024];
    size_t show_old = strlen(old_str) > 200 ? 200 : strlen(old_str);
    size_t show_new = new_len > 200 ? 200 : new_len;
    snprintf(diff_buf, sizeof(diff_buf),
             "--- a/%s\n+++ b/%s\n@@ -1 +1 @@\n-%.*s\n+%.*s",
             path, path, (int)show_old, old_str, (int)show_new, new_str);
    json_object_set(r, "diff", json_new_string(diff_buf));

    char *json_out = json_serialize(r);
    json_free(r);
    free(content);
    free(result);
    return json_out;
}

/* ================================================================
 *  Handler
 * ================================================================ */

char *patch_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json) return strdup("{\"error\":\"No arguments provided\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        /* Return valid JSON even if error message contains special chars */
        json_node_t *e = json_new_object();
        json_object_set(e, "error", json_new_string("JSON parse error"));
        if (err) json_object_set(e, "detail", json_new_string(err));
        free(err);
        char *out = json_serialize(e);
        json_free(e);
        if (!out) out = strdup("{\"error\":\"JSON parse error\"}");
        return out;
    }

    const char *path = json_object_get_string(args, "path", NULL);
    const char *old_string = json_object_get_string(args, "old_string", NULL);
    const char *new_string = json_object_get_string(args, "new_string", "");
    bool replace_all = json_object_get_bool(args, "replace_all", false);

    /* strdup BEFORE json_free — values point into JSON tree */
    char *path_dup = path ? strdup(path) : NULL;
    char *old_str_dup = old_string ? strdup(old_string) : NULL;
    char *new_str_dup = new_string ? strdup(new_string) : NULL;

    json_free(args);

    char *result = apply_patch(path_dup, old_str_dup, new_str_dup, replace_all);
    free(path_dup);
    free(old_str_dup);
    free(new_str_dup);
    return result;
}

/* Auto-registration */
void registry_init_patch(void) {
    registry_register("patch",
        "Find and replace text in a file. Uses exact string matching "
        "with 4 fuzzy fallback strategies (line_trimmed, whitespace_normalized, "
        "indentation_flexible). Returns a diff and the matching strategy used. "
        "Set replace_all=true to replace all occurrences.",
        SCHEMA_PATCH, patch_handler);
}
