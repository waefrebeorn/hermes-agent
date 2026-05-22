/*
 * provider_anthropic.c — Anthropic Messages API provider.
 * Supports Claude models via api.anthropic.com.
 *
 * Key differences from OpenAI:
 *  - x-api-key header (not Bearer)
 *  - System message is top-level "system" field, not in messages array
 *  - Only "user" and "assistant" roles (no "system", no "tool")
 *  - Content is array of content blocks, not a string
 *  - Tool calls are "tool_use" content blocks
 *  - Tool results are "tool_result" content blocks in user messages
 *  - Tools use "input_schema" not "parameters", no "type":"function" wrapper
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

static char *anthropic_build_url(const provider_t *p, const char *base_url) {
    (void)p;
    if (!base_url || !*base_url)
        base_url = "https://api.anthropic.com/v1";

    /* If URL already includes /messages, use as-is */
    if (strstr(base_url, "/messages"))
        return strdup(base_url);

    size_t len = strlen(base_url);
    char *url = (char *)malloc(len + 12);
    if (!url) return NULL;

    if (base_url[len-1] == '/')
        snprintf(url, len + 12, "%smessages", base_url);
    else
        snprintf(url, len + 12, "%s/messages", base_url);
    return url;
}

/* ================================================================
 *  Headers
 * ================================================================ */

static char *anthropic_build_headers(const provider_t *p, const char *api_key) {
    (void)p;
    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    if (api_key && *api_key) {
        snprintf(headers, 1024,
            "x-api-key: %s\r\n"
            "anthropic-version: 2023-06-01\r\n"
            "Content-Type: application/json\r\n"
            "Accept: application/json",
            api_key);
    } else {
        snprintf(headers, 1024,
            "anthropic-version: 2023-06-01\r\n"
            "Content-Type: application/json\r\n"
            "Accept: application/json");
    }
    return headers;
}

/* ================================================================
 *  Request body building
 * ================================================================ */

/* Build a content block array from a text string */
static json_t *text_block(const char *text) {
    json_t *block = json_object();
    json_set(block, "type", json_string("text"));
    json_set(block, "text", json_string(text ? text : ""));
    return block;
}

/* Build a tool_use content block */
static json_t *tool_use_block(const tool_call_t *tc) {
    json_t *block = json_object();
    json_set(block, "type", json_string("tool_use"));
    json_set(block, "id", json_string(tc->id));
    json_set(block, "name", json_string(tc->name));

    /* Parse arguments JSON as an object */
    char *err = NULL;
    json_t *input = json_parse(tc->arguments, &err);
    if (input) {
        json_set(block, "input", input);
    } else {
        json_set(block, "input", json_object()); /* empty object fallback */
        free(err);
    }
    return block;
}

/* Build a tool_result content block */
static json_t *tool_result_block(const char *tool_use_id, const char *content) {
    json_t *block = json_object();
    json_set(block, "type", json_string("tool_result"));
    json_set(block, "tool_use_id", json_string(tool_use_id));
    json_set(block, "content", json_string(content ? content : ""));
    return block;
}

static char *anthropic_build_request_body(const provider_t *p,
                                           const message_t **messages,
                                           size_t msg_count,
                                           json_t *tools_json,
                                           bool streaming) {
    (void)p;
    json_t *root = json_object();
    if (!root) return NULL;

    /* Model */
    json_set(root, "model", json_string(
        p->model[0] ? p->model : "claude-sonnet-4-20250514"));

    /* LLM params from config */
    int max_tok = p->config.max_tokens > 0 ? p->config.max_tokens : 4096;
    json_set(root, "max_tokens", json_number(max_tok));
    if (p->config.temperature > 0.0f)
        json_set(root, "temperature", json_number(p->config.temperature));
    if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
        json_set(root, "top_p", json_number(p->config.top_p));
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_append(stop_arr, json_string(p->config.stop_sequences[i]));
        if (json_len(stop_arr) > 0) json_set(root, "stop_sequences", stop_arr);
        else json_free(stop_arr);
    }

    /* Stream flag */
    if (streaming)
        json_set(root, "stream", json_bool(true));

    /* Tools (convert from OpenAI format to Anthropic format) */
    if (tools_json && json_len(tools_json) > 0) {
        json_t *anthropic_tools = json_array();
        size_t n = json_len(tools_json);
        for (size_t i = 0; i < n; i++) {
            json_t *ot = json_get(tools_json, i);
            /* OpenAI: {"type":"function","function":{"name":"...","description":"...","parameters":{...}}} */
            /* Anthropic: {"name":"...","description":"...","input_schema":{...}} */
            json_t *fn = json_obj_get(ot, "function");
            if (!fn) continue;

            json_t *at = json_object();
            json_set(at, "name", json_copy(json_obj_get(fn, "name")));
            json_set(at, "description", json_copy(json_obj_get(fn, "description")));

            /* Map OpenAI "parameters" to Anthropic "input_schema" */
            json_t *params = json_obj_get(fn, "parameters");
            if (params)
                json_set(at, "input_schema", json_copy(params));
            else
                json_set(at, "input_schema", json_object());

            json_append(anthropic_tools, at);
        }
        if (json_len(anthropic_tools) > 0)
            json_set(root, "tools", anthropic_tools);
        else
            json_free(anthropic_tools);
    }

    /* System message (top-level, separate from messages) */
    /* Collect all system messages into one string */
    char system_text[4096] = "";
    bool has_system = false;

    /* Messages array: Anthropic only uses user + assistant roles.
     * System messages are extracted to the top-level "system" field.
     * Tool results become user messages with tool_result content blocks. */
    json_t *msgs = json_array();
    if (!msgs) { json_free(root); return NULL; }

    for (size_t i = 0; i < msg_count; i++) {
        const message_t *msg = messages[i];
        if (!msg) continue;

        /* Extract system messages */
        if (msg->role == MSG_SYSTEM) {
            if (msg->content) {
                if (has_system) {
                    size_t cur = strlen(system_text);
                    size_t add = strlen(msg->content);
                    if (cur + add + 1 < sizeof(system_text)) {
                        memcpy(system_text + cur, msg->content, add);
                        system_text[cur + add] = '\0';
                    }
                } else {
                    snprintf(system_text, sizeof(system_text), "%s", msg->content);
                    has_system = true;
                }
            }
            continue;
        }

        /* Build Anthropic message */
        const char *role_str = NULL;
        json_t *content_arr = json_array();

        if (msg->role == MSG_USER) {
            role_str = "user";
            if (msg->tool_call_id) {
                /* This is a tool result — wrap in tool_result block */
                json_append(content_arr, tool_result_block(
                    msg->tool_call_id,
                    msg->content ? msg->content : ""));
            } else {
                /* Normal user text */
                json_append(content_arr, text_block(msg->content));
            }
        } else if (msg->role == MSG_ASSISTANT) {
            role_str = "assistant";
            if (msg->tool_calls_count > 0) {
                /* Text content first (if any) */
                if (msg->content && msg->content[0])
                    json_append(content_arr, text_block(msg->content));
                /* Tool use blocks */
                for (int j = 0; j < msg->tool_calls_count; j++)
                    json_append(content_arr, tool_use_block(&msg->tool_calls[j]));
            } else {
                json_append(content_arr, text_block(msg->content));
            }
        } else if (msg->role == MSG_TOOL) {
            /* Tool result messages: convert to user with tool_result block.
             * The agent loop creates MSG_TOOL messages. Anthropic
             * represents these as user messages with tool_result content. */
            role_str = "user";
            if (msg->tool_call_id) {
                json_append(content_arr, tool_result_block(
                    msg->tool_call_id,
                    msg->content ? msg->content : ""));
            } else {
                json_append(content_arr, text_block(msg->content));
            }
        } else {
            continue; /* skip unknown roles */
        }

        if (json_len(content_arr) == 0) {
            json_free(content_arr);
            continue;
        }

        json_t *am = json_object();
        json_set(am, "role", json_string(role_str));
        json_set(am, "content", content_arr);
        json_append(msgs, am);
    }

    if (json_len(msgs) == 0) {
        json_free(msgs);
        /* At minimum, add a "hello" user message */
        json_t *dummy_msg = json_object();
        json_set(dummy_msg, "role", json_string("user"));
        json_t *dummy_content = json_array();
        json_append(dummy_content, text_block("Hello"));
        json_set(dummy_msg, "content", dummy_content);
        msgs = json_array();
        json_append(msgs, dummy_msg);
    }

    json_set(root, "messages", msgs);

    /* Set system field if we extracted any */
    if (has_system && system_text[0]) {
        /* P91: Prompt caching — first turn uses cache_control */
        if (p->system_cached) {
            /* Already cached — plain system string */
            json_set(root, "system", json_string(system_text));
        } else {
            /* First request — wrap with cache_control */
            json_t *sys_blocks = json_array();
            json_t *block = json_object();
            json_set(block, "type", json_string("text"));
            json_set(block, "text", json_string(system_text));
            json_t *cc = json_object();
            json_set(cc, "type", json_string("ephemeral"));
            json_set(block, "cache_control", cc);
            json_append(sys_blocks, block);
            json_set(root, "system", sys_blocks);
        }
    }

    /* Serialize */
    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* ================================================================
 *  Response parsing
 * ================================================================ */

static provider_response_t *anthropic_parse_response(const provider_t *p,
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

    /* Check for error response */
    json_t *error_obj = json_obj_get(root, "error");
    if (error_obj) {
        const char *err_type = json_get_str(error_obj, "type", "unknown");
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        resp->content = (char *)malloc(1024);
        if (resp->content)
            snprintf(resp->content, 1024, "Anthropic API error [%s]: %s", err_type, err_msg);
        json_free(root);
        return resp;
    }

    /* Usage */
    json_t *usage = json_obj_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "input_tokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "output_tokens", 0);
    }

    /* Content array */
    json_t *content_arr = json_obj_get(root, "content");
    if (content_arr && json_len(content_arr) > 0) {
        /* First pass: count total text content for allocation.
         * Anthropic returns multiple content blocks:
         *   {"type":"text","text":"..."}
         *   {"type":"tool_use","id":"...","name":"...","input":{...}} */
        size_t text_len = 0;
        int tc_count = 0;
        size_t n = json_len(content_arr);
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(content_arr, i);
            const char *type = json_get_str(block, "type", "");
            if (strcmp(type, "text") == 0) {
                const char *text = json_get_str(block, "text", "");
                text_len += strlen(text);
            } else if (strcmp(type, "tool_use") == 0) {
                if (tc_count < 64) tc_count++;
            }
        }

        /* Allocate content buffer */
        if (text_len > 0) {
            resp->content = (char *)calloc(text_len + 1, 1);
            if (resp->content) {
                size_t pos = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *block = json_get(content_arr, i);
                    const char *type = json_get_str(block, "type", "");
                    if (strcmp(type, "text") == 0) {
                        const char *text = json_get_str(block, "text", "");
                        size_t add = strlen(text);
                        if (pos + add < text_len + 1) {
                            memcpy(resp->content + pos, text, add);
                            pos += add;
                        }
                    }
                }
                resp->content[pos] = '\0';
            }
        } else {
            resp->content = strdup("");
        }

        /* Extract tool calls */
        if (tc_count > 0) {
            resp->tool_calls_count = 0;
            for (size_t i = 0; i < n && resp->tool_calls_count < 64; i++) {
                json_t *block = json_get(content_arr, i);
                const char *type = json_get_str(block, "type", "");
                if (strcmp(type, "tool_use") == 0) {
                    int idx = resp->tool_calls_count;
                    snprintf(resp->tool_calls[idx].id,
                             sizeof(resp->tool_calls[idx].id), "%s",
                             json_get_str(block, "id", ""));
                    snprintf(resp->tool_calls[idx].name,
                             sizeof(resp->tool_calls[idx].name), "%s",
                             json_get_str(block, "name", ""));

                    /* Serialize the input object back to JSON string */
                    json_t *input = json_obj_get(block, "input");
                    if (input) {
                        char *args = json_serialize(input);
                        if (args) {
                            snprintf(resp->tool_calls[idx].arguments,
                                     sizeof(resp->tool_calls[idx].arguments),
                                     "%s", args);
                            free(args);
                        }
                    }
                    resp->tool_calls_count++;
                }
            }
        }

        /* Check stop_reason for "end_turn" vs "tool_use" */
        const char *stop = json_get_str(root, "stop_reason", "");
        if (strcmp(stop, "tool_use") != 0 && resp->tool_calls_count > 0) {
            /* If stop_reason isn't tool_use but we have tool calls,
             * keep them (Anthropic stops on tool_use) */
        }
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Streaming chunk parsing
 * ================================================================ */

static provider_response_t *anthropic_parse_stream_chunk(const provider_t *p,
                                                          const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    /* Anthropic SSE format:
     *   event: content_block_delta
     *   data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}
     *
     *   event: content_block_start
     *   data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}
     *
     *   event: message_start
     *   data: {"type":"message_start","message":{...}}
     *
     *   event: message_delta
     *   data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":N}}
     */

    /* We handle two patterns:
     * 1. Event-lines followed by data-lines
     * 2. Direct "data: {...}" lines (some proxies simplify) */

    const char *json_str = NULL;

    /* Try event: ... data: ... pattern */
    if (strncmp(chunk, "event:", 6) == 0) {
        /* Next line should be "data: {...}" */
        const char *nl = strchr(chunk, '\n');
        if (nl) {
            const char *data_prefix = strstr(nl, "data: ");
            if (data_prefix)
                json_str = data_prefix + 6;
        }
    } else if (strncmp(chunk, "data: ", 6) == 0) {
        json_str = chunk + 6;
    }

    if (!json_str || !*json_str) {
        resp->content = strdup(chunk);
        return resp;
    }

    /* Check for event stream termination */
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

    /* Determine event type */
    const char *type = json_get_str(root, "type", "");

    if (strcmp(type, "content_block_delta") == 0) {
        json_t *delta = json_obj_get(root, "delta");
        if (delta) {
            const char *delta_type = json_get_str(delta, "type", "");
            if (strcmp(delta_type, "text_delta") == 0) {
                resp->content = strdup(json_get_str(delta, "text", ""));
            } else if (strcmp(delta_type, "input_json_delta") == 0) {
                /* Partial tool call arguments — we accumulate these
                 * in the streaming callback context, not per-chunk.
                 * Return empty content for now. */
                resp->content = strdup("");
            } else {
                resp->content = strdup("");
            }
        } else {
            resp->content = strdup("");
        }
    } else if (strcmp(type, "content_block_start") == 0) {
        json_t *cb = json_obj_get(root, "content_block");
        if (cb) {
            const char *cb_type = json_get_str(cb, "type", "");
            if (strcmp(cb_type, "text") == 0) {
                resp->content = strdup(json_get_str(cb, "text", ""));
            } else {
                resp->content = strdup("");
            }
        } else {
            resp->content = strdup("");
        }
    } else if (strcmp(type, "message_start") == 0) {
        json_t *msg = json_obj_get(root, "message");
        if (msg) {
            json_t *usage = json_obj_get(msg, "usage");
            if (usage) {
                resp->input_tokens = (int)json_get_num(usage, "input_tokens", 0);
            }
        }
        resp->content = strdup("");
    } else if (strcmp(type, "message_delta") == 0) {
        json_t *delta = json_obj_get(root, "delta");
        json_t *usage = json_obj_get(root, "usage");
        if (usage)
            resp->output_tokens = (int)json_get_num(usage, "output_tokens", 0);

        /* Check stop_reason for tool_use */
        if (delta) {
            const char *stop = json_get_str(delta, "stop_reason", "");
            if (strcmp(stop, "tool_use") == 0) {
                /* Signal: more tool calls may follow.
                 * The streaming callback accumulates these. */
            }
        }
        resp->content = strdup("");
    } else {
        /* ping or unknown — ignore */
        resp->content = strdup("");
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Free response
 * ================================================================ */

static void anthropic_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Provider Operations Table
 * ================================================================ */

const provider_ops_t PROVIDER_OPS_ANTHROPIC = {
    .build_url = anthropic_build_url,
    .build_headers = anthropic_build_headers,
    .build_request_body = anthropic_build_request_body,
    .parse_response = anthropic_parse_response,
    .parse_stream_chunk = anthropic_parse_stream_chunk,
    .free_response = anthropic_free_response,
    .name = "anthropic"
};
