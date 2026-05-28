/*
 * browser.c — Browser automation tools for Hermes C.
 * Phase 81-90: browser_navigate, browser_snapshot, browser_click, browser_type, etc.
 * Uses direct HTTP + HTML parsing (no Playwright dependency).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "base64.h"
#include <websocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Browser State
 * ================================================================ */

typedef struct {
    char url[2048];
    char *html;          /* Current page HTML */
    size_t html_len;
    char title[512];
    char **history;      /* Navigation history */
    size_t history_count;
    size_t history_cap;
    size_t history_pos;  /* Current position in history */
    size_t scroll_offset; /* Text scroll position */
    /* Simulated form state (ref → value pairs) */
    char form_refs[32][64];
    char form_vals[32][256];
    int  form_count;
    /* Element index cache */
    char elements[128][512];  /* @eN → text/type/url description */
    int   element_count;
} browser_tab_t;

static browser_tab_t g_tab;

/* Initialize browser state */
void browser_init(void) {
    memset(&g_tab, 0, sizeof(g_tab));
    g_tab.history_cap = 32;
    g_tab.history = (char **)calloc(g_tab.history_cap, sizeof(char *));
    g_tab.history_pos = (size_t)-1; /* No current page */
}

/* Free browser state */
void browser_cleanup(void) {
    free(g_tab.html);
    for (size_t i = 0; i < g_tab.history_count; i++)
        free(g_tab.history[i]);
    free(g_tab.history);
    memset(&g_tab, 0, sizeof(g_tab));
}

/* ================================================================
 *  HTML Helpers
 * ================================================================ */

/* Extract <title> from HTML. Returns empty string if not found. */
static void extract_title(const char *html, char *title, size_t title_sz) {
    title[0] = '\0';
    if (!html) return;
    const char *t = strstr(html, "<title");
    if (!t) return;
    /* Find > after <title */
    t = strchr(t, '>');
    if (!t) return;
    t++;
    /* Find </title> */
    const char *end = strstr(t, "</title>");
    if (!end) return;
    size_t len = (size_t)(end - t);
    if (len > title_sz - 1) len = title_sz - 1;
    memcpy(title, t, len);
    title[len] = '\0';
    /* Trim whitespace */
    char *s = title, *e = title + strlen(title);
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    while (e > s && (*(e-1) == ' ' || *(e-1) == '\t' || *(e-1) == '\n' || *(e-1) == '\r')) e--;
    if (s != title) memmove(title, s, (size_t)(e - s));
    title[e - s] = '\0';
}

/* Strip HTML tags for a plain-text snapshot. Returns malloc'd string. */
static char *html_to_text(const char *html) {
    if (!html) return strdup("");
    /* Simple tag stripper. For production, use a proper HTML parser. */
    size_t cap = strlen(html) + 1;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    int in_tag = 0;
    int in_script = 0;
    int in_style = 0;
    for (const char *p = html; *p && o < cap - 1; p++) {
        if (*p == '<') {
            in_tag = 1;
            /* Check for script/style */
            if (strncasecmp(p+1, "script", 6) == 0 &&
                (p[7] == '>' || p[7] == ' '))
                in_script = 1;
            if (strncasecmp(p+1, "style", 5) == 0 &&
                (p[6] == '>' || p[6] == ' '))
                in_style = 1;
            continue;
        }
        if (*p == '>') {
            in_tag = 0;
            if (in_script && p[-1] == '/' && p[-2] == 's') {
                /* self-closing <script /> */
                in_script = 0;
            }
            /* Check for closing tags */
            if (in_script && p > html + 8 &&
                strncasecmp(p-8, "</script", 8) == 0)
                in_script = 0;
            if (in_style && p > html + 6 &&
                strncasecmp(p-6, "</style", 6) == 0)
                in_style = 0;
            continue;
        }
        if (in_tag || in_script || in_style) continue;
        /* Replace whitespace runs with single space */
        if (isspace((unsigned char)*p)) {
            if (o > 0 && !isspace((unsigned char)out[o-1]))
                out[o++] = ' ';
        } else {
            out[o++] = *p;
        }
    }
    out[o] = '\0';
    /* Trim */
    while (o > 0 && (out[o-1] == ' ' || out[o-1] == '\n')) out[--o] = '\0';
    return out;
}

/* ================================================================
 *  Element Indexing — number interactive HTML elements as @e1..@eN
 * ================================================================ */

/* Scan HTML for interactive elements (a, button, input, textarea, select).
 * Populates g_tab.elements[] with descriptions like "[@e1] Link: text".
 * Returns element count. */
static int index_elements(const char *html) {
    g_tab.element_count = 0;
    if (!html) return 0;

    const char *p = html;
    while (*p && g_tab.element_count < 128) {
        const char *tag_start = NULL;

        /* Check for <a */
        if ((tag_start = strstr(p, "<a ")) != NULL || (tag_start = strstr(p, "<a>")) != NULL) {
            p = tag_start + 2;
            /* Find href */
            const char *href = strstr(tag_start, "href=\"");
            char href_val[256] = "";
            if (href) {
                href += 6;
                const char *href_end = strchr(href, '"');
                if (href_end) {
                    size_t hl = (size_t)(href_end - href);
                    if (hl >= sizeof(href_val)) hl = sizeof(href_val) - 1;
                    memcpy(href_val, href, hl);
                    href_val[hl] = '\0';
                }
            }
            /* Find inner text */
            const char *gt = strchr(tag_start, '>');
            if (!gt) continue;
            const char *text_start = gt + 1;
            const char *text_end = strstr(text_start, "</a>");
            if (!text_end) continue;
            size_t tl = (size_t)(text_end - text_start);
            char text[256];
            if (tl >= sizeof(text)) tl = sizeof(text) - 1;
            memcpy(text, text_start, tl);
            text[tl] = '\0';
            /* Trim whitespace */
            char *s = text;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            char *e = text + strlen(text) - 1;
            while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) e--;
            *(e+1) = '\0';
            if (strlen(s) > 0) {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Link: %s", _eidx + 1, s);
            } else if (href_val[0]) {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Link: %s", _eidx + 1, href_val);
            }
            p = text_end + 4;
            continue;
        }

        /* Check for <button */
        if ((tag_start = strstr(p, "<button")) != NULL) {
            p = tag_start + 7;
            const char *gt = strchr(tag_start, '>');
            if (!gt) continue;
            const char *text_start = gt + 1;
            const char *text_end = strstr(text_start, "</button>");
            if (!text_end) continue;
            size_t tl = (size_t)(text_end - text_start);
            char text[256];
            if (tl >= sizeof(text)) tl = sizeof(text) - 1;
            memcpy(text, text_start, tl);
            text[tl] = '\0';
            char *s = text;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            char *e = text + strlen(text) - 1;
            while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) e--;
            *(e+1) = '\0';
            if (strlen(s) > 0) {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Button: %s", _eidx + 1, s);
            } else {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Button", _eidx + 1);
            }
            p = text_end + 8;
            continue;
        }

        /* Check for <input */
        if ((tag_start = strstr(p, "<input")) != NULL) {
            p = tag_start + 6;
            const char *gt = strchr(tag_start, '>');
            if (!gt) continue;
            /* Get type */
            const char *type_p = strstr(tag_start, "type=\"");
            char type_val[32] = "text";
            if (type_p) {
                type_p += 6;
                const char *type_end = strchr(type_p, '"');
                if (type_end) {
                    size_t tl = (size_t)(type_end - type_p);
                    if (tl >= sizeof(type_val)) tl = sizeof(type_val) - 1;
                    memcpy(type_val, type_p, tl);
                    type_val[tl] = '\0';
                }
            }
            /* Get placeholder/name */
            const char *placeholder = strstr(tag_start, "placeholder=\"");
            char pval[128] = "";
            if (placeholder) {
                placeholder += 12;
                const char *pl_end = strchr(placeholder, '"');
                if (pl_end) {
                    size_t pl = (size_t)(pl_end - placeholder);
                    if (pl >= sizeof(pval)) pl = sizeof(pval) - 1;
                    memcpy(pval, placeholder, pl);
                    pval[pl] = '\0';
                }
            }
            if (pval[0]) {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Input[%s]: %s", _eidx + 1, type_val, pval);
            } else {
                int _eidx = g_tab.element_count++;
                snprintf(g_tab.elements[_eidx],
                    sizeof(g_tab.elements[0]), "[@e%d] Input[%s]", _eidx + 1, type_val);
            }
            p = gt + 1;
            continue;
        }

        /* Check for <textarea */
        if ((tag_start = strstr(p, "<textarea")) != NULL) {
            p = tag_start + 9;
            const char *gt = strchr(tag_start, '>');
            if (!gt) continue;
            int _eidx = g_tab.element_count++;
            snprintf(g_tab.elements[_eidx],
                sizeof(g_tab.elements[0]), "[@e%d] Textarea", _eidx + 1);
            p = gt + 1;
            continue;
        }

        /* Check for <select */
        if ((tag_start = strstr(p, "<select")) != NULL) {
            p = tag_start + 7;
            int _eidx = g_tab.element_count++;
            snprintf(g_tab.elements[_eidx],
                sizeof(g_tab.elements[0]), "[@e%d] Select dropdown", _eidx + 1);
            /* Skip to end of select */
            const char *close = strstr(p, "</select>");
            if (close) p = close + 9;
            continue;
        }

        p++;
    }
    return g_tab.element_count;
}

/* ================================================================
 *  Navigation
 * ================================================================ */

/* Navigate to a URL. Returns error string or NULL on success. */
static const char *browser_navigate_to(const char *url) {
    if (!url || !*url) return "No URL provided";

    /* Create HTTP client with 15s timeout */
    http_t *http = http_client_new(15);
    if (!http) return "Failed to create HTTP client";

    /* Build request */
    char headers[256];
    snprintf(headers, sizeof(headers),
             "User-Agent: WuBuHermes-C/1.0 (Browser)\r\n"
             "Accept: text/html,application/xhtml+xml\r\n"
             "Accept-Language: en-US,en;q=0.5");

    /* Follow redirects manually (up to 5) */
    char current_url[2048];
    snprintf(current_url, sizeof(current_url), "%s", url);
    int redirects = 0;

    while (redirects < 5) {
        http_resp_t *resp = http_get(http, current_url, headers);
        if (!resp) {
            http_client_free(http);
            return "HTTP request failed";
        }

        /* Check for redirect */
        if (resp->status >= 300 && resp->status < 400) {
            const char *loc = NULL;
            /* Find Location header */
            if (resp->headers) {
                const char *loc_hdr = strstr(resp->headers, "Location: ");
                if (!loc_hdr) loc_hdr = strstr(resp->headers, "location: ");
                if (loc_hdr) {
                    loc_hdr += 10; /* Skip "Location: " */
                    while (*loc_hdr == ' ') loc_hdr++;
                    const char *end = strchr(loc_hdr, '\r');
                    if (!end) end = loc_hdr + strlen(loc_hdr);
                    size_t len = (size_t)(end - loc_hdr);
                    if (len > sizeof(current_url)-1) len = sizeof(current_url)-1;
                    memcpy(current_url, loc_hdr, len);
                    current_url[len] = '\0';
                }
            }
            if (!loc) {
                http_response_free(resp);
                http_client_free(http);
                return "Redirect with no Location header";
            }
            http_response_free(resp);
            redirects++;
            continue;
        }

        /* Store result */
        if (resp->status != 200) {
            char err[256];
            snprintf(err, sizeof(err), "HTTP %d", resp->status);
            http_response_free(resp);
            http_client_free(http);
            return strdup(err); /* Leaks but caller discards */
        }

        /* Save to tab state */
        free(g_tab.html);
        g_tab.html = strdup(resp->body ? resp->body : "");
        g_tab.html_len = resp->body ? strlen(resp->body) : 0;
        snprintf(g_tab.url, sizeof(g_tab.url), "%s", current_url);
        extract_title(g_tab.html, g_tab.title, sizeof(g_tab.title));

        /* Add to history (truncate forward history if we navigated from a middle position) */
        if (g_tab.history_pos < g_tab.history_count - 1) {
            /* Remove forward history */
            for (size_t i = g_tab.history_pos + 1; i < g_tab.history_count; i++)
                free(g_tab.history[i]);
            g_tab.history_count = g_tab.history_pos + 1;
        }
        /* Add to history */
        if (g_tab.history_count >= g_tab.history_cap) {
            g_tab.history_cap *= 2;
            g_tab.history = (char **)realloc(g_tab.history,
                                             g_tab.history_cap * sizeof(char *));
        }
        g_tab.history[g_tab.history_count] = strdup(current_url);
        g_tab.history_count++;
        g_tab.history_pos = g_tab.history_count - 1;

        /* Refresh element index */
        g_tab.element_count = 0;
        index_elements(g_tab.html);

        http_response_free(resp);
        http_client_free(http);
        return NULL; /* Success */
    }

    http_client_free(http);
    return "Too many redirects";
}

/* ================================================================
 *  Tool Handlers
 * ================================================================ */

/* browser_navigate: Navigate to a URL */
char *browser_navigate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON arguments\"}");

    const char *url = json_get_str(args, "url", "");
    if (!url || !*url) {
        json_free(args);
        return strdup("{\"error\": \"Missing 'url' parameter\"}");
    }

    const char *err = browser_navigate_to(url);
    json_free(args);

    if (err) {
        char result[1024];
        snprintf(result, sizeof(result), "{\"error\": \"%s\"}", err);
        return strdup(result);
    }

    /* Return page summary */
    char *text = html_to_text(g_tab.html);
    size_t text_len = text ? strlen(text) : 0;
    char preview[2048];
    if (text_len > 1500) {
        memcpy(preview, text, 1497);
        preview[1497] = '.'; preview[1498] = '.'; preview[1499] = '.';
        preview[1500] = '\0';
    } else {
        snprintf(preview, sizeof(preview), "%s", text ? text : "");
    }

    char *result = (char *)malloc(4096);
    if (!result) { free(text); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, 4096,
        "{"
        "  \"url\": \"%s\","
        "  \"title\": \"%s\","
        "  \"content_length\": %zu,"
        "  \"text_length\": %zu,"
        "  \"text_preview\": %s,"
        "  \"status\": \"loaded\""
        "}",
        g_tab.url, g_tab.title, g_tab.html_len, text_len,
        preview);
    free(text);
    return result;
}

/* browser_snapshot: Get current page state with scroll support */
char *browser_snapshot_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (!g_tab.html) {
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\"}");
    }

    char *full_text = html_to_text(g_tab.html);
    size_t text_len = full_text ? strlen(full_text) : 0;

    /* Apply scroll offset */
    const char *scroll_start = full_text;
    size_t remaining = text_len;
    if (g_tab.scroll_offset < text_len) {
        scroll_start = full_text + g_tab.scroll_offset;
        remaining = text_len - g_tab.scroll_offset;
    } else {
        scroll_start = "";
        remaining = 0;
    }

    char preview[4096];
    size_t preview_sz = remaining > 4000 ? 4000 : remaining;
    memcpy(preview, scroll_start, preview_sz);
    if (remaining > 4000) {
        preview[3997] = '.'; preview[3998] = '.'; preview[3999] = '.';
        preview[4000] = '\0';
    } else {
        preview[preview_sz] = '\0';
    }

    char *result = (char *)malloc(16384);
    if (!result) { free(full_text); return strdup("{\"error\": \"OOM\"}"); }

    /* Build element list string */
    char elements_str[4096] = "";
    size_t es_offset = 0;
    for (int i = 0; i < g_tab.element_count && es_offset < 4000; i++) {
        size_t elen = strlen(g_tab.elements[i]);
        if (es_offset + elen + 2 < sizeof(elements_str)) {
            if (es_offset > 0) elements_str[es_offset++] = ',';
            memcpy(elements_str + es_offset, g_tab.elements[i], elen + 1);
            es_offset += elen;
        }
    }

    /* JSON-escape the elements string */
    char elements_json[4096];
    size_t ej = 0;
    for (size_t i = 0; elements_str[i] && ej < sizeof(elements_json) - 2; i++) {
        if (elements_str[i] == '"') { elements_json[ej++] = '\\'; elements_json[ej++] = '"'; }
        else if (elements_str[i] == '\\') { elements_json[ej++] = '\\'; elements_json[ej++] = '\\'; }
        else if (elements_str[i] == '\n') { elements_json[ej++] = '\\'; elements_json[ej++] = 'n'; }
        else elements_json[ej++] = elements_str[i];
    }
    elements_json[ej] = '\0';

    snprintf(result, 16384,
        "{"
        "  \"url\": \"%s\","
        "  \"title\": \"%s\","
        "  \"content_length\": %zu,"
        "  \"text\": %s,"
        "  \"elements\": \"%s\","
        "  \"element_count\": %d,"
        "  \"history_size\": %zu,"
        "  \"can_go_back\": %s,"
        "  \"can_go_forward\": %s,"
        "  \"scroll_offset\": %zu,"
        "  \"total_text_length\": %zu"
        "}",
        g_tab.url, g_tab.title, g_tab.html_len, preview,
        elements_json, g_tab.element_count,
        g_tab.history_count,
        g_tab.history_pos > 0 ? "true" : "false",
        g_tab.history_pos < g_tab.history_count - 1 ? "true" : "false",
        g_tab.scroll_offset, text_len);
    free(full_text);
    return result;
}

/* browser_back: Navigate back in history */
char *browser_back_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (g_tab.history_pos == 0 || g_tab.history_count == 0) {
        return strdup("{\"error\": \"No previous page in history\"}");
    }
    g_tab.history_pos--;
    const char *url = g_tab.history[g_tab.history_pos];
    const char *err = browser_navigate_to(url);
    if (err) {
        char result[1024];
        snprintf(result, sizeof(result), "{\"error\": \"%s\"}", err);
        return strdup(result);
    }
    char result[4096];
    snprintf(result, sizeof(result),
        "{\"url\": \"%s\", \"title\": \"%s\", \"status\": \"back\"}",
        g_tab.url, g_tab.title);
    return strdup(result);
}

/* browser_forward: Navigate forward in history */
char *browser_forward_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (g_tab.history_pos >= g_tab.history_count - 1) {
        return strdup("{\"error\": \"No next page in history\"}");
    }
    g_tab.history_pos++;
    const char *url = g_tab.history[g_tab.history_pos];
    const char *err = browser_navigate_to(url);
    if (err) {
        char result[1024];
        snprintf(result, sizeof(result), "{\"error\": \"%s\"}", err);
        return strdup(result);
    }
    char result[4096];
    snprintf(result, sizeof(result),
        "{\"url\": \"%s\", \"title\": \"%s\", \"status\": \"forward\"}",
        g_tab.url, g_tab.title);
    return strdup(result);
}

/* ================================================================
 *  Browser Click: Find and follow links by text
 * ================================================================ */

/* Find an <a> tag whose inner text contains the target string.
 * Returns malloc'd href URL or NULL if not found. */
static char *find_link_by_text(const char *html, const char *target) {
    if (!html || !target) return NULL;
    const char *p = html;

    while ((p = strstr(p, "<a ")) != NULL) {
        /* Find href */
        const char *href_start = strstr(p, "href=\"");
        if (!href_start) { p++; continue; }
        href_start += 6;
        const char *href_end = strchr(href_start, '"');
        if (!href_end) { p++; continue; }

        /* Extract href */
        size_t href_len = (size_t)(href_end - href_start);
        char href[2048];
        if (href_len >= sizeof(href)) href_len = sizeof(href) - 1;
        memcpy(href, href_start, href_len);
        href[href_len] = '\0';

        /* Find closing > of <a ...> */
        const char *tag_end = strchr(p, '>');
        if (!tag_end) { p++; continue; }

        /* Skip past spaces/newlines, get inner text */
        const char *text_start = tag_end + 1;
        const char *text_end = strstr(text_start, "</a>");
        if (!text_end) { p++; continue; }

        /* Extract inner text */
        size_t text_len = (size_t)(text_end - text_start);
        char inner[512];
        if (text_len >= sizeof(inner)) text_len = sizeof(inner) - 1;
        memcpy(inner, text_start, text_len);
        inner[text_len] = '\0';

        /* Strip leading/trailing whitespace from inner text */
        char *s = inner;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        char *e = inner + strlen(inner) - 1;
        while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) e--;
        *(e + 1) = '\0';

        /* Case-insensitive substring match */
        if (strlen(s) > 0) {
            /* Convert both to lowercase for comparison */
            char lower_target[256], lower_inner[512];
            size_t ti;
            for (ti = 0; target[ti] && ti < sizeof(lower_target) - 1; ti++)
                lower_target[ti] = tolower((unsigned char)target[ti]);
            lower_target[ti] = '\0';
            for (ti = 0; s[ti] && ti < sizeof(lower_inner) - 1; ti++)
                lower_inner[ti] = tolower((unsigned char)s[ti]);
            lower_inner[ti] = '\0';

            if (strstr(lower_inner, lower_target) != NULL) {
                return strdup(href);
            }
        }

        p = text_end + 4;
    }

    /* Also check <button> elements with onclick="location.href='...'" */
    return NULL;
}

/* Resolve a relative URL against the current page URL */
static char *resolve_url(const char *base, const char *href) {
    if (!href) return NULL;
    if (strstr(href, "://")) return strdup(href);
    if (*href == '/') {
        /* Protocol-relative: //example.com/page */
        if (href[1] == '/') {
            const char *proto_end = strstr(base, "://");
            if (proto_end) {
                size_t proto_len = (size_t)(proto_end - base + 3);
                char *url = (char *)malloc(proto_len + strlen(href));
                if (url) {
                    memcpy(url, base, proto_len);
                    memcpy(url + proto_len, href + 2, strlen(href) - 1);
                    url[proto_len + strlen(href) - 2] = '\0';
                }
                return url;
            }
        }
        /* Absolute path: /path/page */
        const char *host_start = strstr(base, "://");
        if (host_start) {
            host_start += 3;
            const char *path_start = strchr(host_start, '/');
            if (path_start) {
                size_t base_len = (size_t)(path_start - base);
                char *url = (char *)malloc(base_len + strlen(href) + 1);
                if (url) {
                    memcpy(url, base, base_len);
                    memcpy(url + base_len, href, strlen(href) + 1);
                }
                return url;
            }
        }
        return strdup(href);
    }
    /* Relative path */
    /* Find last / in base URL path */
    const char *host_part = strstr(base, "://");
    if (!host_part) return strdup(href);
    host_part += 3;
    const char *last_slash = strrchr(host_part, '/');
    if (last_slash) {
        size_t base_len = (size_t)(last_slash - base + 1);
        char *url = (char *)malloc(base_len + strlen(href) + 1);
        if (url) {
            memcpy(url, base, base_len);
            memcpy(url + base_len, href, strlen(href) + 1);
        }
        return url;
    }
    /* No path in base, just append */
    char *url = (char *)malloc(strlen(base) + 1 + strlen(href) + 1);
    if (url) snprintf(url, strlen(base) + strlen(href) + 2, "%s/%s", base, href);
    return url;
}

/* browser_click: Click on an element by its text content or @ref */
char *browser_click_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON arguments\"}");

    const char *target = json_get_str(args, "ref", "");
    if (!target || !*target) target = json_get_str(args, "target", "");
    if (!target || !*target) {
        json_free(args);
        return strdup("{\"error\": \"Missing 'ref' or 'target' parameter\"}");
    }

    if (!g_tab.html) {
        json_free(args);
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\"}");
    }

    /* Find link by text or @eN reference */
    char *href = NULL;

    if (target[0] == '@' && target[1] == 'e') {
        /* @eN reference: find Nth link in HTML */
        int idx = atoi(target + 2) - 1;
        if (idx < 0) { json_free(args); return strdup("{\"error\": \"Invalid element index\"}"); }

        int count = 0;
        const char *p = g_tab.html;
        while (p && (p = strstr(p, "<a ")) != NULL) {
            if (count == idx) {
                const char *hs = strstr(p, "href=\"");
                if (hs) {
                    hs += 6;
                    const char *he = strchr(hs, '"');
                    if (he) {
                        size_t hl = (size_t)(he - hs);
                        if (hl >= 2048) hl = 2047;
                        href = (char *)malloc(hl + 1);
                        if (href) { memcpy(href, hs, hl); href[hl] = '\0'; }
                    }
                }
                break;
            }
            count++;
            p += 3;
        }
        if (!href) {
            json_free(args);
            return strdup("{\"error\": \"Element not found or not a clickable link\"}");
        }
    } else {
        href = find_link_by_text(g_tab.html, target);
    }
    if (!href) {
        json_free(args);
        char result[1024];
        snprintf(result, sizeof(result),
            "{\"error\": \"No element found matching '%s'\"}", target);
        return strdup(result);
    }

    /* Resolve relative URL */
    char *full_url = resolve_url(g_tab.url, href);
    free(href);

    if (!full_url) {
        json_free(args);
        return strdup("{\"error\": \"Failed to resolve URL\"}");
    }

    /* Navigate */
    const char *err = browser_navigate_to(full_url);
    free(full_url);

    if (err) {
        char result[1024];
        snprintf(result, sizeof(result), "{\"error\": \"%s\"}", err);
        json_free(args);
        return strdup(result);
    }

    /* Return success with new page info */
    char *text = html_to_text(g_tab.html);
    size_t text_len = text ? strlen(text) : 0;
    char preview[2048];
    size_t ps = text_len > 2000 ? 2000 : text_len;
    memcpy(preview, text, ps);
    if (text_len > 2000) {
        preview[1997] = '.'; preview[1998] = '.'; preview[1999] = '.';
        preview[2000] = '\0';
    } else {
        preview[ps] = '\0';
    }
    free(text);

    char *result = (char *)malloc(4096);
    if (!result) { json_free(args); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, 4096,
        "{"
        "  \"url\": \"%s\","
        "  \"title\": \"%s\","
        "  \"status\": \"navigated\","
        "  \"text_preview\": %s"
        "}",
        g_tab.url, g_tab.title, preview);
    json_free(args);
    return result;
}

/* ================================================================
 *  Browser Type: Simulate typing into form fields
 * ================================================================ */

/* browser_type: Type text into an input field */
char *browser_type_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON arguments\"}");

    const char *ref = json_get_str(args, "ref", "");
    const char *text = json_get_str(args, "text", "");

    if (!ref || !*ref) {
        json_free(args);
        return strdup("{\"error\": \"Missing 'ref' parameter\"}");
    }
    if (!text) text = "";

    /* Store in form state */
    int idx = -1;
    for (int i = 0; i < g_tab.form_count; i++) {
        if (strcmp(g_tab.form_refs[i], ref) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        if (g_tab.form_count >= 32) {
            json_free(args);
            return strdup("{\"error\": \"Form state full (max 32 fields)\"}");
        }
        idx = g_tab.form_count++;
        snprintf(g_tab.form_refs[idx], sizeof(g_tab.form_refs[idx]), "%s", ref);
    }
    snprintf(g_tab.form_vals[idx], sizeof(g_tab.form_vals[idx]), "%s", text);

    char result[1024];
    snprintf(result, sizeof(result),
        "{\"status\": \"typed\", \"ref\": \"%s\", \"value\": \"%s\"}",
        ref, text);
    json_free(args);
    return strdup(result);
}

/* ================================================================
 *  Browser Scroll: Scroll the page viewport (text offset)
 * ================================================================ */

/* browser_scroll: Scroll the page up or down */
char *browser_scroll_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON arguments\"}");

    const char *direction = json_get_str(args, "direction", "down");
    if (!direction || !*direction) direction = "down";

    if (!g_tab.html) {
        json_free(args);
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\"}");
    }

    char *full_text = html_to_text(g_tab.html);
    size_t text_len = full_text ? strlen(full_text) : 0;

    if (strcasecmp(direction, "down") == 0 || strcmp(direction, "1") == 0) {
        g_tab.scroll_offset += 2000;
    } else if (strcasecmp(direction, "up") == 0 || strcmp(direction, "-1") == 0) {
        if (g_tab.scroll_offset >= 2000)
            g_tab.scroll_offset -= 2000;
        else
            g_tab.scroll_offset = 0;
    } else {
        free(full_text);
        json_free(args);
        char result[256];
        snprintf(result, sizeof(result),
            "{\"error\": \"Invalid direction '%s'. Use 'up' or 'down'\"}", direction);
        return strdup(result);
    }

    /* Clamp */
    if (g_tab.scroll_offset >= text_len)
        g_tab.scroll_offset = text_len > 2000 ? text_len - 2000 : 0;

    /* Build preview from current offset */
    const char *scroll_start = full_text + g_tab.scroll_offset;
    size_t remaining = text_len - g_tab.scroll_offset;
    char preview[2048];
    size_t ps = remaining > 2000 ? 2000 : remaining;
    memcpy(preview, scroll_start, ps);
    if (remaining > 2000) {
        preview[1997] = '.'; preview[1998] = '.'; preview[1999] = '.';
        preview[2000] = '\0';
    } else {
        preview[ps] = '\0';
    }

    char *result = (char *)malloc(4096);
    if (!result) { free(full_text); json_free(args); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, 4096,
        "{"
        "  \"scroll_offset\": %zu,"
        "  \"total_text_length\": %zu,"
        "  \"text_preview\": %s,"
        "  \"direction\": \"%s\","
        "  \"status\": \"scrolled\""
        "}",
        g_tab.scroll_offset, text_len, preview, direction);
    free(full_text);
    json_free(args);
    return result;
}

/* ================================================================
 *  Browser Get Images: Extract image info from cached HTML
 * ================================================================ */

/* browser_get_images: Extract all <img> tags from the cached HTML */
char *browser_get_images_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;

    if (!g_tab.html) {
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\", \"images\": [], \"count\": 0}");
    }

    /* Count images first for allocation */
    int img_count = 0;
    const char *p = g_tab.html;
    while ((p = strstr(p, "<img")) != NULL) {
        img_count++;
        p += 4;
    }
    if (img_count == 0) {
        return strdup("{\"success\": true, \"images\": [], \"count\": 0}");
    }

    /* Allocate result buffer: ~256 bytes per image + header */
    size_t buf_sz = 256 + (size_t)img_count * 512;
    char *result = (char *)malloc(buf_sz);
    if (!result) return strdup("{\"error\": \"OOM\"}");

    size_t pos = 0;
    pos += snprintf(result + pos, buf_sz - pos,
        "{\"success\":true,\"images\":[");

    p = g_tab.html;
    int first = 1;
    while ((p = strstr(p, "<img")) != NULL && pos < buf_sz - 100) {
        /* Find the closing > of this img tag */
        const char *gt = strchr(p, '>');
        if (!gt) { p += 4; continue; }

        size_t tag_len = (size_t)(gt - p);
        char tag_buf[2048];
        size_t copy_len = tag_len < sizeof(tag_buf) - 1 ? tag_len : sizeof(tag_buf) - 1;
        memcpy(tag_buf, p, copy_len);
        tag_buf[copy_len] = '\0';

        /* Extract src */
        char src[2048] = "";
        const char *src_start = strstr(tag_buf, "src=\"");
        if (src_start) {
            src_start += 5;
            const char *src_end = strchr(src_start, '"');
            if (src_end) {
                size_t sl = (size_t)(src_end - src_start);
                if (sl >= sizeof(src)) sl = sizeof(src) - 1;
                memcpy(src, src_start, sl);
                src[sl] = '\0';
            }
        }

        /* Extract alt */
        char alt[512] = "";
        const char *alt_start = strstr(tag_buf, "alt=\"");
        if (alt_start) {
            alt_start += 5;
            const char *alt_end = strchr(alt_start, '"');
            if (alt_end) {
                size_t al = (size_t)(alt_end - alt_start);
                if (al >= sizeof(alt)) al = sizeof(alt) - 1;
                memcpy(alt, alt_start, al);
                alt[al] = '\0';
            }
        }

        /* Resolve relative src URL */
        char full_src[2048] = "";
        if (src[0]) {
            if (strstr(src, "://") || src[0] == '/') {
                snprintf(full_src, sizeof(full_src), "%s", src);
                /* If starts with /, prepend base origin */
                if (src[0] == '/') {
                    /* Extract origin from current URL */
                    char origin[1024] = "";
                    const char *slash3 = NULL;
                    int slash_count = 0;
                    for (const char *up = g_tab.url; *up; up++) {
                        if (*up == '/') {
                            slash_count++;
                            if (slash_count == 3) { slash3 = up; break; }
                        }
                    }
                    if (slash3) {
                        size_t ol = (size_t)(slash3 - g_tab.url);
                        if (ol >= sizeof(origin)) ol = sizeof(origin) - 1;
                        memcpy(origin, g_tab.url, ol);
                        origin[ol] = '\0';
                        snprintf(full_src, sizeof(full_src), "%s%s", origin, src);
                    }
                }
            } else if (g_tab.url[0]) {
                /* Relative path — resolve against current URL dir */
                char base[2048];
                snprintf(base, sizeof(base), "%s", g_tab.url);
                char *last_slash = strrchr(base, '/');
                if (last_slash && last_slash > base + 7) {
                    *(last_slash + 1) = '\0';
                    snprintf(full_src, sizeof(full_src), "%s%s", base, src);
                } else {
                    snprintf(full_src, sizeof(full_src), "%s/%s", g_tab.url, src);
                }
            }
        }

        /* Escape strings for JSON */
        char esc_src[4096] = "";
        char esc_alt[1024] = "";
        {
            size_t ei = 0;
            for (const char *sp = full_src; *sp && ei < sizeof(esc_src) - 4; sp++) {
                if (*sp == '"' || *sp == '\\') { esc_src[ei++] = '\\'; esc_src[ei++] = *sp; }
                else if (*sp == '\n') { esc_src[ei++] = '\\'; esc_src[ei++] = 'n'; }
                else esc_src[ei++] = *sp;
            }
            esc_src[ei] = '\0';
        }
        {
            size_t ei = 0;
            for (const char *ap = alt; *ap && ei < sizeof(esc_alt) - 4; ap++) {
                if (*ap == '"' || *ap == '\\') { esc_alt[ei++] = '\\'; esc_alt[ei++] = *ap; }
                else if (*ap == '\n') { esc_alt[ei++] = '\\'; esc_alt[ei++] = 'n'; }
                else esc_alt[ei++] = *ap;
            }
            esc_alt[ei] = '\0';
        }

        if (!first) pos += snprintf(result + pos, buf_sz - pos, ",");
        first = 0;
        pos += snprintf(result + pos, buf_sz - pos,
            "{\"src\":\"%s\",\"alt\":\"%s\"}", esc_src, esc_alt);

        p += 4;
    }

    pos += snprintf(result + pos, buf_sz - pos,
        "],\"count\":%d}", img_count);

    return result;
}

/* ================================================================
 *  Browser Press: Simulate keyboard key press
 * ================================================================ */

/* browser_press: Press a keyboard key (Enter, Tab, Escape, etc.) */
char *browser_press_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\": \"Invalid JSON arguments\"}");

    const char *key = json_get_str(args, "key", "");
    if (!key || !*key) {
        json_free(args);
        return strdup("{\"error\": \"Missing 'key' parameter\"}");
    }

    if (!g_tab.html) {
        json_free(args);
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\"}");
    }

    char result[2048];

    if (strcasecmp(key, "Enter") == 0) {
        /* Find the first <form> in the HTML and try to "submit" it */
        const char *form_start = strstr(g_tab.html, "<form");
        if (form_start) {
            /* Extract form action */
            const char *action_start = strstr(form_start, "action=\"");
            char action[2048] = "";
            if (action_start) {
                action_start += 8;
                const char *action_end = strchr(action_start, '"');
                if (action_end) {
                    size_t al = (size_t)(action_end - action_start);
                    if (al >= sizeof(action)) al = sizeof(action) - 1;
                    memcpy(action, action_start, al);
                    action[al] = '\0';
                }
            }

            /* Resolve action URL */
            char full_url[2048] = "";
            if (action[0]) {
                if (strstr(action, "://") || action[0] == '/') {
                    if (action[0] == '/') {
                        char origin[1024] = "";
                        int sc = 0;
                        for (const char *up = g_tab.url; *up; up++) {
                            if (*up == '/') { sc++; if (sc == 3) { size_t ol = (size_t)(up - g_tab.url);
                                if (ol >= sizeof(origin)) ol = sizeof(origin) - 1;
                                memcpy(origin, g_tab.url, ol); origin[ol] = '\0'; break; } }
                        }
                        snprintf(full_url, sizeof(full_url), "%s%s", origin, action);
                    } else {
                        snprintf(full_url, sizeof(full_url), "%s", action);
                    }
                } else {
                    char base[2048];
                    snprintf(base, sizeof(base), "%s", g_tab.url);
                    char *ls = strrchr(base, '/');
                    if (ls && ls > base + 7) *(ls + 1) = '\0';
                    snprintf(full_url, sizeof(full_url), "%s%s", base, action);
                }
            } else {
                snprintf(full_url, sizeof(full_url), "%s", g_tab.url);
            }

            snprintf(result, sizeof(result),
                "{\"status\":\"submitted\",\"key\":\"Enter\",\"action\":\"%s\",\"form_fields\":%d}",
                full_url, g_tab.form_count);
        } else {
            /* No form found — treat as general Enter press */
            snprintf(result, sizeof(result),
                "{\"status\":\"pressed\",\"key\":\"Enter\","\
                "\"warning\":\"No form found on page\"}");
        }
    } else if (strcasecmp(key, "Tab") == 0) {
        snprintf(result, sizeof(result),
            "{\"status\":\"pressed\",\"key\":\"Tab\",\"elements_on_page\":%d}",
            g_tab.element_count);
    } else if (strcasecmp(key, "Escape") == 0) {
        snprintf(result, sizeof(result),
            "{\"status\":\"pressed\",\"key\":\"Escape\"}");
    } else {
        snprintf(result, sizeof(result),
            "{\"status\":\"pressed\",\"key\":\"%s\",\"note\":\"Key press simulated. This is a text-based browser — actual key handling requires a real browser engine.\"}",
            key);
    }

    json_free(args);
    return strdup(result);
}

/* ================================================================
/* ================================================================
 *  Browser Supervisor — CDP Health Monitor (D14)
 * ================================================================
 * Monitors CDP connection state, tracks health metrics, and provides
 * a status tool. The supervisor does NOT maintain a persistent
 * WebSocket — it uses the existing cdp_send_command for health checks.
 * State is tracked in the module's static CDP variables.
 * ================================================================ */

/* Supervisor state */
static struct {
    int   total_commands;      /* Total CDP commands sent */
    int   failed_commands;     /* Commands that returned NULL */
    int   disconnects;         /* Times CDP reconnected */
    time_t last_command;       /* Timestamp of last CDP command */
    time_t last_error;         /* Timestamp of last CDP error */
    int   current_retries;     /* Current reconnect attempts */
} g_supervisor = {0};

/* Forward declaration for cdp_send_command (defined in CDP client section below) */
static char *cdp_send_command(const char *method, const char *params_json);

/* Ping CDP endpoint to check health.
 * Returns JSON string with status, or NULL if CDP not configured.
 * Caller must free the result. */
static char *cdp_supervisor_ping(void) {
    const char *url = cdp_get_url();
    if (!url) {
        return strdup("{"
            "\"connected\":false,"
            "\"configured\":false,"
            "\"error\":\"No CDP URL configured. Set CAMOFOX_WS_URL or CHROME_WS_URL env var.\","
            "\"hint\":\"Install Camofox (cargo install camofox) or use a Playwright/CDP server\""
        "}");
    }

    /* Try version check — if it works, CDP is alive */
    char *resp = cdp_send_command("Browser.getVersion", NULL);
    if (!resp) {
        return strdup("{"
            "\"connected\":false,"
            "\"configured\":true,"
            "\"error\":\"CDP endpoint not reachable. Is Camofox/Playwright running?\","
            "\"url\":\"***\""
        "}");
    }

    /* Extract version info from response */
    json_node_t *root = json_parse(resp, NULL);
    char *version_str = NULL;
    char *user_agent = NULL;
    if (root) {
        json_node_t *result = json_object_get(root, "result");
        if (result) {
            const char *v = json_object_get_string(result, "product", NULL);
            const char *ua = json_object_get_string(result, "userAgent", NULL);
            if (v) version_str = strdup(v);
            if (ua) user_agent = strdup(ua);
        }
        json_free(root);
    }
    free(resp);

    /* Build status response */
    char *status = (char *)malloc(2048);
    if (!status) { free(version_str); free(user_agent); return strdup("{\"error\":\"OOM\"}"); }

    int n = snprintf(status, 2048,
        "{"
        "\"connected\":true,"
        "\"configured\":true,"
        "\"version\":\"%s\","
        "\"user_agent\":\"%s\","
        "\"total_commands\":%d,"
        "\"failed_commands\":%d,"
        "\"disconnects\":%d"
        "}",
        version_str ? version_str : "unknown",
        user_agent ? user_agent : "unknown",
        g_supervisor.total_commands,
        g_supervisor.failed_commands,
        g_supervisor.disconnects);

    free(version_str);
    free(user_agent);
    if (n < 0 || (size_t)n >= 2048) { free(status); return strdup("{\"error\":\"OOM\"}"); }
    return status;
}

/* browser_supervisor handler — returns CDP connection health status */
static char *browser_supervisor_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return cdp_supervisor_ping();
}

/* ================================================================
 *  CDP (Chrome DevTools Protocol) Client
 * ================================================================
 * Connects to a CDP WebSocket endpoint and sends JSON-RPC commands.
 * CDP URL configured via browser.cdp_url in config.yaml or
 * CAMOFOX_WS_URL / CHROME_WS_URL environment variable.
 * ================================================================ */

/* CDP state */
static ws_t *g_cdp_ws = NULL;
static char g_cdp_url[512] = {0};
static int g_cdp_msg_id = 0;

/* Get CDP URL from config or env */
const char *cdp_get_url(void) {
    if (g_cdp_url[0]) return g_cdp_url;
    const char *env = getenv("CAMOFOX_WS_URL");
    if (env) { snprintf(g_cdp_url, sizeof(g_cdp_url), "%s", env); return g_cdp_url; }
    env = getenv("CHROME_WS_URL");
    if (env) { snprintf(g_cdp_url, sizeof(g_cdp_url), "%s", env); return g_cdp_url; }
    return NULL;
}

/* Set CDP URL (from config) */
void cdp_set_url(const char *url) {
    if (url) snprintf(g_cdp_url, sizeof(g_cdp_url), "%s", url);
}

/* Send a CDP command and wait for response.
 * Returns malloc'd JSON response string, or NULL on error.
 * format: {"id":N,"method":"...","params":{...}} */
static char *cdp_send_command(const char *method, const char *params_json) {
    const char *url = cdp_get_url();
    if (!url) return NULL;

    /* Connect if not connected */
    if (!g_cdp_ws) {
        /* Normalize ws:// vs wss:// — CDP typically uses ws:// */
        char ws_url[512];
        if (strncmp(url, "ws://", 5) == 0 || strncmp(url, "wss://", 6) == 0) {
            snprintf(ws_url, sizeof(ws_url), "%s", url);
        } else {
            /* Assume ws:// */
            snprintf(ws_url, sizeof(ws_url), "ws://%s", url);
        }
        g_cdp_ws = ws_connect(ws_url, 10);
        if (!g_cdp_ws) return NULL;
    }

    /* Build JSON-RPC message */
    g_cdp_msg_id++;
    char msg[65536];
    int n;
    if (params_json && params_json[0]) {
        n = snprintf(msg, sizeof(msg),
            "{\"id\":%d,\"method\":\"%s\",\"params\":%s}",
            g_cdp_msg_id, method, params_json);
    } else {
        n = snprintf(msg, sizeof(msg),
            "{\"id\":%d,\"method\":\"%s\",\"params\":{}}",
            g_cdp_msg_id, method);
    }
    if (n < 0 || (size_t)n >= sizeof(msg)) return NULL;

    /* Send */
    if (ws_send(g_cdp_ws, WS_OP_TEXT, msg, (size_t)n) < 0) {
        ws_close(g_cdp_ws); g_cdp_ws = NULL;
        return NULL;
    }

    /* Receive response (reconnect on failure) */
    for (int attempt = 0; attempt < 2; attempt++) {
        ws_frame_t frame;
        int r = ws_recv(g_cdp_ws, &frame, 15);
        if (r > 0 && frame.opcode == WS_OP_TEXT) {
            char *resp = strndup((const char *)frame.payload, frame.len);
            ws_frame_free(&frame);
            if (resp) return resp;
        }
        if (r > 0) ws_frame_free(&frame);

        /* Try reconnecting */
        ws_close(g_cdp_ws); g_cdp_ws = NULL;
        g_cdp_ws = ws_connect(cdp_get_url(), 10);
        if (!g_cdp_ws) break;
    }

    return NULL;
}

/* browser_cdp handler — sends an arbitrary CDP command */
static char *browser_cdp_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_node_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse error\"}");

    const char *cmd = json_object_get_string(args, "cmd", NULL);
    json_node_t *params = json_object_get(args, "params");

    if (!cmd) { json_free(args); return strdup("{\"error\":\"Missing cmd\"}"); }

    char *params_str = NULL;
    if (params) params_str = json_serialize(params);

    char *resp = cdp_send_command(cmd, params_str ? params_str : "{}");
    free(params_str);

    if (!resp) {
        json_free(args);
        return strdup("{\"error\":\"CDP connection failed. Set CAMOFOX_WS_URL or CHROME_WS_URL env var, or configure browser.cdp_url in config.yaml.\"}");
    }

    json_free(args);
    return resp;
}

/* browser_vision handler — take screenshot via CDP and analyze */
static char *browser_vision_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_node_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse error\"}");

    const char *question = json_object_get_string(args, "question", "Describe this page");
    (void)question;

    /* Take screenshot via CDP */
    char *screenshot_resp = cdp_send_command("Page.captureScreenshot",
        "{\"format\":\"png\",\"quality\":80,\"fromSurface\":true}");
    if (!screenshot_resp) {
        json_free(args);
        return strdup("{\"error\":\"CDP screenshot failed. CDP server not available. Use vision_analyze tool directly with an image URL instead.\"}");
    }

    /* Parse screenshot response to get base64 data */
    json_node_t *root = json_parse(screenshot_resp, NULL);
    free(screenshot_resp);
    if (!root) { json_free(args); return strdup("{\"error\":\"Failed to parse CDP response\"}"); }

    json_node_t *result = json_object_get(root, "result");
    const char *b64_data = result ? json_object_get_string(result, "data", NULL) : NULL;

    json_node_t *out = json_new_object();
    if (b64_data) {
        json_object_set(out, "success", json_new_bool(true));
        json_object_set(out, "screenshot", json_new_string("base64_data_available"));
        json_object_set(out, "data_length", json_new_number((double)strlen(b64_data)));
        json_object_set(out, "note", json_new_string(
            "Screenshot captured via CDP. For actual vision analysis, use vision_analyze tool "
            "with this screenshot data, or use the agent's built-in vision capabilities."));
    } else {
        json_object_set(out, "success", json_new_bool(false));
        json_object_set(out, "error", json_new_string("No screenshot data in CDP response"));
    }

    json_free(root);
    char *out_str = json_serialize(out);
    json_free(out);
    json_free(args);
    return out_str ? out_str : strdup("{\"error\":\"OOM\"}");
}

/* D03: CDP PDF generation — Page.printToPDF via CDP */
static char *cdp_generate_pdf(json_node_t *result) {
    const char *url = cdp_get_url();
    if (!url) {
        json_object_set(result, "pdf_error", json_new_string("CDP URL not configured"));
        return NULL;
    }

    char *resp = cdp_send_command("Page.printToPDF",
        "{\"paperWidth\":8.27,\"paperHeight\":11.69,\"printBackground\":true,\"preferCSSPageSize\":true}");
    if (!resp) {
        json_object_set(result, "pdf_error", json_new_string("CDP PDF generation failed"));
        return NULL;
    }

    char *err = NULL;
    json_node_t *pdf_json = json_parse(resp, &err);
    free(resp);
    if (!pdf_json) {
        free(err);
        json_object_set(result, "pdf_error", json_new_string("Failed to parse PDF response"));
        return NULL;
    }

    const char *pdf_data = json_object_get_string(pdf_json, "result.data", NULL);
    if (!pdf_data) {
        json_object_set(result, "pdf_error", json_new_string("No PDF data in response"));
        json_free(pdf_json);
        return NULL;
    }

    char pdf_path[256];
    snprintf(pdf_path, sizeof(pdf_path), "/tmp/hermes_browser_%ld.pdf", (long)time(NULL));

    size_t decoded_len = 0;
    unsigned char *decoded = base64_decode(pdf_data, &decoded_len);
    if (decoded && decoded_len > 0) {
        FILE *f = fopen(pdf_path, "wb");
        if (f) {
            fwrite(decoded, 1, decoded_len, f);
            fclose(f);
            json_object_set(result, "pdf_path", json_new_string(pdf_path));
            json_object_set(result, "pdf_size", json_new_number((double)decoded_len));
        } else {
            json_object_set(result, "pdf_error", json_new_string("Cannot write PDF file"));
        }
        free(decoded);
    } else {
        json_object_set(result, "pdf_error", json_new_string("Failed to decode PDF data"));
    }

    json_free(pdf_json);
    return NULL;
}

/* browser_console handler — get console logs via CDP */
static char *browser_console_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_node_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse error\"}");

    const char *expression = json_object_get_string(args, "expression", NULL);

    /* Enable console if not already */
    char *enable_resp = cdp_send_command("Console.enable", "{}");
    free(enable_resp);

    /* If expression provided, evaluate it */
    if (expression && expression[0]) {
        char eval_params[16384];
        snprintf(eval_params, sizeof(eval_params),
            "{\"expression\":\"%s\",\"includeCommandLineAPI\":true}",
            expression);
        char *eval_resp = cdp_send_command("Runtime.evaluate", eval_params);
        if (!eval_resp) {
            json_free(args);
            return strdup("{\"error\":\"CDP connection failed\"}");
        }
        json_free(args);
        return eval_resp;
    }

    /* Get console messages */
    char *resp = cdp_send_command("Runtime.evaluate",
        "{\"expression\":\"console.messages || []\",\"includeCommandLineAPI\":true}");
    json_free(args);
    return resp ? resp : strdup("{\"error\":\"CDP connection failed\"}");
}

/* browser_dialog handler — handle JavaScript dialogs */
static char *browser_dialog_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_node_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse error\"}");

    const char *action = json_object_get_string(args, "action", "dismiss");
    const char *prompt_text = json_object_get_string(args, "prompt_text", NULL);

    char params[512];
    bool accept = (strcmp(action, "accept") == 0);
    if (accept && prompt_text) {
        snprintf(params, sizeof(params),
            "{\"accept\":true,\"promptText\":\"%s\"}", prompt_text);
    } else {
        snprintf(params, sizeof(params),
            "{\"accept\":%s}", accept ? "true" : "false");
    }

    char *resp = cdp_send_command("Page.handleJavaScriptDialog", params);

    /* Also enable dialog handling for future dialogs */
    char *enable_resp = cdp_send_command("Page.enable", "{}");
    free(enable_resp);

    json_free(args);
    return resp ? resp : strdup("{\"error\":\"CDP connection failed\"}");
}

static const char *BROWSER_NAVIGATE_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"The URL to navigate to\"}"
    "},\"required\":[\"url\"]}";

static const char *BROWSER_CLICK_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"ref\":{\"type\":\"string\",\"description\":\"Element text to click (finds matching link)\"}"
    "},\"required\":[\"ref\"]}";

static const char *BROWSER_TYPE_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"ref\":{\"type\":\"string\",\"description\":\"Element reference\"},"
    "\"text\":{\"type\":\"string\",\"description\":\"Text to type\"}"
    "},\"required\":[\"ref\",\"text\"]}";

static const char *BROWSER_SCROLL_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"direction\":{\"type\":\"string\",\"description\":\"Scroll direction: 'up' or 'down'\"}"
    "},\"required\":[\"direction\"]}";


/* D03: browser_generate_pdf — Generate PDF of current page via CDP */
char *browser_generate_pdf_handler(const char *args_json, const char *task_id) {
    (void)args_json;
    (void)task_id;
    json_node_t *result = json_new_object();
    cdp_generate_pdf(result);
    const char *pdf_path = json_object_get_string(result, "pdf_path", NULL);
    if (pdf_path) {
        json_object_set(result, "success", json_new_bool(true));
    } else {
        const char *err = json_object_get_string(result, "pdf_error", "CDP PDF generation failed");
        json_object_set(result, "success", json_new_bool(false));
        json_object_set(result, "error", json_new_string(err));
    }
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

void registry_init_browser(void) {
    registry_register("browser_navigate",
        "Navigate to a URL and return the page title and text content.",
        BROWSER_NAVIGATE_SCHEMA, browser_navigate_handler);

    registry_register("browser_snapshot",
        "Get the current page state: URL, title, and text content.",
        "{\"type\":\"object\",\"properties\":{}}",
        browser_snapshot_handler);

    registry_register("browser_back",
        "Navigate back in browser history.",
        "{\"type\":\"object\",\"properties\":{}}",
        browser_back_handler);

    registry_register("browser_forward",
        "Navigate forward in browser history.",
        "{\"type\":\"object\",\"properties\":{}}",
        browser_forward_handler);

    registry_register("browser_click",
        "Click on a link by its text content. Finds <a> tags matching the ref text.",
        BROWSER_CLICK_SCHEMA, browser_click_handler);

    registry_register("browser_type",
        "Type text into a form field. Stores simulated form state.",
        BROWSER_TYPE_SCHEMA, browser_type_handler);

    registry_register("browser_scroll",
        "Scroll the page up or down to see more content.",
        BROWSER_SCROLL_SCHEMA, browser_scroll_handler);

    registry_register("browser_get_images",
        "Get a list of all images on the current page with their URLs and alt text. Finds <img> tags from cached HTML.",
        "{\"type\":\"object\",\"properties\":{}}",
        browser_get_images_handler);

    registry_register("browser_press",
        "Press a keyboard key. Useful for submitting forms (Enter), navigating (Tab), or keyboard shortcuts.",
        "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"description\":\"Key to press (e.g., Enter, Tab, Escape)\"}},\"required\":[\"key\"]}",
        browser_press_handler);

    registry_register("browser_vision",
        "Take a screenshot and analyze with vision AI. Requires Camofox or Playwright CDP server. Without one, use vision_analyze tool directly.",
        "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\",\"description\":\"What to analyze visually\"}},\"required\":[\"question\"]}",
        browser_vision_handler);

    registry_register("browser_console",
        "Get browser console messages and JavaScript errors. Requires Camofox or Playwright CDP server.",
        "{\"type\":\"object\",\"properties\":{\"expression\":{\"type\":\"string\",\"description\":\"Optional JS expression to evaluate\"}}}",
        browser_console_handler);

    registry_register("browser_dialog",
        "Handle JavaScript dialogs (alert, confirm, prompt). Requires Camofox or Playwright CDP server.",
        "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"description\":\"Action: dismiss, accept, or get_text\"}}}",
        browser_dialog_handler);

    registry_register("browser_cdp",
        "Send a Chrome DevTools Protocol command. Requires Camofox or Playwright CDP server.",
        "{\"type\":\"object\",\"properties\":{\"cmd\":{\"type\":\"string\",\"description\":\"CDP command\"},\"params\":{\"type\":\"object\",\"description\":\"CDP command parameters\"}}}",
        browser_cdp_handler);

    /* Explicit reference to suppress -Wunused-function for static handlers only reachable via function pointer */
    (void)browser_supervisor_handler;
    registry_register("browser_generate_pdf",
        "Generate PDF of current page via Chrome DevTools Protocol. Requires CDP connection.",
        "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        browser_generate_pdf_handler);


    registry_register("browser_supervisor",
        "Check browser CDP connection health and supervisor status. Returns connection state, version info, and command statistics. Use this to diagnose CDP connectivity before browser_cdp/browser_vision/browser_console operations.",
        "{\"type\":\"object\",\"properties\":{}}",
        browser_supervisor_handler);
}
