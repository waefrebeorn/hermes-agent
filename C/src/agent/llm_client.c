/*
 * llm_client.c — LLM API client for Hermes C.
 * Sends chat completions requests via HTTP, parses JSON response.
 * Supports: chat completions, tool calls, streaming (optional), reasoning.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
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

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    if (prov && prov->ops) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        char *headers = ops->build_headers(prov, cfg->api_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, false);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            provider_free(prov); free(resp); return NULL;
        }
        http_client_t *client = http_client_new_with_retry(30, 3, 1000);
        http_response_t *http_resp = http_request(client, HTTP_POST, url,
                                                   headers, body, strlen(body));
        free(url); free(headers); free(body);
        if (!http_resp || http_resp->status < 0) {
            resp->content = strdup("HTTP request failed");
            if (http_resp) http_response_free(http_resp);
            http_client_free(client); provider_free(prov);
            return resp;
        }
        provider_response_t *presp = ops->parse_response(prov, http_resp->body);
        http_response_free(http_resp); http_client_free(client); provider_free(prov);
        if (presp) {
            resp->content = presp->content; presp->content = NULL;
            resp->reasoning = presp->reasoning; presp->reasoning = NULL;
            resp->input_tokens = presp->input_tokens;
            resp->output_tokens = presp->output_tokens;
            resp->tool_calls_count = presp->tool_calls_count;
            for (int i = 0; i < presp->tool_calls_count && i < 64; i++)
                resp->tool_calls[i] = presp->tool_calls[i];
            ops->free_response(presp);
        }
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
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
    http_client_t *client = http_client_new_with_retry(30, 3, 1000);
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

/* ================================================================
 *  Streaming: per-chunk callback context
 * ================================================================ */

typedef struct {
    llm_response_t  *resp;      /* accumulated response */
    llm_token_cb_t   token_cb;  /* token callback */
    void            *userdata;  /* callback userdata */
    const provider_ops_t *prov_ops;  /* provider ops for per-chunk parsing (optional) */
    provider_t      *prov;      /* provider instance (optional) */
    /* Track tool_calls across chunks (streaming builds them incrementally) */
    int              max_tc_idx; /* highest tool_call index seen */
    char             tc_id[64][64];  /* id per index */
    char             tc_name[64][128]; /* name per index */
    char             tc_args[64][4096]; /* args buffer per index */
    bool             streaming_done; /* set when streaming is complete */
} stream_ctx_t;

/* Provider-aware stream chunk handler.
 * Uses provider's parse_stream_chunk for text deltas, falls back
 * to OpenAI-style delta parsing for tool calls. */
static int on_provider_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    if (!ctx->prov_ops || !ctx->prov) {
        /* No provider — skip */
        return 0;
    }

    /* Try provider's parse_stream_chunk first */
    provider_response_t *delta = ctx->prov_ops->parse_stream_chunk(ctx->prov, data);
    if (delta) {
        /* Text content from provider */
        if (delta->content && delta->content[0]) {
            size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
            size_t add = strlen(delta->content);
            char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
            if (newc) {
                ctx->resp->content = newc;
                memcpy(ctx->resp->content + cur, delta->content, add + 1);
            }
            if (ctx->token_cb)
                ctx->token_cb(delta->content, ctx->userdata);
        }

        /* Token counts from delta (Anthropic sends in message_delta) */
        if (delta->input_tokens > 0)
            ctx->resp->input_tokens = delta->input_tokens;
        if (delta->output_tokens > 0)
            ctx->resp->output_tokens = delta->output_tokens;

        /* Tool calls from provider (Anthropic sends complete tool_use blocks,
         * OpenAI sends deltas — handled below) */
        if (delta->tool_calls_count > 0) {
            for (int i = 0; i < delta->tool_calls_count && i < 64; i++) {
                int idx = ctx->max_tc_idx;
                if (idx < 64) {
                    snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s",
                             delta->tool_calls[i].id);
                    snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s",
                             delta->tool_calls[i].name);
                    snprintf(ctx->tc_args[idx], sizeof(ctx->tc_args[idx]), "%s",
                             delta->tool_calls[i].arguments);
                    ctx->max_tc_idx = idx + 1;
                }
            }
        }

        ctx->prov_ops->free_response(delta);
    }

    /* Fallback: also try OpenAI-style delta parsing for tool calls.
     * This handles the case where parse_stream_chunk returns empty
     * but the chunk has tool_call deltas (OpenAI format). */
    if (strncmp(data, "data: ", 6) == 0) {
        const char *json_str = data + 6;
        if (strncmp(json_str, "[DONE]", 6) == 0) {
            ctx->streaming_done = true;
            return 0;
        }

        char *err = NULL;
        json_node_t *root = json_parse(json_str, &err);
        if (!root) { free(err); return 0; }

        json_node_t *choices = json_object_get(root, "choices");
        if (choices && json_array_count(choices) > 0) {
            json_node_t *choice = json_array_get(choices, 0);
            json_node_t *delta = json_object_get(choice, "delta");

            /* OpenAI tool call deltas */
            if (delta) {
                json_node_t *tc_delta = json_object_get(delta, "tool_calls");
                if (tc_delta && json_array_count(tc_delta) > 0) {
                    for (size_t i = 0; i < json_array_count(tc_delta); i++) {
                        json_node_t *tc = json_array_get(tc_delta, i);
                        int idx = (int)json_object_get_number(tc, "index", 0);
                        if (idx >= 64) continue;
                        if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;
                        const char *id = json_object_get_string(tc, "id", NULL);
                        if (id && id[0])
                            snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);
                        json_node_t *fn = json_object_get(tc, "function");
                        if (fn) {
                            const char *name = json_object_get_string(fn, "name", NULL);
                            if (name && name[0])
                                snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);
                            const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                            if (args_chunk && args_chunk[0]) {
                                size_t cur = strlen(ctx->tc_args[idx]);
                                size_t add = strlen(args_chunk);
                                if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                                    memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                                    ctx->tc_args[idx][cur + add] = '\0';
                                }
                            }
                        }
                    }
                }

                /* OpenAI finish reason */
                const char *finish = json_object_get_string(choice, "finish_reason", NULL);
                if (finish && finish[0]) {
                    ctx->streaming_done = true;
                    json_node_t *usage = json_object_get(root, "usage");
                    if (usage) {
                        ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
                        ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
                    }
                }
            }
        }

        json_free(root);
    }

    return 0;
}

static int on_stream_chunk(const char *data, size_t len, void *userdata) {
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    (void)len;

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(data, &err);
    if (!root) { free(err); return 0; } /* Skip non-JSON events */

    json_node_t *choices = json_object_get(root, "choices");
    if (!choices || json_array_count(choices) == 0) { json_free(root); return 0; }

    json_node_t *choice = json_array_get(choices, 0);
    json_node_t *delta = json_object_get(choice, "delta");
    if (!delta) { json_free(root); return 0; }

    /* Extract finish_reason */
    const char *finish = json_object_get_string(choice, "finish_reason", NULL);

    /* Extract delta content */
    const char *content = json_object_get_string(delta, "content", NULL);
    if (content && content[0]) {
        /* Append to accumulated content */
        size_t cur = ctx->resp->content ? strlen(ctx->resp->content) : 0;
        size_t add = strlen(content);
        char *newc = (char *)realloc(ctx->resp->content, cur + add + 1);
        if (newc) {
            ctx->resp->content = newc;
            memcpy(ctx->resp->content + cur, content, add + 1);
        }
        /* Call token callback */
        if (ctx->token_cb)
            ctx->token_cb(content, ctx->userdata);
    }

    /* Extract delta tool_calls (streaming builds them across chunks) */
    json_node_t *tc_delta = json_object_get(delta, "tool_calls");
    if (tc_delta && json_array_count(tc_delta) > 0) {
        for (size_t i = 0; i < json_array_count(tc_delta); i++) {
            json_node_t *tc = json_array_get(tc_delta, i);
            int idx = (int)json_object_get_number(tc, "index", 0);
            if (idx >= 64) continue;

            /* Track highest index */
            if (idx >= ctx->max_tc_idx) ctx->max_tc_idx = idx + 1;

            /* id (only present in first chunk for this tool_call) */
            const char *id = json_object_get_string(tc, "id", NULL);
            if (id && id[0]) snprintf(ctx->tc_id[idx], sizeof(ctx->tc_id[idx]), "%s", id);

            /* function delta */
            json_node_t *fn = json_object_get(tc, "function");
            if (fn) {
                const char *name = json_object_get_string(fn, "name", NULL);
                if (name && name[0]) snprintf(ctx->tc_name[idx], sizeof(ctx->tc_name[idx]), "%s", name);

                const char *args_chunk = json_object_get_string(fn, "arguments", NULL);
                if (args_chunk && args_chunk[0]) {
                    size_t cur = strlen(ctx->tc_args[idx]);
                    size_t add = strlen(args_chunk);
                    if (cur + add < sizeof(ctx->tc_args[idx]) - 1) {
                        memcpy(ctx->tc_args[idx] + cur, args_chunk, add);
                        ctx->tc_args[idx][cur + add] = '\0';
                    }
                }
            }
        }
    }

    /* Extract reasoning (some providers send it in chunks too) */
    const char *reasoning = json_object_get_string(delta, "reasoning", NULL);
    if (!reasoning) reasoning = json_object_get_string(delta, "reasoning_content", NULL);
    if (reasoning && reasoning[0]) {
        size_t cur = ctx->resp->reasoning ? strlen(ctx->resp->reasoning) : 0;
        size_t add = strlen(reasoning);
        char *newr = (char *)realloc(ctx->resp->reasoning, cur + add + 1);
        if (newr) {
            ctx->resp->reasoning = newr;
            memcpy(ctx->resp->reasoning + cur, reasoning, add + 1);
        }
    }

    /* Extract usage if present (final chunk with finish_reason) */
    if (finish && finish[0]) {
        json_node_t *usage = json_object_get(root, "usage");
        if (usage) {
            ctx->resp->input_tokens = (int)json_object_get_number(usage, "prompt_tokens", 0);
            ctx->resp->output_tokens = (int)json_object_get_number(usage, "completion_tokens", 0);
        }
    }

    json_free(root);
    return 0;
}

/* Build tool_calls array from accumulated streaming data */
static void finalize_stream_toolcalls(stream_ctx_t *ctx) {
    ctx->resp->tool_calls_count = ctx->max_tc_idx;
    for (int i = 0; i < ctx->max_tc_idx; i++) {
        snprintf(ctx->resp->tool_calls[i].id, sizeof(ctx->resp->tool_calls[i].id), "%s", ctx->tc_id[i]);
        snprintf(ctx->resp->tool_calls[i].name, sizeof(ctx->resp->tool_calls[i].name), "%s", ctx->tc_name[i]);
        snprintf(ctx->resp->tool_calls[i].arguments, sizeof(ctx->resp->tool_calls[i].arguments), "%s", ctx->tc_args[i]);
    }
}

llm_response_t *llm_chat_completion_stream(llm_config_t *cfg,
                                            const message_t **messages,
                                            size_t message_count,
                                            json_node_t *tools_json,
                                            llm_token_cb_t token_cb,
                                            void *userdata) {
    llm_response_t *resp = (llm_response_t *)calloc(1, sizeof(llm_response_t));
    if (!resp) return NULL;
    resp->content = NULL;
    resp->reasoning = NULL;
    resp->input_tokens = 0;
    resp->output_tokens = 0;
    resp->tool_calls_count = 0;

    /* Create provider instance from config */
    provider_t *prov = provider_create(
        cfg->provider[0] ? cfg->provider : "openai",
        cfg->model, cfg->api_key, cfg->base_url);

    if (prov && prov->ops) {
        const provider_ops_t *ops = prov->ops;
        char *url = ops->build_url(prov, cfg->base_url);
        char *headers = ops->build_headers(prov, cfg->api_key);
        char *body = ops->build_request_body(prov, messages, message_count, tools_json, true);
        if (!url || !headers || !body) {
            free(url); free(headers); free(body);
            provider_free(prov); free(resp); return NULL;
        }
        /* Use true streaming via http_stream_request with provider-aware callback */
        stream_ctx_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.resp = resp;
        ctx.token_cb = token_cb;
        ctx.userdata = userdata;
        ctx.prov_ops = ops;
        ctx.prov = prov;

        http_t *h = http_new(60);
        int r = http_stream_request(h, HTTP_POST, url,
                                    headers, body, strlen(body),
                                    on_provider_stream_chunk, &ctx);
        free(url); free(headers); free(body);
        http_free(h);

        if (r != 0 && r != -2) {
            if (!resp->content)
                resp->content = strdup("HTTP stream request failed");
            provider_free(prov);
            return resp;
        }

        /* Finalize tool calls from accumulated chunks */
        finalize_stream_toolcalls(&ctx);
        provider_free(prov);
        return resp;
    }
    provider_free(prov);

    /* --- legacy fallback --- */
    json_node_t *req = json_new_object();
    json_object_set(req, "model", json_new_string(cfg->model));
    json_object_set(req, "stream", json_new_bool(true));

    /* Stream options: include usage in final chunk */
    json_node_t *stream_opts = json_new_object();
    json_object_set(stream_opts, "include_usage", json_new_bool(true));
    json_object_set(req, "stream_options", stream_opts);

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
    stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.resp = resp;
    ctx.token_cb = token_cb;
    ctx.userdata = userdata;

    /* Make streaming request */
    http_t *h = http_new(60);
    int r = http_stream_request(h, HTTP_POST, url,
                                auth_header, body, strlen(body),
                                on_stream_chunk, &ctx);
    json_free(req);
    free(body);
    http_free(h);

    if (r != 0 && r != -2 /* aborted by callback OK */) {
        resp->content = strdup("HTTP stream request failed");
        return resp;
    }

    /* Finalize tool calls from accumulated chunks */
    finalize_stream_toolcalls(&ctx);

    return resp;
}

void llm_response_free(llm_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}
