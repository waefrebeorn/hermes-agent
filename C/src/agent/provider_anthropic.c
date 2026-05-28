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
    char *cached = NULL;
    if (p && provider_get_system_cached(p))
        cached = "anthropic-beta: ephemeral-cache-2025-05-20\r\n";

    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    if (api_key && *api_key) {
        snprintf(headers, 1024,
            "x-api-key: %s\r\n"
            "anthropic-version: 2023-06-01\r\n"
            "%s"
            "Content-Type: application/json\r\n"
            "Accept: application/json",
            api_key, cached ? cached : "");
    } else {
        snprintf(headers, 1024,
            "anthropic-version: 2023-06-01\r\n"
            "%s"
            "Content-Type: application/json\r\n"
            "Accept: application/json",
            cached ? cached : "");
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
    if (p->config.temperature >= 0.0f)
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

    /* P06: Anthropic service_tier (e.g. "default") */
    if (p->config.service_tier[0])
        json_set(root, "service_tier", json_string(p->config.service_tier));

    /* response_format + metadata */
    if (p->config.response_format[0]) {
        json_t *rf = json_parse(p->config.response_format, NULL);
        if (rf) { json_set(root, "response_format", json_copy(rf)); json_free(rf); }
    } else if (p->config.json_mode) {
        json_t *rf = json_new_object();
        json_object_set(rf, "type", json_new_string("json_object"));
        json_set(root, "response_format", rf);
    }
    if (p->config.metadata[0]) {
        json_t *md = json_parse(p->config.metadata, NULL);
        if (md) { json_set(root, "metadata", md); json_free(md); }
    }

    /* tool_choice + parallel_tool_calls */
    if (p->config.tool_choice[0]) {
        json_t *tc = json_parse(p->config.tool_choice, NULL);
        if (tc) { json_set(root, "tool_choice", tc); json_free(tc); }
        else { json_set(root, "tool_choice", json_string(p->config.tool_choice)); }
    }
    if (!p->config.parallel_tool_calls)
        json_set(root, "disable_parallel_tool_use", json_bool(true));

    /* Stream flag */
    if (streaming)
        json_set(root, "stream", json_bool(true));

    /* extra_body — merge arbitrary JSON fields into request body */
    if (p->config.extra_body[0]) {
        json_t *eb = json_parse(p->config.extra_body, NULL);
        if (eb && eb->type == JSON_OBJECT) {
            for (size_t i = 0; i < eb->c.count; i++) {
                json_t *copy = json_copy(eb->c.items[i]);
                if (copy)
                    json_set(root, eb->c.keys[i], copy);
            }
        }
        json_free(eb);
    }

    /* B26: Anthropic thinking blocks — extended reasoning */
    if (p->config.reasoning_effort[0]) {
        int budget = 0;
        const char *effort = p->config.reasoning_effort;
        if (strcmp(effort, "low") == 0)        budget = 8192;
        else if (strcmp(effort, "medium") == 0) budget = 16384;
        else if (strcmp(effort, "high") == 0)   budget = 32768;
        else budget = atoi(effort); /* direct number */
        if (budget >= 1024) {
            json_t *thinking = json_object();
            json_set(thinking, "type", json_string("enabled"));
            json_set(thinking, "budget_tokens", json_number(budget));
            json_set(root, "thinking", thinking);
        }
    }

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
        if (json_len(anthropic_tools) > 0) {
            json_set(root, "tools", anthropic_tools);
            /* B27: cache_control on tools — top-level field in request body */
            json_t *tools_cc = json_object();
            json_set(tools_cc, "type", json_string("ephemeral"));
            json_set(root, "cache_control", tools_cc);
        } else
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

    /* B27: cache_control on the last user message's last content block */
    if (json_len(msgs) > 0) {
        json_t *last_msg = json_get(msgs, json_len(msgs) - 1);
        const char *last_role = json_get_str(last_msg, "role", "");
        if (strcmp(last_role, "user") == 0) {
            json_t *last_content = json_obj_get(last_msg, "content");
            if (last_content && json_len(last_content) > 0) {
                json_t *last_block = json_get(last_content, json_len(last_content) - 1);
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(last_block, "cache_control", cc);
            }
        }
    }

    /* CACHE: system_and_3 strategy — mark last 3 non-system messages for cache.
     * Python prompt_caching.py apply_anthropic_cache_control().
     * We already marked the last user message above, so we scan backwards
     * and mark up to 2 more (indices 2 and 3 from end). */
    {
        int breakpoints = 0;
        int n_msgs = (int)json_len(msgs);
        for (int i = n_msgs - 2; i >= 0 && breakpoints < 2; i--) {
            json_t *msg = json_get(msgs, i);
            const char *r = json_get_str(msg, "role", "");
            if (strcmp(r, "system") == 0) continue; /* system handled separately */
            breakpoints++;

            json_t *content = json_obj_get(msg, "content");
            if (!content || json_len(content) == 0) continue;

            if (strcmp(r, "tool") == 0) {
                /* Tool messages: cache_control at message level */
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(msg, "cache_control", cc);
            } else {
                /* User/assistant: cache_control on last content block */
                json_t *last_block = json_get(content, json_len(content) - 1);
                json_t *cc = json_object();
                json_set(cc, "type", json_string("ephemeral"));
                json_set(last_block, "cache_control", cc);
            }
        }
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
        size_t thinking_len = 0;
        int tc_count = 0;
        size_t n = json_len(content_arr);
        for (size_t i = 0; i < n; i++) {
            json_t *block = json_get(content_arr, i);
            const char *type = json_get_str(block, "type", "");
            if (strcmp(type, "text") == 0) {
                const char *text = json_get_str(block, "text", "");
                text_len += strlen(text);
            } else if (strcmp(type, "thinking") == 0) {
                const char *text = json_get_str(block, "thinking", "");
                thinking_len += strlen(text);
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

        /* Extract reasoning from thinking blocks (B26) */
        if (thinking_len > 0) {
            resp->reasoning = (char *)calloc(thinking_len + 1, 1);
            if (resp->reasoning) {
                size_t pos = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *block = json_get(content_arr, i);
                    const char *type = json_get_str(block, "type", "");
                    if (strcmp(type, "thinking") == 0) {
                        const char *text = json_get_str(block, "thinking", "");
                        size_t add = strlen(text);
                        if (pos + add < thinking_len + 1) {
                            memcpy(resp->reasoning + pos, text, add);
                            pos += add;
                        }
                    }
                }
                resp->reasoning[pos] = '\0';
            }
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

    /* Null-safe */
    if (!chunk) {
        resp->content = strdup("");
        return resp;
    }

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

    /* Handle three input formats used by different callers:
     * 1. Raw JSON (HTTP parser stripped "data: " prefix)
     * 2. Full SSE event+data line ("event: ...\ndata: {...}")
     * 3. Bare "data: {...}" line */
    const char *json_str = NULL;

    if (strncmp(chunk, "{", 1) == 0) {
        /* Raw JSON — HTTP streaming parser already stripped framing */
        json_str = chunk;
    } else if (strncmp(chunk, "event:", 6) == 0) {
        /* Full SSE: event: ...\ndata: {...} */
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
            } else if (strcmp(delta_type, "thinking_delta") == 0) {
                /* B26: Anthropic thinking blocks — streaming reasoning */
                resp->reasoning = strdup(json_get_str(delta, "thinking", ""));
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
            if (stop[0]) snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", stop);
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
