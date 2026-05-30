/*
 * html.c — C HTML escaping/unescaping library.
 *
 * Escapes: & < > " '  —  Unescapes: &amp; &lt; &gt; &quot; &#39; &#x...;
 * Strips HTML tags. Thread-safe — no global state.
 */

#include "html.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ================================================================
 *  Escape
 * ================================================================ */

char *html_escape(const char *text) {
    if (!text) return NULL;
    return html_escape_len(text, strlen(text));
}

char *html_escape_len(const char *text, size_t len) {
    if (!text) { return NULL; }
    if (len == 0) { return strdup(""); }

    /* First pass: count expanded size */
    size_t out_len = 0;
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
            case '&':  out_len += 5; break;  /* &amp; */
            case '<':  out_len += 4; break;  /* &lt; */
            case '>':  out_len += 4; break;  /* &gt; */
            case '"':  out_len += 6; break;  /* &quot; */
            case '\'': out_len += 5; break;  /* &#39; */
            default:   out_len += 1; break;
        }
    }

    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;

    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
            case '&':  memcpy(out + o, "&amp;", 5);  o += 5; break;
            case '<':  memcpy(out + o, "&lt;", 4);   o += 4; break;
            case '>':  memcpy(out + o, "&gt;", 4);   o += 4; break;
            case '"':  memcpy(out + o, "&quot;", 6); o += 6; break;
            case '\'': memcpy(out + o, "&#39;", 5);  o += 5; break;
            default:   out[o++] = text[i];           break;
        }
    }
    out[o] = '\0';
    return out;
}

/* ================================================================
 *  Unescape helper: decode a single HTML entity
 * ================================================================ */

/* Decode numeric entity &#NNN; or &#xHH;. Returns code point or -1. */
static int decode_numeric(const char *s, size_t len, size_t *consumed) {
    *consumed = 0;
    if (len < 3 || s[0] != '&' || s[1] != '#') return -1;
    size_t i = 2;
    unsigned long val = 0;
    bool is_hex = (s[i] == 'x' || s[i] == 'X');
    if (is_hex) {
        i++;
        while (i < len && s[i] != ';') {
            char c = (char)tolower((unsigned char)s[i]);
            if (c >= '0' && c <= '9') val = val * 16 + (unsigned long)(c - '0');
            else if (c >= 'a' && c <= 'f') val = val * 16 + (unsigned long)(c - 'a' + 10);
            else return -1;
            i++;
        }
    } else {
        while (i < len && s[i] != ';') {
            if (s[i] < '0' || s[i] > '9') return -1;
            val = val * 10 + (unsigned long)(s[i] - '0');
            i++;
        }
    }
    if (i >= len || s[i] != ';') return -1;
    *consumed = i + 1; /* including ';' */
    return (int)val;
}

char *html_unescape(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    if (len == 0) return strdup("");

    /* Max output = len (entities expand to shorter) */
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t o = 0;
    for (size_t i = 0; i < len; ) {
        if (text[i] == '&') {
            /* Try named entities */
            if (strncmp(text + i, "&amp;", 5) == 0)  { out[o++] = '&';  i += 5; }
            else if (strncmp(text + i, "&lt;", 4) == 0)  { out[o++] = '<';  i += 4; }
            else if (strncmp(text + i, "&gt;", 4) == 0)  { out[o++] = '>';  i += 4; }
            else if (strncmp(text + i, "&quot;", 6) == 0) { out[o++] = '"';  i += 6; }
            else if (strncmp(text + i, "&#39;", 5) == 0)  { out[o++] = '\''; i += 5; }
            else if (strncmp(text + i, "&#x27;", 6) == 0) { out[o++] = '\''; i += 6; }
            else if (strncmp(text + i, "&#x2F;", 6) == 0) { out[o++] = '/';  i += 6; }
            else if (strncmp(text + i, "&#x60;", 6) == 0) { out[o++] = '`';  i += 6; }
            else {
                /* Try numeric entity */
                size_t consumed = 0;
                int cp = decode_numeric(text + i, len - i, &consumed);
                if (cp > 0 && cp < 256 && consumed > 0) {
                    out[o++] = (char)cp;
                    i += consumed;
                } else {
                    out[o++] = text[i++]; /* passthrough */
                }
            }
        } else {
            out[o++] = text[i++];
        }
    }
    out[o] = '\0';
    return out;
}

/* ================================================================
 *  Strip tags
 * ================================================================ */

char *html_strip_tags(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    if (len == 0) return strdup("");

    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t o = 0;
    bool in_tag = false;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '<') {
            in_tag = true;
        } else if (text[i] == '>') {
            in_tag = false;
        } else if (!in_tag) {
            /* Also decode entities in remaining text */
            if (text[i] == '&' && strncmp(text + i, "&amp;", 5) == 0) {
                out[o++] = '&'; i += 4;
            } else if (text[i] == '&' && strncmp(text + i, "&lt;", 4) == 0) {
                out[o++] = '<'; i += 3;
            } else if (text[i] == '&' && strncmp(text + i, "&gt;", 4) == 0) {
                out[o++] = '>'; i += 3;
            } else {
                out[o++] = text[i];
            }
        }
    }
    out[o] = '\0';
    return out;
}

/* Strip YAML frontmatter (--- delimited) from content.
 * Port of Python agent/prompt_builder.py _strip_yaml_frontmatter().
 * If content starts with "---", finds closing "\n---" and returns content after it.
 * Returns copy of content unchanged if no frontmatter found.
 * Caller must free the returned string. Returns NULL on NULL input. */
char *strip_yaml_frontmatter(const char *content) {
    if (!content) return NULL;

    /* Check if starts with --- */
    if (strncmp(content, "---", 3) != 0)
        return strdup(content);

    /* Find closing \n--- */
    const char *end = strstr(content + 3, "\n---");
    if (!end)
        return strdup(content);

    /* Skip past closing --- and trailing newlines */
    const char *body = end + 4;
    while (*body == '\n') body++;

    /* If body is empty (only frontmatter), return original */
    if (!*body)
        return strdup(content);

    return strdup(body);
}
