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
#include "hermes_url_safety.h"
#include "provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Model-aware helpers
 * ================================================================ */

/* Substring patterns for model version detection */
static const char *ADAPTIVE_SUBSTRINGS[] = {"4-6", "4.6", "4-7", "4.7", "4-8", "4.8", NULL};
static const char *XHIGH_EFFORT_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *NO_SAMPLING_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *FAST_MODE_SUBSTRINGS[] = {"opus-4-6", "opus-4.6", NULL};

/* Normalize model name: replace '.' with '-' for matching */
static void normalize_model_key(const char *model, char *out, size_t out_size) {
    if (!model || !*model) { out[0] = '\0'; return; }
    const char *slash = strrchr(model, '/');
    if (slash) model = slash + 1;  /* strip provider prefix */
    size_t i, j = 0;
    for (i = 0; model[i] && j < out_size - 1; i++) {
        if (model[i] == '.')
            out[j++] = '-';
        else
            out[j++] = model[i];
    }
    out[j] = '\0';
}

static bool model_contains_any(const char *model, const char **patterns) {
    if (!model || !*model) return false;
    char normalized[128];
    normalize_model_key(model, normalized, sizeof(normalized));
    for (int i = 0; patterns[i]; i++) {
        if (strstr(normalized, patterns[i]))
            return true;
    }
    return false;
}

/* Claude 4.6+ models support adaptive thinking (type="adaptive" + output_config.effort) */
static bool supports_adaptive_thinking(const char *model) {
    return model_contains_any(model, ADAPTIVE_SUBSTRINGS);
}

/* Opus 4.7+ models accept the "xhigh" effort level */
static bool supports_xhigh_effort(const char *model) {
    return model_contains_any(model, XHIGH_EFFORT_SUBSTRINGS);
}

/* Opus 4.7+ models reject temperature/top_p/top_k (any non-null value → 400) */
static bool forbids_sampling_params(const char *model) {
    return model_contains_any(model, NO_SAMPLING_SUBSTRINGS);
}

/* Opus 4.6 supports fast mode (speed="fast" + beta header) */
__attribute__((unused)) static bool supports_fast_mode(const char *model) {
    return model_contains_any(model, FAST_MODE_SUBSTRINGS);
}

/* Adventure effort → adaptive effort mapping.
 * Low/medium/high/xhigh/max preserved; minimal→low; others→medium. */
static const char *map_to_adaptive_effort(const char *effort) {
    if (!effort || !*effort) return "medium";
    if (strcmp(effort, "low") == 0)     return "low";
    if (strcmp(effort, "medium") == 0)   return "medium";
    if (strcmp(effort, "high") == 0)     return "high";
    if (strcmp(effort, "xhigh") == 0)    return "xhigh";
    if (strcmp(effort, "max") == 0)      return "max";
    if (strcmp(effort, "minimal") == 0)  return "low";
    return "medium";
}

/* Model-specific max output token limits */
static int get_anthropic_max_output(const char *model) {
    if (!model || !*model) return 128000;
    char normalized[128];
    normalize_model_key(model, normalized, sizeof(normalized));

    /* Table sorted by longest key first for longest-prefix match */
    struct { const char *key; int limit; } limits[] = {
        {"claude-opus-4-8",      128000},
        {"claude-opus-4-7",      128000},
        {"claude-opus-4-6",      128000},
        {"claude-sonnet-4-6",     64000},
        {"claude-opus-4-5",       64000},
        {"claude-sonnet-4-5",     64000},
        {"claude-haiku-4-5",      64000},
        {"claude-opus-4",         32000},
        {"claude-sonnet-4",       64000},
        {"claude-3-7-sonnet",    128000},
        {"claude-3-5-sonnet",      8192},
        {"claude-3-5-haiku",       8192},
        {"claude-3-opus",          4096},
        {"claude-3-sonnet",        4096},
        {"claude-3-haiku",         4096},
        {"minimax",              131072},
        {"qwen3",                 65536},
        {NULL, 0}
    };
    int best = 128000;
    size_t best_len = 0;
    for (int i = 0; limits[i].key; i++) {
        if (strstr(normalized, limits[i].key)) {
            size_t klen = strlen(limits[i].key);
            if (klen > best_len) {
                best_len = klen;
                best = limits[i].limit;
            }
        }
    }
    return best;
}

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
    const char *base_url = p ? p->base_url : NULL;

    /* Determine auth header type: Bearer for MiniMax/Azure, x-api-key for native Anthropic */
    bool use_bearer = anthropic_requires_bearer_auth(base_url);

    /* Collect beta headers using endpoint-aware beta resolution */
    json_t *beta_list = anthropic_common_betas_for_base_url(base_url, false);
    char betas[512] = "";
    size_t pos = 0;
    size_t n = json_len(beta_list);

    /* Prompt caching beta — always sent first if applicable */
    if (p && provider_get_system_cached(p)) {
        pos += snprintf(betas + pos, sizeof(betas) - pos,
                        "anthropic-beta: ephemeral-cache-2025-05-20");
    }

    for (size_t i = 0; i < n; i++) {
        json_t *beta_item = json_get(beta_list, i);
        const char *b = (beta_item && beta_item->type == JSON_STRING) ? beta_item->str_val : "";
        if (!*b) continue;
        pos += snprintf(betas + pos, sizeof(betas) - pos,
                        "%s%s",
                        pos > 0 ? "\r\nanthropic-beta: " : "anthropic-beta: ",
                        b);
    }
    json_free(beta_list);

    char *headers = (char *)malloc(1536);
    if (!headers) return NULL;

    const char *auth_header = "";
    const char *auth_value = "";
    if (api_key && *api_key) {
        if (use_bearer) {
            auth_header = "Authorization: Bearer ";
        } else {
            auth_header = "x-api-key: ";
        }
        auth_value = api_key;
    }

    snprintf(headers, 1536,
        "%s%s\r\n"
        "anthropic-version: 2023-06-01\r\n"
        "%s"
        "Content-Type: application/json\r\n"
        "Accept: application/json",
        auth_header, auth_value,
        betas);

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

    /* LLM params from config — skip temperature/top_p on 4.7+ that reject them */
    bool is_adaptive = supports_adaptive_thinking(p->model);
    bool skip_sampling = forbids_sampling_params(p->model);

    /* max_tokens with model-aware output ceiling */
    int max_tok = p->config.max_tokens > 0
        ? p->config.max_tokens
        : get_anthropic_max_output(p->model);
    json_set(root, "max_tokens", json_number(max_tok));

    if (!skip_sampling) {
        if (p->config.temperature >= 0.0f)
            json_set(root, "temperature", json_number(p->config.temperature));
        if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
            json_set(root, "top_p", json_number(p->config.top_p));
    }
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

    /* S8 R01: Anthropic extended thinking — adaptive vs classic */
    if (p->config.reasoning_effort[0] && strcmp(p->config.reasoning_effort, "none") != 0) {
        const char *effort = p->config.reasoning_effort;

        if (is_adaptive) {
            /* Claude 4.6+ uses adaptive thinking with output_config.effort */
            json_t *thinking = json_object();
            json_set(thinking, "type", json_string("adaptive"));
            /* display: "summarized" keeps reasoning blocks populated for Hermes CLI */
            json_set(thinking, "display", json_string("summarized"));
            json_set(root, "thinking", thinking);

            /* Map effort */
            const char *adaptive_effort = map_to_adaptive_effort(effort);
            /* Downgrade xhigh→max on models that don't support xhigh (pre-4.7) */
            if (strcmp(adaptive_effort, "xhigh") == 0 && !supports_xhigh_effort(p->model))
                adaptive_effort = "max";

            json_t *output_config = json_object();
            json_set(output_config, "effort", json_string(adaptive_effort));
            json_set(root, "output_config", output_config);
        } else {
            /* Classic thinking with budget_tokens for pre-4.6 models */
            int budget = 0;
            if (strcmp(effort, "low") == 0)        budget = 8192;
            else if (strcmp(effort, "medium") == 0) budget = 16384;
            else if (strcmp(effort, "high") == 0)   budget = 32768;
            else budget = atoi(effort); /* direct number */
            if (budget >= 1024) {
                json_t *thinking = json_object();
                json_set(thinking, "type", json_string("enabled"));
                json_set(thinking, "budget_tokens", json_number(budget));
                json_set(root, "thinking", thinking);

                /* Anthropic requires temperature=1 when thinking is enabled on classic models */
                if (!skip_sampling)
                    json_set(root, "temperature", json_number(1.0f));

                /* Ensure max_tokens >= budget + 4096 for thinking to have room */
                if (max_tok < budget + 4096)
                    json_set(root, "max_tokens", json_number(budget + 4096));
            }
        }
    }

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
        resp->content = strdup("");
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
        resp->content = strdup("");
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
 *  Endpoint detection utilities — ported from Python anthropic_adapter.py
 * ================================================================ */

bool anthropic_is_oauth_token(const char *key) {
    if (!key || !*key) return false;
    /* Regular Anthropic Console API keys — x-api-key auth, not OAuth */
    if (strncmp(key, "sk-ant-api", 10) == 0) return false;
    /* Anthropic-issued tokens (setup-tokens sk-ant-oat-*, managed keys) */
    if (strncmp(key, "sk-ant-", 7) == 0) return true;
    /* JWTs from Anthropic OAuth flow */
    if (strncmp(key, "eyJ", 3) == 0) return true;
    /* Claude Code OAuth access tokens */
    if (strncmp(key, "cc-", 3) == 0) return true;
    return false;
}

char *anthropic_normalize_base_url_text(const char *base_url) {
    if (!base_url || !*base_url) return strdup("");
    /* Strip leading/trailing whitespace */
    while (*base_url == ' ' || *base_url == '\t') base_url++;
    if (!*base_url) return strdup("");
    size_t len = strlen(base_url);
    while (len > 0 && (base_url[len-1] == ' ' || base_url[len-1] == '\t')) len--;
    return strndup(base_url, len);
}

bool anthropic_is_third_party_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    /* Lowercase for comparison */
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* Strip trailing slash */
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "anthropic.com") == NULL);
    free(lower);
    free(norm);
    return result;
}

bool anthropic_is_kimi_coding_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strncmp(lower, "https://api.kimi.com/coding", 27) == 0);
    free(lower);
    free(norm);
    return result;
}

static bool str_is_kimi_prefix(const char *m) {
    if (!m || !*m) return false;
    static const char *prefixes[] = {
        "kimi-", "kimi_", "moonshot-", "moonshot_",
        "k1.", "k1-", "k2.", "k2-", "k25", "k2.5", NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(m, prefixes[i], plen) == 0) return true;
    }
    return false;
}

bool anthropic_model_name_is_kimi_family(const char *model) {
    if (!model || !*model) return false;
    const char *m = model;
    while (*m == ' ' || *m == '\t') m++;
    if (!*m) return false;
    size_t len = strlen(m);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)m[i]);
    lower[len] = '\0';
    /* Strip vendor prefix */
    char *slash = strrchr(lower, '/');
    char *prefix = slash ? slash + 1 : lower;
    /* Strip trailing whitespace */
    size_t plen = strlen(prefix);
    while (plen > 0 && (prefix[plen-1] == ' ' || prefix[plen-1] == '\t')) prefix[--plen] = '\0';
    bool result = str_is_kimi_prefix(prefix);
    free(lower);
    return result;
}

bool anthropic_is_kimi_family_endpoint(const char *base_url, const char *model) {
    if (anthropic_is_kimi_coding_endpoint(base_url)) return true;
    /* Check known domains */
    if (base_url && *base_url) {
        if (url_host_matches(base_url, "api.kimi.com") ||
            url_host_matches(base_url, "moonshot.ai") ||
            url_host_matches(base_url, "moonshot.cn"))
            return true;
    }
    if (anthropic_model_name_is_kimi_family(model)) return true;
    return false;
}

bool anthropic_is_deepseek_endpoint(const char *base_url) {
    if (!base_url || !*base_url) return false;
    if (!url_host_matches(base_url, "api.deepseek.com")) return false;
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (strstr(lower, "/anthropic") != NULL);
    free(lower);
    free(norm);
    return result;
}

bool anthropic_requires_bearer_auth(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        (strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
         strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0 ||
         strstr(lower, "azure.com") != NULL)
    );
    free(lower);
    free(norm);
    return result;
}

bool anthropic_base_url_needs_1m_beta(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    bool result = (strstr(lower, "azure.com") != NULL);
    free(lower);
    free(norm);
    return result;
}

bool anthropic_is_minimax_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    while (len > 0 && lower[len-1] == '/') lower[--len] = '\0';
    bool result = (
        strncmp(lower, "https://api.minimax.io/anthropic", 33) == 0 ||
        strncmp(lower, "https://api.minimaxi.com/anthropic", 34) == 0
    );
    free(lower);
    free(norm);
    return result;
}

bool anthropic_is_azure_anthropic_endpoint(const char *base_url) {
    char *norm = anthropic_normalize_base_url_text(base_url);
    if (!norm || !*norm) { free(norm); return false; }
    size_t len = strlen(norm);
    char *lower = malloc(len + 1);
    if (!lower) { free(norm); return false; }
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)norm[i]);
    lower[len] = '\0';
    /* URL parsing: extract hostname */
    const char *host_start = NULL;
    const char *proto = strstr(lower, "://");
    if (proto) host_start = proto + 3; else host_start = lower;
    const char *path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
    /* Extract host portion for dot-padded matching */
    char host_buf[512];
    size_t hl = host_len < 511 ? host_len : 511;
    memcpy(host_buf, host_start, hl);
    host_buf[hl] = '\0';
    /* Check with dot-padded boundaries */
    char padded[520];
    snprintf(padded, sizeof(padded), ".%s.", host_buf);
    const char *path = path_start ? path_start : "";
    bool is_foundry = (strstr(padded, ".services.ai.azure.") != NULL);
    bool is_legacy = (strstr(padded, ".openai.azure.") != NULL);
    bool has_anthropic_path = (strstr(path, "/anthropic") != NULL);
    bool result = (is_foundry || is_legacy) && has_anthropic_path;
    free(lower);
    free(norm);
    return result;
}

json_t *anthropic_common_betas_for_base_url(const char *base_url, bool drop_context_1m_beta) {
    json_t *betas = json_array();
    json_append(betas, json_string("interleaved-thinking-2025-05-14"));
    json_append(betas, json_string("fine-grained-tool-streaming-2025-05-14"));

    if (anthropic_base_url_needs_1m_beta(base_url) && !drop_context_1m_beta)
        json_append(betas, json_string("context-1m-2025-08-07"));

    if (anthropic_is_minimax_endpoint(base_url)) {
        /* Strip tool-streaming and context-1m for MiniMax */
        json_t *filtered = json_array();
        size_t n = json_len(betas);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(betas, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                if (strcmp(item->str_val, "fine-grained-tool-streaming-2025-05-14") != 0 &&
                    strcmp(item->str_val, "context-1m-2025-08-07") != 0)
                    json_append(filtered, json_copy(item));
            }
        }
        json_free(betas);
        return filtered;
    }

    if (drop_context_1m_beta) {
        json_t *filtered = json_array();
        size_t n = json_len(betas);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(betas, i);
            if (item && item->type == JSON_STRING && item->str_val) {
                if (strcmp(item->str_val, "context-1m-2025-08-07") != 0)
                    json_append(filtered, json_copy(item));
            }
        }
        json_free(betas);
        return filtered;
    }

    return betas;
}

bool anthropic_is_bedrock_model_id(const char *model_id) {
    if (!model_id || !*model_id) return false;
    /* Bedrock model IDs: anthropic.claude-* */
    if (strncmp(model_id, "anthropic.claude-", 17) == 0) return true;
    /* Also check for common Bedrock ARN format */
    if (strstr(model_id, "bedrock") && strstr(model_id, "anthropic")) return true;
    return false;
}

int anthropic_resolve_positive_max_tokens(int value) {
    return (value > 0) ? value : 0;
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
