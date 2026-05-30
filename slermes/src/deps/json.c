/*
 * json.c — Minimal JSON parser/serializer for Hermes C.
 *
 * Recursive-descent parser, zero external deps.
 * MIT-style license (Hermes project internal).
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "json: OOM\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t sz) {
    p = realloc(p, sz ? sz : 1);
    if (!p) { fprintf(stderr, "json: OOM\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)xmalloc(n + 1);
    memcpy(d, s, n + 1);
    return d;
}

/* Parser state */
typedef struct {
    const char *pos;
    const char *end;
    const char *start;  /* for error reporting */
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
    while (ctx->pos < ctx->end && (unsigned char)*ctx->pos <= 0x20)
        ctx->pos++;
}

static int peek(parse_ctx *ctx) {
    return ctx->pos < ctx->end ? (unsigned char)*ctx->pos : EOF;
}

static int next(parse_ctx *ctx) {
    return ctx->pos < ctx->end ? (unsigned char)*ctx->pos++ : EOF;
}

static bool expect(parse_ctx *ctx, int c) {
    skip_ws(ctx);
    if (peek(ctx) != c) return false;
    ctx->pos++;
    return true;
}

/* ================================================================
 *  String parsing (JSON escape sequences)
 * ================================================================ */

static char *parse_string_raw(parse_ctx *ctx) {
    if (next(ctx) != '"') {
        set_error(ctx, "expected '\"' at offset %zu", (size_t)(ctx->pos - ctx->start - 1));
        return NULL;
    }

    /* First pass: measure */
    const char *p = ctx->pos;
    size_t len = 0;
    bool esc = false;
    while (p < ctx->end) {
        if (esc) { esc = false; len++; p++; continue; }
        if (*p == '\\') { esc = true; p++; continue; }
        if (*p == '"') { break; }
        len++;
        p++;
    }
    if (p >= ctx->end) {
        set_error(ctx, "unterminated string");
        return NULL;
    }

    /* Second pass: decode */
    char *out = (char *)xmalloc(len + 1);
    size_t i = 0;
    while (ctx->pos < ctx->end) {
        int c = next(ctx);
        if (c == '"') break;
        if (c == '\\') {
            int c2 = next(ctx);
            switch (c2) {
                case '"':  out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                case '/':  out[i++] = '/';  break;
                case 'b':  out[i++] = '\b'; break;
                case 'f':  out[i++] = '\f'; break;
                case 'n':  out[i++] = '\n'; break;
                case 'r':  out[i++] = '\r'; break;
                case 't':  out[i++] = '\t'; break;
                case 'u': {
                    /* Basic \uXXXX — only handles BMP, no surrogate pairs */
                    unsigned u = 0;
                    for (int k = 0; k < 4; k++) {
                        int hex = next(ctx);
                        u <<= 4;
                        if (hex >= '0' && hex <= '9')      u |= (hex - '0');
                        else if (hex >= 'a' && hex <= 'f') u |= (hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F') u |= (hex - 'A' + 10);
                    }
                    if (u < 0x80)       out[i++] = (char)u;
                    else if (u < 0x800) { out[i++] = 0xC0 | (u >> 6);  out[i++] = 0x80 | (u & 0x3F); }
                    else                { out[i++] = 0xE0 | (u >> 12); out[i++] = 0x80 | ((u>>6)&0x3F); out[i++] = 0x80 | (u & 0x3F); }
                    break;
                }
                default: out[i++] = (char)c2; break;
            }
        } else {
            out[i++] = (char)c;
        }
    }
    out[i] = '\0';
    return out;
}

/* ================================================================
 *  Value parsing (forward declaration for recursive descent)
 * ================================================================ */

static json_node_t *parse_value(parse_ctx *ctx);

static json_node_t *parse_object(parse_ctx *ctx) {
    json_node_t *obj = json_new_object();
    next(ctx); /* skip '{' */

    skip_ws(ctx);
    if (peek(ctx) == '}') { next(ctx); return obj; }

    while (true) {
        skip_ws(ctx);
        char *key = parse_string_raw(ctx);
        if (!key) { json_free(obj); return NULL; }
        if (!expect(ctx, ':')) {
            set_error(ctx, "expected ':' after key");
            free(key); json_free(obj); return NULL;
        }
        json_node_t *val = parse_value(ctx);
        if (!val) { free(key); json_free(obj); return NULL; }
        json_object_set(obj, key, val);
        free(key);

        skip_ws(ctx);
        int c = next(ctx);
        if (c == '}') break;
        if (c != ',') {
            set_error(ctx, "expected ',' or '}' in object");
            json_free(obj); return NULL;
        }
    }
    return obj;
}

static json_node_t *parse_array(parse_ctx *ctx) {
    json_node_t *arr = json_new_array();
    next(ctx); /* skip '[' */

    skip_ws(ctx);
    if (peek(ctx) == ']') { next(ctx); return arr; }

    while (true) {
        json_node_t *val = parse_value(ctx);
        if (!val) { json_free(arr); return NULL; }
        json_array_append(arr, val);
        skip_ws(ctx);
        int c = next(ctx);
        if (c == ']') break;
        if (c != ',') {
            set_error(ctx, "expected ',' or ']' in array");
            json_free(arr); return NULL;
        }
    }
    return arr;
}

static json_node_t *parse_number(parse_ctx *ctx) {
    const char *start = ctx->pos;
    if (peek(ctx) == '-') ctx->pos++;
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
    char buf[64];
    if (len >= sizeof(buf)) { set_error(ctx, "number too long"); return NULL; }
    memcpy(buf, start, len);
    buf[len] = '\0';
    return json_new_number(strtod(buf, NULL));
}

static json_node_t *parse_value(parse_ctx *ctx) {
    skip_ws(ctx);
    int c = peek(ctx);
    switch (c) {
        case '{': return parse_object(ctx);
        case '[': return parse_array(ctx);
        case '"': return json_new_string(parse_string_raw(ctx));
        case 't':
            if (ctx->end - ctx->pos >= 4 && memcmp(ctx->pos, "true", 4) == 0) {
                ctx->pos += 4; return json_new_bool(true);
            }
            set_error(ctx, "unexpected token at offset %zu", (size_t)(ctx->pos - ctx->start));
            return NULL;
        case 'f':
            if (ctx->end - ctx->pos >= 5 && memcmp(ctx->pos, "false", 5) == 0) {
                ctx->pos += 5; return json_new_bool(false);
            }
            set_error(ctx, "unexpected token at offset %zu", (size_t)(ctx->pos - ctx->start));
            return NULL;
        case 'n':
            if (ctx->end - ctx->pos >= 4 && memcmp(ctx->pos, "null", 4) == 0) {
                ctx->pos += 4; return json_new_null();
            }
            set_error(ctx, "unexpected token at offset %zu", (size_t)(ctx->pos - ctx->start));
            return NULL;
        case '-': case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number(ctx);
        default:
            set_error(ctx, "unexpected character '%c' at offset %zu",
                      c, (size_t)(ctx->pos - ctx->start));
            return NULL;
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */

json_node_t *json_parse(const char *input, char **error_msg) {
    if (!input) {
        if (error_msg) *error_msg = xstrdup("NULL input");
        return NULL;
    }
    parse_ctx ctx;
    ctx.pos = input;
    ctx.end = input + strlen(input);
    ctx.start = input;
    ctx.err = NULL;

    json_node_t *result = parse_value(&ctx);
    if (!result && ctx.err && error_msg)
        *error_msg = ctx.err;
    else if (ctx.err)
        free(ctx.err);
    else if (error_msg)
        *error_msg = NULL;

    /* Check trailing garbage */
    if (result) {
        skip_ws(&ctx);
        if (ctx.pos < ctx.end) {
            json_free(result);
            if (error_msg) *error_msg = xstrdup("trailing garbage after JSON value");
            return NULL;
        }
    }
    return result;
}

json_node_t *json_parse_file(const char *path, char **error_msg) {
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
    json_node_t *result = json_parse(buf, error_msg);
    free(buf);
    return result;
}

/* ================================================================
 *  Construction
 * ================================================================ */

json_node_t *json_new_null(void) {
    json_node_t *n = (json_node_t *)xmalloc(sizeof(json_node_t));
    memset(n, 0, sizeof(*n));
    n->type = JSON_NULL;
    return n;
}

json_node_t *json_new_bool(bool val) {
    json_node_t *n = json_new_null();
    n->type = JSON_BOOL;
    n->bool_val = val;
    return n;
}

json_node_t *json_new_number(double val) {
    json_node_t *n = json_new_null();
    n->type = JSON_NUMBER;
    n->num_val = val;
    return n;
}

json_node_t *json_new_string(const char *val) {
    json_node_t *n = json_new_null();
    n->type = JSON_STRING;
    n->str_val = val ? xstrdup(val) : xstrdup("");
    return n;
}

json_node_t *json_new_array(void) {
    json_node_t *n = json_new_null();
    n->type = JSON_ARRAY;
    n->collection.items = NULL;
    n->collection.keys = NULL;
    n->collection.count = 0;
    return n;
}

json_node_t *json_new_object(void) {
    json_node_t *n = json_new_null();
    n->type = JSON_OBJECT;
    n->collection.items = NULL;
    n->collection.keys = NULL;
    n->collection.count = 0;
    return n;
}

void json_array_append(json_node_t *arr, json_node_t *item) {
    if (!arr || arr->type != JSON_ARRAY || !item) return;
    arr->collection.count++;
    arr->collection.items = (json_node_t **)xrealloc(arr->collection.items,
        arr->collection.count * sizeof(json_node_t *));
    arr->collection.items[arr->collection.count - 1] = item;
}

json_node_t *json_array_get(const json_node_t *arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY || index >= arr->collection.count) return NULL;
    return arr->collection.items[index];
}

size_t json_array_count(const json_node_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->collection.count;
}

void json_object_set(json_node_t *obj, const char *key, json_node_t *val) {
    if (!obj || obj->type != JSON_OBJECT || !key || !val) return;

    /* Check for existing key */
    for (size_t i = 0; i < obj->collection.count; i++) {
        if (strcmp(obj->collection.keys[i], key) == 0) {
            json_free(obj->collection.items[i]);
            obj->collection.items[i] = val;
            return;
        }
    }

    /* New key */
    obj->collection.count++;
    size_t n = obj->collection.count;
    obj->collection.keys = (char **)xrealloc(obj->collection.keys, n * sizeof(char *));
    obj->collection.items = (json_node_t **)xrealloc(obj->collection.items, n * sizeof(json_node_t *));
    obj->collection.keys[n - 1] = xstrdup(key);
    obj->collection.items[n - 1] = val;
}

json_node_t *json_object_get(const json_node_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->collection.count; i++) {
        if (strcmp(obj->collection.keys[i], key) == 0)
            return obj->collection.items[i];
    }
    return NULL;
}

const char *json_object_get_string(const json_node_t *obj, const char *key, const char *def) {
    json_node_t *v = json_object_get(obj, key);
    if (!v || v->type != JSON_STRING) return def;
    return v->str_val;
}

double json_object_get_number(const json_node_t *obj, const char *key, double def) {
    json_node_t *v = json_object_get(obj, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return v->num_val;
}

bool json_object_get_bool(const json_node_t *obj, const char *key, bool def) {
    json_node_t *v = json_object_get(obj, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val;
}

/* ================================================================
 *  Deep copy
 * ================================================================ */

json_node_t *json_copy(const json_node_t *node) {
    if (!node) return NULL;
    switch (node->type) {
        case JSON_NULL:   return json_new_null();
        case JSON_BOOL:   return json_new_bool(node->bool_val);
        case JSON_NUMBER: return json_new_number(node->num_val);
        case JSON_STRING: return json_new_string(node->str_val);
        case JSON_ARRAY: {
            json_node_t *a = json_new_array();
            for (size_t i = 0; i < node->collection.count; i++)
                json_array_append(a, json_copy(node->collection.items[i]));
            return a;
        }
        case JSON_OBJECT: {
            json_node_t *o = json_new_object();
            for (size_t i = 0; i < node->collection.count; i++)
                json_object_set(o, node->collection.keys[i], json_copy(node->collection.items[i]));
            return o;
        }
    }
    return NULL;
}

/* ================================================================
 *  Serialization
 * ================================================================ */

/* Dynamic string builder */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sb_t;

static void sb_init(sb_t *sb) {
    sb->buf = (char *)xmalloc(256);
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = 256;
}

static void sb_append(sb_t *sb, const char *s) {
    size_t slen = strlen(s);
    if (sb->len + slen + 1 > sb->cap) {
        sb->cap = sb->cap * 2 + slen + 1;
        sb->buf = (char *)xrealloc(sb->buf, sb->cap);
    }
    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
}

static void sb_append_char(sb_t *sb, char c) {
    char s[2] = {c, '\0'};
    sb_append(sb, s);
}

static void sb_append_indent(sb_t *sb, int depth) {
    for (int i = 0; i < depth; i++) sb_append(sb, "  ");
}

/* Forward */
static void serialize_node(const json_node_t *node, sb_t *sb, int depth, bool pretty);

static void serialize_string(sb_t *sb, const char *s) {
    sb_append_char(sb, '"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\b': sb_append(sb, "\\b");  break;
            case '\f': sb_append(sb, "\\f");  break;
            case '\n': sb_append(sb, "\\n");  break;
            case '\r': sb_append(sb, "\\r");  break;
            case '\t': sb_append(sb, "\\t");  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    sb_append(sb, buf);
                } else {
                    sb_append_char(sb, (char)c);
                }
                break;
        }
    }
    sb_append_char(sb, '"');
}

static void serialize_node(const json_node_t *node, sb_t *sb, int depth, bool pretty) {
    if (!node) { sb_append(sb, "null"); return; }

    switch (node->type) {
        case JSON_NULL:
            sb_append(sb, "null");
            break;
        case JSON_BOOL:
            sb_append(sb, node->bool_val ? "true" : "false");
            break;
        case JSON_NUMBER: {
            char buf[64];
            double v = node->num_val;
            if (v == (double)(long long)v)
                snprintf(buf, sizeof(buf), "%lld", (long long)v);
            else
                snprintf(buf, sizeof(buf), "%g", v);
            sb_append(sb, buf);
            break;
        }
        case JSON_STRING:
            serialize_string(sb, node->str_val ? node->str_val : "");
            break;
        case JSON_ARRAY: {
            sb_append_char(sb, '[');
            if (node->collection.count == 0) {
                sb_append_char(sb, ']');
                break;
            }
            if (pretty) sb_append_char(sb, '\n');
            for (size_t i = 0; i < node->collection.count; i++) {
                if (pretty) sb_append_indent(sb, depth + 1);
                serialize_node(node->collection.items[i], sb, depth + 1, pretty);
                if (i < node->collection.count - 1) sb_append_char(sb, ',');
                if (pretty) sb_append_char(sb, '\n');
            }
            if (pretty) sb_append_indent(sb, depth);
            sb_append_char(sb, ']');
            break;
        }
        case JSON_OBJECT: {
            sb_append_char(sb, '{');
            if (node->collection.count == 0) {
                sb_append_char(sb, '}');
                break;
            }
            if (pretty) sb_append_char(sb, '\n');
            for (size_t i = 0; i < node->collection.count; i++) {
                if (pretty) sb_append_indent(sb, depth + 1);
                serialize_string(sb, node->collection.keys[i]);
                sb_append(sb, pretty ? ": " : ":");
                serialize_node(node->collection.items[i], sb, depth + 1, pretty);
                if (i < node->collection.count - 1) sb_append_char(sb, ',');
                if (pretty) sb_append_char(sb, '\n');
            }
            if (pretty) sb_append_indent(sb, depth);
            sb_append_char(sb, '}');
            break;
        }
    }
}

char *json_serialize(const json_node_t *node) {
    sb_t sb;
    sb_init(&sb);
    serialize_node(node, &sb, 0, false);
    return sb.buf;
}

char *json_serialize_pretty(const json_node_t *node, int indent) {
    (void)indent;
    sb_t sb;
    sb_init(&sb);
    serialize_node(node, &sb, 0, true);
    return sb.buf;
}

/* ================================================================
 *  Free
 * ================================================================ */

void json_free(json_node_t *node) {
    if (!node) return;
    switch (node->type) {
        case JSON_STRING:
            free(node->str_val);
            break;
        case JSON_ARRAY:
        case JSON_OBJECT:
            for (size_t i = 0; i < node->collection.count; i++) {
                if (node->collection.keys) free(node->collection.keys[i]);
                json_free(node->collection.items[i]);
            }
            free(node->collection.keys);
            free(node->collection.items);
            break;
        default:
            break;
    }
    free(node);
}
