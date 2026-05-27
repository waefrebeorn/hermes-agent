/*
 * web.c — Web tools for Hermes C.
 * HTTP GET requests via libcurl or raw sockets.
 * Web search interface (uses http client).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_tool_config.h"
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
      "\"proxy\":{\"type\":\"string\",\"description\":\"HTTP proxy URL (e.g., http://proxy:8080). Uses CONNECT tunnel for HTTPS.\"}"
    "},"
    "\"required\":[\"url\"]"
"\"}";

/* Resolve HTTP method string to enum */
static http_method_t method_str_to_enum(const char *method) {
    if (!method) return HTTP_GET;
    if (strcasecmp(method, "POST") == 0) return HTTP_POST;
    if (strcasecmp(method, "PUT") == 0) return HTTP_PUT;
    if (strcasecmp(method, "DELETE") == 0) return HTTP_DELETE;
    return HTTP_GET;
}

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
    const char *proxy = json_object_get_string(args, "proxy", NULL);

    /* Strdup values that survive json_free */
    char *headers_copy = headers_str ? strdup(headers_str) : NULL;
    char *body_copy = body ? strdup(body) : NULL;
    char *proxy_copy = proxy ? strdup(proxy) : NULL;
    char method_buf[16];
    snprintf(method_buf, sizeof(method_buf), "%s", method_str ? method_str : "GET");
    http_method_t method = method_str_to_enum(method_buf);

    json_free(args);

    if (!url) {
        free(headers_copy);
        free(body_copy);
        free(proxy_copy);
        return strdup("{\"error\":\"Missing url\"}");
    }

    http_client_t *client = http_client_new(timeout);
    if (proxy_copy && proxy_copy[0]) {
        http_client_set_proxy(client, proxy_copy);
    }
    const char *default_headers = "Accept: text/html,application/json";
    const char *use_headers = headers_copy ? headers_copy : default_headers;
    http_response_t *resp = http_request(client, method, url,
                                          use_headers,
                                          body_copy,
                                          body_copy ? strlen(body_copy) : 0);

    if (!resp) {
        http_client_free(client);
        free(headers_copy);
        free(body_copy);
        free(proxy_copy);
        return strdup("{\"error\":\"HTTP request failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "url", json_new_string(url));
    json_object_set(result, "status_code", json_new_number((double)resp->status));
    json_object_set(result, "body", json_new_string(resp->body ? resp->body : ""));
    json_object_set(result, "body_length", json_new_number((double)resp->body_len));

    char *json_out = json_serialize(result);
    json_free(result);
    http_response_free(resp);
    http_client_free(client);
    free(headers_copy);
    free(body_copy);
    free(proxy_copy);
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
      "\"url\":{\"type\":\"string\",\"description\":\"URL to extract content from\"},"
      "\"prompt\":{\"type\":\"string\",\"description\":\"What to extract from the page (e.g., 'key metrics', 'main arguments', 'pricing info')\",\"default\":\"Extract key information\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":30}"
    "},"
    "\"required\":[\"url\"]"
"}";

char *web_extract_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"error\":\"JSON parse\"}");

    const char *url = json_get_str(args, "url", NULL);
    const char *extract_prompt = json_get_str(args, "prompt", "Extract key information from this page");
    int timeout = (int)json_get_num(args, "timeout", 30);

    json_free(args);

    if (!url) return strdup("{\"error\":\"Missing url\"}");

    /* Build delegate script path relative to project root or HERMES_HOME */
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
