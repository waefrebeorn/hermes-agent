/*
 * provider_deepseek.c — Native DeepSeek provider (P79).
 *
 * DeepSeek API is OpenAI-compatible for chat completions but adds:
 *   - Context caching via x-ds-cache-ttl header (reduces cost on cached inputs)
 *   - FIM (Fill-in-the-Middle) endpoint at /beta/completions
 *   - Reasoning content in response (deepseek-reasoner models)
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

static char *deepseek_build_url(const provider_t *p, const char *base_url) {
    (void)p;
    if (!base_url || !*base_url)
        base_url = "https://api.deepseek.com/v1";

    if (strstr(base_url, "/chat/completions"))
        return strdup(base_url);

    size_t len = strlen(base_url);
    char *url = (char *)malloc(len + 20);
    if (!url) return NULL;
    snprintf(url, len + 20, "%s/chat/completions",
             base_url[len-1] == '/' ? base_url : base_url);
    char *ds = strstr(url, "//");
    if (ds) { char *s = strstr(ds + 2, "//"); if (s) memmove(s, s + 1, strlen(s)); }
    return url;
}

/* ================================================================
 *  Headers — add context caching header
 * ================================================================ */

static char *deepseek_build_headers(const provider_t *p, const char *api_key) {
    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    /* B33: Context caching TTL \xe2\x80\x94 configurable via deepseek_cache_ttl */
    int ttl = 300;
    if (p && p->config.deepseek_cache_ttl > 0) {
        ttl = p->config.deepseek_cache_ttl;
    } else if (p && p->config.deepseek_cache_ttl == -1) {
        ttl = -1;
    }

    if (api_key && *api_key) {
        if (ttl > 0) {
            snprintf(headers, 1024,
                "Authorization: Bearer %s\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Accept: application/json\r\n"
                "x-ds-cache-ttl: %d",
                api_key, ttl);
        } else {
            snprintf(headers, 1024,
                "Authorization: Bearer %s\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Accept: application/json",
                api_key);
        }
    } else {
        if (ttl > 0) {
            snprintf(headers, 1024,
                "Content-Type: application/json; charset=utf-8\r\n"
                "Accept: application/json\r\n"
                "x-ds-cache-ttl: %d", ttl);
        } else {
            snprintf(headers, 1024,
                "Content-Type: application/json; charset=utf-8\r\n"
                "Accept: application/json");
        }
    }
    return headers;
}

/* ================================================================
 *  Request body — OpenAI-compatible with DeepSeek-specific model
 * ================================================================ */

static char *deepseek_build_request_body(const provider_t *p,
                                          const message_t **messages,
                                          size_t msg_count,
                                          json_node_t *tools_json,
                                          bool streaming) {
    (void)p;
    json_t *root = json_new_object();
    if (!root) return NULL;

    json_object_set(root, "model", json_new_string(
        p->model[0] ? p->model : "deepseek-chat"));
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
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_array_append(stop_arr, json_new_string(p->config.stop_sequences[i]));
        if (json_array_count(stop_arr) > 0) json_object_set(root, "stop", stop_arr);
        else json_free(stop_arr);
    }
    if (p->config.service_tier[0])
        json_object_set(root, "service_tier", json_new_string(p->config.service_tier));
    if (p->config.reasoning_effort[0])
        json_object_set(root, "reasoning_effort", json_new_string(p->config.reasoning_effort));
    /* I07: DeepSeek V4 requires explicit thinking.type in every request.
     * Without it, server defaults to thinking=ON and enforces reasoning_content
     * echo contract. Always send explicit thinking block to match Python. */
    {
        json_t *thinking = json_new_object();
        json_object_set(thinking, "type", json_new_string("enabled"));
        json_object_set(root, "thinking", thinking);
    }
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
        if (rf) {
            /* B24: strict JSON schema enforcement */
            if (p->config.response_format_strict)
                json_object_set(rf, "strict", json_new_bool(true));
            json_object_set(root, "response_format", json_copy(rf));
            json_free(rf);
        }
    } else if (p->config.json_mode) {
        json_t *rf = json_new_object();
        json_object_set(rf, "type", json_new_string("json_object"));
        json_object_set(root, "response_format", rf);
    }
    if (p->config.metadata[0]) {
        json_t *md = json_parse(p->config.metadata, NULL);
        if (md) { json_object_set(root, "metadata", md); json_free(md); }
    }

    /* tool_choice + parallel_tool_calls */
    if (p->config.tool_choice[0]) {
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

        if (messages[i]->role == MSG_TOOL && messages[i]->tool_call_id)
            json_object_set(msg, "tool_call_id", json_new_string(messages[i]->tool_call_id));

        if (messages[i]->role == MSG_ASSISTANT && messages[i]->tool_calls_count > 0) {
            json_t *tcs = json_new_array();
            for (int j = 0; j < messages[i]->tool_calls_count && j < 64; j++) {
                json_t *tc = json_new_object();
                json_object_set(tc, "id", json_new_string(messages[i]->tool_calls[j].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_t *func = json_new_object();
                json_object_set(func, "name", json_new_string(messages[i]->tool_calls[j].name));
                json_object_set(func, "arguments", json_new_string(messages[i]->tool_calls[j].arguments));
                json_object_set(tc, "function", func);
                json_array_append(tcs, tc);
            }
            json_object_set(msg, "tool_calls", tcs);
        }
        json_array_append(msgs, msg);
    }
    json_object_set(root, "messages", msgs);

    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(root, "tools", json_copy(tools_json));

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* ================================================================
 *  Response parsing — extracts reasoning_content for deepseek-reasoner
 * ================================================================ */

static provider_response_t *deepseek_parse_response(const provider_t *p,
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

    json_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "completion_tokens", 0);
    }
    /* Check for API error response */
    json_t *error_obj = json_object_get(root, "error");
    if (error_obj) {
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        resp->content = (char *)malloc(1024);
        if (resp->content)
            snprintf(resp->content, 1024, "DeepSeek API error: %s", err_msg);
        json_free(root);
        return resp;
    }
    json_t *choices = json_object_get(root, "choices");
    if (choices && json_len(choices) > 0) {
        json_t *choice = json_get(choices, 0);
        json_t *message = json_object_get(choice, "message");
        if (message) {
            resp->content = strdup(json_get_str(message, "content", ""));
            /* DeepSeek-specific: reasoning_content field */
            const char *reason = json_get_str(message, "reasoning_content", NULL);
            if (!reason) reason = json_get_str(message, "reasoning", NULL);
            if (reason) resp->reasoning = strdup(reason);

            json_t *tcs = json_object_get(message, "tool_calls");
            if (tcs && json_len(tcs) > 0) {
                resp->tool_calls_count = (int)json_len(tcs);
                for (int i = 0; i < resp->tool_calls_count && i < 64; i++) {
                    json_t *tc = json_get(tcs, i);
                    snprintf(resp->tool_calls[i].id, sizeof(resp->tool_calls[i].id), "%s",
                             json_get_str(tc, "id", ""));
                    json_t *func = json_object_get(tc, "function");
                    if (func) {
                        snprintf(resp->tool_calls[i].name, sizeof(resp->tool_calls[i].name), "%s",
                                 json_get_str(func, "name", ""));
                        snprintf(resp->tool_calls[i].arguments, sizeof(resp->tool_calls[i].arguments), "%s",
                                 json_get_str(func, "arguments", "{}"));
                    }
                }
            }
        }
    }
    json_free(root);
    return resp;
}

static provider_response_t *deepseek_parse_stream_chunk(const provider_t *p,
                                                         const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;
    if (!chunk) { resp->content = strdup(""); return resp; }

    const char *prefix = "data: ";
    if (strncmp(chunk, prefix, 6) != 0) {
        resp->content = strdup(chunk);
        return resp;
    }
    const char *json_str = chunk + 6;
    if (strncmp(json_str, "[DONE]", 6) == 0) {
        resp->content = strdup("");
        return resp;
    }

    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { resp->content = strdup(json_str); free(err); return resp; }

    json_t *choices = json_object_get(root, "choices");
    if (choices && json_len(choices) > 0) {
        json_t *choice = json_get(choices, 0);
        /* B22: finish_reason */
        { const char *fr = json_get_str(choice, "finish_reason", NULL); if (fr) snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", fr); }
        json_t *delta = json_object_get(choice, "delta");
        if (delta) {
            resp->content = strdup(json_get_str(delta, "content", ""));
            /* DeepSeek streams reasoning in delta.reasoning_content */
            const char *reason = json_get_str(delta, "reasoning_content", NULL);
            if (reason) resp->reasoning = strdup(reason);
        }
    }
    json_free(root);
    return resp;
}

static void deepseek_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  B32: FIM (Fill-in-the-Middle) — /beta/completions endpoint
 * ================================================================ */

static char *deepseek_build_fim_url(const provider_t *p, const char *base_url) {
    (void)p;
    if (!base_url || !*base_url)
        base_url = "https://api.deepseek.com/v1";

    /* Replace trailing /chat/completions with /beta/completions */
    size_t len = strlen(base_url);
    char *url = (char *)malloc(len + 24);
    if (!url) return NULL;

    const char *chat_suffix = "/chat/completions";
    size_t chat_len = strlen(chat_suffix);
    if (len >= chat_len && strcmp(base_url + len - chat_len, chat_suffix) == 0) {
        /* Replace chat completions suffix with beta/completions */
        size_t prefix_len = len - chat_len;
        memcpy(url, base_url, prefix_len);
        strcpy(url + prefix_len, "/beta/completions");
    } else {
        /* Append /beta/completions to base */
        snprintf(url, len + 24, "%s%s/beta/completions",
                 base_url, base_url[len-1] == '/' ? "" : "/");
    }
    return url;
}

static char *deepseek_build_fim_body(const provider_t *p,
                                      const char *prompt,
                                      const char *suffix,
                                      int max_tokens) {
    (void)p;
    json_t *root = json_new_object();
    if (!root) return NULL;

    json_object_set(root, "model", json_new_string(
        p->model[0] ? p->model : "deepseek-chat"));
    json_object_set(root, "prompt", json_new_string(prompt ? prompt : ""));
    if (suffix && suffix[0])
        json_object_set(root, "suffix", json_new_string(suffix));
    if (max_tokens > 0) {
        json_object_set(root, "max_tokens", json_new_number(max_tokens));
    } else {
        json_object_set(root, "max_tokens", json_new_number(256));
    }
    /* FIM does not stream — single response */
    json_object_set(root, "stream", json_new_bool(false));

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

static provider_response_t *deepseek_parse_fim_response(const provider_t *p,
                                                         const char *response_body) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    if (!response_body) {
        resp->content = strdup("");
        return resp;
    }

    char *err = NULL;
    json_t *root = json_parse(response_body, &err);
    if (!root) {
        resp->content = (char *)malloc(256);
        if (resp->content)
            snprintf(resp->content, 256, "FIM JSON parse error: %s", err ? err : "unknown");
        free(err);
        return resp;
    }

    /* Check for API error */
    json_t *error_obj = json_object_get(root, "error");
    if (error_obj) {
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        resp->content = (char *)malloc(1024);
        if (resp->content)
            snprintf(resp->content, 1024, "DeepSeek FIM API error: %s", err_msg);
        json_free(root);
        return resp;
    }

    /* Parse FIM response: choices[0].text (not message.content!) */
    json_t *choices = json_object_get(root, "choices");
    if (choices && json_len(choices) > 0) {
        json_t *choice = json_get(choices, 0);
        const char *text = json_get_str(choice, "text", "");
        resp->content = strdup(text);

        /* B22: finish_reason */
        const char *fr = json_get_str(choice, "finish_reason", NULL);
        if (fr) snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", fr);
    } else {
        resp->content = strdup("");
    }

    /* Usage metadata */
    json_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "completion_tokens", 0);
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Provider Operations Table
 * ================================================================ */

const provider_ops_t PROVIDER_OPS_DEEPSEEK = {
    .build_url = deepseek_build_url,
    .build_headers = deepseek_build_headers,
    .build_request_body = deepseek_build_request_body,
    .parse_response = deepseek_parse_response,
    .parse_stream_chunk = deepseek_parse_stream_chunk,
    .free_response = deepseek_free_response,
    .build_fim_body = deepseek_build_fim_body,
    .parse_fim_response = deepseek_parse_fim_response,
    .build_fim_url = deepseek_build_fim_url,
    .name = "deepseek"
};
