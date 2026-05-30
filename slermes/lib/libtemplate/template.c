/*
 * template.c — Standalone template engine for C.
 * Replaces Python jinja2. Compiles to opcodes, then renders.
 * MIT License — WuBu Hermes Project
 */

#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "json.h"

/* ================================================================
 *  Opcodes
 * ================================================================ */

typedef enum {
    OP_TEXT,          /* literal text */
    OP_VAR,           /* {{ variable }} */
    OP_VAR_DEFAULT,   /* {{ variable|default }} */
    OP_FOR_OPEN,      /* {% for item in list %} */
    OP_FOR_CLOSE,     /* {% endfor %} */
    OP_IF_OPEN,       /* {% if condition %} */
    OP_ELSE,          /* {% else %} */
    OP_IF_CLOSE,      /* {% endif %} */
} op_type_t;

typedef struct op_s op_t;
struct op_s {
    op_type_t type;
    char *arg;       /* variable name, loop var, etc. */
    char *arg2;      /* second arg: default value, list name */
    op_t *next;
};

struct template_t {
    op_t *head;
    op_t *tail;
};

/* ================================================================
 *  Compilation helpers
 * ================================================================ */

static op_t *op_alloc(op_type_t type, const char *arg, const char *arg2) {
    op_t *op = (op_t *)calloc(1, sizeof(op_t));
    if (!op) return NULL;
    op->type = type;
    if (arg) { op->arg = strdup(arg); if (!op->arg) { free(op); return NULL; } }
    if (arg2) { op->arg2 = strdup(arg2); if (!op->arg2) { free(op->arg); free(op); return NULL; } }
    return op;
}

static void op_append(template_t *tpl, op_t *op) {
    if (!tpl->head) { tpl->head = tpl->tail = op; }
    else { tpl->tail->next = op; tpl->tail = op; }
}

/* Extract variable name and optional default from {{ expr }} */
static int parse_var_expr(const char *start, const char *end,
                          char **var, char **default_val) {
    const char *p = start;
    while (p < end && isspace((unsigned char)*p)) p++;
    const char *var_start = p;
    while (p < end && *p != '|' && !isspace((unsigned char)*p)) p++;
    const char *var_end = p;

    size_t vlen = (size_t)(var_end - var_start);
    *var = (char *)malloc(vlen + 1);
    if (!*var) return -1;
    memcpy(*var, var_start, vlen);
    (*var)[vlen] = '\0';

    if (p < end && *p == '|') {
        p++;
        while (p < end && isspace((unsigned char)*p)) p++;
        const char *def_start = p;
        while (p < end && !isspace((unsigned char)*p)) p++;
        size_t dlen = (size_t)(p - def_start);
        *default_val = (char *)malloc(dlen + 1);
        if (!*default_val) { free(*var); return -1; }
        memcpy(*default_val, def_start, dlen);
        (*default_val)[dlen] = '\0';
    }
    return 0;
}

/* Parse {% for item in list %} — extract item and list */
static int parse_for_expr(const char *start, const char *end,
                          char **item, char **list) {
    const char *p = start;
    while (p < end && isspace((unsigned char)*p)) p++;
    /* skip 'for' */
    if (strncmp(p, "for", 3) != 0) return -1;
    p += 3;
    while (p < end && isspace((unsigned char)*p)) p++;
    const char *item_start = p;
    while (p < end && !isspace((unsigned char)*p)) p++;
    size_t ilen = (size_t)(p - item_start);
    *item = (char *)malloc(ilen + 1);
    if (!*item) return -1;
    memcpy(*item, item_start, ilen);
    (*item)[ilen] = '\0';

    while (p < end && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "in", 2) != 0) { free(*item); return -1; }
    p += 2;
    while (p < end && isspace((unsigned char)*p)) p++;
    const char *list_start = p;
    while (p < end && !isspace((unsigned char)*p)) p++;
    size_t llen = (size_t)(p - list_start);
    *list = (char *)malloc(llen + 1);
    if (!*list) { free(*item); return -1; }
    memcpy(*list, list_start, llen);
    (*list)[llen] = '\0';
    return 0;
}

/* ================================================================
 *  Compilation
 * ================================================================ */

template_t *template_compile(const char *input, char **error_msg) {
    if (!input) {
        if (error_msg) *error_msg = strdup("NULL input");
        return NULL;
    }

    template_t *tpl = (template_t *)calloc(1, sizeof(template_t));
    if (!tpl) return NULL;

    const char *p = input;
    const char *text_start = p;

    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            /* Emit preceding text */
            if (p > text_start) {
                size_t len = (size_t)(p - text_start);
                op_t *op = op_alloc(OP_TEXT, NULL, NULL);
                if (!op) goto oom;
                op->arg = (char *)malloc(len + 1);
                if (!op->arg) { free(op); goto oom; }
                memcpy(op->arg, text_start, len);
                op->arg[len] = '\0';
                op_append(tpl, op);
            }
            /* Find }} */
            const char *end = strstr(p + 2, "}}");
            if (!end) {
                if (error_msg) *error_msg = strdup("Unclosed '{{'");
                template_free(tpl); return NULL;
            }
            char *var = NULL, *def = NULL;
            if (parse_var_expr(p + 2, end, &var, &def) != 0) {
                if (error_msg) *error_msg = strdup("Bad variable expression");
                template_free(tpl); return NULL;
            }
            op_type_t t = def ? OP_VAR_DEFAULT : OP_VAR;
            op_t *op = op_alloc(t, var, def);
            free(var); free(def);
            if (!op) goto oom;
            op_append(tpl, op);
            p = end + 2;
            text_start = p;
        }
        else if (p[0] == '{' && p[1] == '%') {
            /* Emit preceding text */
            if (p > text_start) {
                size_t len = (size_t)(p - text_start);
                op_t *op = op_alloc(OP_TEXT, NULL, NULL);
                if (!op) goto oom;
                op->arg = (char *)malloc(len + 1);
                if (!op->arg) { free(op); goto oom; }
                memcpy(op->arg, text_start, len);
                op->arg[len] = '\0';
                op_append(tpl, op);
            }
            /* Find %} */
            const char *end = strstr(p + 2, "%}");
            if (!end) {
                if (error_msg) *error_msg = strdup("Unclosed '{%'");
                template_free(tpl); return NULL;
            }
            const char *expr_start = p + 2;
            const char *expr_end = end;
            /* Skip whitespace around expression */
            while (expr_start < expr_end && isspace((unsigned char)*expr_start)) expr_start++;
            while (expr_end > expr_start && isspace((unsigned char)*(expr_end - 1))) expr_end--;

            size_t expr_len = (size_t)(expr_end - expr_start);
            op_t *op = NULL;

            if (expr_len >= 3 && strncmp(expr_start, "for", 3) == 0) {
                char *item = NULL, *list = NULL;
                if (parse_for_expr(expr_start, expr_end, &item, &list) != 0) {
                    if (error_msg) *error_msg = strdup("Bad 'for' expression");
                    template_free(tpl); return NULL;
                }
                op = op_alloc(OP_FOR_OPEN, item, list);
                free(item); free(list);
            }
            else if (expr_len >= 2 && strncmp(expr_start, "if", 2) == 0) {
                const char *cond_start = expr_start + 2;
                while (cond_start < expr_end && isspace((unsigned char)*cond_start)) cond_start++;
                size_t cond_len = (size_t)(expr_end - cond_start);
                char *cond = (char *)malloc(cond_len + 1);
                if (!cond) goto oom;
                memcpy(cond, cond_start, cond_len);
                cond[cond_len] = '\0';
                op = op_alloc(OP_IF_OPEN, cond, NULL);
                free(cond);
            }
            else if (expr_len >= 4 && strncmp(expr_start, "else", 4) == 0) {
                op = op_alloc(OP_ELSE, NULL, NULL);
            }
            else if (expr_len >= 5 && strncmp(expr_start, "endif", 5) == 0) {
                op = op_alloc(OP_IF_CLOSE, NULL, NULL);
            }
            else if (expr_len >= 5 && strncmp(expr_start, "endfor", 6) == 0) {
                op = op_alloc(OP_FOR_CLOSE, NULL, NULL);
            }
            else {
                if (error_msg) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Unknown tag: '%.*s'", (int)expr_len, expr_start);
                    *error_msg = strdup(buf);
                }
                template_free(tpl); return NULL;
            }

            if (!op) goto oom;
            op_append(tpl, op);
            p = end + 2;
            text_start = p;
        }
        else {
            p++;
        }
    }

    /* Emit trailing text */
    if (p > text_start) {
        size_t len = (size_t)(p - text_start);
        op_t *op = op_alloc(OP_TEXT, NULL, NULL);
        if (!op) goto oom;
        op->arg = (char *)malloc(len + 1);
        if (!op->arg) { free(op); goto oom; }
        memcpy(op->arg, text_start, len);
        op->arg[len] = '\0';
        op_append(tpl, op);
    }

    return tpl;

oom:
    if (error_msg) *error_msg = strdup("Out of memory");
    template_free(tpl);
    return NULL;
}
/* ================================================================
 *  Rendering
 * ================================================================ */

/* Forward declare the json accessor functions we need */
/* (Included via json.h above) */

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} render_ctx;

static void r_puts(render_ctx *r, const char *s) {
    if (!s) return;
    while (*s) {
        if (r->len + 1 >= r->cap) {
            r->cap = r->cap ? r->cap * 2 : 256;
            char *nb = (char *)realloc(r->buf, r->cap);
            if (!nb) return;
            r->buf = nb;
        }
        r->buf[r->len++] = *s++;
    }
    /* Always null-terminate */
    if (r->len >= r->cap) {
        r->cap = r->cap ? r->cap * 2 : 256;
        char *nb = (char *)realloc(r->buf, r->cap);
        if (!nb) return;
        r->buf = nb;
    }
    r->buf[r->len] = '\0';
}

/* Resolve variable — render as string from any JSON type */
static void render_var_value(render_ctx *r, const void *ctx, const char *var, const char *def) {
    const json_t *node = ctx ? json_obj_get(ctx, var) : NULL;
    if (!node) { r_puts(r, def ? def : ""); return; }
    switch (node->type) {
        case JSON_STRING: r_puts(r, node->str_val); break;
        case JSON_NUMBER: {
            char buf[64];
            if (node->num_val == (double)(long long)node->num_val)
                snprintf(buf, sizeof(buf), "%.0f", node->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", node->num_val);
            r_puts(r, buf);
            break;
        }
        case JSON_BOOL: r_puts(r, node->bool_val ? "true" : "false"); break;
        case JSON_NULL: r_puts(r, "null"); break;
        default:
            /* Array/object — serialize inline */
            {
                char *s = json_serialize(node);
                r_puts(r, s ? s : "");
                free(s);
            }
            break;
    }
}

/* Get nested value for dotted path — returns the node at the end */
static const json_t *resolve_path(const void *ctx, const char *path) {
    if (!ctx || !path) return NULL;
    const json_t *node = (const json_t *)ctx;
    const char *p = path;
    const char *dot;
    char key[256];

    while ((dot = strchr(p, '.')) != NULL) {
        size_t klen = (size_t)(dot - p);
        if (klen >= sizeof(key)) return NULL;
        memcpy(key, p, klen);
        key[klen] = '\0';
        node = json_obj_get(node, key);
        if (!node) return NULL;
        p = dot + 1;
    }
    return json_obj_get(node, p);
}

/* Check if a condition is truthy by examining the JSON node directly */
static bool eval_condition(const void *ctx, const char *cond) {
    if (!cond) return true;
    const json_t *node = ctx ? json_obj_get(ctx, cond) : NULL;
    if (!node) return false;
    switch (node->type) {
        case JSON_BOOL:   return node->bool_val;
        case JSON_STRING: return strlen(node->str_val) > 0;
        case JSON_NUMBER: return node->num_val != 0;
        case JSON_NULL:   return false;
        default:          return true; /* array/object = truthy */
    }
}

/* Build a merged context: copy parent + set loop_variable = loop_value.
 * Caller must json_free() result. */
static json_t *build_loop_context(const void *parent, const char *loop_var, const json_t *loop_val) {
    json_t *merged = json_object();
    if (!merged) return NULL;
    /* Copy all keys from parent */
    if (parent) {
        const json_t *p = (const json_t *)parent;
        if (p->type == JSON_OBJECT) {
            for (size_t i = 0; i < p->c.count; i++) {
                json_set(merged, p->c.keys[i], json_copy(p->c.items[i]));
            }
        }
    }
    /* Set loop variable (overrides parent if same key) */
    json_set(merged, loop_var, json_copy(loop_val));
    return merged;
}

/* Render a single op to output buffer */
static void render_op(render_ctx *r, const op_t *op, const void *ctx) {
    switch (op->type) {
        case OP_TEXT:
            r_puts(r, op->arg);
            break;
        case OP_VAR: {
            render_var_value(r, ctx, op->arg, "");
            break;
        }
        case OP_VAR_DEFAULT: {
            /* Check if var exists */
            const json_t *node = ctx ? json_obj_get(ctx, op->arg) : NULL;
            if (node)
                render_var_value(r, ctx, op->arg, NULL);
            else
                r_puts(r, op->arg2);
            break;
        }
        default:
            break;
    }
}

/* Recursive render: ops starting at 'start', stop at for/if close */
static const op_t *render_ops(render_ctx *r, const op_t *op, const void *ctx) {
    while (op) {
        switch (op->type) {
            case OP_TEXT:
            case OP_VAR:
            case OP_VAR_DEFAULT:
                render_op(r, op, ctx);
                op = op->next;
                break;

            case OP_FOR_OPEN: {
                /* Get the list from context */
                const json_t *list = resolve_path(ctx, op->arg2);
                size_t count = list ? json_len(list) : 0;

                /* Find matching endfor */
                const op_t *body_start = op->next;
                const op_t *for_close = NULL;
                int depth = 1;
                const op_t *scan = body_start;
                while (scan && depth > 0) {
                    if (scan->type == OP_FOR_OPEN) depth++;
                    else if (scan->type == OP_FOR_CLOSE) depth--;
                    if (depth > 0) scan = scan->next;
                }
                for_close = scan;

                for (size_t i = 0; i < count; i++) {
                    const json_t *item = json_get(list, i);
                    /* Build merged context: parent ctx + loop variable */
                    json_t *local = build_loop_context(ctx, op->arg, item);
                    render_ops(r, body_start, local);
                    json_free(local);
                }

                op = for_close ? for_close->next : NULL;
                break;
            }

            case OP_IF_OPEN: {
                bool cond = eval_condition(ctx, op->arg);
                const op_t *branch = op->next;
                const op_t *else_op = NULL;
                const op_t *endif_op = NULL;

                /* Find matching else/endif */
                int depth = 1;
                const op_t *scan = branch;
                while (scan && depth > 0) {
                    if (scan->type == OP_IF_OPEN) depth++;
                    else if (scan->type == OP_IF_CLOSE) {
                        depth--;
                        if (depth == 0 && !endif_op) endif_op = scan;
                    }
                    else if (scan->type == OP_ELSE && depth == 1) else_op = scan;
                    if (depth > 0) scan = scan->next;
                }

                if (cond) {
                    render_ops(r, branch, ctx);
                } else if (else_op) {
                    render_ops(r, else_op->next, ctx);
                }

                op = endif_op ? endif_op->next : NULL;
                break;
            }

            case OP_ELSE:
            case OP_FOR_CLOSE:
            case OP_IF_CLOSE:
                /* These are handled by their openers — return to caller */
                return op;

            default:
                op = op->next;
                break;
        }
    }
    return NULL;
}

char *template_render(const template_t *tpl, const void *json_ctx) {
    if (!tpl) return strdup("");

    render_ctx r = {NULL, 0, 0};
    render_ops(&r, tpl->head, json_ctx);
    r_puts(&r, ""); /* null terminate */

    if (!r.buf) return strdup("");
    return r.buf;
}

/* ================================================================
 *  Introspection
 * ================================================================ */

char **template_variables(const template_t *tpl, size_t *count) {
    if (!tpl || !count) return NULL;
    *count = 0;

    /* First pass: count */
    size_t cap = 0;
    for (const op_t *op = tpl->head; op; op = op->next) {
        if (op->type == OP_VAR || op->type == OP_VAR_DEFAULT) cap++;
    }

    if (cap == 0) return NULL;

    char **vars = (char **)calloc(cap, sizeof(char *));
    if (!vars) return NULL;

    size_t idx = 0;
    for (const op_t *op = tpl->head; op && idx < cap; op = op->next) {
        if (op->type == OP_VAR || op->type == OP_VAR_DEFAULT) {
            vars[idx] = strdup(op->arg);
            if (vars[idx]) idx++;
        }
    }
    *count = idx;
    return vars;
}

/* ================================================================
 *  Cleanup
 * ================================================================ */

void template_free(template_t *tpl) {
    if (!tpl) return;
    op_t *op = tpl->head;
    while (op) {
        op_t *next = op->next;
        free(op->arg);
        free(op->arg2);
        free(op);
        op = next;
    }
    free(tpl);
}

/* ================================================================
 *  Quick one-shot
 * ================================================================ */

char *template_quick(const char *template_str, const void *json_ctx, char **error) {
    template_t *tpl = template_compile(template_str, error);
    if (!tpl) return NULL;
    char *result = template_render(tpl, json_ctx);
    template_free(tpl);
    return result;
}
