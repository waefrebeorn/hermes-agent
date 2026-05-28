/*
 * test_template.c — Smoke test for template engine.
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
    /* Test 1: simple variable substitution */
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
            TEST("render output", strcmp(out, "Hello World!") == 0);
            free(out);
        }
        json_free(ctx);
        template_free(tpl);
    }

    /* Test 2: template_quick one-shot */
    json_t *ctx2 = json_object();
    json_set(ctx2, "item", json_string("test"));
    char *quick = template_quick("Item: {{ item }}", ctx2, &err);
    TEST("template_quick non-NULL", quick != NULL);
    if (quick) {
        TEST("template_quick output", strcmp(quick, "Item: test") == 0);
        free(quick);
    }
    free(err);
    json_free(ctx2);

    /* Test 3: variable introspection */
    template_t *tpl2 = template_compile("{{ a }} and {{ b }}", NULL);
    if (tpl2) {
        size_t count = 0;
        char **vars = template_variables(tpl2, &count);
        TEST("template_variables count", count == 2);
        free(vars);
        template_free(tpl2);
    }

    /* Test 4: compile error */
    char *err2 = NULL;
    template_t *bad = template_compile("{{ unclosed", &err2);
    TEST("template_compile error returns NULL", bad == NULL);
    TEST("template_compile error message", err2 != NULL);
    free(err2);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All template tests PASSED");
    return failures ? 1 : 0;
}
