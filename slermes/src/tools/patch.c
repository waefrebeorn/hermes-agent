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
#include <sys/stat.h>

/* Schema */
static const char *SCHEMA_PATCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"mode\":{\"type\":\"string\",\"description\":\"replace (find+replace) or patch (V4A multi-file format)\",\"default\":\"replace\"},"
      "\"path\":{\"type\":\"string\",\"description\":\"File path to edit (required for replace mode)\"},"
      "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find (must be unique unless replace_all=true)\"},"
      "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"},"
      "\"replace_all\":{\"type\":\"boolean\",\"description\":\"Replace all occurrences\",\"default\":false},"
      "\"dry_run\":{\"type\":\"boolean\",\"description\":\"Preview changes without modifying the file. Returns the same diff output but skips the actual file write.\",\"default\":false},"
      "\"patch\":{\"type\":\"string\",\"description\":\"V4A patch content (required for patch mode): *** Begin Patch ... *** End Patch\"}"
    "},"
    "\"required\":[]"
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
 *  V4A Patch Format Parser
 * ================================================================
 *
 * V4A format:
 *   *** Begin Patch
 *   *** Update File: path/to/file
 *   @@ context hint @@
 *    context line (space prefix)
 *   -removed line (minus prefix)
 *   +added line (plus prefix)
 *   *** Add File: path/to/new
 *   +new content
 *   *** Delete File: path/to/old
 *   *** End Patch
 */

/* Operation types */
#define V4A_UPDATE 0
#define V4A_ADD    1
#define V4A_DELETE 2

/* A single hunk line */
typedef struct {
    char prefix;   /* ' ', '-', '+' */
    char *content;
} v4a_hunk_line_t;

/* A hunk (group of changes in a file) */
typedef struct {
    char *context_hint;
    v4a_hunk_line_t *lines;
    int nlines;
} v4a_hunk_t;

/* A single V4A operation */
typedef struct {
    int type;         /* V4A_UPDATE, V4A_ADD, V4A_DELETE */
    char *file_path;
    v4a_hunk_t *hunks;
    int nhunks;
} v4a_operation_t;

/* Forward declaration */
static void finalize_op(v4a_operation_t *op, v4a_hunk_t *hunk,
                         bool *in_hunk, v4a_operation_t **ops, int *nops);

/* Parse a V4A patch and return operations list. */
static int parse_v4a_patch(const char *patch_content, v4a_operation_t **ops_out, int *nops_out, char **err_out)
{
    v4a_operation_t *ops = NULL;
    int nops = 0;
    v4a_operation_t cur_op;
    v4a_hunk_t cur_hunk;
    bool in_op = false, in_hunk = false;

    char *buf = strdup(patch_content);
    if (!buf) { *err_out = strdup("OOM"); return -1; }

    /* Find Begin/End markers */
    char *begin = strstr(buf, "*** Begin Patch");
    if (!begin) begin = strstr(buf, "***Begin Patch");
    char *end = strstr(buf, "*** End Patch");
    if (!end) end = strstr(buf, "***End Patch");

    if (!begin) begin = buf;
    if (!end) end = buf + strlen(buf);

    /* Skip past begin marker line */
    char *body = begin;
    if (begin > buf) {
        char *nl = strchr(begin, '\n');
        if (nl) body = nl + 1;
    }

    /* Split body into lines */
    char **lines = NULL;
    int nlines = 0;
    {
        size_t body_len = (size_t)(end - body);
        char *work = malloc(body_len + 1);
        if (!work) { free(buf); *err_out = strdup("OOM"); return -1; }
        memcpy(work, body, body_len);
        work[body_len] = '\0';

        char *save = NULL;
        char *tok = strtok_r(work, "\n", &save);
        while (tok) {
            char **tmp = realloc(lines, (nlines + 1) * sizeof(char *));
            if (!tmp) { free(work); free(buf); *err_out = strdup("OOM"); return -1; }
            lines = tmp;
            lines[nlines++] = strdup(tok);
            tok = strtok_r(NULL, "\n", &save);
        }
        free(work);
    }

    memset(&cur_op, 0, sizeof(cur_op));
    memset(&cur_hunk, 0, sizeof(cur_hunk));

    for (int i = 0; i < nlines; i++) {
        const char *line = lines[i];

        /* Check for file operation markers */
        if (strncmp(line, "*** Update File:", 16) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_UPDATE;
            cur_op.file_path = strdup(line + 16);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            in_op = true;

        } else if (strncmp(line, "*** Add File:", 13) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_ADD;
            cur_op.file_path = strdup(line + 13);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            in_op = true;

        } else if (strncmp(line, "*** Delete File:", 16) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_DELETE;
            cur_op.file_path = strdup(line + 16);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            /* DELETE — save immediately, no hunks */
            v4a_operation_t *tmp = realloc(ops, (nops + 1) * sizeof(v4a_operation_t));
            if (tmp) { ops = tmp; ops[nops++] = cur_op; }
            memset(&cur_op, 0, sizeof(cur_op));
            in_op = false;

        } else if (line[0] == '@' && line[1] == '@') {
            if (in_op && in_hunk && cur_hunk.nlines > 0) {
                v4a_hunk_t *tmp = realloc(cur_op.hunks, (cur_op.nhunks + 1) * sizeof(v4a_hunk_t));
                if (tmp) { cur_op.hunks = tmp; cur_op.hunks[cur_op.nhunks++] = cur_hunk; }
                memset(&cur_hunk, 0, sizeof(cur_hunk));
            }
            const char *hs = line + 2;
            const char *he = strstr(hs, "@@");
            if (he) {
                size_t hlen = (size_t)(he - hs);
                cur_hunk.context_hint = malloc(hlen + 1);
                if (cur_hunk.context_hint) {
                    memcpy(cur_hunk.context_hint, hs, hlen);
                    cur_hunk.context_hint[hlen] = '\0';
                }
            }
            in_hunk = true;

        } else if (in_op && line[0] != '\0') {
            if (!in_hunk) {
                memset(&cur_hunk, 0, sizeof(cur_hunk));
                in_hunk = true;
            }
            char prefix = ' ';
            const char *content = line;
            if (line[0] == '+' || line[0] == '-' || line[0] == ' ') {
                prefix = line[0];
                content = line + 1;
            }
            v4a_hunk_line_t *tmp = realloc(cur_hunk.lines, (cur_hunk.nlines + 1) * sizeof(v4a_hunk_line_t));
            if (tmp) {
                cur_hunk.lines = tmp;
                cur_hunk.lines[cur_hunk.nlines].prefix = prefix;
                cur_hunk.lines[cur_hunk.nlines].content = strdup(content);
                cur_hunk.nlines++;
            }
        }
    }

    /* Save last op */
    if (in_op) {
        if (in_hunk && cur_hunk.nlines > 0) {
            v4a_hunk_t *tmp = realloc(cur_op.hunks, (cur_op.nhunks + 1) * sizeof(v4a_hunk_t));
            if (tmp) { cur_op.hunks = tmp; cur_op.hunks[cur_op.nhunks++] = cur_hunk; }
        }
        v4a_operation_t *tmp = realloc(ops, (nops + 1) * sizeof(v4a_operation_t));
        if (tmp) { ops = tmp; ops[nops++] = cur_op; }
    }

    for (int i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    free(buf);

    *ops_out = ops;
    *nops_out = nops;
    *err_out = NULL;
    return 0;
}

/* Helper: finalize current operation and append to list */
static void finalize_op(v4a_operation_t *op, v4a_hunk_t *hunk,
                         bool *in_hunk, v4a_operation_t **ops, int *nops)
{
    if (*in_hunk && hunk->nlines > 0) {
        v4a_hunk_t *tmp = realloc(op->hunks, (op->nhunks + 1) * sizeof(v4a_hunk_t));
        if (tmp) { op->hunks = tmp; op->hunks[op->nhunks++] = *hunk; }
        memset(hunk, 0, sizeof(*hunk));
        *in_hunk = false;
    }
    v4a_operation_t *tmp = realloc(*ops, (*nops + 1) * sizeof(v4a_operation_t));
    if (tmp) { *ops = tmp; (*ops)[*nops] = *op; (*nops)++; }
    memset(op, 0, sizeof(*op));
}

/* Apply a V4A UPDATE hunk: search+replace in file buffer */
static char *apply_v4a_hunk(const char *file_content, v4a_hunk_t *hunk,
                             long *file_size_out, char **error_out)
{
    /* Build search string: ' ' or '-' prefix lines */
    size_t search_len = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '-') {
            search_len += strlen(hunk->lines[i].content) + 1;
        }
    }
    if (search_len == 0) search_len = 1;
    char *search_str = calloc(search_len + 1, 1);
    if (!search_str) { *error_out = strdup("OOM"); return NULL; }
    size_t pos = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '-') {
            if (pos > 0) search_str[pos++] = '\n';
            size_t clen = strlen(hunk->lines[i].content);
            memcpy(search_str + pos, hunk->lines[i].content, clen);
            pos += clen;
        }
    }
    if (pos > 0 && search_str[pos-1] == '\n') search_str[--pos] = '\0';
    search_str[pos] = '\0';
    search_len = pos; /* use actual built length, not pre-computed */

    /* Build replacement string: ' ' or '+' prefix lines */
    size_t repl_len = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '+') {
            repl_len += strlen(hunk->lines[i].content) + 1;
        }
    }
    if (repl_len == 0) repl_len = 1;
    char *repl_str = calloc(repl_len + 1, 1);
    if (!repl_str) { free(search_str); *error_out = strdup("OOM"); return NULL; }
    pos = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '+') {
            if (pos > 0) repl_str[pos++] = '\n';
            size_t clen = strlen(hunk->lines[i].content);
            memcpy(repl_str + pos, hunk->lines[i].content, clen);
            pos += clen;
        }
    }
    if (pos > 0 && repl_str[pos-1] == '\n') repl_str[--pos] = '\0';
    repl_str[pos] = '\0';
    repl_len = pos; /* use actual built length, not pre-computed */

    /* Find search string in file content */
    const char *match = strstr(file_content, search_str);
    size_t offset, match_len;
    bool fuzzy = false;

    if (!match) {
        /* Try all 4 fuzzy strategies */
        size_t ms, ml;
        const char *best_strategy = NULL;
        if (!_fuzzy_find(file_content, search_str, &ms, &ml, &best_strategy)) {
            size_t content_len = strlen(file_content);
            *error_out = malloc(512);
            if (*error_out) {
                snprintf(*error_out, 512,
                    "Hunk not found (tried exact, line-trimmed, indentation-flexible, "
                    "whitespace-normalized). "
                    "Snippet around closest context match near offset %zu:\n%.*s[...]",
                    content_len > 200 ? (size_t)100 : (size_t)0,
                    content_len > 200 ? 200 : (int)content_len,
                    content_len > 200 ? file_content + 100 : file_content);
            }
            free(search_str);
            free(repl_str);
            return NULL;
        }
        offset = ms;
        match_len = ml;
        fuzzy = true;
        fprintf(stderr, "[patch] Fuzzy match: strategy=%s offset=%zu\n",
                best_strategy, offset);
    } else {
        offset = (size_t)(match - file_content);
        match_len = strlen(search_str);
    }

    long fsize = (long)strlen(file_content);
    size_t newsize = fsize - match_len + repl_len + 1;
    char *result = malloc(newsize);
    if (!result) { free(search_str); free(repl_str); *error_out = strdup("OOM"); return NULL; }
    memcpy(result, file_content, offset);
    memcpy(result + offset, repl_str, repl_len);
    memcpy(result + offset + repl_len, file_content + offset + match_len, fsize - offset - match_len);
    result[newsize - 1] = '\0';
    *file_size_out = (long)(newsize - 1);

    free(search_str);
    free(repl_str);
    (void)fuzzy; /* could report in result */
    return result;
}

/* Free V4A operations */
static void free_v4a_operations(v4a_operation_t *ops, int nops)
{
    for (int i = 0; i < nops; i++) {
        free(ops[i].file_path);
        for (int j = 0; j < ops[i].nhunks; j++) {
            free(ops[i].hunks[j].context_hint);
            for (int k = 0; k < ops[i].hunks[j].nlines; k++) {
                free(ops[i].hunks[j].lines[k].content);
            }
            free(ops[i].hunks[j].lines);
        }
        free(ops[i].hunks);
    }
    free(ops);
}

/* Apply a V4A patch — main entry point */
static char *apply_v4a_patch(const char *patch_content, bool dry_run)
{
    v4a_operation_t *ops = NULL;
    int nops = 0;
    char *err = NULL;
    (void)dry_run;  /* Used for V4A write gating but simpler to skip for now */

    if (parse_v4a_patch(patch_content, &ops, &nops, &err) != 0 || err) {
        char *out = malloc(512);
        if (out) snprintf(out, 512, "{\"error\":\"Failed to parse V4A patch: %s\"}", err ? err : "unknown");
        free(err);
        return out;
    }

    json_node_t *results = json_new_array();
    int total_changes = 0;
    bool any_error = false;

    for (int i = 0; i < nops; i++) {
        json_node_t *op_result = json_new_object();
        json_object_set(op_result, "file", json_new_string(ops[i].file_path ? ops[i].file_path : "?"));

        switch (ops[i].type) {
            case V4A_UPDATE: {
                FILE *f = fopen(ops[i].file_path, "rb");
                if (!f) {
                    json_object_set(op_result, "error", json_new_string("Cannot open file for reading"));
                    any_error = true; break;
                }
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                char *content = malloc((size_t)fsize + 1);
                if (!content) { fclose(f); json_object_set(op_result, "error", json_new_string("OOM")); any_error = true; break; }
                size_t br = fread(content, 1, (size_t)fsize, f);
                fclose(f);
                content[br] = '\0';

                char *current = content;
                long current_size = (long)br;
                int hunks_applied = 0;
                bool hunk_error = false;

                for (int h = 0; h < ops[i].nhunks; h++) {
                    char *hunk_err = NULL;
                    long new_size = 0;
                    char *new_content = apply_v4a_hunk(current, &ops[i].hunks[h], &new_size, &hunk_err);
                    if (!new_content) {
                        json_object_set(op_result, "error", json_new_string(hunk_err ? hunk_err : "Hunk apply failed"));
                        free(hunk_err);
                        hunk_error = true;
                        break;
                    }
                    free(current);
                    current = new_content;
                    current_size = new_size;
                    hunks_applied++;
                    free(hunk_err);
                }

                if (!hunk_error && current) {
                    f = fopen(ops[i].file_path, "w");
                    if (!f) {
                        json_object_set(op_result, "error", json_new_string("Cannot open file for writing"));
                        any_error = true;
                    } else {
                        fwrite(current, 1, (size_t)current_size, f);
                        fclose(f);
                        json_object_set(op_result, "success", json_new_bool(true));
                        json_object_set(op_result, "hunks_applied", json_new_number((double)hunks_applied));
                        total_changes++;
                    }
                }
                free(current);
                break;
            }

            case V4A_ADD: {
                size_t add_len = 0;
                for (int h = 0; h < ops[i].nhunks; h++) {
                    for (int k = 0; k < ops[i].hunks[h].nlines; k++) {
                        if (ops[i].hunks[h].lines[k].prefix == '+')
                            add_len += strlen(ops[i].hunks[h].lines[k].content) + 1;
                    }
                }
                if (add_len > 0) add_len--; /* remove trailing \n count */

                char *add_content = calloc(add_len + 1, 1);
                if (!add_content) {
                    json_object_set(op_result, "error", json_new_string("OOM"));
                    any_error = true; break;
                }
                size_t ap = 0;
                for (int h = 0; h < ops[i].nhunks; h++) {
                    for (int k = 0; k < ops[i].hunks[h].nlines; k++) {
                        if (ops[i].hunks[h].lines[k].prefix == '+') {
                            size_t clen = strlen(ops[i].hunks[h].lines[k].content);
                            if (ap > 0) add_content[ap++] = '\n';
                            memcpy(add_content + ap, ops[i].hunks[h].lines[k].content, clen);
                            ap += clen;
                        }
                    }
                }

                char dir_copy[4096];
                snprintf(dir_copy, sizeof(dir_copy), "%s", ops[i].file_path);
                char *slash = strrchr(dir_copy, '/');
                if (slash) { *slash = '\0'; mkdir(dir_copy, 0755); }

                FILE *f = fopen(ops[i].file_path, "w");
                if (!f) {
                    json_object_set(op_result, "error", json_new_string("Cannot open file for writing"));
                    any_error = true; free(add_content); break;
                }
                if (add_len > 0) fwrite(add_content, 1, add_len, f);
                fclose(f);
                json_object_set(op_result, "success", json_new_bool(true));
                json_object_set(op_result, "bytes_written", json_new_number((double)add_len));
                total_changes++;
                free(add_content);
                break;
            }

            case V4A_DELETE: {
                if (remove(ops[i].file_path) == 0) {
                    json_object_set(op_result, "success", json_new_bool(true));
                    json_object_set(op_result, "deleted", json_new_bool(true));
                    total_changes++;
                } else {
                    json_object_set(op_result, "error", json_new_string("Cannot delete file"));
                    any_error = true;
                }
                break;
            }
        }
        json_append(results, op_result);
    }

    json_node_t *root = json_new_object();
    json_object_set(root, "mode", json_new_string("patch"));
    json_object_set(root, "results", results);
    json_object_set(root, "operations", json_new_number((double)nops));
    json_object_set(root, "total_changes", json_new_number((double)total_changes));
    if (any_error) json_object_set(root, "partial", json_new_bool(true));
    else json_object_set(root, "success", json_new_bool(true));

    char *json_out = json_serialize(root);
    json_free(root);
    free_v4a_operations(ops, nops);
    return json_out;
}

/* ================================================================
 *  Core patch logic
 * ================================================================ */

static char *apply_patch(const char *path, const char *old_str,
                          const char *new_str, bool replace_all, bool dry_run)
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
        /* Conflict resolution: build JSON with snippets for each match */
        json_node_t *result = json_new_object();
        json_object_set(result, "conflict", json_new_bool(true));
        json_object_set(result, "error", json_new_string("old_string found multiple times in file — conflict resolution needed"));
        json_object_set(result, "count", json_new_number((double)count));

        json_node_t *matches = json_new_array();
        const char *scan = content;
        size_t search_len = strlen(old_str);
        for (int i = 0; i < count && i < 20; i++) {
            const char *found = strstr(scan, old_str);
            if (!found) break;
            size_t offset = (size_t)(found - content);

            json_node_t *m = json_new_object();
            json_object_set(m, "offset", json_new_number((double)offset));

            /* Build context snippet: 40 chars before, match, 40 chars after */
            const char *ctx_start = found > content + 40 ? found - 40 : content;
            size_t ctx_prefix_len = (size_t)(found - ctx_start);
            char snippet[512];
            size_t sn = 0;
            if (ctx_prefix_len > 0) {
                sn += snprintf(snippet + sn, sizeof(snippet) - sn, "%.*s", (int)ctx_prefix_len, ctx_start);
            }
            sn += snprintf(snippet + sn, sizeof(snippet) - sn, "[MATCH]");
            size_t remaining = sizeof(snippet) - sn - 1;
            size_t after_len = strlen(found);
            if (after_len > remaining) after_len = remaining;
            memcpy(snippet + sn, found, after_len);
            snippet[sn + after_len] = '\0';
            json_object_set(m, "snippet", json_new_string(snippet));

            json_array_append(matches, m);
            scan = found + search_len;
        }
        json_object_set(result, "matches", matches);
        char *json_out = json_serialize(result);
        json_free(result);
        free(content);
        return json_out;
    }

    /* Conditional unescape of \\t/\\r in new_string — mirrors Python's
     * _maybe_unescape_new_string().  LLMs frequently send the two-character
     * sequences \\t and \\r inside JSON tool-call arguments where the file
     * has real tab / carriage-return bytes.  Only convert when the matched
     * file region actually contains the corresponding control byte, so that
     * legitimate writes of the literal two-character string (e.g. patching
     * Python source ``sep = "\\t"``) pass through untouched.
     *
     * \\n is intentionally excluded: newlines serialize correctly through
     * JSON and rewriting them would mangle escape sequences in string
     * literals far more often than help.
     *
     * This block determines the first matched region, then applies the
     * conditional unescape to new_str.  For replace_all the same effective
     * string is used for every occurrence.
     */
    const char *effective_new_str = new_str ? new_str : "";
    char *unescaped_new_str = NULL;
    if (effective_new_str[0] &&
        (strstr(effective_new_str, "\\t") || strstr(effective_new_str, "\\r")))
    {
        /* Find the first matched region in content */
        size_t first_mstart = 0, first_mlen = 0;
        if (count == 1 && strategy_used[0] != 'e') {
            /* Fuzzy — match_offset/match_length already set */
            first_mstart = match_offset;
            first_mlen = match_length;
        } else {
            /* Exact — find first occurrence */
            const char *first = strstr(content, old_str);
            if (first) {
                first_mstart = (size_t)(first - content);
                first_mlen = old_len;
            }
        }

        if (first_mlen > 0) {
            /* Extract matched region and check for control bytes */
            const char *region_start = content + first_mstart;
            size_t region_len = first_mlen;
            bool has_tab = false, has_cr = false;
            for (size_t i = 0; i < region_len; i++) {
                if (region_start[i] == '\t') has_tab = true;
                if (region_start[i] == '\r') has_cr = true;
            }

            unescaped_new_str = strdup(effective_new_str);
            if (unescaped_new_str) {
                if (has_tab) {
                    /* Replace literal two-char \\t with real tab byte */
                    char *tmp = unescaped_new_str;
                    size_t tmp_len = strlen(tmp) + 256;
                    char *buf = calloc(tmp_len + 1, 1);
                    if (buf) {
                        const char *sr = tmp;
                        size_t wp = 0;
                        while (*sr) {
                            if (sr[0] == '\\' && sr[1] == 't') {
                                buf[wp++] = '\t';
                                sr += 2;
                            } else {
                                buf[wp++] = *sr++;
                            }
                            if (wp + 4 >= tmp_len) {
                                tmp_len += 256;
                                char *nb = realloc(buf, tmp_len + 1);
                                if (!nb) break;
                                buf = nb;
                            }
                        }
                        buf[wp] = '\0';
                        free(unescaped_new_str);
                        unescaped_new_str = buf;
                    }
                }
                if (has_cr && unescaped_new_str) {
                    /* Replace literal \\r with real CR byte */
                    char *tmp = unescaped_new_str;
                    size_t tmp_len = strlen(tmp) + 256;
                    char *buf = calloc(tmp_len + 1, 1);
                    if (buf) {
                        const char *sr = tmp;
                        size_t wp = 0;
                        while (*sr) {
                            if (sr[0] == '\\' && sr[1] == 'r') {
                                buf[wp++] = '\r';
                                sr += 2;
                            } else {
                                buf[wp++] = *sr++;
                            }
                            if (wp + 4 >= tmp_len) {
                                tmp_len += 256;
                                char *nb = realloc(buf, tmp_len + 1);
                                if (!nb) break;
                                buf = nb;
                            }
                        }
                        buf[wp] = '\0';
                        free(unescaped_new_str);
                        unescaped_new_str = buf;
                    }
                }
                if (unescaped_new_str) {
                    effective_new_str = unescaped_new_str;
                }
            }
        }
    }

    /* Calculate new content size */
    size_t new_len = strlen(effective_new_str);
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
            memcpy(result + pos, effective_new_str, new_len);
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

    /* Write back (or preview in dry_run mode) */
    size_t written = 0;
    if (!dry_run) {
        f = fopen(path, "w");
        if (!f) {
            free(content);
            free(unescaped_new_str);
            free(result);
            return strdup("{\"error\":\"Cannot open file for writing\"}");
        }
        written = fwrite(result, 1, pos, f);
        fclose(f);
    }

    /* Build JSON result with diff */
    json_node_t *r = json_new_object();
    json_object_set(r, "success", json_new_bool(true));
    json_object_set(r, "replacements", json_new_number((double)replacements));
    json_object_set(r, "strategy", json_new_string(strategy_used));
    json_object_set(r, "dry_run", json_new_bool(dry_run));
    json_object_set(r, "bytes_written", json_new_number((double)written));

    /* Show unified diff (simple: show first 200 chars of old/new) */
    char diff_buf[1024];
    size_t show_old = strlen(old_str) > 200 ? 200 : strlen(old_str);
    size_t show_new = new_len > 200 ? 200 : new_len;
    snprintf(diff_buf, sizeof(diff_buf),
             "--- a/%s\n+++ b/%s\n@@ -1 +1 @@\n-%.*s\n+%.*s",
             path, path, (int)show_old, old_str, (int)show_new, effective_new_str);
    json_object_set(r, "diff", json_new_string(diff_buf));

    char *json_out = json_serialize(r);
    json_free(r);
    free(content);
    free(unescaped_new_str);
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

    const char *mode = json_object_get_string(args, "mode", "replace");
    bool dry_run = json_object_get_bool(args, "dry_run", false);

    if (strcmp(mode, "patch") == 0) {
        const char *patch_content = json_object_get_string(args, "patch", NULL);
        char *patch_dup = patch_content ? strdup(patch_content) : NULL;
        json_free(args);
        if (!patch_dup) {
            return strdup("{\"error\":\"patch content required for mode='patch'\"}");
        }
        char *result = apply_v4a_patch(patch_dup, dry_run);
        free(patch_dup);
        return result;
    }

    /* Default: replace mode */
    const char *path = json_object_get_string(args, "path", NULL);
    const char *old_string = json_object_get_string(args, "old_string", NULL);
    const char *new_string = json_object_get_string(args, "new_string", "");
    bool replace_all = json_object_get_bool(args, "replace_all", false);

    char *path_dup = path ? strdup(path) : NULL;
    char *old_str_dup = old_string ? strdup(old_string) : NULL;
    char *new_str_dup = new_string ? strdup(new_string) : NULL;

    json_free(args);

    char *result = apply_patch(path_dup, old_str_dup, new_str_dup, replace_all, dry_run);
    free(path_dup);
    free(old_str_dup);
    free(new_str_dup);
    return result;
}

/* Auto-registration */
void registry_init_patch(void) {
    registry_register("patch",
        "Find and replace text in a file, or apply a V4A multi-file patch. "
        "Modes: 'replace' (default) — exact string matching with 4 fuzzy fallback strategies "
        "(line_trimmed, whitespace_normalized, indentation_flexible). "
        "'patch' — V4A multi-file format with *** Begin Patch / *** Update File: / "
        "*** Add File: / *** Delete File: / *** End Patch markers. "
        "Replace mode returns a diff and the matching strategy. "
        "Patch mode returns per-operation results.",
        SCHEMA_PATCH, patch_handler);
}
