/*
 * provider_google.c — Google Gemini API provider.
 * Supports Gemini models via generativelanguage.googleapis.com.
 *
 * Key differences from OpenAI:
 *  - x-goog-api-key header
 *  - Endpoint: /v1beta/models/{model}:generateContent
 *  - "contents" array with "user"/"model" roles (not "assistant")
 *  - Parts array for content blocks
 *  - functionCall / functionResponse parts for tool calls
 *  - Separate system_instruction field
 *  - functionDeclarations tool format
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Gemini native adapter helpers
 * ================================================================ */

/* Map Google finish reason to OpenAI-compatible format */
static const char *google_map_finish_reason(const char *reason) {
    if (!reason || !*reason) return "stop";
    /* Google returns: STOP, MAX_TOKENS, SAFETY, RECITATION, OTHER, BLOCKLIST, PROHIBITED_CONTENT, SPAM, IMAGE_SAFETY */
    if (strcasecmp(reason, "STOP") == 0) return "stop";
    if (strcasecmp(reason, "MAX_TOKENS") == 0) return "length";
    if (strcasecmp(reason, "SAFETY") == 0) return "content_filter";
    if (strcasecmp(reason, "RECITATION") == 0) return "content_filter";
    if (strcasecmp(reason, "BLOCKLIST") == 0) return "content_filter";
    if (strcasecmp(reason, "PROHIBITED_CONTENT") == 0) return "content_filter";
    if (strcasecmp(reason, "SPAM") == 0) return "content_filter";
    if (strcasecmp(reason, "IMAGE_SAFETY") == 0) return "content_filter";
    return "stop";
}

/* Detect free-tier quota exhaustion in Gemini error responses */
static bool google_is_free_tier_quota_error(const char *error_message) {
    if (!error_message) return false;
    /* Python: "free_tier" in error_message.lower() */
    const char *p = error_message;
    while (*p) {
        if ((*p == 'f' || *p == 'F') &&
            strncasecmp(p, "free_tier", 9) == 0)
            return true;
        p++;
    }
    return false;
}

/* ================================================================
 *  URL building
 * ================================================================ */

static char *google_build_url(const provider_t *p, const char *base_url) {
    if (!base_url || !*base_url)
        base_url = "https://generativelanguage.googleapis.com/v1beta";

    const char *model = p->model[0] ? p->model : "gemini-2.0-flash";

    /* If URL already includes :generateContent, use as-is */
    if (strstr(base_url, ":generateContent") || strstr(base_url, ":streamGenerateContent"))
        return strdup(base_url);

    size_t base_len = strlen(base_url);
    size_t model_len = strlen(model);
    char *url = (char *)malloc(base_len + model_len + 40);
    if (!url) return NULL;

    /* Strip trailing slash to avoid //models */
    while (base_len > 0 && base_url[base_len-1] == '/') base_len--;

    snprintf(url, base_len + model_len + 40, "%.*s/models/%s:generateContent",
             (int)base_len, base_url, model);

    return url;
}

/* ================================================================
 *  Headers
 * ================================================================ */

static char *google_build_headers(const provider_t *p, const char *api_key) {
    (void)p;
    char *headers = (char *)malloc(1024);
    if (!headers) return NULL;

    if (api_key && *api_key) {
        snprintf(headers, 1024,
            "x-goog-api-key: %s\r\n"
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

static char *google_build_request_body(const provider_t *p,
                                        const message_t **messages,
                                        size_t msg_count,
                                        json_t *tools_json,
                                        bool streaming) {
    (void)p;
    json_t *root = json_object();
    if (!root) return NULL;

    /* Generation config */
    json_t *gen_config = json_object();
    int max_tok = p->config.max_tokens > 0 ? p->config.max_tokens : 4096;
    json_set(gen_config, "maxOutputTokens", json_number(max_tok));
    if (p->config.temperature >= 0.0f)
        json_set(gen_config, "temperature", json_number(p->config.temperature));
    if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
        json_set(gen_config, "topP", json_number(p->config.top_p));
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_append(stop_arr, json_string(p->config.stop_sequences[i]));
        if (json_len(stop_arr) > 0) json_set(gen_config, "stopSequences", stop_arr);
        else json_free(stop_arr);
    }
    json_set(root, "generationConfig", gen_config);

    /* B30: generation_config depth — top_k, candidate_count */
    if (p->config.top_k > 0)
        json_set(gen_config, "topK", json_number(p->config.top_k));
    if (p->config.candidate_count > 0)
        json_set(gen_config, "candidateCount", json_number(p->config.candidate_count));

    /* B28/L05: extra_body — merge arbitrary JSON fields into request body */
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

    /* B29: Google safety settings — parsed from JSON array string */
    if (p->config.safety_settings[0]) {
        json_t *ss = json_parse(p->config.safety_settings, NULL);
        if (ss && ss->type == JSON_ARRAY) {
            json_set(root, "safetySettings", ss);
        } else if (ss) {
            json_free(ss);
        }
    }

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
        json_set(root, "parallel_tool_calls", json_bool(false));

    /* System instruction (separate from contents) */
    char system_text[4096] = "";
    bool has_system = false;

    /* Tools (convert from OpenAI format to Google format) */
    /* OpenAI: {"type":"function","function":{"name":"...","description":"...","parameters":{...}}} */
    /* Google: {"functionDeclarations":[{"name":"...","description":"...","parameters":{...}}]} */
    if (tools_json && json_len(tools_json) > 0) {
        size_t n = json_len(tools_json);
        /* Count how many have function definitions */
        int fd_count = 0;
        for (size_t i = 0; i < n; i++) {
            json_t *ot = json_get(tools_json, i);
            json_t *fn = json_obj_get(ot, "function");
            if (fn) fd_count++;
        }

        if (fd_count > 0) {
            json_t *tools_arr = json_array();
            json_t *decls = json_array();
            for (size_t i = 0; i < n; i++) {
                json_t *ot = json_get(tools_json, i);
                json_t *fn = json_obj_get(ot, "function");
                if (!fn) continue;

                json_t *fd = json_object();
                json_set(fd, "name", json_copy(json_obj_get(fn, "name")));
                json_set(fd, "description", json_copy(json_obj_get(fn, "description")));

                /* Map OpenAI "parameters" to Google "parameters" (same name, same structure) */
                json_t *params = json_obj_get(fn, "parameters");
                if (params)
                    json_set(fd, "parameters", json_copy(params));

                json_append(decls, fd);
            }
            /* tools: [{"functionDeclarations": [...]}] — array of objects with functionDeclarations key */
            json_t *fd_obj = json_object();
            json_set(fd_obj, "functionDeclarations", decls);
            json_append(tools_arr, fd_obj);
            json_set(root, "tools", tools_arr);
        }
    }

    /* Contents array */
    json_t *contents = json_array();
    if (!contents) { json_free(root); return NULL; }

    /* Track tool call IDs mapping: generated tool_call_id -> function name
     * For Google, functionResponse needs the function name (not an ID), so
     * we store the mapping from tool_call_id to function name */
    int tc_map_count = 0;
    struct {
        char id[64];
        char name[128];
    } tc_map[128];

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

        /* For Google, we need to build "contents" array entries.
         * Roles: "user" and "model" (not "assistant", not "tool").
         * Tool results: functionResponse parts in a user message.
         * Tool calls: functionCall parts in a model message. */

        if (msg->role == MSG_USER) {
            json_t *content = json_object();
            json_set(content, "role", json_string("user"));
            json_t *parts = json_array();

            if (msg->tool_call_id) {
                /* This is a tool result wrapped as user message.
                 * Find the function name from the tool_call_id. */
                const char *fn_name = NULL;
                for (int k = 0; k < tc_map_count; k++) {
                    if (strcmp(tc_map[k].id, msg->tool_call_id) == 0) {
                        fn_name = tc_map[k].name;
                        break;
                    }
                }
                if (!fn_name) fn_name = msg->tool_name ? msg->tool_name : "unknown";

                json_t *fr = json_object();
                json_t *resp_obj = json_object();
                /* Parse result as JSON object if possible */
                char *err = NULL;
                json_t *parsed = NULL;
                if (msg->content)
                    parsed = json_parse(msg->content, &err);
                if (parsed) {
                    json_set(resp_obj, "result", parsed);
                } else {
                    json_set(resp_obj, "result", json_string(msg->content ? msg->content : ""));
                    free(err);
                }
                json_set(fr, "functionResponse", resp_obj);
                /* Set name at outer level of functionResponse part */
                /* Google format: {functionResponse: {name: "...", response: {...}}} */
                json_t *fr_wrapped = json_object();
                json_set(fr_wrapped, "name", json_string(fn_name));
                json_set(fr_wrapped, "response", resp_obj);
                /* Put functionResponse into a part */
                json_t *part = json_object();
                json_set(part, "functionResponse", json_copy(fr_wrapped));
                json_append(parts, part);

                json_free(fr_wrapped);
            } else {
                json_t *part = json_object();
                json_set(part, "text", json_string(msg->content ? msg->content : ""));
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
        else if (msg->role == MSG_ASSISTANT) {
            json_t *content = json_object();
            json_set(content, "role", json_string("model"));
            json_t *parts = json_array();

            /* Text part */
            if (msg->content && msg->content[0]) {
                json_t *part = json_object();
                json_set(part, "text", json_string(msg->content));
                json_append(parts, part);
            }

            /* Function call parts */
            for (int j = 0; j < msg->tool_calls_count; j++) {
                /* Store in map for tool result lookup */
                if (tc_map_count < 128) {
                    snprintf(tc_map[tc_map_count].id, sizeof(tc_map[tc_map_count].id),
                             "%s", msg->tool_calls[j].id);
                    snprintf(tc_map[tc_map_count].name, sizeof(tc_map[tc_map_count].name),
                             "%s", msg->tool_calls[j].name);
                    tc_map_count++;
                }

                json_t *fc = json_object();
                json_set(fc, "name", json_string(msg->tool_calls[j].name));

                /* Parse args as JSON object */
                char *err = NULL;
                json_t *args = json_parse(msg->tool_calls[j].arguments, &err);
                if (args) {
                    json_set(fc, "args", args);
                } else {
                    json_set(fc, "args", json_object());
                    free(err);
                }

                json_t *part = json_object();
                json_set(part, "functionCall", fc);
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
        else if (msg->role == MSG_TOOL) {
            /* MSG_TOOL is a tool result. For Google API, this goes
             * into a "user" role content with functionResponse parts. */
            json_t *content = json_object();
            json_set(content, "role", json_string("user"));
            json_t *parts = json_array();

            const char *fn_name = NULL;
            for (int k = 0; k < tc_map_count; k++) {
                if (strcmp(tc_map[k].id, msg->tool_call_id) == 0) {
                    fn_name = tc_map[k].name;
                    break;
                }
            }
            if (!fn_name) fn_name = msg->tool_name ? msg->tool_name : "unknown";

            json_t *fr_resp = json_object();
            json_t *fr_part = json_object();
            /* Parse result content as JSON */
            char *err = NULL;
            json_t *parsed = json_parse(msg->content ? msg->content : "{}", &err);
            if (parsed) {
                json_set(fr_resp, "response", parsed);
            } else {
                json_t *resp_obj = json_object();
                json_set(resp_obj, "result", json_string(msg->content ? msg->content : ""));
                json_set(fr_resp, "response", resp_obj);
                free(err);
            }
            json_set(fr_part, "name", json_string(fn_name));
            /* Merge name into response */
            json_t *final_resp = json_obj_get(fr_resp, "response");
            if (final_resp) {
                /* Google format expects: {functionResponse: {name: "fn", response: {...}}} */
                json_t *part = json_object();
                json_t *fr = json_object();
                json_set(fr, "name", json_string(fn_name));
                json_set(fr, "response", json_copy(final_resp));
                json_set(part, "functionResponse", fr);
                json_append(parts, part);
            }

            json_set(content, "parts", parts);
            json_append(contents, content);
        }
    }

    if (json_len(contents) == 0) {
        /* Dummy content */
        json_t *dummy = json_object();
        json_set(dummy, "role", json_string("user"));
        json_t *parts = json_array();
        json_t *part = json_object();
        json_set(part, "text", json_string("Hello"));
        json_append(parts, part);
        json_set(dummy, "parts", parts);
        json_append(contents, dummy);
    }

    json_set(root, "contents", contents);

    /* System instruction */
    if (has_system && system_text[0]) {
        json_t *si = json_object();
        json_t *si_parts = json_array();
        json_t *si_part = json_object();
        json_set(si_part, "text", json_string(system_text));
        json_append(si_parts, si_part);
        json_set(si, "parts", si_parts);
        json_set(root, "systemInstruction", si);
    }

    /* Streaming — Google uses a different endpoint for streaming */
    (void)streaming;

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* ================================================================
 *  Response parsing
 * ================================================================ */

static provider_response_t *google_parse_response(const provider_t *p,
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

    /* Check for error */
    json_t *error_obj = json_obj_get(root, "error");
    if (error_obj) {
        const char *err_msg = json_get_str(error_obj, "message", "unknown error");
        int status = (int)json_get_num(error_obj, "code", 0);
        resp->content = (char *)malloc(1024);
        if (resp->content) {
            if (status == 429 && google_is_free_tier_quota_error(err_msg)) {
                snprintf(resp->content, 1024,
                    "Google API free-tier quota exhausted: %s\n\n"
                    "Your API key is on the free tier (<= 250 requests/day). "
                    "Enable billing at https://aistudio.google.com/apikey",
                    err_msg);
            } else {
                snprintf(resp->content, 1024, "Google API error: %s", err_msg);
            }
        }
        json_free(root);
        return resp;
    }

    /* Usage metadata */
    json_t *usage = json_obj_get(root, "usageMetadata");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "promptTokenCount", 0);
        resp->output_tokens = (int)json_get_num(usage, "candidatesTokenCount", 0);
    }

    /* Candidates[0].content.parts[] */
    json_t *candidates = json_obj_get(root, "candidates");
    if (candidates && json_len(candidates) > 0) {
        json_t *candidate = json_get(candidates, 0);
        json_t *content = json_obj_get(candidate, "content");
        if (content) {
            json_t *parts = json_obj_get(content, "parts");
            if (parts && json_len(parts) > 0) {
                size_t n = json_len(parts);

                /* First pass: count text length and tool calls */
                size_t text_len = 0;
                int tc_count = 0;
                for (size_t i = 0; i < n; i++) {
                    json_t *part = json_get(parts, i);
                    if (json_obj_get(part, "text")) {
                        const char *t = json_get_str(part, "text", "");
                        text_len += strlen(t);
                    }
                    if (json_obj_get(part, "functionCall")) {
                        if (tc_count < 64) tc_count++;
                    }
                }

                /* Allocate and fill text */
                if (text_len > 0) {
                    resp->content = (char *)calloc(text_len + 1, 1);
                    if (resp->content) {
                        size_t pos = 0;
                        for (size_t i = 0; i < n; i++) {
                            json_t *part = json_get(parts, i);
                            if (json_obj_get(part, "text")) {
                                const char *t = json_get_str(part, "text", "");
                                size_t add = strlen(t);
                                if (pos + add <= text_len) {
                                    memcpy(resp->content + pos, t, add);
                                    pos += add;
                                }
                            }
                        }
                        resp->content[pos] = '\0';
                    }
                } else {
                    resp->content = strdup("");
                }

                /* Extract tool calls (functionCall parts) */
                if (tc_count > 0) {
                    resp->tool_calls_count = 0;
                    for (size_t i = 0; i < n && resp->tool_calls_count < 64; i++) {
                        json_t *part = json_get(parts, i);
                        json_t *fc = json_obj_get(part, "functionCall");
                        if (!fc) continue;

                        int idx = resp->tool_calls_count;
                        const char *fn_name = json_get_str(fc, "name", "");
                        snprintf(resp->tool_calls[idx].name,
                                 sizeof(resp->tool_calls[idx].name), "%s", fn_name);

                        /* Generate a tool call ID (Google doesn't provide one) */
                        static int g_tc_counter = 0;
                        snprintf(resp->tool_calls[idx].id,
                                 sizeof(resp->tool_calls[idx].id), "call_google_%d",
                                 ++g_tc_counter);

                        /* Serialize args to JSON string */
                        json_t *args = json_obj_get(fc, "args");
                        if (args) {
                            char *args_str = json_serialize(args);
                            if (args_str) {
                                snprintf(resp->tool_calls[idx].arguments,
                                         sizeof(resp->tool_calls[idx].arguments),
                                         "%s", args_str);
                                free(args_str);
                            }
                        }

                        resp->tool_calls_count++;
                    }
                }
            }
        }

        /* Check finish reason */
        const char *finish = json_get_str(candidate, "finishReason", "");
        const char *mapped = google_map_finish_reason(finish);
        snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", mapped);

        /* Handle blocked content (finishReason=SAFETY/BLOCKLIST/etc with no content text) */
        if (!resp->content && mapped && strcmp(mapped, "content_filter") == 0) {
            /* Extract safety ratings for user feedback */
            resp->content = strdup("[Content blocked by Google safety filters]");
        }

        /* If finishReason is "STOP" but we have function calls,
         * that's normal for Google — keep tool calls. */
    }

    json_free(root);
    return resp;
}

/* ================================================================
 *  Streaming chunk parsing
 * ================================================================ */

static provider_response_t *google_parse_stream_chunk(const provider_t *p,
                                                       const char *chunk) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    /* Null-safe */
    if (!chunk) {
        resp->content = strdup("");
        return resp;
    }

    /* Strip "data: " prefix if present (Google SSE) */
    const char *json_str = chunk;
    if (strncmp(chunk, "data: ", 6) == 0)
        json_str = chunk + 6;

    if (!json_str || !*json_str) {
        resp->content = strdup(chunk);
        return resp;
    }

    /* Skip "[DONE]" */
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

    /* Google streaming response structure:
     * {
     *   "candidates": [{
     *     "content": {
     *       "parts": [{"text": "chunk"}]
     *     }
     *   }],
     *   "usageMetadata": {...}
     * } */

    json_t *candidates = json_obj_get(root, "candidates");
    if (candidates && json_len(candidates) > 0) {
        json_t *candidate = json_get(candidates, 0);

        /* Check finish reason FIRST (before content, since final chunk
         * may have finishReason + text content simultaneously) */
        const char *finish = json_get_str(candidate, "finishReason", NULL);
        if (finish) {
            const char *mapped = google_map_finish_reason(finish);
            snprintf(resp->finish_reason, sizeof(resp->finish_reason), "%s", mapped);
            /* Also extract text if present before signaling end */
        }

        json_t *content = json_obj_get(candidate, "content");
        if (content) {
            json_t *parts = json_obj_get(content, "parts");
            if (parts && json_len(parts) > 0) {
                json_t *part = json_get(parts, 0);
                /* Text delta */
                const char *text = json_get_str(part, "text", NULL);
                if (text) {
                    resp->content = strdup(text);
                    if (finish) {
                        /* Final chunk with both finishReason and text */
                        json_free(root);
                        return resp;
                    }
                    json_free(root);
                    return resp;
                }
                /* functionCall delta (may be partial or first chunk) */
                json_t *fc = json_obj_get(part, "functionCall");
                if (fc) {
                    /* Return empty content — streaming accumulates these */
                    resp->content = strdup("");
                    json_free(root);
                    return resp;
                }
            }
        }

        if (finish) {
            /* finishReason but no content → signal end of stream */
            resp->content = strdup("");
            json_free(root);
            return resp;
        }
    }

    /* Usage metadata in last chunk */
    json_t *usage = json_obj_get(root, "usageMetadata");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "promptTokenCount", 0);
        resp->output_tokens = (int)json_get_num(usage, "candidatesTokenCount", 0);
    }

    resp->content = strdup("");
    json_free(root);
    return resp;
}

/* ================================================================
 *  Free response
 * ================================================================ */

static void google_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Google provider utility functions — ported from Python
 *  gemini_native_adapter.py
 * ================================================================ */

/* Check if a base URL speaks Gemini's native REST API.
 * Returns true when the URL contains "generativelanguage.googleapis.com"
 * and does NOT end with "/openai" (OpenAI-compat endpoint). */
bool google_is_native_base_url(const char *base_url) {
    if (!base_url || !*base_url) return false;

    /* Normalize: strip, lowercase */
    char buf[512];
    size_t len = 0;
    const char *p = base_url;
    while (*p && len < sizeof(buf) - 1) {
        buf[len++] = (char)tolower((unsigned char)*p);
        p++;
    }
    buf[len] = '\0';

    /* Trim trailing whitespace */
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    /* Trim trailing slashes */
    while (len > 0 && buf[len - 1] == '/') buf[--len] = '\0';

    if (len == 0) return false;
    if (!strstr(buf, "generativelanguage.googleapis.com")) return false;

    /* Check it doesn't end with /openai (the OpenAI-compat endpoint) */
    if (len >= 7 && strcmp(buf + len - 7, "/openai") == 0) return false;

    return true;
}

/* Port of Python gemini_native_adapter._coerce_content_to_text().
 * Extracts text from a Gemini message content value.
 * Handles: NULL/JSON_NULL → "", string → copy, array → join text parts,
 * object with type=="text" → extract text field.
 * Returns malloc'd string, caller must free. */
char *google_coerce_content_to_text(const json_t *content) {
    if (!content || content->type == JSON_NULL) return strdup("");

    /* String: return content directly */
    if (content->type == JSON_STRING)
        return strdup(content->str_val ? content->str_val : "");

    /* Array: iterate parts, collect text pieces */
    if (content->type == JSON_ARRAY) {
        char *pieces[256];
        int n = 0;
        size_t total = 0;

        for (size_t i = 0; i < content->c.count && n < 256; i++) {
            json_t *item = content->c.items[i];
            if (!item) continue;

            if (item->type == JSON_STRING) {
                const char *s = item->str_val ? item->str_val : "";
                pieces[n] = strdup(s);
                if (pieces[n]) { total += strlen(s); n++; }
            } else if (item->type == JSON_OBJECT) {
                const char *type_str = json_get_str(item, "type", "");
                if (strcmp(type_str, "text") == 0) {
                    const char *text = json_get_str(item, "text", "");
                    pieces[n] = strdup(text);
                    if (pieces[n]) { total += strlen(text); n++; }
                }
            }
        }

        if (n == 0) return strdup("");

        size_t needed = total + (n > 0 ? (size_t)(n - 1) : 0) + 1;
        char *result = (char *)malloc(needed);
        if (!result) {
            for (int i = 0; i < n; i++) free(pieces[i]);
            return NULL;
        }
        result[0] = '\0';
        for (int i = 0; i < n; i++) {
            if (i > 0) strcat(result, "\n");
            strcat(result, pieces[i]);
            free(pieces[i]);
        }
        return result;
    }

    return strdup("");
}

/* Port of Python gemini_native_adapter._tool_call_extra_signature().
 * Extracts thought signature from tool_call.extra_content.google(.thought_signature)
 * or tool_call.extra_content.thought_signature. Returns malloc'd string or NULL. */
char *google_tool_call_extra_signature(const json_t *tool_call) {
    if (!tool_call) return NULL;
    json_t *extra = json_obj_get(tool_call, "extra_content");
    if (!extra) return NULL;

    json_t *google = json_obj_get(extra, "google");
    if (!google) google = json_obj_get(extra, "thought_signature");
    if (!google) return NULL;

    if (google->type == JSON_OBJECT) {
        json_t *sig = json_obj_get(google, "thought_signature");
        if (!sig) sig = json_obj_get(google, "thoughtSignature");
        if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
            return strdup(sig->str_val);
    } else if (google->type == JSON_STRING && google->str_val && google->str_val[0]) {
        return strdup(google->str_val);
    }
    return NULL;
}

/* Port of Python gemini_native_adapter._translate_tool_call_to_gemini().
 * Translates an OpenAI-format tool_call to a Gemini functionCall part.
 * Returns a json_t object: {functionCall: {name, args}} with optional thoughtSignature. */
json_t *google_translate_tool_call(const json_t *tool_call) {
    json_t *fc = json_object();
    json_t *fn = NULL;
    const char *name = "";
    const char *args_raw = "";

    if (tool_call) {
        fn = json_obj_get(tool_call, "function");
        if (fn) {
            name = json_get_str(fn, "name", "");
            args_raw = json_get_str(fn, "arguments", "");
        }
    }

    json_set(fc, "name", json_string(name));

    /* Parse arguments as JSON — use args_raw already extracted above */
    if (args_raw && *args_raw) {
        char *err = NULL;
        json_t *args = json_parse(args_raw, &err);
        if (args) {
            json_set(fc, "args", args);
        } else {
            json_set(fc, "args", json_object());
            free(err);
        }
    } else {
        json_set(fc, "args", json_object());
    }

    json_t *result = json_object();
    json_set(result, "functionCall", fc);

    /* Optional thought signature */
    char *sig = google_tool_call_extra_signature(tool_call);
    if (sig) {
        json_set(result, "thoughtSignature", json_string(sig));
        free(sig);
    }

    return result;
}

/* ================================================================
 *  Free response
 * ================================================================ */

const provider_ops_t PROVIDER_OPS_GOOGLE = {
    .build_url = google_build_url,
    .build_headers = google_build_headers,
    .build_request_body = google_build_request_body,
    .parse_response = google_parse_response,
    .parse_stream_chunk = google_parse_stream_chunk,
    .free_response = google_free_response,
    .name = "google"
};
