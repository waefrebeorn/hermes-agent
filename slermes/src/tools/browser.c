/*
 * browser.c — Browser automation tools for Hermes C.
 * Phase 81-90: browser_navigate, browser_snapshot, browser_click, browser_type, etc.
 * Uses direct HTTP + HTML parsing (no Playwright dependency).
 */

#include "slermes.h"
#include "slermes_json.h"
#include "slermes_http.h"
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

/* browser_snapshot: Get current page state */
char *browser_snapshot_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    if (!g_tab.html) {
        return strdup("{\"error\": \"No page loaded. Navigate to a URL first.\"}");
    }

    char *text = html_to_text(g_tab.html);
    size_t text_len = text ? strlen(text) : 0;
    char preview[4096];
    if (text_len > 4000) {
        memcpy(preview, text, 3997);
        preview[3997] = '.'; preview[3998] = '.'; preview[3999] = '.';
        preview[4000] = '\0';
    } else {
        snprintf(preview, sizeof(preview), "%s", text ? text : "");
    }

    char *result = (char *)malloc(8192);
    if (!result) { free(text); return strdup("{\"error\": \"OOM\"}"); }
    snprintf(result, 8192,
        "{"
        "  \"url\": \"%s\","
        "  \"title\": \"%s\","
        "  \"content_length\": %zu,"
        "  \"text\": %s,"
        "  \"history_size\": %zu,"
        "  \"can_go_back\": %s,"
        "  \"can_go_forward\": %s"
        "}",
        g_tab.url, g_tab.title, g_tab.html_len, preview,
        g_tab.history_count,
        g_tab.history_pos > 0 ? "true" : "false",
        g_tab.history_pos < g_tab.history_count - 1 ? "true" : "false");
    free(text);
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

/* Register all browser tools. Call from tool_init.c */
static const char *BROWSER_NAVIGATE_SCHEMA =
    "{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"The URL to navigate to\"}"
    "},\"required\":[\"url\"]}";

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
}
