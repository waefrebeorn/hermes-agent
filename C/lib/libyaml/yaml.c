/*
 * yaml.c — Standalone YAML config parser for C.
 * Parses key:value, nested indent, lists, comments. Zero external deps.
 * Never calls exit(). Returns NULL on OOM.
 * MIT License — WuBu Hermes Project
 */

#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Internal helpers — graceful OOM
 * ================================================================ */

static bool oom_flag = false;

static void *xmalloc(size_t sz) {
    if (oom_flag) return NULL;
    void *p = malloc(sz ? sz : 1);
    if (!p) { oom_flag = true; return NULL; }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    if (oom_flag) return NULL;
    void *r = realloc(p, sz ? sz : 1);
    if (!r) { oom_flag = true; return NULL; }
    return r;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* ================================================================
 *  Internal tree
 * ================================================================ */

typedef enum { YVAL_STRING, YVAL_LIST, YVAL_MAP } yaml_val_type_t;

typedef struct yaml_entry {
    char *key;
    yaml_val_type_t type;
    char *str_val;
    struct yaml_entry **items;
    size_t item_count;
    struct yaml_entry **children;
    size_t child_count;
} yaml_entry_t;

struct yaml_doc {
    yaml_entry_t *root;
};

/* ================================================================
 *  Lexer
 * ================================================================ */

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
} yaml_lexer_t;

typedef struct {
    char   *line;
    char   *raw;
    int     indent;
    size_t  lineno;
} yaml_line_t;

static void lex_init(yaml_lexer_t *lx, const char *text) {
    lx->text = text;
    lx->len = strlen(text);
    lx->pos = 0;
}

static bool lex_next(yaml_lexer_t *lx, yaml_line_t *line) {
    memset(line, 0, sizeof(*line));
    if (lx->pos >= lx->len) return false;

    const char *start = lx->text + lx->pos;
    const char *end = start;
    while (lx->pos < lx->len && lx->text[lx->pos] != '\n') {
        lx->pos++;
        end = lx->text + lx->pos;
    }
    if (lx->pos < lx->len) lx->pos++;

    size_t raw_len = (size_t)(end - start);
    line->raw = (char *)xmalloc(raw_len + 1);
    if (!line->raw) return false;
    memcpy(line->raw, start, raw_len);
    line->raw[raw_len] = '\0';

    line->indent = 0;
    while (line->raw[line->indent] == ' ') line->indent++;

    const char *p = line->raw + line->indent;
    const char *comment = NULL;
    for (const char *q = p; *q; q++) {
        if (*q == '#' && (q == p || *(q-1) != '\\')) { comment = q; break; }
    }
    size_t content_end = comment ? (size_t)(comment - line->raw) : raw_len;
    while (content_end > 0 && (line->raw[content_end-1] == ' ' || line->raw[content_end-1] == '\t'))
        content_end--;

    size_t clen = content_end - (size_t)line->indent;
    line->line = (char *)xmalloc(clen + 1);
    if (!line->line) return false;
    memcpy(line->line, p, clen);
    line->line[clen] = '\0';
    return true;
}

static void line_free(yaml_line_t *line) {
    free(line->raw);
    free(line->line);
}

/* ================================================================
 *  Parser — same logic as original hermes_yaml
 * ================================================================ */

static yaml_entry_t *new_entry(const char *key) {
    yaml_entry_t *e = (yaml_entry_t *)xmalloc(sizeof(yaml_entry_t));
    if (!e) return NULL;
    memset(e, 0, sizeof(*e));
    e->key = key ? xstrdup(key) : NULL;
    return e;
}

static void free_entry(yaml_entry_t *e) {
    if (!e) return;
    free(e->key);
    free(e->str_val);
    for (size_t i = 0; i < e->item_count; i++) free_entry(e->items[i]);
    free(e->items);
    for (size_t i = 0; i < e->child_count; i++) free_entry(e->children[i]);
    free(e->children);
    free(e);
}

static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err);

static yaml_entry_t *parse_value(const char *val_str) {
    yaml_entry_t *e = new_entry(NULL);
    if (!e) return NULL;
    e->type = YVAL_STRING;
    e->str_val = xstrdup(val_str ? val_str : "");
    return e;
}

static void parse_inline(yaml_entry_t *e, const char *val) {
    while (*val == ' ') val++;
    if (*val == '\0') {
        e->type = YVAL_STRING;
        e->str_val = xstrdup("");
        return;
    }
    if (val[0] == '-' && (val[1] == ' ' || val[1] == '\0')) {
        e->type = YVAL_LIST;
        val += 1;
        while (*val == ' ') val++;
        yaml_entry_t *item = parse_value(val);
        e->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
        if (e->items) { e->items[0] = item; e->item_count = 1; }
        return;
    }
    e->type = YVAL_STRING;
    e->str_val = xstrdup(val);
}

static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err)
{
    yaml_entry_t *map = new_entry(NULL);
    if (!map) return NULL;
    map->type = YVAL_MAP;
    *consumed = 0;

    size_t i = 0;
    while (i < count) {
        yaml_line_t *ln = &lines[i];
        if (ln->indent <= parent_indent) break;
        if (ln->line[0] == '\0') { i++; continue; }

        char *colon = strchr(ln->line, ':');
        if (!colon) {
            if (ln->line[0] == '-' && (ln->line[1] == ' ' || ln->line[1] == '\0')) {
                i++;
                continue;
            }
            if (err) *err = xstrdup("expected ':' in YAML line");
            free_entry(map);
            return NULL;
        }

        size_t key_len = (size_t)(colon - ln->line);
        char *key = (char *)xmalloc(key_len + 1);
        if (!key) { free_entry(map); return NULL; }
        memcpy(key, ln->line, key_len);
        key[key_len] = '\0';
        while (key_len > 0 && key[key_len-1] == ' ') key[--key_len] = '\0';

        yaml_entry_t *child = new_entry(key);
        free(key);
        if (!child) { free_entry(map); return NULL; }

        const char *val = colon + 1;
        parse_inline(child, val);

        if (child->type == YVAL_STRING) {
            size_t j = i + 1;
            if (j < count && lines[j].indent > ln->indent && lines[j].line[0] == '-') {
                child->type = YVAL_LIST;
                free(child->str_val);
                child->str_val = NULL;
                child->items = NULL;
                child->item_count = 0;

                const char *orig_val = colon + 1;
                while (*orig_val == ' ') orig_val++;
                if (*orig_val) {
                    child->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
                    if (child->items) {
                        child->items[0] = parse_value(orig_val);
                        child->item_count = 1;
                    }
                }

                while (j < count && lines[j].indent > ln->indent) {
                    if (lines[j].line[0] == '-' && (lines[j].line[1] == ' ' || lines[j].line[1] == '\0')) {
                        const char *item_text = lines[j].line + 1;
                        while (*item_text == ' ') item_text++;
                        child->items = (yaml_entry_t **)xrealloc(child->items,
                            (child->item_count + 1) * sizeof(yaml_entry_t *));
                        if (child->items)
                            child->items[child->item_count++] = parse_value(item_text);
                    }
                    j++;
                }
                i = j - 1;
            } else {
                size_t k = i + 1;
                while (k < count && lines[k].line[0] == '\0') k++;
                if (k < count && lines[k].indent > ln->indent) {
                    free(child->str_val);
                    child->str_val = NULL;
                    child->type = YVAL_MAP;
                    size_t child_consumed = 0;
                    yaml_entry_t *submap = parse_map(&lines[k], count - k,
                                                       ln->indent, &child_consumed, err);
                    if (submap) {
                        child->children = submap->children;
                        child->child_count = submap->child_count;
                        free(submap->key);
                        free(submap);
                        i = k + child_consumed - 1;
                    }
                }
            }
        } else if (child->type == YVAL_MAP) {
            size_t child_consumed = 0;
            yaml_entry_t *submap = parse_map(&lines[i+1], count - i - 1,
                                               ln->indent, &child_consumed, err);
            if (submap) {
                child->children = submap->children;
                child->child_count = submap->child_count;
                free(submap->key);
                free(submap);
                i += child_consumed;
            }
        }

        map->children = (yaml_entry_t **)xrealloc(map->children,
            (map->child_count + 1) * sizeof(yaml_entry_t *));
        if (map->children)
            map->children[map->child_count++] = child;
        i++;
    }

    *consumed = i;
    return map;
}

/* ================================================================
 *  Public API
 * ================================================================ */

yaml_doc_t *yaml_parse(const char *input, char **error_msg) {
    oom_flag = false;
    if (!input) {
        if (error_msg) *error_msg = xstrdup("NULL input");
        return NULL;
    }

    yaml_lexer_t lx;
    lex_init(&lx, input);

    size_t cap = 64, count = 0;
    yaml_line_t *lines = (yaml_line_t *)xmalloc(cap * sizeof(yaml_line_t));
    if (!lines) return NULL;
    yaml_line_t ln;
    while (lex_next(&lx, &ln)) {
        if (count >= cap) {
            cap *= 2;
            yaml_line_t *nb = (yaml_line_t *)xrealloc(lines, cap * sizeof(yaml_line_t));
            if (!nb) { for (size_t i = 0; i < count; i++) line_free(&lines[i]); free(lines); return NULL; }
            lines = nb;
        }
        lines[count++] = ln;
    }

    yaml_doc_t *doc = (yaml_doc_t *)xmalloc(sizeof(yaml_doc_t));
    if (!doc) { for (size_t i = 0; i < count; i++) line_free(&lines[i]); free(lines); return NULL; }
    size_t consumed = 0;
    doc->root = parse_map(lines, count, -1, &consumed, error_msg);

    for (size_t i = 0; i < count; i++) line_free(&lines[i]);
    free(lines);

    if (!doc->root) { free(doc); return NULL; }
    return doc;
}

yaml_doc_t *yaml_parse_file(const char *path, char **error_msg) {
    oom_flag = false;
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cannot open '%s'", path);
            *error_msg = xstrdup(buf);
        }
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = (char *)xmalloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    (void)n;
    fclose(f);
    buf[sz] = '\0';
    yaml_doc_t *doc = yaml_parse(buf, error_msg);
    free(buf);
    return doc;
}

/* ================================================================
 *  Path navigation
 * ================================================================ */

static yaml_entry_t *navigate(const yaml_doc_t *doc, const char *path) {
    if (!doc || !doc->root || !path) return NULL;
    yaml_entry_t *e = doc->root;

    char *path_copy = xstrdup(path);
    if (!path_copy) return NULL;
    char *saveptr;
    char *tok = strtok_r(path_copy, ".", &saveptr);

    while (tok && e) {
        if (e->type != YVAL_MAP) { e = NULL; break; }
        yaml_entry_t *found = NULL;
        for (size_t i = 0; i < e->child_count; i++) {
            if (strcmp(e->children[i]->key, tok) == 0) { found = e->children[i]; break; }
        }
        e = found;
        tok = strtok_r(NULL, ".", &saveptr);
    }

    free(path_copy);
    return e;
}

const char *yaml_get_string(const yaml_doc_t *doc, const char *path) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_STRING) return NULL;
    return e->str_val;
}

bool yaml_get_bool(const yaml_doc_t *doc, const char *path, bool def) {
    const char *s = yaml_get_string(doc, path);
    if (!s) return def;
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0) return true;
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return false;
    return def;
}

int yaml_get_int(const yaml_doc_t *doc, const char *path, int def) {
    const char *s = yaml_get_string(doc, path);
    if (!s) return def;
    return (int)strtol(s, NULL, 10);
}

size_t yaml_list_count(const yaml_doc_t *doc, const char *path) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_LIST) return 0;
    return e->item_count;
}

const char *yaml_list_get(const yaml_doc_t *doc, const char *path, size_t index) {
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_LIST || index >= e->item_count) return NULL;
    if (e->items[index]->type != YVAL_STRING) return NULL;
    return e->items[index]->str_val;
}

void yaml_iterate(const yaml_doc_t *doc,
                  void (*fn)(const char *key, const char *value, void *user),
                  void *user)
{
    if (!doc || !doc->root || !fn) return;
    if (doc->root->type != YVAL_MAP) return;
    for (size_t i = 0; i < doc->root->child_count; i++) {
        yaml_entry_t *child = doc->root->children[i];
        if (child->type == YVAL_STRING)
            fn(child->key, child->str_val, user);
    }
}

char **yaml_map_keys(const yaml_doc_t *doc, const char *path, size_t *count) {
    if (count) *count = 0;
    if (!doc || !path) return NULL;
    yaml_entry_t *e = navigate(doc, path);
    if (!e || e->type != YVAL_MAP) return NULL;

    size_t n = e->child_count;
    char **keys = (char **)malloc(n * sizeof(char *));
    if (!keys) return NULL;

    size_t out = 0;
    for (size_t i = 0; i < n; i++) {
        if (e->children[i]->key) {
            keys[out] = strdup(e->children[i]->key);
            if (keys[out]) out++;
        }
    }

    if (out == 0) { free(keys); return NULL; }
    if (count) *count = out;
    return keys;
}

void yaml_free(yaml_doc_t *doc) {
    if (!doc) return;
    if (doc->root) free_entry(doc->root);
    free(doc);
}
