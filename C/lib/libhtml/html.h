/*
 * html.h — C HTML escaping/unescaping library.
 *
 * Provides HTML entity encoding for safe output in HTML contexts.
 * Thread-safe — no global state.
 */

#ifndef HERMES_HTML_H
#define HERMES_HTML_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** html_escape(text) — Escape HTML special chars: & < > " '. Caller free(). Returns NULL on error. */
char *html_escape(const char *text);

/** html_escape_len(text, len) — Escape with explicit length (binary-safe). Caller free(). */
char *html_escape_len(const char *text, size_t len);

/** html_unescape(text) — Decode HTML entities: &amp; &lt; &gt; &quot; &#39; &#x3C; etc. Caller free(). */
char *html_unescape(const char *text);

/** html_strip_tags(text) — Remove all HTML tags, leaving text content. Caller free(). */
char *html_strip_tags(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_HTML_H */
