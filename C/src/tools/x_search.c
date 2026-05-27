/*
 * x_search.c — xAI X/Twitter search tool for Hermes C.
 * Searches X/Twitter via xAI's Responses API with the x_search tool.
 * Requires XAI_API_KEY environment variable.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define XAI_BASE_URL "https://api.x.ai/v1"
#define DEFAULT_MODEL "grok-4.20-reasoning"

/* Schema */
static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search query for X/Twitter\"},"
      "\"allowed_x_handles\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Only search posts from these handles\"},"
      "\"excluded_x_handles\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"description\":\"Exclude posts from these handles\"},"
      "\"from_date\":{\"type\":\"string\",\"description\":\"Start date YYYY-MM-DD\"},"
      "\"to_date\":{\"type\":\"string\",\"description\":\"End date YYYY-MM-DD\"},"
      "\"enable_image_understanding\":{\"type\":\"boolean\",\"description\":\"Analyze images in matching posts\",\"default\":false},"
      "\"enable_video_understanding\":{\"type\":\"boolean\",\"description\":\"Analyze videos in matching posts\",\"default\":false},"
      "\"media_filter\":{\"type\":\"string\",\"description\":\"Filter by media type: 'images', 'videos', 'news', 'links', or empty for all\"}"
    "},"
    "\"required\":[\"query\"]"
"}";

/* ================================================================
 *  Extract response text from xAI response payload
 * ================================================================ */

static char *extract_response_text(json_node_t *payload) {
    /* Try output_text first */
    const char *output_text = json_get_str(payload, "output_text", NULL);
    if (output_text && *output_text)
        return strdup(output_text);

    /* Fall through to output array */
    json_node_t *output = json_obj_get(payload, "output");
    if (!output) return NULL;

    size_t n = json_len(output);
    for (size_t i = 0; i < n; i++) {
        json_node_t *item = json_get(output, i);
        const char *type = json_get_str(item, "type", "");
        if (strcmp(type, "message") != 0) continue;

        json_node_t *content = json_obj_get(item, "content");
        if (!content) continue;

        size_t cn = json_len(content);
        for (size_t j = 0; j < cn; j++) {
            json_node_t *citem = json_get(content, j);
            const char *ctype = json_get_str(citem, "type", "");
            if (strcmp(ctype, "output_text") != 0 &&
                strcmp(ctype, "text") != 0) continue;

            const char *text = json_get_str(citem, "text", NULL);
            if (text && *text) return strdup(text);
        }
    }
    return NULL;
}

/* ================================================================
 *  Handler
 * ================================================================ */

char *x_search_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    /* Check for API key */
    const char *api_key = getenv("XAI_API_KEY");
    const char *base_url = getenv("XAI_BASE_URL");
    if (!base_url || !*base_url) base_url = XAI_BASE_URL;
    if (!api_key || !*api_key) {
        return strdup("{\"error\":\"XAI_API_KEY environment variable not set. "
                       "Set it to use the x_search tool.\"}");
    }

    /* Parse args */
    if (!args_json) return strdup("{\"error\":\"No args\"}");
    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse failed\"}"); }

    const char *query = json_get_str(args, "query", NULL);
    if (!query || !*query) {
        json_free(args);
        return strdup("{\"error\":\"Missing query\"}");
    }

    /* Build tool definition */
    json_node_t *tool_def = json_new_object();
    json_set(tool_def, "type", json_string("x_search"));

    const char *allowed = json_get_str(args, "allowed_x_handles", NULL);
    if (allowed) {
        json_set(tool_def, "allowed_x_handles",
                 json_string(allowed));
    }
    const char *excluded = json_get_str(args, "excluded_x_handles", NULL);
    if (excluded) {
        json_set(tool_def, "excluded_x_handles",
                 json_string(excluded));
    }
    const char *from_date = json_get_str(args, "from_date", NULL);
    if (from_date) json_set(tool_def, "from_date", json_string(from_date));
    const char *to_date = json_get_str(args, "to_date", NULL);
    if (to_date) json_set(tool_def, "to_date", json_string(to_date));

    if (json_get_bool(args, "enable_image_understanding", false))
        json_set(tool_def, "enable_image_understanding", json_bool(true));
    if (json_get_bool(args, "enable_video_understanding", false))
        json_set(tool_def, "enable_video_understanding", json_bool(true));
    const char *media_filter = json_get_str(args, "media_filter", NULL);
    if (media_filter && *media_filter)
        json_set(tool_def, "media_filter", json_string(media_filter));

    /* Build request payload */
    json_node_t *payload = json_new_object();
    json_set(payload, "model", json_string(DEFAULT_MODEL));

    json_node_t *input = json_new_array();
    json_node_t *msg = json_new_object();
    json_set(msg, "role", json_string("user"));
    json_set(msg, "content", json_string(query));
    json_append(input, msg);
    json_set(payload, "input", input);

    json_node_t *tools = json_new_array();
    json_append(tools, tool_def);
    json_set(payload, "tools", tools);

    json_set(payload, "store", json_bool(false));

    char *payload_str = json_serialize(payload);
    json_free(payload);
    json_free(args);

    if (!payload_str) return strdup("{\"error\":\"Failed to serialize request\"}");

    /* Make the API call */
    char url[2048];
    snprintf(url, sizeof(url), "%s/v1/responses", base_url);

    char headers[4096];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Content-Type: application/json\r\n"
             "User-Agent: WuBuHermes-C/1.0",
             api_key);

    http_client_t *client = http_client_new(180);
    http_response_t *resp = http_request(client, HTTP_POST, url,
                                          headers, payload_str, strlen(payload_str));
    free(payload_str);

    if (!resp) {
        http_client_free(client);
        return strdup("{\"error\":\"xAI API request failed\"}");
    }

    if (resp->status != 200) {
        char err_buf[4096];
        snprintf(err_buf, sizeof(err_buf),
                 "{\"error\":\"HTTP %d\",\"api_error\":%s}",
                 resp->status,
                 resp->body ? resp->body : "\"(no body)\"");
        http_response_free(resp);
        http_client_free(client);
        return strdup(err_buf);
    }

    /* Parse response */
    char *parse_err = NULL;
    json_node_t *root = json_parse(resp->body, &parse_err);
    if (!root) {
        char err_buf[4096];
        snprintf(err_buf, sizeof(err_buf),
                 "{\"error\":\"JSON parse: %s\"}", parse_err ? parse_err : "unknown");
        free(parse_err);
        http_response_free(resp);
        http_client_free(client);
        return strdup(err_buf);
    }

    /* Extract text */
    char *text = extract_response_text(root);
    if (!text) {
        /* Return raw response if no text extracted */
        text = strdup(resp->body ? resp->body : "{}");
    }

    /* Build result */
    json_node_t *result = json_new_object();
    json_set(result, "result", json_string(text));

    /* Extract citations if present */
    json_node_t *citations = json_obj_get(root, "citations");
    if (citations) {
        json_set(result, "citations", citations);
    }

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(root);
    free(text);
    http_response_free(resp);
    http_client_free(client);

    return json_out ? json_out : strdup("{\"error\":\"Output serialization failed\"}");
}

/* Auto-registration */
void registry_init_x_search(void) {
    registry_register("x_search",
        "Search X/Twitter via xAI's API. Returns search results with citations. "
        "Requires XAI_API_KEY environment variable to be set.",
        SCHEMA, x_search_handler);
}
