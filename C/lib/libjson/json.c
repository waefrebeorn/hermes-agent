/*
 * json.c — Standalone JSON parser/serializer.
 * Recursive-descent, zero external deps. Never calls exit().
 *
 * MIT License — WuBu Hermes Project
 */

#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

/* ================================================================
 *  Internal helpers — graceful OOM, never exit()
 * ================================================================ */

static bool oom_flag = false;

bool json_oom_occurred(void) { return oom_flag; }

static void set_oom(void) { oom_flag = true; }

static void *xmalloc(size_t sz) {
    if (oom_flag) return NULL;
    void *p = malloc(sz ? sz : 1);
    if (!p) { set_oom(); return NULL; }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    if (oom_flag) return NULL;
    void *r = realloc(p, sz ? sz : 1);
    if (!r) { set_oom(); return NULL; }
    return r;
}

static char *xstrdup(const char *s) {
    if (!s || oom_flag) return NULL;
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* ================================================================
 *  Parser state
 * ================================================================ */

typedef struct {
    const char *pos;
    const char *end;
    const char *start;
    char *err;
} parse_ctx;

static void set_error(parse_ctx *ctx, const char *fmt, ...) {
    if (ctx->err) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ctx->err = xstrdup(buf);
}

static void skip_ws(parse_ctx *ctx) {
    while (ctx->pos < ctx->end && (unsigned char)*ctx->pos <= ' ') ctx->pos++;
}

static bool match(parse_ctx *ctx, char c) {
    skip_ws(ctx);
    if (ctx->pos < ctx->end && *ctx->pos == c) { ctx->pos++; return true; }
    return false;
}

/* ================================================================
 *  Low-level parsers
 * ================================================================ */

static json_t *parse_value(parse_ctx *ctx);

static json_t *parse_string(parse_ctx *ctx) {
    if (!match(ctx, '"')) { set_error(ctx, "Expected '\"'"); return NULL; }
    size_t cap = 64, len = 0;
    char *buf = (char *)xmalloc(cap);
    if (!buf) return NULL;

    while (ctx->pos < ctx->end && *ctx->pos != '"') {
        if (len >= cap - 1) {
            cap *= 2;
            char *nb = (char *)xrealloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        if (*ctx->pos == '\\') {
            ctx->pos++;
            if (ctx->pos >= ctx->end) { free(buf); set_error(ctx, "Unexpected end in escape"); return NULL; }
            switch (*ctx->pos) {
                case '"':  buf[len++] = '"';  break;
                case '\\': buf[len++] = '\\'; break;
                case '/':  buf[len++] = '/';  break;
                case 'b':  buf[len++] = '\b'; break;
                case 'f':  buf[len++] = '\f'; break;
                case 'n':  buf[len++] = '\n'; break;
                case 'r':  buf[len++] = '\r'; break;
                case 't':  buf[len++] = '\t'; break;
                case 'u': {
                    /* Basic 4-digit unicode — store as raw bytes */
                    if (ctx->pos + 4 >= ctx->end) {
                        free(buf); set_error(ctx, "Truncated \\uXXXX"); return NULL;
                    }
                    char tmp[5] = {ctx->pos[1], ctx->pos[2], ctx->pos[3], ctx->pos[4], 0};
                    long cp = strtol(tmp, NULL, 16);
                    ctx->pos += 4;
                    /* Handle surrogate pairs (U+D800-U+DBFF high + U+DC00-U+DFFF low) */
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        /* ctx->pos points to 4th hex digit. Low surrogate \uXXXX starts at ctx->pos+1
                         * The while loop does ctx->pos++ after the switch, so we advance by 6
                         * to land on the closing quote after the loop increment. */
                        if (ctx->pos + 7 <= ctx->end && ctx->pos[1] == '\\' && ctx->pos[2] == 'u') {
                            char lo_tmp[5] = {ctx->pos[3], ctx->pos[4], ctx->pos[5], ctx->pos[6], 0};
                            long lo = strtol(lo_tmp, NULL, 16);
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00);
                                ctx->pos += 6; /* loop does ++, total +7 past current digit + \uXXXX */
                                goto encode_cp;
                            }
                        }
                        /* Invalid/low surrogate pair — replace with U+FFFD */
                        cp = 0xFFFD;
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        /* Lone low surrogate — replace with U+FFFD */
                        cp = 0xFFFD;
                    }
                  encode_cp:
                    if (cp < 128) buf[len++] = (char)cp;
                    else if (cp < 0x800) {
                        buf[len++] = (char)(0xC0 | (cp >> 6));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        buf[len++] = (char)(0xE0 | (cp >> 12));
                        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[len++] = (char)(0xF0 | (cp >> 18));
                        buf[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                        buf[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: buf[len++] = *ctx->pos; break;
            }
        } else {
            buf[len++] = *ctx->pos;
        }
        ctx->pos++;
    }
    buf[len] = '\0';
    if (!match(ctx, '"')) { free(buf); set_error(ctx, "Unterminated string"); return NULL; }

    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) { free(buf); return NULL; }
    n->type = JSON_STRING;
    n->str_val = buf;
    return n;
}

static json_t *parse_number(parse_ctx *ctx) {
    const char *start = ctx->pos;
    if (*ctx->pos == '-') ctx->pos++;
    while (ctx->pos < ctx->end && isdigit((unsigned char)*ctx->pos)) ctx->pos++;
    if (ctx->pos < ctx->end && *ctx->pos == '.') {
        ctx->pos++;
        while (ctx->pos < ctx->end && isdigit((unsigned char)*ctx->pos)) ctx->pos++;
    }
    if (ctx->pos < ctx->end && (*ctx->pos == 'e' || *ctx->pos == 'E')) {
        ctx->pos++;
        if (ctx->pos < ctx->end && (*ctx->pos == '+' || *ctx->pos == '-')) ctx->pos++;
        while (ctx->pos < ctx->end && isdigit((unsigned char)*ctx->pos)) ctx->pos++;
    }

    size_t len = (size_t)(ctx->pos - start);
    char tmp[128];
    if (len >= sizeof(tmp)) { set_error(ctx, "Number too long"); return NULL; }
    memcpy(tmp, start, len);
    tmp[len] = '\0';

    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_NUMBER;
    n->num_val = strtod(tmp, NULL);
    return n;
}

static json_t *parse_array(parse_ctx *ctx) {
    json_t *arr = (json_t *)xmalloc(sizeof(json_t));
    if (!arr) return NULL;
    arr->type = JSON_ARRAY;
    arr->c.items = NULL;
    arr->c.keys = NULL;
    arr->c.count = 0;

    if (match(ctx, ']')) return arr;

    do {
        json_t *val = parse_value(ctx);
        if (!val) { json_free(arr); return NULL; }
        json_append(arr, val);
    } while (match(ctx, ','));

    if (!match(ctx, ']')) {
        set_error(ctx, "Expected ']' or ',' in array");
        json_free(arr);
        return NULL;
    }
    return arr;
}

static json_t *parse_object(parse_ctx *ctx) {
    json_t *obj = (json_t *)xmalloc(sizeof(json_t));
    if (!obj) return NULL;
    obj->type = JSON_OBJECT;
    obj->c.items = NULL;
    obj->c.keys = NULL;
    obj->c.count = 0;

    if (match(ctx, '}')) return obj;

    do {
        json_t *key = parse_string(ctx);
        if (!key) { json_free(obj); return NULL; }
        if (!match(ctx, ':')) {
            json_free(key); json_free(obj);
            set_error(ctx, "Expected ':' after object key");
            return NULL;
        }
        json_t *val = parse_value(ctx);
        if (!val) { json_free(key); json_free(obj); return NULL; }

        json_set(obj, key->str_val, val);
        json_free(key);
    } while (match(ctx, ','));

    if (!match(ctx, '}')) {
        set_error(ctx, "Expected '}' or ',' in object");
        json_free(obj);
        return NULL;
    }
    return obj;
}

static json_t *parse_value(parse_ctx *ctx) {
    skip_ws(ctx);
    if (ctx->pos >= ctx->end) { set_error(ctx, "Unexpected end of input"); return NULL; }
    switch (*ctx->pos) {
        case '"': return parse_string(ctx);
        case '{': ctx->pos++; return parse_object(ctx);
        case '[': ctx->pos++; return parse_array(ctx);
        case 't': case 'f': {
            json_t *n = (json_t *)xmalloc(sizeof(json_t));
            if (!n) return NULL;
            n->type = JSON_BOOL;
            if (strncmp(ctx->pos, "true", 4) == 0) { n->bool_val = true; ctx->pos += 4; }
            else if (strncmp(ctx->pos, "false", 5) == 0) { n->bool_val = false; ctx->pos += 5; }
            else { free(n); set_error(ctx, "Expected 'true' or 'false'"); return NULL; }
            return n;
        }
        case 'n': {
            json_t *n = (json_t *)xmalloc(sizeof(json_t));
            if (!n) return NULL;
            n->type = JSON_NULL;
            if (strncmp(ctx->pos, "null", 4) != 0) { free(n); set_error(ctx, "Expected 'null'"); return NULL; }
            ctx->pos += 4;
            return n;
        }
        default:
            if (*ctx->pos == '-' || isdigit((unsigned char)*ctx->pos)) return parse_number(ctx);
            set_error(ctx, "Unexpected character '%c'", *ctx->pos);
            return NULL;
    }
}

/* ================================================================
 *  Public API: Parse
 * ================================================================ */

json_t *json_parse(const char *input, char **error_msg) {
    oom_flag = false;
    if (!input) { if (error_msg) *error_msg = xstrdup("NULL input"); return NULL; }
    parse_ctx ctx;
    ctx.pos = input;
    ctx.end = input + strlen(input);
    ctx.start = input;
    ctx.err = NULL;

    json_t *result = parse_value(&ctx);
    skip_ws(&ctx);

    if (ctx.err || ctx.pos < ctx.end) {
        if (!result && !ctx.err) set_error(&ctx, "Parse failed");
        if (result) { json_free(result); result = NULL; }
    }

    if (error_msg && ctx.err) *error_msg = ctx.err;
    else free(ctx.err);
    return result;
}

json_t *json_parse_file(const char *path, char **error_msg) {
    oom_flag = false;
    if (!path) { if (error_msg) *error_msg = xstrdup("NULL path"); return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) { if (error_msg) *error_msg = xstrdup("Cannot open file"); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); if (error_msg) *error_msg = xstrdup("Cannot stat file"); return NULL; }
    char *buf = (char *)xmalloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    json_t *result = json_parse(buf, error_msg);
    free(buf);
    return result;
}

/* ================================================================
 *  Public API: Builders
 * ================================================================ */

json_t *json_null(void) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_NULL;
    return n;
}

json_t *json_bool(bool val) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_BOOL;
    n->bool_val = val;
    return n;
}

json_t *json_number(double val) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_NUMBER;
    n->num_val = val;
    return n;
}

json_t *json_string(const char *val) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_STRING;
    n->str_val = xstrdup(val);
    return n;
}

json_t *json_array(void) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_ARRAY;
    n->c.items = NULL;
    n->c.keys = NULL;
    n->c.count = 0;
    return n;
}

json_t *json_object(void) {
    json_t *n = (json_t *)xmalloc(sizeof(json_t));
    if (!n) return NULL;
    n->type = JSON_OBJECT;
    n->c.items = NULL;
    n->c.keys = NULL;
    n->c.count = 0;
    return n;
}

/* ================================================================
 *  Public API: Array ops
 * ================================================================ */

void json_append(json_t *arr, json_t *item) {
    if (!arr || !item || arr->type != JSON_ARRAY || oom_flag) return;
    size_t i = arr->c.count;
    json_t **ni = (json_t **)xrealloc(arr->c.items, (i + 1) * sizeof(json_t *));
    if (!ni) return;
    arr->c.items = ni;
    arr->c.items[i] = item;
    arr->c.count = i + 1;
}

json_t *json_get(const json_t *arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY || index >= arr->c.count) return NULL;
    return arr->c.items[index];
}

size_t json_len(const json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->c.count;
}

/* ================================================================
 *  Public API: Object ops
 * ================================================================ */

void json_set(json_t *obj, const char *key, json_t *val) {
    if (!obj || !key || !val || obj->type != JSON_OBJECT || oom_flag) return;
    /* Replace existing key */
    for (size_t i = 0; i < obj->c.count; i++) {
        if (strcmp(obj->c.keys[i], key) == 0) {
            json_free(obj->c.items[i]);
            obj->c.items[i] = val;
            return;
        }
    }
    /* New key */
    size_t i = obj->c.count;
    char **nk = (char **)xrealloc(obj->c.keys, (i + 1) * sizeof(char *));
    json_t **ni = (json_t **)xrealloc(obj->c.items, (i + 1) * sizeof(json_t *));
    if (!nk || !ni) return;
    obj->c.keys = nk;
    obj->c.items = ni;
    obj->c.keys[i] = xstrdup(key);
    obj->c.items[i] = val;
    obj->c.count = i + 1;
}

json_t *json_obj_get(const json_t *obj, const char *key) {
    if (!obj || !key || obj->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->c.count; i++)
        if (strcmp(obj->c.keys[i], key) == 0)
            return obj->c.items[i];
    return NULL;
}

const char *json_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING) return def;
    return v->str_val;
}

double json_get_num(const json_t *obj, const char *key, double def) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return v->num_val;
}

bool json_get_bool(const json_t *obj, const char *key, bool def) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val;
}

/* ================================================================
 *  JSON Pointer (RFC 6901)
 * ================================================================
 * Traverse a JSON document along a JSON Pointer path.
 * Supports: /key, /key/nested, /array/0/index, /escaped~0~1
 * Returns the referenced node or NULL if path doesn't exist.
 * The root pointer "" returns the root node itself.
 * Non-empty paths must start with '/'. */

static const char *unescape_token(const char *start, size_t len,
                                   char *buf, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; i < len && j < cap - 1; i++) {
        if (start[i] == '~' && i + 1 < len) {
            if (start[i + 1] == '0') { buf[j++] = '~'; i++; }
            else if (start[i + 1] == '1') { buf[j++] = '/'; i++; }
            else { buf[j++] = '~'; }
        } else {
            buf[j++] = start[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

json_t *json_pointer_get(const json_t *root, const char *path) {
    if (!root || !path) return NULL;

    /* Empty pointer → root */
    if (*path == '\0') return (json_t *)root;

    /* Path must start with '/' */
    if (*path != '/') return NULL;

    const json_t *cur = root;
    const char *p = path + 1; /* skip leading '/' */

    while (*p && cur) {
        /* Find end of current token */
        const char *end = p;
        while (*end && *end != '/') end++;
        size_t tok_len = (size_t)(end - p);

        if (cur->type == JSON_OBJECT) {
            char key[256];
            unescape_token(p, tok_len, key, sizeof(key));
            cur = json_obj_get(cur, key);
        } else if (cur->type == JSON_ARRAY) {
            /* Parse array index */
            char idx_buf[32];
            size_t cp = tok_len < sizeof(idx_buf) - 1 ? tok_len : sizeof(idx_buf) - 1;
            memcpy(idx_buf, p, cp);
            idx_buf[cp] = '\0';
            char *endp = NULL;
            long idx = strtol(idx_buf, &endp, 10);
            if (endp == idx_buf || idx < 0 || (size_t)idx >= json_len(cur))
                return NULL;
            cur = json_get(cur, (size_t)idx);
        } else {
            /* Scalar — can't descend further */
            return NULL;
        }

        p = end;
        if (*p == '/') p++;
    }

    return (json_t *)cur;
}

/* ================================================================
 *  Serialization
 * ================================================================ */

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool oom;
} ser_ctx;

static void ser_putc(ser_ctx *s, char c) {
    if (s->oom) return;
    if (s->len + 1 >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 256;
        char *nb = (char *)realloc(s->buf, s->cap);
        if (!nb) { s->oom = true; return; }
        s->buf = nb;
    }
    s->buf[s->len++] = c;
}

static void ser_puts(ser_ctx *s, const char *str) {
    if (s->oom || !str) return;
    while (*str) ser_putc(s, *str++);
}

static void ser_put_escaped(ser_ctx *s, const char *str) {
    ser_putc(s, '"');
    while (*str) {
        unsigned char c = (unsigned char)*str;
        switch (c) {
            case '"':  ser_puts(s, "\\\""); break;
            case '\\': ser_puts(s, "\\\\"); break;
            case '\b': ser_puts(s, "\\b");  break;
            case '\f': ser_puts(s, "\\f");  break;
            case '\n': ser_puts(s, "\\n");  break;
            case '\r': ser_puts(s, "\\r");  break;
            case '\t': ser_puts(s, "\\t");  break;
            default:
                if (c < 32) { char esc[8]; snprintf(esc, sizeof(esc), "\\u%04x", c); ser_puts(s, esc); }
                else ser_putc(s, (char)c);
                break;
        }
        str++;
    }
    ser_putc(s, '"');
}

static void ser_node(ser_ctx *s, const json_t *n, int depth, int indent) {
    if (!n) { ser_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_NULL:  ser_puts(s, "null"); break;
        case JSON_BOOL:  ser_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", n->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", n->num_val);
            ser_puts(s, tmp);
            break;
        }
        case JSON_STRING: ser_put_escaped(s, n->str_val); break;
        case JSON_ARRAY: {
            ser_putc(s, '[');
            int new_indent = indent > 0 ? depth + 1 : 0;
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) ser_putc(s, ',');
                if (indent > 0) { ser_putc(s, '\n'); for (int j = 0; j < new_indent * indent; j++) ser_putc(s, ' '); }
                ser_node(s, n->c.items[i], depth + 1, indent);
            }
            if (indent > 0 && n->c.count > 0) { ser_putc(s, '\n'); for (int j = 0; j < depth * indent; j++) ser_putc(s, ' '); }
            ser_putc(s, ']');
            break;
        }
        case JSON_OBJECT: {
            ser_putc(s, '{');
            int new_indent = indent > 0 ? depth + 1 : 0;
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) ser_putc(s, ',');
                if (indent > 0) { ser_putc(s, '\n'); for (int j = 0; j < new_indent * indent; j++) ser_putc(s, ' '); }
                ser_put_escaped(s, n->c.keys[i]);
                ser_puts(s, indent > 0 ? ": " : ":");
                ser_node(s, n->c.items[i], depth + 1, indent);
            }
            if (indent > 0 && n->c.count > 0) { ser_putc(s, '\n'); for (int j = 0; j < depth * indent; j++) ser_putc(s, ' '); }
            ser_putc(s, '}');
            break;
        }
    }
}

char *json_serialize(const json_t *node) {
    ser_ctx s = {NULL, 0, 0, false};
    ser_node(&s, node, 0, 0);
    ser_putc(&s, '\0');
    if (s.oom) { free(s.buf); set_oom(); return xstrdup("null"); }
    return s.buf ? s.buf : xstrdup("null");
}

char *json_serialize_pretty(const json_t *node, int indent) {
    ser_ctx s = {NULL, 0, 0, false};
    ser_node(&s, node, 0, indent > 0 ? indent : 2);
    ser_putc(&s, '\0');
    if (s.oom) { free(s.buf); set_oom(); return xstrdup("null"); }
    return s.buf ? s.buf : xstrdup("null");
}

/* ================================================================
 *  Copy / Free
 * ================================================================ */

json_t *json_copy(const json_t *node) {
    if (!node) return NULL;
    switch (node->type) {
        case JSON_NULL:  return json_null();
        case JSON_BOOL:  return json_bool(node->bool_val);
        case JSON_NUMBER: return json_number(node->num_val);
        case JSON_STRING: return json_string(node->str_val);
        case JSON_ARRAY: {
            json_t *c = json_array();
            if (!c) return NULL;
            for (size_t i = 0; i < node->c.count; i++)
                json_append(c, json_copy(node->c.items[i]));
            return c;
        }
        case JSON_OBJECT: {
            json_t *c = json_object();
            if (!c) return NULL;
            for (size_t i = 0; i < node->c.count; i++)
                json_set(c, node->c.keys[i], json_copy(node->c.items[i]));
            return c;
        }
        default: return json_null();
    }
}

void json_free(json_t *node) {
    if (!node) return;
    switch (node->type) {
        case JSON_STRING: free(node->str_val); break;
        case JSON_ARRAY:
            for (size_t i = 0; i < node->c.count; i++) json_free(node->c.items[i]);
            free(node->c.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < node->c.count; i++) {
                free(node->c.keys[i]);
                json_free(node->c.items[i]);
            }
            free(node->c.keys);
            free(node->c.items);
            break;
        default: break;
    }
    free(node);
}

/* ================================================================
 *  JSON Schema Validation
 * ================================================================ */

/* Set error message, returning false */
static bool schema_fail(char **error_out, const char *fmt, const char *arg) {
    if (error_out) {
        size_t sz = strlen(fmt) + (arg ? strlen(arg) : 0) + 1;
        *error_out = (char *)malloc(sz);
        if (*error_out) snprintf(*error_out, sz, fmt, arg ? arg : "");
    }
    return false;
}

/* Recursive schema validation */
static bool json_validate_schema_inner(const json_t *schema, const json_t *value,
                                        const char *path, char **error_out) {
    if (!schema || schema->type != JSON_OBJECT) return true; /* no schema = valid */
    if (!value) return true; /* null value = valid (optional field) */

    /* Get expected type */
    json_t *type_node = json_obj_get(schema, "type");
    const char *type_str = (type_node && type_node->type == JSON_STRING) ? type_node->str_val : NULL;

    /* Type validation */
    if (type_str) {
        bool type_ok = false;
        if (strcmp(type_str, "string") == 0)  type_ok = (value->type == JSON_STRING);
        else if (strcmp(type_str, "number") == 0) type_ok = (value->type == JSON_NUMBER);
        else if (strcmp(type_str, "integer") == 0) {
            type_ok = (value->type == JSON_NUMBER && value->num_val == (double)(int)value->num_val);
        }
        else if (strcmp(type_str, "boolean") == 0) type_ok = (value->type == JSON_BOOL);
        else if (strcmp(type_str, "array") == 0)   type_ok = (value->type == JSON_ARRAY);
        else if (strcmp(type_str, "object") == 0)  type_ok = (value->type == JSON_OBJECT);
        else if (strcmp(type_str, "null") == 0)    type_ok = (value->type == JSON_NULL);
        if (!type_ok) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: expected type '%s'", path ? path : "root", type_str);
            return schema_fail(error_out, "%s", buf);
        }
    }

    /* Enum validation */
    json_t *enum_node = json_obj_get(schema, "enum");
    if (enum_node && enum_node->type == JSON_ARRAY && enum_node->c.count > 0) {
        bool found = false;
        for (size_t i = 0; i < enum_node->c.count; i++) {
            /* Compare serialized forms */
            char *v_str = json_serialize(value);
            char *e_str = json_serialize(enum_node->c.items[i]);
            if (v_str && e_str && strcmp(v_str, e_str) == 0) found = true;
            free(v_str);
            free(e_str);
            if (found) break;
        }
        if (!found) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: value not in enum", path ? path : "root");
            return schema_fail(error_out, "%s", buf);
        }
    }

    /* String constraints */
    if (value->type == JSON_STRING) {
        size_t len = strlen(value->str_val);
        json_t *min_v = json_obj_get(schema, "minLength");
        if (min_v && min_v->type == JSON_NUMBER && len < (size_t)min_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: string too short (min %d)",
                     path ? path : "root", (int)min_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }
        json_t *max_v = json_obj_get(schema, "maxLength");
        if (max_v && max_v->type == JSON_NUMBER && len > (size_t)max_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: string too long (max %d)",
                     path ? path : "root", (int)max_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }
    }

    /* Number constraints */
    if (value->type == JSON_NUMBER) {
        json_t *min_v = json_obj_get(schema, "minimum");
        if (min_v && min_v->type == JSON_NUMBER && value->num_val < min_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: below minimum (%.0f)",
                     path ? path : "root", min_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }
        json_t *max_v = json_obj_get(schema, "maximum");
        if (max_v && max_v->type == JSON_NUMBER && value->num_val > max_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: above maximum (%.0f)",
                     path ? path : "root", max_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }
    }

    /* Array constraints */
    if (value->type == JSON_ARRAY) {
        json_t *min_v = json_obj_get(schema, "minItems");
        if (min_v && min_v->type == JSON_NUMBER && value->c.count < (size_t)min_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: too few items (min %d)",
                     path ? path : "root", (int)min_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }
        json_t *max_v = json_obj_get(schema, "maxItems");
        if (max_v && max_v->type == JSON_NUMBER && value->c.count > (size_t)max_v->num_val) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[schema] at %s: too many items (max %d)",
                     path ? path : "root", (int)max_v->num_val);
            return schema_fail(error_out, "%s", buf);
        }

        /* Items schema for each element */
        json_t *items_schema = json_obj_get(schema, "items");
        if (items_schema) {
            char child_path[256];
            for (size_t i = 0; i < value->c.count; i++) {
                snprintf(child_path, sizeof(child_path), "%s/%zu", path ? path : "root", i);
                if (!json_validate_schema_inner(items_schema, value->c.items[i], child_path, error_out))
                    return false;
            }
        }
    }

    /* Object properties */
    if (value->type == JSON_OBJECT) {
        /* Required properties */
        json_t *required = json_obj_get(schema, "required");
        if (required && required->type == JSON_ARRAY) {
            for (size_t i = 0; i < required->c.count; i++) {
                if (required->c.items[i]->type != JSON_STRING) continue;
                if (!json_has(value, required->c.items[i]->str_val)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "[schema] at %s: missing required property '%s'",
                             path ? path : "root", required->c.items[i]->str_val);
                    return schema_fail(error_out, "%s", buf);
                }
            }
        }

        /* Property schemas */
        json_t *properties = json_obj_get(schema, "properties");
        if (properties && properties->type == JSON_OBJECT) {
            char child_path[256];
            for (size_t i = 0; i < properties->c.count; i++) {
                const char *key = properties->c.keys[i];
                json_t *prop_schema = properties->c.items[i];
                json_t *prop_val = json_obj_get(value, key);
                if (prop_val) {
                    snprintf(child_path, sizeof(child_path), "%s/%s", path ? path : "root", key);
                    if (!json_validate_schema_inner(prop_schema, prop_val, child_path, error_out))
                        return false;
                }
            }
        }
    }

    return true;
}

bool json_validate_schema(const json_t *schema, const json_t *value, char **error_out) {
    if (error_out) *error_out = NULL;
    return json_validate_schema_inner(schema, value, NULL, error_out);
}
