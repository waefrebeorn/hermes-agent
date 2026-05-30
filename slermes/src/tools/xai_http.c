/*
 * xai_http.c — xAI HTTP tool for Hermes C.
 * Exposes xAI API credential resolution and URL building as a tool.
 * Wraps libxai_http functions for agent-accessible credential/URL queries.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "xai_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"Action: check_credentials (default), get_api_key, get_base_url\",\"default\":\"check_credentials\"}"
    "},"
    "\"required\":[]"
"}";

static char *xai_http_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "check_credentials");

    json_node_t *result = json_new_object();

    if (strcmp(action, "check_credentials") == 0) {
        bool has_creds = xai_has_credentials();
        json_object_set(result, "has_credentials", json_new_bool(has_creds));
        if (has_creds) {
            char key_buf[256]; key_buf[0] = '\0';
            bool has_key = xai_get_api_key(key_buf);
            char url_buf[256];
            xai_get_base_url(url_buf);
            const char *key = has_key ? key_buf : NULL;
            const char *url = url_buf;
            json_object_set(result, "api_key_available", json_new_bool(key && key[0]));
            if (key) {
                /* Mask key for display */
                size_t klen = strlen(key);
                char masked[128];
                if (klen > 8) {
                    snprintf(masked, sizeof(masked), "%.*s...%s", 4, key, key + klen - 4);
                } else {
                    snprintf(masked, sizeof(masked), "****");
                }
                json_object_set(result, "api_key_masked", json_new_string(masked));
            }
            json_object_set(result, "base_url", json_new_string(url && url[0] ? url : "https://api.x.ai/v1"));
        }
        json_object_set(result, "user_agent", json_new_string(xai_user_agent()));
    } else if (strcmp(action, "get_api_key") == 0) {
        char key_buf2[256]; key_buf2[0] = '\0';
        bool has_key2 = xai_get_api_key(key_buf2);
        if (has_key2 && key_buf2[0]) {
            json_object_set(result, "api_key_available", json_new_bool(true));
        } else {
            json_object_set(result, "api_key_available", json_new_bool(false));
            json_object_set(result, "warning", json_new_string("xAI API key not configured"));
        }
    } else if (strcmp(action, "get_base_url") == 0) {
        char url_buf2[256];
        xai_get_base_url(url_buf2);
        json_object_set(result, "base_url", json_new_string(url_buf2[0] ? url_buf2 : XAI_DEFAULT_BASE_URL));
    } else {
        json_object_set(result, "error", json_new_string("Unknown action"));
    }

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_xai_http(void) {
    registry_register("xai_http",
        "Query xAI API configuration: check credentials availability, "
        "get masked API key status, base URL, and user agent string. "
        "Actions: check_credentials, get_api_key, get_base_url.",
        SCHEMA, xai_http_handler);
}
