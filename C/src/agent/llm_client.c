/*
 * llm_client.c — LLM API client for Hermes C.
 * Sends chat completions requests via HTTP, parses JSON response.
 * Supports: chat completions, tool calls, streaming (optional), reasoning.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

/* Build the messages array JSON from message list */
static json_node_t *build_messages_json(const message_t **msgs, size_t count) {
    json_node_t *arr = json_new_array();
    for (size_t i = 0; i < count; i++) {
        json_node_t *msg = json_new_object();
        const char *role_str;
        switch (msgs[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));
        if (msgs[i]->content)
            json_object_set(msg, "content", json_new_string(msgs[i]->content));

        /* Tool call result */
        if (msgs[i]->role == MSG_TOOL && msgs[i]->tool_call_id)
            json_object_set(msg, "tool_call_id", json_new_string(msgs[i]->tool_call_id));

        /* Tool calls from assistant */
        if (msgs[i]->role == MSG_ASSISTANT && msgs[i]->tool_calls_count > 0) {
            json_node_t *tc_arr = json_new_array();
            for (int j = 0; j < msgs[i]->tool_calls_count; j++) {
                json_node_t *tc = json_new_object();
                json_object_set(tc, "id", json_new_string(msgs[i]->tool_calls[j].id));
                json_object_set(tc, "type", json_new_string("function"));
                json_node_t *fn = json_new_object();
                json_object_set(fn, "name", json_new_string(msgs[i]->tool_calls[j].name));
                json_object_set(fn, "arguments", json_new_string(msgs[i]->tool_calls[j].arguments));
                json_object_set(tc, "function", fn);
                json_array_append(tc_arr, tc);
            }
            json_object_set(msg, "tool_calls", tc_arr);
        }

        json_array_append(arr, msg);
    }
    return arr;
}

/* ================================================================
 *  Token counting + context management
 * ================================================================ */

/* Count approximate tokens in a message list.
 * Stops counting after max_tokens is exceeded. */
size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!msgs || !msgs[i]) continue;
        /* Role tokens (~1 token each) */
        total += 1;
        /* Content tokens */
        total += llm_estimate_tokens(msgs[i]->content);
        /* Tool call overhead */
        if (msgs[i]->tool_calls_count > 0)
            total += (size_t)msgs[i]->tool_calls_count * 10;
        /* Tool result overhead */
        if (msgs[i]->tool_call_id)
            total += 2;
        if (total > max_tokens) break;
    }
    return total;
}

/* Truncate context to fit within max_tokens while keeping
 * system message (index 0) and most recent messages. */
void llm_truncate_context(agent_state_t *state, size_t max_tokens) {
    if (!state || state->message_count < 2) return;

    size_t current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current <= max_tokens) return;

    /* Need to drop messages. Keep system + most recent. */
    size_t keep = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t target = keep + 4; /* Keep system + 4 most recent turns */
    if (state->message_count <= target) return;

    /* Remove from index keep to (message_count - target + keep) */
    size_t remove_count = state->message_count - target;
    for (size_t i = 0; i < remove_count; i++)
        message_free(state->messages[keep + i]);
    memmove(&state->messages[keep], &state->messages[keep + remove_count],
            (state->message_count - keep - remove_count) * sizeof(message_t *));
    state->message_count -= remove_count;

    /* Re-check after truncation */
    current = llm_count_context_tokens(
        (const message_t **)state->messages, state->message_count, max_tokens);
    if (current > max_tokens && state->message_count > keep + 2) {
        /* Still over budget — drop more aggressively */
        llm_truncate_context(state, max_tokens);
    }
}

/* ================================================================
 *  LLM API call
 * ================================================================ */

llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_node_t *tools_json)
{
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Build request JSON */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));

    /* Messages */
    json_node_t *msgs = build_messages_json(messages, message_count);
    json_object_set(req, "messages", msgs);

    /* Add tools if provided */
    if (tools_json && json_array_count(tools_json) > 0)
        json_object_set(req, "tools", json_copy(tools_json));

    /* Serialize */
    char *body = json_serialize(req);

    /* Determine URL */
    char url[512];
    const char *base = cfg->base_url;
    if (base && strlen(base) > 0) {
        /* Check if base already ends with /chat/completions */
        if (strstr(base, "/chat/completions"))
            snprintf(url, sizeof(url), "%s", base);
        else
            snprintf(url, sizeof(url), "%s/chat/completions", base);
    } else {
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");
    }

    /* Build auth header */
    char auth_header[512];
    if (cfg->api_key[0]) {
        snprintf(auth_header, sizeof(auth_header),
                "Authorization: Bearer %s\r\nContent-Type: application/json",
                 cfg->api_key);
    } else {
        snprintf(auth_header, sizeof(auth_header),
                 "Content-Type: application/json");
    }

    /* Make HTTP request */
    http_client_t *client = http_client_new(30);
    http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                               auth_header, body, strlen(body));

    json_free(req);
    free(body);

    if (!http_resp ||        http_resp->status < 0) {
        resp->content = xstrdup("HTTP request failed");
        if (http_resp) http_response_free(http_resp);
        http_client_free(client);
        return resp;
    }

    /* Parse JSON response */
    char *json_err = NULL;
    json_node_t *root = json_parse(http_resp->body, &json_err);
    http_response_free(http_resp);

    if (!root) {
        resp->content = (char *)malloc(256);
        snprintf(resp->content, 256, "JSON parse error: %s", json_err ? json_err : "unknown");
        free(json_err);
        http_client_free(client);
        return resp;
    }

    /* Extract usage if present */
    json_node_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
        resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
    }

    /* Extract choices */
    json_node_t *choices = json_object_get(root, "choices");
    if (choices && json_array_count(choices) > 0) {
        json_node_t *choice = json_array_get(choices, 0);
        json_node_t *message = json_object_get(choice, "message");

        if (message) {
            resp->content = xstrdup(json_object_get_string(message, "content", ""));

            /* Check for reasoning */
            json_node_t *reasoning = json_object_get(message, "reasoning");
            if (reasoning && reasoning->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning->str_val);

            /* Check for tool calls */
            json_node_t *tool_calls = json_object_get(message, "tool_calls");
            if (tool_calls && json_array_count(tool_calls) > 0) {
                resp->tool_calls_count = (int)json_array_count(tool_calls);
                if (resp->tool_calls_count > 64)
                    resp->tool_calls_count = 64;
                for (int i = 0; i < resp->tool_calls_count; i++) {
                    json_node_t *tc = json_array_get(tool_calls, (size_t)i);
                    const char *id = json_object_get_string(tc, "id", "");
                    snprintf(resp->tool_calls[i].id, sizeof(resp->tool_calls[i].id), "%s", id);
                    json_node_t *fn = json_object_get(tc, "function");
                    if (fn) {
                        const char *name = json_object_get_string(fn, "name", "");
                        const char *args = json_object_get_string(fn, "arguments", "{}");
                        snprintf(resp->tool_calls[i].name, sizeof(resp->tool_calls[i].name), "%s", name);
                        snprintf(resp->tool_calls[i].arguments, sizeof(resp->tool_calls[i].arguments), "%s", args);
                    }
                }
            }

            /* Reasoning content field (some providers use this instead of "reasoning") */
            json_node_t *reasoning_content = json_object_get(message, "reasoning_content");
            if (!resp->reasoning && reasoning_content && reasoning_content->type == JSON_STRING)
                resp->reasoning = xstrdup(reasoning_content->str_val);
        }
    }

    json_free(root);
    http_client_free(client);
    return resp;
}

void llm_response_free(llm_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}
