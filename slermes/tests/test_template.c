/*
 * test_template.c — Expanded test suite for template engine.
 * Tests: basic substitution, default values, loops, conditionals,
 *        NULL safety, edge cases, introspection.
 */
#include "template.h"
#include "json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* ── Test 1: simple variable substitution ── */
    char *err = NULL;
    template_t *tpl = template_compile("Hello {{ name }}!", &err);
    TEST("template_compile success", tpl != NULL && err == NULL);
    free(err);
    err = NULL;

    if (tpl) {
        json_t *ctx = json_object();
        json_set(ctx, "name", json_string("World"));
        char *out = template_render(tpl, ctx);
        TEST("template_render non-NULL", out != NULL);
        if (out) {
            TEST("render output 'Hello World!'",
                 strcmp(out, "Hello World!") == 0);
            free(out);
        }
        json_free(ctx);
        template_free(tpl);
    }

    /* ── Test 2: template_quick one-shot ── */
    json_t *ctx2 = json_object();
    json_set(ctx2, "item", json_string("test"));
    char *quick = template_quick("Item: {{ item }}", ctx2, &err);
    TEST("template_quick non-NULL", quick != NULL);
    if (quick) {
        TEST("template_quick output 'Item: test'",
             strcmp(quick, "Item: test") == 0);
        free(quick);
    }
    free(err);
    json_free(ctx2);

    /* ── Test 3: variable introspection ── */
    template_t *tpl2 = template_compile("{{ a }} and {{ b }}", NULL);
    if (tpl2) {
        size_t count = 0;
        char **vars = template_variables(tpl2, &count);
        TEST("template_variables count 2", count == 2);
        if (vars && count >= 2) {
            TEST("template_variables contains 'a'",
                 strcmp(vars[0], "a") == 0 || strcmp(vars[1], "a") == 0);
        }
        if (vars) {
            for (size_t i = 0; i < count; i++) free(vars[i]);
            free(vars);
        }
        template_free(tpl2);
    }

    /* ── Test 4: compile error ── */
    char *err2 = NULL;
    template_t *bad = template_compile("{{ unclosed", &err2);
    TEST("template_compile error returns NULL", bad == NULL);
    TEST("template_compile error message set", err2 != NULL);
    free(err2);

    /* ── Test 5: no variables (plain text passthrough) ── */
    template_t *plain = template_compile("Hello world", NULL);
    TEST("template_compile plain text", plain != NULL);
    if (plain) {
        json_t *empty_ctx = json_object();
        char *out = template_render(plain, empty_ctx);
        TEST("render plain text passthrough",
             out && strcmp(out, "Hello world") == 0);
        free(out);
        json_free(empty_ctx);
        template_free(plain);
    }

    /* ── Test 6: empty template string ── */
    template_t *empty_tpl = template_compile("", NULL);
    if (empty_tpl) {
        char *out = template_render(empty_tpl, NULL);
        TEST("render empty template", out && strlen(out) == 0);
        free(out);
        template_free(empty_tpl);
    }

    /* ── Test 7: NULL template compile error ── */
    char *nerr = NULL;
    template_t *null_tpl = template_compile(NULL, &nerr);
    TEST("template_compile(NULL) returns NULL", null_tpl == NULL);
    TEST("template_compile(NULL) error msg", nerr != NULL);
    free(nerr);

    /* ── Test 8: default value ({{ var|default }}) ── */
    template_t *def_tpl = template_compile("{{ missing|fallback }}", NULL);
    TEST("template_compile with default", def_tpl != NULL);
    if (def_tpl) {
        json_t *ctx = json_object();
        json_set(ctx, "other", json_string("x"));
        char *out = template_render(def_tpl, ctx);
        TEST("render missing var with default 'fallback'",
             out && strcmp(out, "fallback") == 0);
        free(out);
        /* Variable exists → uses value, not default */
        json_set(ctx, "missing", json_string("present"));
        char *out2 = template_render(def_tpl, ctx);
        TEST("render existing var with default uses value",
             out2 && strcmp(out2, "present") == 0);
        free(out2);
        json_free(ctx);
        template_free(def_tpl);
    }

    /* ── Test 9: for loop ── */
    template_t *for_tpl = template_compile("{% for item in items %}{{ item }},{% endfor %}", NULL);
    TEST("template_compile for loop", for_tpl != NULL);
    if (for_tpl) {
        json_t *list = json_array();
        json_append(list, json_string("a"));
        json_append(list, json_string("b"));
        json_append(list, json_string("c"));
        json_t *ctx = json_object();
        json_set(ctx, "items", list);
        char *out = template_render(for_tpl, ctx);
        TEST("render for loop 'a,b,c,'",
             out && strcmp(out, "a,b,c,") == 0);
        free(out);
        json_free(ctx);
        template_free(for_tpl);
    }

    /* ── Test 10: for loop with empty list ── */
    template_t *for_empty = template_compile("{% for item in items %}X{% endfor %}", NULL);
    if (for_empty) {
        json_t *ctx = json_object();
        json_set(ctx, "items", json_array());
        char *out = template_render(for_empty, ctx);
        TEST("render for loop empty list ''", out && strlen(out) == 0);
        free(out);
        json_free(ctx);
        template_free(for_empty);
    }

    /* ── Test 11: if condition true ── */
    template_t *if_tpl = template_compile("{% if flag %}yes{% endif %}", NULL);
    if (if_tpl) {
        json_t *ctx = json_object();
        json_set(ctx, "flag", json_bool(true));
        char *out = template_render(if_tpl, ctx);
        TEST("render if true 'yes'",
             out && strcmp(out, "yes") == 0);
        free(out);
        json_free(ctx);
        template_free(if_tpl);
    }

    /* ── Test 12: if condition false ── */
    template_t *if2 = template_compile("{% if flag %}yes{% endif %}", NULL);
    if (if2) {
        json_t *ctx = json_object();
        json_set(ctx, "flag", json_bool(false));
        char *out = template_render(if2, ctx);
        TEST("render if false ''", out && strlen(out) == 0);
        free(out);
        json_free(ctx);
        template_free(if2);
    }

    /* ── Test 13: if/else ── */
    template_t *ifelse = template_compile("{% if flag %}Y{% else %}N{% endif %}", NULL);
    if (ifelse) {
        json_t *ctx = json_object();
        json_set(ctx, "flag", json_bool(false));
        char *out = template_render(ifelse, ctx);
        TEST("render if/else false 'N'",
             out && strcmp(out, "N") == 0);
        free(out);
        json_free(ctx);
        template_free(ifelse);
    }

    /* ── Test 14: NULL template_render (returns empty string) ── */
    char *null_render = template_render(NULL, NULL);
    TEST("template_render(NULL) returns non-NULL", null_render != NULL);
    TEST("template_render(NULL) empty string",
         null_render && strlen(null_render) == 0);
    free(null_render);

    /* ── Test 15: template_free(NULL) no crash ── */
    template_free(NULL);
    TEST("template_free(NULL) no crash", 1);

    /* ── Test 16: missing variable (no default) renders empty */
    template_t *miss = template_compile("{{ missing }}", NULL);
    if (miss) {
        json_t *ctx = json_object();
        char *out = template_render(miss, ctx);
        TEST("render missing var empty", out && strlen(out) == 0);
        free(out);
        json_free(ctx);
        template_free(miss);
    }

    /* ── Test 17: variable introspection with no vars (returns NULL, count=0) ── */
    template_t *no_vars = template_compile("plain text here", NULL);
    if (no_vars) {
        size_t count = 99;
        char **v = template_variables(no_vars, &count);
        TEST("template_variables no vars count 0", count == 0);
        free(v);
        template_free(no_vars);
    }

    /* ── Test 18: template_quick with no variables ── */
    json_t *empty = json_object();
    char *q2 = template_quick("static text", empty, NULL);
    TEST("template_quick static passthrough",
         q2 && strcmp(q2, "static text") == 0);
    free(q2);
    json_free(empty);

    /* ── Test 19: multiple renders with different contexts ── */
    template_t *multi = template_compile("Val: {{ x }}", NULL);
    if (multi) {
        json_t *c1 = json_object(); json_set(c1, "x", json_string("A"));
        json_t *c2 = json_object(); json_set(c2, "x", json_string("B"));
        char *o1 = template_render(multi, c1);
        char *o2 = template_render(multi, c2);
        TEST("multi render 1 'Val: A'",
             o1 && strcmp(o1, "Val: A") == 0);
        TEST("multi render 2 'Val: B'",
             o2 && strcmp(o2, "Val: B") == 0);
        free(o1); free(o2);
        json_free(c1); json_free(c2);
        template_free(multi);
    }

    /* ── Test 20: template_quick with NULL context ── */
    char *q3 = template_quick("{{ x }}", NULL, NULL);
    TEST("template_quick NULL ctx no crash", q3 != NULL);
    free(q3);

    /* ── Test 21: default with multiple missing vars ── */
    template_t *multi_def = template_compile("{{ a|A }} or {{ b|B }}", NULL);
    if (multi_def) {
        json_t *ctx = json_object();
        json_set(ctx, "a", json_string("actual"));
        char *out = template_render(multi_def, ctx);
        TEST("multi default: actual for a, B for b",
             out && strcmp(out, "actual or B") == 0);
        free(out);
        json_free(ctx);
        template_free(multi_def);
    }

    /* ── Test 22: nested template (for inside if) ── */
    template_t *nested = template_compile(
        "{% if flag %}{% for item in items %}{{ item }}{% endfor %}{% endif %}", NULL);
    if (nested) {
        json_t *list = json_array();
        json_append(list, json_string("x"));
        json_append(list, json_string("y"));
        json_t *ctx = json_object();
        json_set(ctx, "flag", json_bool(true));
        json_set(ctx, "items", list);
        char *out = template_render(nested, ctx);
        TEST("render nested for-in-if 'xy'",
             out && strcmp(out, "xy") == 0);
        free(out);
        json_free(ctx);
        template_free(nested);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All template tests PASSED");
    return failures ? 1 : 0;
}
