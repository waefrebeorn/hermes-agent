/*
 * browser.c — Browser automation tools for Hermes C.
 * Phase 81-90: browser_navigate, browser_snapshot, browser_click, browser_type, etc.
 * Uses direct HTTP + HTML parsing (no Playwright dependency).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
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

    char *result = (char *)malloc(8192);
    if (!result) { free(full_text); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, 8192,
        "{"
        "  \"url\": \"%s\","
        "  \"title\": \"%s\","
        "  \"content_length\": %zu,"
        "  \"text\": %s,"
        "  \"history_size\": %zu,"
        "  \"can_go_back\": %s,"
        "  \"can_go_forward\": %s,"
        "  \"scroll_offset\": %zu,"
        "  \"total_text_length\": %zu"
        "}",
        g_tab.url, g_tab.title, g_tab.html_len, preview,
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

    /* Find link by text */
    char *href = find_link_by_text(g_tab.html, target);
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
}
