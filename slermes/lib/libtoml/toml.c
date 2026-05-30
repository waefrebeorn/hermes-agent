/*
 * toml.c — Minimal TOML v1.0 parser (J19: Python tomllib port).
 *
 * Supports: key-value pairs, tables, dotted keys, comments,
 * strings (basic + literal), integers (dec/hex/oct/bin),
 * floats, booleans, arrays.
 * Not yet: multi-line strings, inline tables, array of tables, date/time.
 */
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Forward declarations */
static void toml_node_free(toml_node_t *n);

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Skip whitespace and comments (inline only — # to end-of-line).
 * Returns pointer to first non-whitespace, non-comment character. */
static const char *skip_ws_and_comments(const char *p) {
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
        if (*p == '#') { while (*p && *p != '\n') p++; }
        else break;
    }
    return p;
}

/* Skip whitespace (including newlines for table context). */
static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* Parse a bare key (alphanumeric, underscore, dash). */
static const char *parse_bare_key(const char *p, char *key, size_t key_sz) {
    size_t i = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
        if (i < key_sz - 1) key[i++] = *p;
        p++;
    }
    key[i] = '\0';
    return (i > 0) ? p : NULL;
}

/* Parse a basic string ("...") with escape sequences. */
static const char *parse_basic_string(const char *p, char *buf, size_t buf_sz) {
    if (*p != '"') return NULL;
    p++; /* skip opening " */
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            char c = 0;
            switch (*p) {
                case '"':  c = '"'; break;
                case '\\': c = '\\'; break;
                case '/':  c = '/'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': /* skip unicode escape for now */
                case 'U': p += (*p == 'u') ? 4 : 8; continue;
                default: c = *p; break;
            }
            if (c && i < buf_sz - 1) buf[i++] = c;
            p++;
        } else {
            if (i < buf_sz - 1) buf[i++] = *p;
            p++;
        }
    }
    buf[i] = '\0';
    return (*p == '"') ? (p + 1) : NULL;
}

/* Parse a literal string ('...') — no escape processing. */
static const char *parse_literal_string(const char *p, char *buf, size_t buf_sz) {
    if (*p != '\'') return NULL;
    p++; /* skip opening ' */
    size_t i = 0;
    while (*p && *p != '\'') {
        if (i < buf_sz - 1) buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';
    return (*p == '\'') ? (p + 1) : NULL;
}

/* Parse a quoted or bare key into buffer. */
static const char *parse_key_part(const char *p, char *buf, size_t buf_sz) {
    if (*p == '"')  return parse_basic_string(p, buf, buf_sz);
    if (*p == '\'') return parse_literal_string(p, buf, buf_sz);
    return parse_bare_key(p, buf, buf_sz);
}

/* Parse dotted key into keys array. Returns number of parts or -1 on error. */
static int parse_key_path(const char *p, char keys[][128], int max_keys) {
    int count = 0;
    p = skip_ws(p);
    while (*p && count < max_keys) {
        p = parse_key_part(p, keys[count], sizeof(keys[count]));
        if (!p) return -1;
        count++;
        p = skip_ws(p);
        if (*p == '.') { p++; p = skip_ws(p); }
        else break;
    }
    return count;
}

/* Set key=value in a table node. Sets val->key to strdup(key). */
static int set_value(toml_node_t *table, const char *key, toml_node_t *val) {
    if (!table || !key || !val) return -1;
    free(val->key);
    val->key = strdup(key);
    if (!val->key) return -1;
    /* Check if key already exists */
    for (int i = 0; i < table->child_count; i++) {
        if (table->children[i] && strcmp(table->children[i]->key, key) == 0) {
            /* Replace: free old, set new */
            toml_node_free(table->children[i]);
            table->children[i] = val;
            return 0;
        }
    }
    /* Append */
    if (table->child_count >= table->child_capacity) {
        int new_cap = table->child_capacity ? table->child_capacity * 2 : 16;
        toml_node_t **newc = realloc(table->children, new_cap * sizeof(toml_node_t *));
        if (!newc) return -1;
        table->children = newc;
        table->child_capacity = new_cap;
    }
    table->children[table->child_count++] = val;
    return 0;
}

/* Create a value node with given type and string content. */
static toml_node_t *make_value(toml_type_t type, const char *key, const char *str_val) {
    toml_node_t *n = calloc(1, sizeof(toml_node_t));
    if (!n) return NULL;
    n->type = type;
    n->key = key ? strdup(key) : NULL;
    if (str_val) {
        switch (type) {
            case TOML_STRING:
                n->string_val = strdup(str_val); break;
            case TOML_INTEGER:
                if (strncmp(str_val, "0x", 2) == 0 || strncmp(str_val, "0X", 2) == 0)
                    n->int_val = strtol(str_val + 2, NULL, 16);
                else if (strncmp(str_val, "0o", 2) == 0 || strncmp(str_val, "0O", 2) == 0)
                    n->int_val = strtol(str_val + 2, NULL, 8);
                else if (strncmp(str_val, "0b", 2) == 0 || strncmp(str_val, "0B", 2) == 0)
                    n->int_val = strtol(str_val + 2, NULL, 2);
                else
                    n->int_val = atol(str_val);
                break;
            case TOML_FLOAT:
                n->float_val = atof(str_val); break;
            case TOML_BOOL:
                n->bool_val = (strcmp(str_val, "true") == 0); break;
            default: break;
        }
    }
    return n;
}

/* Parse a value (string, number, bool, array) from input.
 * Returns advanced pointer on success, NULL on error. */
static const char *parse_value(const char *p, toml_node_t **out) {
    p = skip_ws(p);
    if (!*p) return NULL;

    *out = NULL;

    /* String (basic or literal) */
    if (*p == '"') {
        char buf[4096];
        p = parse_basic_string(p, buf, sizeof(buf));
        if (!p) return NULL;
        *out = make_value(TOML_STRING, NULL, buf);
        return p;
    }
    if (*p == '\'') {
        char buf[4096];
        p = parse_literal_string(p, buf, sizeof(buf));
        if (!p) return NULL;
        *out = make_value(TOML_STRING, NULL, buf);
        return p;
    }

    /* Boolean */
    if (strncmp(p, "true", 4) == 0 && !isalnum((unsigned char)p[4])) {
        *out = make_value(TOML_BOOL, NULL, "true");
        return p + 4;
    }
    if (strncmp(p, "false", 5) == 0 && !isalnum((unsigned char)p[5])) {
        *out = make_value(TOML_BOOL, NULL, "false");
        return p + 5;
    }

    /* Array */
    if (*p == '[') {
        p++; /* skip [ */
        toml_node_t *arr = calloc(1, sizeof(toml_node_t));
        if (!arr) return NULL;
        arr->type = TOML_ARRAY;
        arr->key = NULL;
        while (*p) {
            p = skip_ws_and_comments(p);
            if (*p == ']') { p++; *out = arr; return p; }
            /* Allow trailing comma */
            if (*p == ',') { p++; continue; }
            toml_node_t *elem = NULL;
            p = parse_value(p, &elem);
            if (!elem) { toml_node_free(arr); return NULL; }
            if (arr->child_count >= arr->child_capacity) {
                int new_cap = arr->child_capacity ? arr->child_capacity * 2 : 8;
                toml_node_t **newc = realloc(arr->children, new_cap * sizeof(toml_node_t *));
                if (!newc) { toml_node_free(elem); toml_node_free(arr); return NULL; }
                arr->children = newc;
                arr->child_capacity = new_cap;
            }
            arr->children[arr->child_count++] = elem;
        }
        toml_node_free(arr);
        return NULL; /* unclosed array */
    }

    /* Number (float or integer) */
    {
        const char *start = p;
        int is_float = 0;
        if (*p == '+' || *p == '-') p++;
        if (*p == 'i' || *p == 'n') { /* inf/nan */
            /* Not properly supported, treat as string? Return error */
            return NULL;
        }
        /* Check for hex/oct/bin prefix */
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X' || p[1] == 'o' || p[1] == 'O' || p[1] == 'b' || p[1] == 'B')) {
            p += 2;
            while (*p && (isxdigit((unsigned char)*p) || *p == '_')) { if (*p != '_') p++; else p++; }
            size_t len = (size_t)(p - start);
            char buf[64];
            snprintf(buf, sizeof(buf), "%.*s", (int)len, start);
            *out = make_value(TOML_INTEGER, NULL, buf);
            return p;
        }
        /* Decimal number */
        while (*p && (isdigit((unsigned char)*p) || *p == '_')) p++;
        if (*p == '.' || *p == 'e' || *p == 'E') {
            is_float = 1;
            if (*p == '.') {
                p++;
                while (*p && (isdigit((unsigned char)*p) || *p == '_')) p++;
            }
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') p++;
                while (*p && (isdigit((unsigned char)*p) || *p == '_')) p++;
            }
        }
        size_t len = (size_t)(p - start);
        if (len == 0) return NULL;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*s", (int)len, start);
        *out = make_value(is_float ? TOML_FLOAT : TOML_INTEGER, NULL, buf);
        return p;
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */

toml_doc_t *toml_parse(const char *input) {
    if (!input) return NULL;

    toml_doc_t *doc = calloc(1, sizeof(toml_doc_t));
    if (!doc) return NULL;

    doc->root = calloc(1, sizeof(toml_node_t));
    if (!doc->root) { free(doc); return NULL; }
    doc->root->type = TOML_TABLE;
    doc->root->key = strdup("(root)");

    toml_node_t *current_table = doc->root;
    const char *p = input;

    while (*p) {
        p = skip_ws(p);
        if (!*p) break;

        /* Comment or blank line */
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* Table header: [table] or [[array]] */
        if (*p == '[') {
            p++;
            int is_array = 0;
            if (*p == '[') { is_array = 1; p++; }
            p = skip_ws(p);

            char keys[TOML_MAX_DEPTH][128];
            int nk = parse_key_path(p, keys, TOML_MAX_DEPTH);
            if (nk < 0) { toml_free(doc); return NULL; }

            /* Find end of header */
            const char *end = p;
            while (*end && *end != ']') end++;
            if (!*end) { toml_free(doc); return NULL; }
            if (is_array) {
                if (end[1] != ']') { toml_free(doc); return NULL; }
                end++;
            }
            p = end + 1;

            /* Navigate to/create table */
            current_table = doc->root;
            for (int i = 0; i < nk; i++) {
                toml_node_t *child = NULL;
                /* Look for existing */
                for (int j = 0; j < current_table->child_count; j++) {
                    if (current_table->children[j] &&
                        strcmp(current_table->children[j]->key, keys[i]) == 0) {
                        child = current_table->children[j];
                        break;
                    }
                }
                if (!child) {
                    child = calloc(1, sizeof(toml_node_t));
                    if (!child) { toml_free(doc); return NULL; }
                    child->type = TOML_TABLE;
                    child->key = strdup(keys[i]);
                    /* Add to parent */
                    if (current_table->child_count >= current_table->child_capacity) {
                        int nc = current_table->child_capacity ? current_table->child_capacity * 2 : 16;
                        toml_node_t **newc = realloc(current_table->children, nc * sizeof(toml_node_t *));
                        if (!newc) { free(child->key); free(child); toml_free(doc); return NULL; }
                        current_table->children = newc;
                        current_table->child_capacity = nc;
                    }
                    current_table->children[current_table->child_count++] = child;
                }
                current_table = child;
            }
            continue;
        }

        /* Key-value pair */
        {
            char keys[TOML_MAX_DEPTH][128];
            int nk = parse_key_path(p, keys, TOML_MAX_DEPTH);
            if (nk < 0) { toml_free(doc); return NULL; }

            /* Advance p past keys */
            p = skip_ws(p);
            int part = 0;
            while (*p && part < nk) {
                p = parse_key_part(p, keys[part], sizeof(keys[part]));
                if (!p) { toml_free(doc); return NULL; }
                part++;
                p = skip_ws(p);
                if (*p == '.') { p++; p = skip_ws(p); }
                else break;
            }

            p = skip_ws(p);
            if (*p != '=') { toml_free(doc); return NULL; }
            p++; /* skip = */
            p = skip_ws(p);

            toml_node_t *val = NULL;
            p = parse_value(p, &val);
            if (!val) { toml_free(doc); return NULL; }

            /* Set value in appropriate table */
            toml_node_t *target = current_table;
            if (nk > 1) {
                /* Navigate to parent of last key */
                target = doc->root;
                for (int i = 0; i < nk - 1; i++) {
                    toml_node_t *child = NULL;
                    for (int j = 0; j < target->child_count; j++) {
                        if (target->children[j] &&
                            strcmp(target->children[j]->key, keys[i]) == 0) {
                            child = target->children[j];
                            break;
                        }
                    }
                    if (!child) {
                        child = calloc(1, sizeof(toml_node_t));
                        if (!child) { toml_node_free(val); toml_free(doc); return NULL; }
                        child->type = TOML_TABLE;
                        child->key = strdup(keys[i]);
                        if (target->child_count >= target->child_capacity) {
                            int nc = target->child_capacity ? target->child_capacity * 2 : 16;
                            toml_node_t **newc = realloc(target->children, nc * sizeof(toml_node_t *));
                            if (!newc) { free(child->key); free(child); toml_node_free(val); toml_free(doc); return NULL; }
                            target->children = newc;
                            target->child_capacity = nc;
                        }
                        target->children[target->child_count++] = child;
                    }
                    target = child;
                }
                set_value(target, keys[nk - 1], val);
            } else {
                set_value(current_table, keys[0], val);
            }

            /* Skip to end of line */
            while (*p && *p != '\n') p++;
            continue;
        }
    }

    return doc;
}

toml_node_t *toml_get(toml_doc_t *doc, const char *key_path) {
    if (!doc || !doc->root || !key_path) return NULL;

    char keys[TOML_MAX_DEPTH][128];
    /* Parse key path manually */
    const char *p = key_path;
    int nk = 0;
    while (*p && nk < TOML_MAX_DEPTH) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len >= sizeof(keys[0])) return NULL;
        memcpy(keys[nk], p, len);
        keys[nk][len] = '\0';
        nk++;
        if (dot) p = dot + 1; else break;
    }

    toml_node_t *cur = doc->root;
    for (int i = 0; i < nk; i++) {
        bool found = false;
        for (int j = 0; j < cur->child_count; j++) {
            if (cur->children[j] && strcmp(cur->children[j]->key, keys[i]) == 0) {
                cur = cur->children[j];
                found = true;
                break;
            }
        }
        if (!found) return NULL;
    }
    return cur;
}

const char *toml_get_string(toml_doc_t *doc, const char *key_path) {
    toml_node_t *n = toml_get(doc, key_path);
    return (n && n->type == TOML_STRING) ? n->string_val : NULL;
}

long toml_get_int(toml_doc_t *doc, const char *key_path) {
    toml_node_t *n = toml_get(doc, key_path);
    return (n && n->type == TOML_INTEGER) ? n->int_val : 0;
}

double toml_get_float(toml_doc_t *doc, const char *key_path) {
    toml_node_t *n = toml_get(doc, key_path);
    return (n && n->type == TOML_FLOAT) ? n->float_val : 0.0;
}

bool toml_get_bool(toml_doc_t *doc, const char *key_path) {
    toml_node_t *n = toml_get(doc, key_path);
    return (n && n->type == TOML_BOOL) ? n->bool_val : false;
}

static void toml_node_free(toml_node_t *n) {
    if (!n) return;
    free(n->key);
    if (n->type == TOML_STRING) free(n->string_val);
    for (int i = 0; i < n->child_count; i++)
        toml_node_free(n->children[i]);
    free(n->children);
    free(n);
}

char *toml_serialize(toml_doc_t *doc) {
    if (!doc || !doc->root) return NULL;
    /* Simple serialization: just format top-level keys */
    char buf[65536];
    size_t pos = 0;

    /* Helper lambda via inline function */
    /* Serialize a node to buffer */
    /* Just do top-level for now */
    (void)buf; (void)pos;

    /* Return empty string for now — full serialization is complex */
    return strdup("");
}

void toml_free(toml_doc_t *doc) {
    if (!doc) return;
    toml_node_free(doc->root);
    free(doc);
}
