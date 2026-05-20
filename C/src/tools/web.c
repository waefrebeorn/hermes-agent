/*
 * web.c — Web tools for Hermes C.
 * HTTP GET requests via libcurl or raw sockets.
 * Web search interface (uses http client).
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Schema */
static const char *SCHEMA_GET = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"url\":{\"type\":\"string\",\"description\":\"URL to fetch\"},"
      "\"timeout\":{\"type\":\"number\",\"description\":\"Timeout in seconds\",\"default\":30}"
    "},"
    "\"required\":[\"url\"]"
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

/* Auto-registration */
void registry_init_web(void) {
    registry_register("web_get",
        "Fetch a URL via HTTP GET. Returns status code and body content.",
        SCHEMA_GET, web_get_handler);
    registry_register("web_search",
        "Search the web (calls web_get internally). Uses a search URL.",
        SCHEMA_GET, web_get_handler);
}
