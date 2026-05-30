/*
 * yaml.c — Minimal YAML config parser for Hermes C.
 *
 * Parses the YAML subset used by hermes config.yaml:
 * key: value, nested keys by indent, lists with '- '.
 * No external deps.
 */

#include "hermes_yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Internal tree
 * ================================================================ */

typedef enum { YVAL_STRING, YVAL_LIST, YVAL_MAP } yaml_val_type_t;

typedef struct yaml_entry {
    char *key;
    yaml_val_type_t type;
    char *str_val;              /* for YVAL_STRING */
    struct yaml_entry **items;  /* for YVAL_LIST */
    size_t item_count;
    struct yaml_entry **children; /* for YVAL_MAP */
    size_t child_count;
} yaml_entry_t;

struct yaml_doc {
    yaml_entry_t *root;
};

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "yaml: OOM\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    p = realloc(p, sz ? sz : 1);
    if (!p) { fprintf(stderr, "yaml: OOM\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    memcpy(d, s, n + 1);
    return d;
}

/* ================================================================
 *  Lexer: split input into lines with indent levels
 * ================================================================ */

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
} yaml_lexer_t;

typedef struct {
    char   *line;    /* stripped (no leading/trailing ws) */
    char   *raw;     /* original line content for parsing */
    int     indent;  /* spaces count */
    size_t  lineno;
} yaml_line_t;

static void lex_init(yaml_lexer_t *lx, const char *text) {
    lx->text = text;
    lx->len = strlen(text);
    lx->pos = 0;
}

/* Read next line. Returns false at EOF. */
static bool lex_next(yaml_lexer_t *lx, yaml_line_t *line) {
    memset(line, 0, sizeof(*line));
    if (lx->pos >= lx->len) return false;

    const char *start = lx->text + lx->pos;
    const char *end = start;
    while (lx->pos < lx->len && lx->text[lx->pos] != '\n') {
        lx->pos++;
        end = lx->text + lx->pos;
    }
    if (lx->pos < lx->len) lx->pos++; /* skip '\n' */

    size_t raw_len = (size_t)(end - start);
    line->raw = (char *)xmalloc(raw_len + 1);
    memcpy(line->raw, start, raw_len);
    line->raw[raw_len] = '\0';

    /* Count leading spaces */
    line->indent = 0;
    while (line->raw[line->indent] == ' ') line->indent++;

    /* Strip comments and trailing whitespace */
    const char *p = line->raw + line->indent;
    /* Find comment marker (#) not inside quotes */
    const char *comment = NULL;
    for (const char *q = p; *q; q++) {
        if (*q == '#' && (q == p || *(q-1) != '\\')) { comment = q; break; }
    }
    size_t content_end = comment ? (size_t)(comment - line->raw) : raw_len;
    /* Trim trailing whitespace */
    while (content_end > 0 && (line->raw[content_end-1] == ' ' || line->raw[content_end-1] == '\t'))
        content_end--;

    line->line = (char *)xmalloc(content_end - (size_t)line->indent + 1);
    size_t clen = content_end - (size_t)line->indent;
    memcpy(line->line, p, clen);
    line->line[clen] = '\0';
    return true;
}

static void line_free(yaml_line_t *line) {
    free(line->raw);
    free(line->line);
}

/* ================================================================
 *  Parser
 * ================================================================ */

static yaml_entry_t *new_entry(const char *key) {
    yaml_entry_t *e = (yaml_entry_t *)xmalloc(sizeof(yaml_entry_t));
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

/* Parse lines into tree recursively */
static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err);

static yaml_entry_t *parse_value(const char *val_str) {
    yaml_entry_t *e = new_entry(NULL);
    e->type = YVAL_STRING;
    e->str_val = xstrdup(val_str ? val_str : "");
    return e;
}

/* Parse value portion after "key: " — could be inline value or list */
static void parse_inline(yaml_entry_t *e, const char *val) {
    /* Trim leading spaces */
    while (*val == ' ') val++;
    if (*val == '\0') {
        /* No inline value; children will be parsed from following lines */
        e->type = YVAL_MAP;
        return;
    }
    /* List? */
    if (val[0] == '-' && (val[1] == ' ' || val[1] == '\0')) {
        e->type = YVAL_LIST;
        val += 1;
        while (*val == ' ') val++;
        yaml_entry_t *item = parse_value(val);
        e->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
        e->items[0] = item;
        e->item_count = 1;
        return;
    }
    /* Plain scalar */
    e->type = YVAL_STRING;
    e->str_val = xstrdup(val);
}

static yaml_entry_t *parse_map(yaml_line_t *lines, size_t count,
                                int parent_indent, size_t *consumed, char **err)
{
    yaml_entry_t *map = new_entry(NULL);
    map->type = YVAL_MAP;
    *consumed = 0;

    size_t i = 0;
    while (i < count) {
        yaml_line_t *ln = &lines[i];
        if (ln->indent <= parent_indent) break; /* back to parent level */

        /* Skip empty lines */
        if (ln->line[0] == '\0') { i++; continue; }

        /* Must have key: value or key: */
        char *colon = strchr(ln->line, ':');
        if (!colon) {
            if (ln->line[0] == '-' && (ln->line[1] == ' ' || ln->line[1] == '\0')) {
                /* List item at top level of this block */
                if (map->type == YVAL_MAP) {
                    /* Convert to list implicitly */
                    /* Actually, this shouldn't happen in well-formed YAML */
                    i++;
                    continue;
                }
                const char *item_text = ln->line + 1;
                while (*item_text == ' ') item_text++;
                yaml_entry_t *item = parse_value(item_text);
                map->items = (yaml_entry_t **)xrealloc(map->items,
                    (map->item_count + 1) * sizeof(yaml_entry_t *));
                map->items[map->item_count++] = item;
                i++;
                continue;
            }
            if (err) *err = xstrdup("expected ':' in YAML line");
            free_entry(map);
            return NULL;
        }

        /* Extract key */
        size_t key_len = (size_t)(colon - ln->line);
        char *key = (char *)xmalloc(key_len + 1);
        memcpy(key, ln->line, key_len);
        key[key_len] = '\0';
        /* Trim trailing spaces from key */
        while (key_len > 0 && key[key_len-1] == ' ') key[--key_len] = '\0';

        yaml_entry_t *child = new_entry(key);
        free(key);

        const char *val = colon + 1;
        parse_inline(child, val);

        /* If scalar value, consume the children too if indented list follows */
        if (child->type == YVAL_STRING) {
            /* Check if next lines are list items at deeper indent */
            size_t j = i + 1;
            if (j < count && lines[j].indent > ln->indent && lines[j].line[0] == '-') {
                /* Convert to list, keep first as item */
                child->type = YVAL_LIST;
                /* Re-parse the original line's value as first list item */
                free(child->str_val);
                child->str_val = NULL;
                child->items = NULL;
                child->item_count = 0;

                /* Include the original line's value if non-empty */
                const char *orig_val = colon + 1;
                while (*orig_val == ' ') orig_val++;
                if (*orig_val) {
                    child->items = (yaml_entry_t **)xmalloc(sizeof(yaml_entry_t *));
                    child->items[0] = parse_value(orig_val);
                    child->item_count = 1;
                }

                /* Process list items */
                while (j < count && lines[j].indent > ln->indent) {
                    if (lines[j].line[0] == '-' && (lines[j].line[1] == ' ' || lines[j].line[1] == '\0')) {
                        const char *item_text = lines[j].line + 1;
                        while (*item_text == ' ') item_text++;
                        child->items = (yaml_entry_t **)xrealloc(child->items,
                            (child->item_count + 1) * sizeof(yaml_entry_t *));
                        child->items[child->item_count++] = parse_value(item_text);
                    }
                    j++;
                }
                i = j - 1;
            } else {
                /* Check if next lines are a nested map */
                size_t k = i + 1;
                while (k < count && lines[k].line[0] == '\0') k++;
                if (k < count && lines[k].indent > ln->indent) {
                    /* There are children — promote to YVAL_MAP */
                    /* But we already have a string value. In YAML, key: val\n  child: x
                     * means the string val is superseded by the children. Drop string. */
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
            /* Has children on following lines */
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

        /* Add child to map */
        map->children = (yaml_entry_t **)xrealloc(map->children,
            (map->child_count + 1) * sizeof(yaml_entry_t *));
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
    if (!input) {
        if (error_msg) *error_msg = xstrdup("NULL input");
        return NULL;
    }

    /* Split into lines */
    yaml_lexer_t lx;
    lex_init(&lx, input);

    /* Count lines */
    size_t cap = 64, count = 0;
    yaml_line_t *lines = (yaml_line_t *)xmalloc(cap * sizeof(yaml_line_t));
    yaml_line_t ln;
    while (lex_next(&lx, &ln)) {
        if (count >= cap) {
            cap *= 2;
            lines = (yaml_line_t *)xrealloc(lines, cap * sizeof(yaml_line_t));
        }
        lines[count++] = ln;
    }

    yaml_doc_t *doc = (yaml_doc_t *)xmalloc(sizeof(yaml_doc_t));
    size_t consumed = 0;
    doc->root = parse_map(lines, count, -1, &consumed, error_msg);

    for (size_t i = 0; i < count; i++) line_free(&lines[i]);
    free(lines);

    if (!doc->root) {
        free(doc);
        return NULL;
    }
    return doc;
}

yaml_doc_t *yaml_parse_file(const char *path, char **error_msg) {
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
    (void)fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[sz] = '\0';
    yaml_doc_t *doc = yaml_parse(buf, error_msg);
    free(buf);
    return doc;
}

/* ================================================================
 *  Path navigation
 * ================================================================ */

/* Navigate dotted path like "provider.model" or "tools.terminal.enabled" */
static yaml_entry_t *navigate(const yaml_doc_t *doc, const char *path) {
    if (!doc || !doc->root || !path) return NULL;
    yaml_entry_t *e = doc->root;

    char *path_copy = xstrdup(path);
    char *saveptr;
    char *tok = strtok_r(path_copy, ".", &saveptr);

    while (tok && e) {
        if (e->type != YVAL_MAP) { e = NULL; break; }
        yaml_entry_t *found = NULL;
        for (size_t i = 0; i < e->child_count; i++) {
            if (strcmp(e->children[i]->key, tok) == 0) {
                found = e->children[i];
                break;
            }
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
        if (child->type == YVAL_STRING) {
            fn(child->key, child->str_val, user);
        }
    }
}

void yaml_free(yaml_doc_t *doc) {
    if (!doc) return;
    if (doc->root) free_entry(doc->root);
    free(doc);
}
