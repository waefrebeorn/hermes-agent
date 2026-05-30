#ifndef LIBTEMPLATE_H
#define LIBTEMPLATE_H

/*
 * libtemplate.h — Standalone template engine for C.
 * Replaces Python's jinja2 for prompt/message template rendering.
 * Zero external dependencies.
 *
 * MIT License — WuBu Hermes Project
 *
 * Syntax:
 *   {{ variable }}          — variable substitution
 *   {{ variable|default }}  — with default value
 *   {% for item in list %}  — loop over array (from json_t *)
 *     {{ item }}
 *   {% endfor %}
 *   {% if condition %}      — conditional
 *   {% else %}
 *   {% endif %}
 *
 * Usage:
 *   template_t *tpl = template_compile("Hello {{ name }}!", NULL);
 *   char *out = template_render(tpl, ctx);  // ctx is json_t object
 *   printf("%s\n", out);
 *   free(out);
 *   template_free(tpl);
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types */
typedef struct template_t template_t;

/* === Compilation === */
/* Compile template string. Returns NULL on parse error. */
template_t *template_compile(const char *input, char **error_msg);

/* === Rendering === */
/* Render template with context (json_t object). Caller free()s result. */
char *template_render(const template_t *tpl, const void *json_ctx);

/* === Introspection === */
/* Get list of referenced variable names. Caller must free. */
char **template_variables(const template_t *tpl, size_t *count);

/* === Cleanup === */
void template_free(template_t *tpl);

/* === Utility === */
/* Quick one-shot: compile + render + free. Returns rendered string. */
char *template_quick(const char *template_str, const void *json_ctx, char **error);

#ifdef __cplusplus
}
#endif

#endif /* LIBTEMPLATE_H */
