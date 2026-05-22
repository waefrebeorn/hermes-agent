/*
 * provider_openai.c — OpenAI-compatible provider implementation.
 * Supports: OpenAI, DeepSeek, OpenRouter, Groq, Together AI, etc.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  URL building
 * ================================================================ */

static char *openai_build_url(const provider_t *p, const char *base_url) {
    if (!base_url || !*base_url)
        base_url = "https://api.openai.com/v1";

    /* If URL already includes /chat/completions, use as-is */
    if (strstr(base_url, "/chat/completions")) {
        return strdup(base_url);
    }

    /* If URL ends with /v1, append chat/completions */
    size_t len = strlen(base_url);
    if (len > 3 && base_url[len-1] == '/' && base_url[len-2] == '1' && base_url[len-3] == 'v') {
        char *url = (char *)malloc(len + 20);
        if (url) snprintf(url, len + 20, "%schat/completions", base_url);
        return url;
    }

    /* Default: append /chat/completions */
    char *url = (char *)malloc(len + 20);
    if (!url) return NULL;
    snprintf(url, len + 20, "%s/chat/completions", 
             base_url[len-1] == '/' ? base_url : base_url);
    /* Remove double slash */
    char *double_slash = strstr(url, "//");
    if (double_slash) {
        char *second = strstr(double_slash + 2, "//");
        if (second) memmove(second, second + 1, strlen(second));
    }
    return url;
}

/* ================================================================
 *  Headers
 * ================================================================ */

static char *openai_build_headers(const provider_t *p, const char *api_key) {
    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    if (api_key && *api_key) {
        snprintf(headers, 1024,
            "Authorization: Bearer %s\r\n"
            "Content-Type: application/json\r\n"
            "Accept: application/json",
            api_key);
    } else {
        snprintf(headers, 1024,
            "Content-Type: application/json\r\n"
            "Accept: application/json");
    }
    return headers;
}

/* ================================================================
 *  Request body building
 * ================================================================ */

static char *openai_build_request_body(const provider_t *p,
                                        const message_t **messages,
                                        size_t msg_count,
                                        json_node_t *tools_json,
                                        bool streaming) {
    json_t *root = json_new_object();
    if (!root) return NULL;

    /* Model */
    json_object_set(root, "model", json_new_string(
        p->model[0] ? p->model : "deepseek-v4-flash"));

    /* Stream flag */
    json_object_set(root, "stream", json_new_bool(streaming));

    /* LLM params from config */
    int max_tok = p->config.max_tokens > 0 ? p->config.max_tokens : 4096;
    json_object_set(root, "max_tokens", json_new_number(max_tok));

    if (p->config.temperature >= 0.0f)
        json_object_set(root, "temperature", json_new_number(p->config.temperature));
    if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
        json_object_set(root, "top_p", json_new_number(p->config.top_p));
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_new_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++) {
            if (p->config.stop_sequences[i][0])
                json_array_append(stop_arr, json_new_string(p->config.stop_sequences[i]));
        }
        if (json_array_count(stop_arr) > 0)
            json_object_set(root, "stop", stop_arr);
        else
            json_free(stop_arr);
    }
    if (p->config.service_tier[0])
        json_object_set(root, "service_tier", json_new_string(p->config.service_tier));
    if (p->config.presence_penalty != 0.0f)
        json_object_set(root, "presence_penalty", json_new_number(p->config.presence_penalty));
    if (p->config.frequency_penalty != 0.0f)
        json_object_set(root, "frequency_penalty", json_new_number(p->config.frequency_penalty));
    if (p->config.seed >= 0)
        json_object_set(root, "seed", json_new_number(p->config.seed));
    if (p->config.logprobs)
        json_object_set(root, "logprobs", json_new_bool(true));
    if (p->config.top_logprobs > 0) {
        json_object_set(root, "logprobs", json_new_bool(true));
        json_object_set(root, "top_logprobs", json_new_number(p->config.top_logprobs));
    }
    if (p->config.user[0])
        json_object_set(root, "user", json_new_string(p->config.user));

    /* response_format + metadata */
    if (p->config.response_format[0]) {
        json_t *rf = json_parse(p->config.response_format, NULL);
        if (rf) { json_object_set(root, "response_format", rf); json_free(rf); }
    }
    if (p->config.metadata[0]) {
        json_t *md = json_parse(p->config.metadata, NULL);
        if (md) { json_object_set(root, "metadata", md); json_free(md); }
    }

    /* tool_choice + parallel_tool_calls */
    if (p->config.tool_choice[0]) {
        /* Try JSON parse first (for specific function tool_choice), fallback to string */
        json_t *tc = json_parse(p->config.tool_choice, NULL);
        if (tc) { json_object_set(root, "tool_choice", tc); json_free(tc); }
        else { json_object_set(root, "tool_choice", json_new_string(p->config.tool_choice)); }
    }
    if (!p->config.parallel_tool_calls)
        json_object_set(root, "parallel_tool_calls", json_new_bool(false));

    /* max_tool_calls */
    if (p->config.max_tool_calls > 0)
        json_object_set(root, "max_tool_calls", json_new_number(p->config.max_tool_calls));
    if (p->config.n > 1)
        json_object_set(root, "n", json_new_number(p->config.n));

    /* L05: extra_body — merge arbitrary JSON fields into request body */
    if (p->config.extra_body[0]) {
        json_t *eb = json_parse(p->config.extra_body, NULL);
        if (eb && eb->type == JSON_OBJECT) {
            for (size_t i = 0; i < eb->c.count; i++) {
                json_t *copy = json_copy(eb->c.items[i]);
                if (copy)
                    json_object_set(root, eb->c.keys[i], copy);
            }
        }
        json_free(eb);
    }

    /* Messages */
    json_t *msgs = json_new_array();
    if (!msgs) { json_free(root); return NULL; }

    for (size_t i = 0; i < msg_count; i++) {
        json_t *msg = json_new_object();
        const char *role_str;
        switch (messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        json_object_set(msg, "content", json_new_string(
            messages[i]->content ? messages[i]->content : ""));

        /* Tool result ID for tool messages */
        if (messages[i]->role == MSG_TOOL && messages[i]->tool_call_id) {
            json_object_set(msg, "tool_call_id",
                json_new_string(messages[i]->tool_call_id));
        }

        /* Tool calls for assistant messages */
        if (messages[i]->role == MSG_ASSISTANT && messages[i]->tool_calls_count > 0) {
            json_t *tcs = json_new_array();
            for (int j = 0; j < messages[i]->tool_calls_count; j++) {
                json_t *tc = json_new_object();
                json_object_set(tc, "id",
                    json_new_string(messages[i]->tool_calls[j].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_t *func = json_new_object();
                json_object_set(func, "name",
                    json_new_string(messages[i]->tool_calls[j].name));
                json_object_set(func, "arguments",
                    json_new_string(messages[i]->tool_calls[j].arguments));
                json_object_set(tc, "function", func);
                json_array_append(tcs, tc);
            }
            json_object_set(msg, "tool_calls", tcs);
        }

        json_array_append(msgs, msg);
    }
    json_object_set(root, "messages", msgs);

    /* Tools */
    if (tools_json && json_array_count(tools_json) > 0) {
        json_object_set(root, "tools", json_copy(tools_json));
    }

    /* Serialize */
    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* ================================================================
 *  Response parsing
 * ================================================================ */

static provider_response_t *openai_parse_response(const provider_t *p,
                                                    const char *response_body) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    char *err = NULL;
    json_t *root = json_parse(response_body, &err);
    if (!root) {
        resp->content = (char *)malloc(256);
        if (resp->content)
            snprintf(resp->content, 256, "JSON parse error: %s", err ? err : "unknown");
        free(err);
        return resp;
    }

    /* Usage */
    json_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "completion_tokens", 0);
    }

    /* Choices */
    json_t *choices = json_object_get(root, "choices");
    if (choices && json_len(choices) > 0) {
        json_t *choice = json_get(choices, 0);
        json_t *message = json_object_get(choice, "message");

        if (message) {
            resp->content = strdup(json_get_str(message, "content", ""));

            /* Reasoning (DeepSeek-specific, in message) */
            const char *reason = json_get_str(message, "reasoning_content", NULL);
            if (!reason) reason = json_get_str(message, "reasoning", NULL);
            if (reason) resp->reasoning = strdup(reason);

            /* Tool calls */
            json_t *tcs = json_object_get(message, "tool_calls");
            if (tcs && json_len(tcs) > 0) {
                resp->tool_calls_count = (int)json_len(tcs);
                for (int i = 0; i < resp->tool_calls_count && i < 64; i++) {
                    json_t *tc = json_get(tcs, i);
                    snprintf(resp->tool_calls[i].id, sizeof(resp->tool_calls[i].id),
                             "%s", json_get_str(tc, "id", ""));
                    json_t *func = json_object_get(tc, "function");
                    if (func) {
                        snprintf(resp->tool_calls[i].name, sizeof(resp->tool_calls[i].name),
                                 "%s", json_get_str(func, "name", ""));
                        snprintf(resp->tool_calls[i].arguments, sizeof(resp->tool_calls[i].arguments),
                                 "%s", json_get_str(func, "arguments", "{}"));
                    }
                }
            }
        }
    }

    json_free(root);
    return resp;
}

static provider_response_t *openai_parse_stream_chunk(const provider_t *p,
                                                       const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    /* SSE format: "data: {...}" */
    const char *prefix = "data: ";
    if (strncmp(chunk, prefix, 6) != 0) {
        resp->content = strdup(chunk);
        return resp;
    }

    const char *json_str = chunk + 6;
    /* Check for [DONE] */
    if (strncmp(json_str, "[DONE]", 6) == 0) {
        resp->content = strdup("");
        return resp;
    }

    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) {
        resp->content = strdup(json_str);
        free(err);
        return resp;
    }

    json_t *choices = json_object_get(root, "choices");
    if (choices && json_len(choices) > 0) {
        json_t *choice = json_get(choices, 0);
        json_t *delta = json_object_get(choice, "delta");
        if (delta) {
            resp->content = strdup(json_get_str(delta, "content", ""));
        }
    }

    json_free(root);
    return resp;
}

static void openai_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Provider Operations Table
 * ================================================================ */

const provider_ops_t PROVIDER_OPS_OPENAI = {
    .build_url = openai_build_url,
    .build_headers = openai_build_headers,
    .build_request_body = openai_build_request_body,
    .parse_response = openai_parse_response,
    .parse_stream_chunk = openai_parse_stream_chunk,
    .free_response = openai_free_response,
    .name = "openai-compatible"
};
