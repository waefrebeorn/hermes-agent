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
                    if (cp < 128) buf[len++] = (char)cp;
                    else if (cp < 0x800) {
                        buf[len++] = (char)(0xC0 | (cp >> 6));
                        buf[len++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        buf[len++] = (char)(0xE0 | (cp >> 12));
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
