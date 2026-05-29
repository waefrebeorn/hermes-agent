/*
 * web.c — Web tools for Hermes C.
 * HTTP GET requests via libcurl or raw sockets.
 * Web search interface (uses http client).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_tool_config.h"
#include "base64.h"
#include "hermes_url_safety.h"
#include "html.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* Schema */
static const char *SCHEMA_GET = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"url\":{\"type\":\"string\",\"description\":\"URL to fetch\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":30},"
      "\"method\":{\"type\":\"string\",\"description\":\"HTTP method: GET, POST, PUT, DELETE\",\"default\":\"GET\"},"
      "\"headers\":{\"type\":\"string\",\"description\":\"Custom HTTP headers as 'Key: Value' lines (newline-separated)\"},"
      "\"body\":{\"type\":\"string\",\"description\":\"Request body for POST/PUT requests\"},"
      "\"proxy\":{\"type\":\"string\",\"description\":\"HTTP proxy URL (e.g., http://proxy:8080). Uses CONNECT tunnel for HTTPS.\"},"
      "\"user_agent\":{\"type\":\"string\",\"description\":\"Custom User-Agent header value. Default: libhttp/X.Y\"},"
      "\"follow_redirects\":{\"type\":\"boolean\",\"description\":\"Follow HTTP redirects (3xx). Default: true\",\"default\":true},"
      "\"max_redirects\":{\"type\":\"integer\",\"description\":\"Max redirects to follow (0=unlimited, default: 5)\",\"default\":5},"
      "\"cookies\":{\"type\":\"string\",\"description\":\"Cookie header value to send with request (e.g., session=abc123; token=xyz)\"},"
      "\"include_body\":{\"type\":\"boolean\",\"description\":\"Include response body in output. Set false to return only status code and URL (faster, less token usage).\",\"default\":true},"
      "\"auth_type\":{\"type\":\"string\",\"description\":\"Authentication type: 'basic' (user:pass) or 'bearer' (token only). Default: basic\"},"
      "\"save_path\":{\"type\":\"string\",\"description\":\"Save response body to file path instead of returning in output. Useful for downloading PDFs, images, and other binary files.\"},"
      "\"cookie_jar\":{\"type\":\"string\",\"description\":\"Path to JSON cookie jar file. Cookies from Set-Cookie headers are auto-saved and re-sent.\"}"
    "},"
    "\"required\":[\"url\"]"
"}"; /* end SCHEMA_GET */

/* Resolve HTTP method string to enum */
static http_method_t method_str_to_enum(const char *method) {
    if (!method) return HTTP_GET;
    if (strcasecmp(method, "POST") == 0) return HTTP_POST;
    if (strcasecmp(method, "PUT") == 0) return HTTP_PUT;
    if (strcasecmp(method, "DELETE") == 0) return HTTP_DELETE;
    return HTTP_GET;
}

/* Cookie jar helpers — load/save cookies from JSON file */
/* Cookie jar format: plain text file, one "name=value" per line.
 * Simpler than JSON — avoids needing JSON object key iteration API. */
#define COOKIE_JAR_MAX_LINE 2048
#define COOKIE_JAR_MAX_COOKIES 128

/* Read cookie jar file into a flat buffer (raw text). */
static char *cookie_jar_read(const char *path) {
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > 65536) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Write cookie entries (null-terminated array of "name=value") to jar file. */
static bool cookie_jar_write(const char *path, const char *entries[], int count) {
    if (!path || !*path || !entries) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            fputs(entries[i], f);
            fputc('\n', f);
        }
    }
    fclose(f);
    return true;
}

/* Parse Set-Cookie headers from raw HTTP response headers.
 * Extracts name=value pairs into entries array (up to max_count).
 * Returns number of cookies found. */
static int cookie_jar_parse_headers(const char *raw_headers, char entries[][COOKIE_JAR_MAX_LINE], int max_count) {
    if (!raw_headers) return 0;
    int count = 0;
    const char *p = raw_headers;
    while ((p = strstr(p, "Set-Cookie:")) != NULL && count < max_count) {
        p += 11;
        while (*p == ' ') p++;
        const char *semi = strchr(p, ';');
        const char *eol = strchr(p, '\r');
        if (!eol) eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);
        const char *end = semi && semi < eol ? semi : eol;
        const char *eq = (const char *)memchr(p, '=', (size_t)(end - p));
        if (eq && eq > p && eq < end) {
            size_t name_len = (size_t)(eq - p);
            size_t val_len = (size_t)(end - eq - 1);
            if (name_len > 0 && name_len + val_len + 2 < COOKIE_JAR_MAX_LINE) {
                memcpy(entries[count], p, name_len);
                entries[count][name_len] = '=';
                memcpy(entries[count] + name_len + 1, eq + 1, val_len);
                entries[count][name_len + 1 + val_len] = '\0';
                count++;
            }
        }
        p = end;
    }
    return count;
}

/* Update cookie jar file from HTTP response headers.
 * Reads existing jar, merges new Set-Cookie entries, writes back.
 * New entries overwrite existing ones with the same name. */
static void cookie_jar_update(const char *path, const char *raw_headers) {
    if (!path || !*path || !raw_headers) return;

    /* Read existing entries */
    char existing[COOKIE_JAR_MAX_COOKIES][COOKIE_JAR_MAX_LINE];
    int existing_count = 0;
    char *jar_data = cookie_jar_read(path);
    if (jar_data) {
        char *line = jar_data;
        char *next;
        while (line && existing_count < COOKIE_JAR_MAX_COOKIES) {
            next = strchr(line, '\n');
            if (next) *next = '\0';
            if (line[0] && strchr(line, '=')) {
                strncpy(existing[existing_count], line, COOKIE_JAR_MAX_LINE - 1);
                existing[existing_count][COOKIE_JAR_MAX_LINE - 1] = '\0';
                existing_count++;
            }
            line = next ? (next + 1) : NULL;
        }
        free(jar_data);
    }

    /* Parse new Set-Cookie entries and merge (overwrite on name match) */
    char new_entries[COOKIE_JAR_MAX_COOKIES][COOKIE_JAR_MAX_LINE];
    int new_count = cookie_jar_parse_headers(raw_headers, new_entries, COOKIE_JAR_MAX_COOKIES);
    for (int i = 0; i < new_count; i++) {
        /* Extract name part (before =) */
        char *eq = strchr(new_entries[i], '=');
        if (!eq) continue;
        size_t name_len = (size_t)(eq - new_entries[i]);

        /* Check if this name already exists in existing entries */
        bool found = false;
        for (int j = 0; j < existing_count; j++) {
            if (strncmp(existing[j], new_entries[i], name_len) == 0 && existing[j][name_len] == '=') {
                strncpy(existing[j], new_entries[i], COOKIE_JAR_MAX_LINE - 1);
                existing[j][COOKIE_JAR_MAX_LINE - 1] = '\0';
                found = true;
                break;
            }
        }
        /* If not found, add new entry */
        if (!found && existing_count < COOKIE_JAR_MAX_COOKIES) {
            strncpy(existing[existing_count], new_entries[i], COOKIE_JAR_MAX_LINE - 1);
            existing[existing_count][COOKIE_JAR_MAX_LINE - 1] = '\0';
            existing_count++;
        }
    }

    /* Build string array for write */
    const char *write_entries[COOKIE_JAR_MAX_COOKIES];
    for (int i = 0; i < existing_count; i++)
        write_entries[i] = existing[i];

    cookie_jar_write(path, write_entries, existing_count);
}

/* url_has_secret() moved to url_safety.c */

/* ================================================================
 *  HTTP GET / POST / PUT / DELETE handler
 * ================================================================ */

char *web_get_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *url = json_object_get_string(args, "url", NULL);
    int timeout = (int)json_object_get_number(args, "timeout", 30);
    const char *method_str = json_object_get_string(args, "method", "GET");
    const char *headers_str = json_object_get_string(args, "headers", NULL);
    const char *body = json_object_get_string(args, "body", NULL);
    const char *auth = json_object_get_string(args, "auth", NULL);
    const char *proxy = json_object_get_string(args, "proxy", NULL);
    const char *user_agent = json_object_get_string(args, "user_agent", NULL);
    const char *cookies = json_object_get_string(args, "cookies", NULL);
    const char *cookie_jar = json_object_get_string(args, "cookie_jar", NULL);
    const char *auth_type = json_object_get_string(args, "auth_type", NULL);
    const char *save_path = json_object_get_string(args, "save_path", NULL);

    /* Strdup values that survive json_free */
    char *url_copy = url ? strdup(url) : NULL;
    char *headers_copy = headers_str ? strdup(headers_str) : NULL;
    char *body_copy = body ? strdup(body) : NULL;
    char *auth_copy = auth ? strdup(auth) : NULL;
    char *proxy_copy = proxy ? strdup(proxy) : NULL;
    char *ua_copy = user_agent ? strdup(user_agent) : NULL;
    char *cookies_copy = cookies ? strdup(cookies) : NULL;
    char *auth_type_copy = auth_type ? strdup(auth_type) : NULL;
    char *save_path_copy = save_path ? strdup(save_path) : NULL;
    bool include_body_val = json_object_get_bool(args, "include_body", true);

    /* Redirect following params */
    bool follow_redirects = json_object_get_bool(args, "follow_redirects", true);
    int max_redirects_val = (int)json_object_get_number(args, "max_redirects", 5);
    char method_buf[16];
    snprintf(method_buf, sizeof(method_buf), "%s", method_str ? method_str : "GET");
    http_method_t method = method_str_to_enum(method_buf);

    json_free(args);

    if (!url_copy) {
        free(headers_copy);
        free(body_copy);
        free(proxy_copy);
        free(cookies_copy);
        free(auth_type_copy);
        free(save_path_copy);
        free(ua_copy);
        return strdup("{\"error\":\"Missing url\"}");
    }

    /* Secret exfiltration prevention: block URLs containing API keys */
    {
        const char *secret_found = url_has_secret(url_copy);
        if (secret_found) {
            char result[128];
            snprintf(result, sizeof(result),
                     "{\"error\":\"Blocked: URL contains what appears to be a %s API key. Secrets must not be sent in URLs.\"}",
                     secret_found);
            free(headers_copy);
            free(body_copy);
            free(proxy_copy);
            free(cookies_copy);
            free(auth_type_copy);
            free(save_path_copy);
            free(ua_copy);
            free(url_copy);
            return strdup(result);
        }
    }

    /* SSRF protection: block internal/private URLs */
    if (!url_is_safe(url_copy)) {
        free(headers_copy);
        free(body_copy);
        free(proxy_copy);
        free(cookies_copy);
        free(auth_type_copy);
        free(save_path_copy);
        free(ua_copy);
        free(url_copy);
        return strdup("{\"error\":\"URL blocked by SSRF protection: private or internal address\"}");
    }

    /* Load cookie jar if specified */
    char *jar_cookies_str = NULL;
    char merged_cookies[8192] = {0};
    if (cookie_jar && cookie_jar[0]) {
        char *jar_data = cookie_jar_read(cookie_jar);
        if (jar_data) {
            /* Parse lines into Cookie header string (name=val; name=val) */
            char *line = jar_data;
            char *next;
            while (line) {
                next = strchr(line, '\n');
                if (next) *next = '\0';
                if (line[0] && strchr(line, '=')) {
                    size_t cur = strlen(merged_cookies);
                    snprintf(merged_cookies + cur, sizeof(merged_cookies) - cur,
                             "%s%s", cur > 0 ? "; " : "", line);
                }
                line = next ? (next + 1) : NULL;
            }
            free(jar_data);
            /* Merge with explicit cookies param if provided */
            if (cookies_copy && cookies_copy[0]) {
                size_t cur = strlen(merged_cookies);
                if (cur > 0)
                    snprintf(merged_cookies + cur, sizeof(merged_cookies) - cur,
                             "; %s", cookies_copy);
                else
                    snprintf(merged_cookies, sizeof(merged_cookies), "%s", cookies_copy);
            }
            if (merged_cookies[0])
                jar_cookies_str = merged_cookies;
        }
    }
    const char *effective_cookies = jar_cookies_str ? jar_cookies_str : cookies_copy;

    http_client_t *client = http_client_new(timeout);
    http_client_enable_cookies((http_t *)client, true);
    if (proxy_copy && proxy_copy[0]) {
        http_client_set_proxy(client, proxy_copy);

    if (!follow_redirects) {
        http_client_set_max_redirects((http_t *)client, 0);
    } else if (max_redirects_val > 0) {
        http_client_set_max_redirects((http_t *)client, max_redirects_val);
    }
    }
    /* Build headers string: prepend User-Agent if custom, then user headers */
    char ua_headers[8192];
    /* Build auth header if auth param provided */
    char auth_headers[4096];
    const char *default_headers = "Accept: text/html,application/json";
    const char *use_headers;

    if (auth_copy && auth_copy[0]) {
        char *encoded = base64_encode((const unsigned char *)auth_copy, strlen(auth_copy));
        if (encoded) {
            if (ua_copy && ua_copy[0]) {
                if (headers_copy && headers_copy[0]) {
                    snprintf(auth_headers, sizeof(auth_headers),
                             "User-Agent: %s\r\nAuthorization: Basic %s\r\n%s",
                             ua_copy, encoded, headers_copy);
                } else {
                    snprintf(auth_headers, sizeof(auth_headers),
                             "User-Agent: %s\r\nAuthorization: Basic %s",
                             ua_copy, encoded);
                }
            } else {
                if (headers_copy && headers_copy[0]) {
                    snprintf(auth_headers, sizeof(auth_headers),
                             "Authorization: Basic %s\r\n%s", encoded, headers_copy);
                } else {
                    snprintf(auth_headers, sizeof(auth_headers),
                             "Authorization: Basic %s", encoded);
                }
            }
            free(encoded);
            use_headers = auth_headers;
        } else {
            use_headers = headers_copy ? headers_copy : default_headers;
        }
    } else if (ua_copy && ua_copy[0]) {
        if (headers_copy && headers_copy[0]) {
            snprintf(ua_headers, sizeof(ua_headers),
                     "User-Agent: %s\r\n%s", ua_copy, headers_copy);
        } else {
            snprintf(ua_headers, sizeof(ua_headers),
                     "User-Agent: %s", ua_copy);
        }
        use_headers = ua_headers;
    } else {
        use_headers = headers_copy ? headers_copy : default_headers;
    }
    /* Inject Cookie header if cookies param provided */
    char cookie_headers[4096];
    const char *final_headers = use_headers;
    if (cookies_copy && cookies_copy[0]) {
        snprintf(cookie_headers, sizeof(cookie_headers),
                 "Cookie: %s\r\n%s", cookies_copy,
                 use_headers ? use_headers : "");
        final_headers = cookie_headers;
    }
    http_response_t *resp = http_request(client, method, url,
                                          final_headers,
                                          body_copy,
                                          body_copy ? strlen(body_copy) : 0);

    if (!resp) {
        http_client_free(client);
        free(headers_copy);
        free(body_copy);
        free(proxy_copy);
        free(cookies_copy);
        free(auth_type_copy);
        free(save_path_copy);
        free(ua_copy);
        free(url_copy);
        return strdup("{\"error\":\"HTTP request failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "url", json_new_string(url_copy));
    json_object_set(result, "status_code", json_new_number((double)resp->status));
    json_object_set(result, "headers", json_new_string(resp->headers ? resp->headers : ""));
    if (save_path_copy && save_path_copy[0]) {
        /* Save response body to file */
        FILE *f = fopen(save_path_copy, "wb");
        if (f && resp->body) {
            size_t written = fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            json_object_set(result, "saved_to", json_new_string(save_path_copy));
            json_object_set(result, "saved_size", json_new_number((double)written));
            json_object_set(result, "body", json_new_string("[saved to file]"));
        } else {
            if (f) fclose(f);
            json_object_set(result, "save_error", json_new_string("Cannot write to save_path"));
            json_object_set(result, "body", json_new_string(resp->body ? resp->body : ""));
        }
    } else if (include_body_val) {
        json_object_set(result, "body", json_new_string(resp->body ? resp->body : ""));
        json_object_set(result, "body_length", json_new_number((double)resp->body_len));
    }

    /* Save updated cookies to jar if enabled */
    if (cookie_jar && cookie_jar[0] && resp && resp->headers) {
        cookie_jar_update(cookie_jar, resp->headers);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    http_response_free(resp);
    http_client_free(client);
    free(headers_copy);
    free(body_copy);
    free(auth_copy);
    free(proxy_copy);
    free(cookies_copy);
    free(ua_copy);
    free(save_path_copy);
    free(url_copy);
    return json_out;
}

/* Web search schema */
static const char *SCHEMA_SEARCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
      "\"backend\":{\"type\":\"string\",\"description\":\"Search backend: searxng/google/brave/tavily/firecrawl/xai (default: config)\"},"
      "\"count\":{\"type\":\"number\",\"description\":\"Number of results (default: 5)\",\"default\":5},"
      "\"lang\":{\"type\":\"string\",\"description\":\"Language filter (e.g., en, de, fr, zh). Supported by searxng backend.\"}"
    "},"
    "\"required\":[\"query\"]"
"}";

/* ================================================================
 *  Web search handler (F16-F20)
 * ================================================================ */

/* Perform a SearXNG search */
static char *search_searxng(http_client_t *client, const char *query, int count, const char *lang) {
    const char *base_url = tool_config_get("searxng", "base_url");
    if (!base_url) base_url = getenv("SEARXNG_BASE_URL");
    if (!base_url) return strdup("{\"error\":\"SearXNG: SEARXNG_BASE_URL not set\"}");

    char url[1024];
    int n = snprintf(url, sizeof(url), "%s/search?q=%s&format=json&limit=%d",
             base_url, query, count);
    if (lang && lang[0]) {
        snprintf(url + n, sizeof(url) - n, "&lang=%s", lang);
    }

    http_response_t *resp = http_request(client, HTTP_GET, url, NULL, NULL, 0);
    if (!resp) return strdup("{\"error\":\"SearXNG: request failed\"}");

    /* Parse JSON response */
    char *out = NULL;
    if (resp->body) {
        /* Pass through the JSON results */
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("searxng"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"SearXNG: empty response\"}");
}

/* Perform a Google Custom Search */
static char *search_google(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("google", "api_key");
    const char *cx = tool_config_get("google", "cx");
    if (!api_key) api_key = getenv("GOOGLE_API_KEY");
    if (!cx) cx = getenv("GOOGLE_CX");
    if (!api_key || !cx) return strdup("{\"error\":\"Google: GOOGLE_API_KEY and GOOGLE_CX required\"}");

    char url[2048];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%d",
             api_key, cx, query, count > 10 ? 10 : count);

    http_response_t *resp = http_request(client, HTTP_GET, url, NULL, NULL, 0);
    if (!resp) return strdup("{\"error\":\"Google: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("google"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Google: empty response\"}");
}

/* Perform a Brave Search */
static char *search_brave(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("brave", "api_key");
    if (!api_key) api_key = getenv("BRAVE_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Brave: BRAVE_API_KEY not set\"}");

    char url[1024];
    snprintf(url, sizeof(url),
             "https://api.search.brave.com/res/v1/web/search?q=%s&count=%d",
             query, count);

    char headers[256];
    snprintf(headers, sizeof(headers), "Accept: application/json, X-Subscription-Token: %s", api_key);

    http_response_t *resp = http_request(client, HTTP_GET, url, headers, NULL, 0);
    if (!resp) return strdup("{\"error\":\"Brave: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("brave"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Brave: empty response\"}");
}

/* Perform a Tavily search */
static char *search_tavily(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("tavily", "api_key");
    if (!api_key) api_key = getenv("TAVILY_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Tavily: TAVILY_API_KEY not set\"}");

    json_node_t *body = json_new_object();
    json_object_set(body, "api_key", json_new_string(api_key));
    json_object_set(body, "query", json_new_string(query));
    json_object_set(body, "max_results", json_new_number((double)count));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request_json(client, HTTP_POST,
        "https://api.tavily.com/search", payload);
    free(payload);

    if (!resp) return strdup("{\"error\":\"Tavily: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("tavily"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Tavily: empty response\"}");
}

/* Perform a Firecrawl scrape */
static char *search_firecrawl(http_client_t *client, const char *query, int count) {
    (void)count;
    const char *api_key = tool_config_get("firecrawl", "api_key");
    if (!api_key) api_key = getenv("FIRECRAWL_API_KEY");
    if (!api_key) return strdup("{\"error\":\"Firecrawl: FIRECRAWL_API_KEY not set\"}");

    /* Firecrawl is URL-based scraping — treat query as URL or use search endpoint */
    json_node_t *body = json_new_object();
    json_object_set(body, "url", json_new_string(query));

    char *payload = json_serialize(body);
    json_free(body);

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s, Content-Type: application/json", api_key);

    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.firecrawl.dev/v1/scrape", auth_header, payload, strlen(payload));
    free(payload);

    if (!resp) return strdup("{\"error\":\"Firecrawl: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("firecrawl"));
        json_object_set(result, "url", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"Firecrawl: empty response\"}");
}

/* L03: xAI Web Search via Responses API */
static char *search_xai(http_client_t *client, const char *query, int count) {
    const char *api_key = tool_config_get("xai", "api_key");
    if (!api_key) api_key = getenv("XAI_API_KEY");
    if (!api_key) return strdup("{\"error\":\"xAI: XAI_API_KEY not set\"}");

    char headers[256];
    snprintf(headers, sizeof(headers), "Authorization: Bearer %s\r\nContent-Type: application/json", api_key);

    json_node_t *body = json_new_object();
    json_object_set(body, "model", json_new_string("grok-4.3"));
    json_object_set(body, "input", json_new_string(query));
    json_t *tools = json_new_array();
    json_t *ws_tool = json_new_object();
    json_object_set(ws_tool, "type", json_new_string("web_search"));
    json_array_append(tools, ws_tool);
    json_object_set(body, "tools", tools);
    json_object_set(body, "max_results", json_new_number((double)count));

    char *payload = json_serialize(body);
    json_free(body);

    http_response_t *resp = http_request(client, HTTP_POST,
        "https://api.x.ai/v1/responses", headers, payload, strlen(payload));
    free(payload);

    if (!resp) return strdup("{\"error\":\"xAI: request failed\"}");

    char *out = NULL;
    if (resp->body) {
        json_node_t *result = json_new_object();
        json_object_set(result, "backend", json_new_string("xai"));
        json_object_set(result, "query", json_new_string(query));
        json_object_set(result, "results", json_new_string(resp->body));
        out = json_serialize(result);
        json_free(result);
    }
    http_response_free(resp);
    return out ? out : strdup("{\"error\":\"xAI: empty response\"}");
}

/* Main web_search handler — dispatches to configured backend */
char *web_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *query = json_object_get_string(args, "query", NULL);
    if (!query) { json_free(args); return strdup("{\"error\":\"Missing query\"}"); }

    /* URL-encode query for GET parameters */
    char encoded_query[2048];
    int ei = 0;
    for (int i = 0; query[i] && ei < (int)sizeof(encoded_query) - 4; i++) {
        if (isalnum((unsigned char)query[i]) || query[i] == '-' || query[i] == '_' ||
            query[i] == '.' || query[i] == '~') {
            encoded_query[ei++] = query[i];
        } else if (query[i] == ' ') {
            encoded_query[ei++] = '+';
        } else {
            ei += snprintf(encoded_query + ei, sizeof(encoded_query) - ei,
                          "%%%02X", (unsigned char)query[i]);
        }
    }
    encoded_query[ei] = '\0';

    int count = (int)json_object_get_number(args, "count", 5);
    const char *backend_ptr = json_object_get_string(args, "backend", NULL);
    const char *lang_ptr = json_object_get_string(args, "lang", NULL);
    char backend_buf[64] = "";
    if (backend_ptr) {
        snprintf(backend_buf, sizeof(backend_buf), "%s", backend_ptr);
    }
    char lang_buf[16] = "";
    if (lang_ptr) {
        snprintf(lang_buf, sizeof(lang_buf), "%s", lang_ptr);
    }
    json_free(args);

    /* Resolve backend: arg → config → default */
    const char *backend = backend_buf[0] ? backend_buf : NULL;
    if (!backend) {
        backend = tool_config_get("web", "search_backend");
        if (!backend) backend = "searxng"; /* default */
    }

    http_client_t *client = http_client_new(30);

    char *result = NULL;
    if (strcmp(backend, "google") == 0)
        result = search_google(client, encoded_query, count);
    else if (strcmp(backend, "brave") == 0)
        result = search_brave(client, encoded_query, count);
    else if (strcmp(backend, "tavily") == 0)
        result = search_tavily(client, query, count); /* Tavily POST needs raw query */
    else if (strcmp(backend, "firecrawl") == 0)
        result = search_firecrawl(client, query, count);
    else if (strcmp(backend, "xai") == 0)
        result = search_xai(client, query, count);
    else /* searxng default */
        result = search_searxng(client, encoded_query, count, lang_buf);

    http_client_free(client);
    return result ? result : strdup("{\"error\":\"Search failed\"}");
}

/* ================================================================
 *  F21: Web extract handler with LLM depth
 * ================================================================ */

static const char *SCHEMA_EXTRACT = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"url\":{\"type\":\"string\",\"description\":\"URL to extract content from (single URL)\"},"
      "\"urls\":{\"type\":\"array\",\"description\":\"List of URLs to extract content from (multi-URL mode)\",\"items\":{\"type\":\"string\"}},"
      "\"prompt\":{\"type\":\"string\",\"description\":\"What to extract from the page (e.g., 'key metrics', 'main arguments', 'pricing info')\",\"default\":\"Extract key information\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":30},"
      "\"format\":{\"type\":\"string\",\"description\":\"Output format: 'markdown' or 'html'\",\"default\":\"markdown\"}"
    "},"
    "\"required\":[]"
"}";

/* Forward declaration for delegate-based extraction (defined below) */
static char *web_extract_delegate(const char *url, const char *extract_prompt, int timeout, const char *format);

/* Native HTML-to-text extraction — no Python dependency */
static char *web_extract_native(const char *url, int timeout) {
    http_t *http = http_new(timeout);
    if (!http) return strdup("{\"error\":\"Failed to create HTTP client\"}");

    http_resp_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        const char *err = resp ? "HTTP error fetching URL" : "Connection failed";
        if (resp) http_resp_free(resp);
        http_free(http);
        json_t *rj = json_object();
        json_set(rj, "url", json_string(url));
        json_set(rj, "error", json_string(err));
        char *out = json_serialize(rj);
        json_free(rj);
        return out;
    }

    char *body = resp->body && resp->body_len > 0 ? strndup(resp->body, resp->body_len) : NULL;
    http_resp_free(resp);
    http_free(http);

    if (!body) return strdup("{\"error\":\"Empty response body\"}");

    /* Strip HTML tags to get clean text */
    char *clean = html_strip_tags(body);
    free(body);
    if (!clean) return strdup("{\"error\":\"HTML stripping failed\"}");

    /* Trim whitespace and collapse blank lines */
    char *src = clean, *dst = clean;
    while (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r') src++;
    bool prev_blank = false;
    while (*src) {
        if (*src == '\n') {
            *dst++ = '\n';
            src++;
            while (*src == '\r') src++;
            prev_blank = true;
        } else if (*src == ' ' || *src == '\t') {
            *dst++ = ' ';
            src++;
        } else {
            if (prev_blank && *src == '\n') { src++; continue; }
            prev_blank = false;
            *dst++ = *src++;
        }
    }
    while (dst > clean && (dst[-1] == ' ' || dst[-1] == '\n' || dst[-1] == '\r')) dst--;
    *dst = '\0';

    /* Truncate at 100KB */
    if (strlen(clean) > 102400) clean[102400] = '\0';

    json_t *result = json_object();
    json_set(result, "url", json_string(url));
    json_set(result, "text", json_string(clean));
    json_set(result, "method", json_string("native"));
    json_set(result, "length", json_number((double)strlen(clean)));
    char *json_out = json_serialize(result);
    json_free(result);
    free(clean);
    return json_out;
}

char *web_extract_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse error\"}");

    const char *url = json_get_str(args, "url", NULL);
    json_t *urls_node = json_has(args, "urls") ? json_obj_get(args, "urls") : NULL;
    const char *extract_prompt = json_get_str(args, "prompt", "Extract key information from this page");
    int timeout = (int)json_get_num(args, "timeout", 30);
    const char *format = json_get_str(args, "format", "markdown");
    bool is_custom_prompt = (extract_prompt && strcmp(extract_prompt, "Extract key information from this page") != 0);

    /* Multi-URL mode: urls array */
    if (urls_node && urls_node->type == JSON_ARRAY && json_len(urls_node) > 0) {
        size_t count = json_len(urls_node);
        json_t *results = json_object();
        json_t *items = json_array();
        json_set(results, "results", items);
        json_set(results, "count", json_number((double)count));

        for (size_t i = 0; i < count; i++) {
            json_t *item_node = json_get(urls_node, i);
            const char *single_url = (item_node && item_node->type == JSON_STRING) ? item_node->str_val : NULL;

            json_t *entry = json_object();
            json_set(entry, "url", json_string(single_url ? single_url : "(null)"));

            if (!single_url) {
                json_set(entry, "error", json_string("URL entry is null or not a string"));
            } else {
                const char *sf = url_has_secret(single_url);
                if (sf) {
                    char err[256];
                    snprintf(err, sizeof(err),
                             "Blocked: URL contains what appears to be a %s API key", sf);
                    json_set(entry, "error", json_string(err));
                } else {
                    char *content;
                    if (is_custom_prompt)
                        content = web_extract_delegate(single_url, extract_prompt, timeout, format);
                    else
                        content = web_extract_native(single_url, timeout);

                    if (content) {
                        json_t *parsed = json_parse(content, NULL);
                        if (parsed) {
                            json_set(entry, "data", parsed);
                            json_free(parsed);
                        } else {
                            json_set(entry, "raw", json_string(content));
                        }
                        free(content);
                    }
                }
            }
            json_append(items, entry);
            json_free(entry);
        }

        json_free(items);
        char *result = json_serialize(results);
        json_free(results);
        json_free(args);
        return result;
    }

    json_free(args);

    /* Single URL mode (backward compatible) */
    if (!url) return strdup("{\"error\":\"Missing url. Provide 'url' (string) or 'urls' (array).\"}");

    /* Secret exfiltration prevention */
    {
        const char *secret_found = url_has_secret(url);
        if (secret_found) {
            char result[256];
            snprintf(result, sizeof(result),
                     "{\"success\":false,\"error\":\"Blocked: URL contains what appears to be a %s API key. Secrets must not be sent in URLs.\"}",
                     secret_found);
            return strdup(result);
        }
    }

    if (is_custom_prompt)
        return web_extract_delegate(url, extract_prompt, timeout, format);

    return web_extract_native(url, timeout);
}

/* Legacy: Python delegate for LLM-based extraction with prompt */
static char *web_extract_delegate(const char *url, const char *extract_prompt, int timeout, const char *format) {
    /* Build delegate script path */
    char script_path[1024];
    const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                       getenv("HOME") ? getenv("HOME") : ".";
    snprintf(script_path, sizeof(script_path),
             "%s/hermes-agent-dev/C/scripts/web_extract_delegate.py", home);

    /* Check if script exists at project path; fallback to home scripts dir */
    struct stat st;
    if (stat(script_path, &st) != 0) {
        snprintf(script_path, sizeof(script_path),
                 "%s/.hermes/scripts/web_extract_delegate.py", home);
    }

    /* Build JSON input for the delegate */
    json_t *input = json_object();
    json_set(input, "url", json_string(url));
    json_set(input, "prompt", json_string(extract_prompt));
    json_set(input, "timeout", json_number((double)timeout));
    json_set(input, "format", json_string(format));
    char *input_json = json_serialize(input);
    json_free(input);

    /* Build command */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "echo '%s' | python3 '%s' 2>/dev/null",
             input_json, script_path);
    free(input_json);

    /* Execute via popen */
    FILE *fp = popen(cmd, "r");
    if (!fp) return strdup("{\"error\":\"Failed to run extractor\"}");

    /* Read output */
    size_t cap = 16384, len = 0;
    char *output = (char *)malloc(cap);
    if (!output) { pclose(fp); return strdup("{\"error\":\"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t add = strlen(line);
        if (len + add + 1 > cap) {
            cap *= 2;
            output = realloc(output, cap);
            if (!output) { pclose(fp); return strdup("{\"error\":\"OOM\"}"); }
        }
        memcpy(output + len, line, add + 1);
        len += add;
    }
    int rc = pclose(fp);

    if (rc != 0 || !output[0]) {
        free(output);
        return strdup("{\"error\":\"Extraction failed\"}");
    }

    /* Trim trailing newline */
    char *end = output + strlen(output) - 1;
    while (end > output && (*end == '\n' || *end == '\r')) *end-- = '\0';

    /* Try to parse as JSON from delegate */
    json_t *result = json_parse(output, NULL);
    if (result) {
        char *json_out = json_serialize(result);
        json_free(result);
        free(output);
        return json_out;
    }

    /* Fallback: wrap raw output in result */
    json_t *fallback = json_object();
    json_set(fallback, "url", json_string(url));
    json_set(fallback, "extraction", json_string(output));
    char *json_out = json_serialize(fallback);
    json_free(fallback);
    free(output);
    return json_out;
}

/* Auto-registration */
void registry_init_web(void) {
    registry_register("web_get",
        "Fetch a URL via HTTP GET. Returns status code and body content.",
        SCHEMA_GET, web_get_handler);
    registry_register("web_search",
        "Search the web. Supports backends: searxng, google, brave, tavily, firecrawl.",
        SCHEMA_SEARCH, web_search_handler);
    registry_register("web_extract",
        "Extract structured content from a web page using LLM. "
        "Fetches URL, sends content to LLM with extraction prompt. "
        "Supports custom extraction instructions via 'prompt' parameter.",
        SCHEMA_EXTRACT, web_extract_handler);
}
