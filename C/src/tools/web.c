/*
 * web.c — Web tools for Hermes C.
 * HTTP GET requests via raw sockets.
 * Web search via DuckDuckGo Instant Answer API (no API key required).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  URL percent-encoding
 * ================================================================ */

static char url_encode_buf[8192];

static const char *url_encode(const char *s) {
    if (!s) return "";
    size_t j = 0;
    const char *hex = "0123456789ABCDEF";
    for (const char *p = s; *p && j < sizeof(url_encode_buf) - 3; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            url_encode_buf[j++] = c;
        } else if (c == ' ') {
            url_encode_buf[j++] = '+';
        } else {
            url_encode_buf[j++] = '%';
            url_encode_buf[j++] = hex[c >> 4];
            url_encode_buf[j++] = hex[c & 0xf];
        }
    }
    url_encode_buf[j] = '\0';
    return url_encode_buf;
}

/* ================================================================
 *  Schema definitions
 * ================================================================ */

static const char *SCHEMA_GET = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"url\":{\"type\":\"string\",\"description\":\"URL to fetch\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":30}"
    "},"
    "\"required\":[\"url\"]"
"}";

static const char *SCHEMA_SEARCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
      "\"max_results\":{\"type\":\"number\",\"description\":\"Max results to return\",\"default\":5},"
      "\"search_url\":{\"type\":\"string\",\"description\":\"Override search URL (use {query} placeholder)\"}"
    "},"
    "\"required\":[\"query\"]"
"}";

/* ================================================================
 *  HTTP GET handler
 * ================================================================ */

char *web_get_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *url = json_object_get_string(args, "url", NULL);
    int timeout = (int)json_object_get_number(args, "timeout", 30);

    json_free(args);

    if (!url) return strdup("{\"error\":\"Missing url\"}");

    http_client_t *client = http_client_new(timeout);
    http_response_t *resp = http_request(client, HTTP_GET, url,
                                          "Accept: text/html,application/json", NULL, 0);

    if (!resp) {
        http_client_free(client);
        return strdup("{\"error\":\"HTTP request failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "url", json_new_string(url));
    json_object_set(result, "status_code", json_new_number((double)resp->status_code));
    json_object_set(result, "body", json_new_string(resp->body ? resp->body : ""));
    json_object_set(result, "body_length", json_new_number((double)resp->body_len));

    char *json_out = json_serialize(result);
    json_free(result);
    http_response_free(resp);
    http_client_free(client);
    return json_out;
}

/* ================================================================
 *  Web search handler (DuckDuckGo Instant Answer API)
 * ================================================================ */

#define DDG_API_URL "https://api.duckduckgo.com/?q=%s&format=json&no_html=1&skip_disambig=1"

/* Extract text from a DDG topic result (handles nested Topics) */
static void ddg_extract_results(json_node_t *topics, json_node_t *out_arr, int *count, int max) {
    if (!topics || json_array_count(topics) == 0) return;
    int n = json_array_count(topics);
    for (int i = 0; i < n && *count < max; i++) {
        json_node_t *topic = json_array_get(topics, i);
        if (!topic) continue;

        const char *text = json_object_get_string(topic, "Text", NULL);
        const char *url = json_object_get_string(topic, "FirstURL", NULL);

        if (text && url && *count < max) {
            json_node_t *r = json_new_object();
            json_object_set(r, "title", json_new_string(text));
            json_object_set(r, "url", json_new_string(url));
            json_array_append(out_arr, r);
            (*count)++;
        }

        /* Nested topics (subcategories) */
        json_node_t *sub = json_object_get(topic, "Topics");
        if (sub) {
            ddg_extract_results(sub, out_arr, count, max);
        }
    }
}

char *web_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *query = json_object_get_string(args, "query", NULL);
    int max_results = (int)json_object_get_number(args, "max_results", 5);
    const char *search_url = json_object_get_string(args, "search_url", NULL);
    json_free(args);

    if (!query || !query[0])
        return strdup("{\"error\":\"Missing query\"}");

    if (max_results < 1) max_results = 1;
    if (max_results > 20) max_results = 20;

    /* Build search URL */
    char url[1024];
    if (search_url && search_url[0]) {
        /* Custom search template — replace {query} with encoded query */
        const char *enc = url_encode(query);
        const char *ph = strstr(search_url, "{query}");
        if (ph) {
            size_t prefix_len = (size_t)(ph - search_url);
            size_t total = prefix_len + strlen(enc) + strlen(ph + 7) + 1;
            if (total > sizeof(url)) total = sizeof(url);
            snprintf(url, sizeof(url), "%.*s%s%s", (int)prefix_len, search_url, enc, ph + 7);
        } else {
            snprintf(url, sizeof(url), "%s&q=%s", search_url, enc);
        }
    } else {
        /* Default: DuckDuckGo Instant Answer API */
        snprintf(url, sizeof(url), DDG_API_URL, url_encode(query));
    }

    /* Make HTTP request */
    http_client_t *client = http_client_new(15);
    http_response_t *resp = http_request(client, HTTP_GET, url, NULL, NULL, 0);

    if (!resp) {
        http_client_free(client);
        return strdup("{\"error\":\"Search request failed\"}");
    }

    /* Build JSON result */
    json_node_t *result = json_new_object();
    json_object_set(result, "query", json_new_string(query));
    json_object_set(result, "status_code", json_new_number((double)resp->status_code));

    /* Parse DDG JSON response */
    char *json_err = NULL;
    json_node_t *ddg = json_parse(resp->body ? resp->body : "", &json_err);

    if (ddg) {
        /* Abstract / summary */
        const char *abstract = json_object_get_string(ddg, "AbstractText", NULL);
        if (abstract && abstract[0]) {
            json_object_set(result, "abstract", json_new_string(abstract));
            const char *src = json_object_get_string(ddg, "AbstractSource", NULL);
            const char *src_url = json_object_get_string(ddg, "AbstractURL", NULL);
            if (src) json_object_set(result, "abstract_source", json_new_string(src));
            if (src_url) json_object_set(result, "abstract_url", json_new_string(src_url));
        }

        /* Definition */
        const char *def = json_object_get_string(ddg, "Definition", NULL);
        if (def && def[0]) {
            json_object_set(result, "definition", json_new_string(def));
        }

        /* Heading */
        const char *heading = json_object_get_string(ddg, "Heading", NULL);
        if (heading && heading[0]) {
            json_object_set(result, "heading", json_new_string(heading));
        }

        /* Extract results */
        json_node_t *results_arr = json_new_array();
        int count = 0;

        /* Main Results */
        json_node_t *main_results = json_object_get(ddg, "Results");
        if (main_results) {
            ddg_extract_results(main_results, results_arr, &count, max_results);
        }

        /* Related Topics */
        if (count < max_results) {
            json_node_t *related = json_object_get(ddg, "RelatedTopics");
            if (related) {
                ddg_extract_results(related, results_arr, &count, max_results);
            }
        }

        json_object_set(result, "results", results_arr);
        json_object_set(result, "result_count", json_new_number((double)count));
        json_free(ddg);
    } else {
        /* Non-JSON response — return raw body snippet */
        json_object_set(result, "raw_response", json_new_string(
            resp->body ? resp->body : ""));
        free(json_err);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    http_response_free(resp);
    http_client_free(client);
    return json_out;
}

/* ================================================================
 *  Auto-registration
 * ================================================================ */

void registry_init_web(void) {
    registry_register("web_get",
        "Fetch a URL via HTTP GET. Returns status code and body content.",
        SCHEMA_GET, web_get_handler);
    registry_register("web_search",
        "Search the web. Uses DuckDuckGo Instant Answer API (no API key required). "
        "Returns abstract, definition, and search results with title+URL. "
        "Supports custom search URL with {query} placeholder.",
        SCHEMA_SEARCH, web_search_handler);
}