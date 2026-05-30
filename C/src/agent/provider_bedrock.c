/*
 * provider_bedrock.c — AWS Bedrock Converse API provider (P77).
 *
 * Uses AWS Signature V4 (SigV4) for authentication and the Bedrock
 * Converse API for chat completions.
 *
 * Dependencies: OpenSSL (libcrypto) for SHA-256 and HMAC-SHA256.
 *
 * Config:
 *   - base_url: https://bedrock-runtime.{region}.amazonaws.com (region auto-detected)
 *   - api_key: not used (AWS credentials from env AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY)
 *   - model: Bedrock model ID e.g. "anthropic.claude-3-sonnet-20240229-v1:0"
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "provider.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>

/* ================================================================
 *  SigV4 helpers
 * ================================================================ */

/* SHA-256 hex digest of a string */
static char *sha256_hex(const char *data, size_t len) {
    unsigned char hash[32];
    unsigned int hash_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    static const char hex[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (!out) return NULL;
    for (unsigned int i = 0; i < hash_len; i++) {
        out[i*2]     = hex[hash[i] >> 4];
        out[i*2+1]   = hex[hash[i] & 0xf];
    }
    out[64] = '\0';
    return out;
}

/* HMAC-SHA256, returns malloc'd binary */
static unsigned char *hmac_sha256(const unsigned char *key, size_t key_len,
                                   const char *data, size_t data_len,
                                   unsigned int *out_len) {
    unsigned char *result = (unsigned char *)malloc(32);
    if (!result) return NULL;
    HMAC(EVP_sha256(), key, (int)key_len,
         (const unsigned char *)data, data_len,
         result, out_len);
    return result;
}

/* Hex-encode binary data */
static char *hex_encode(const unsigned char *data, unsigned int len) {
    static const char hex[] = "0123456789abcdef";
    char *out = (char *)malloc(len * 2 + 1);
    if (!out) return NULL;
    for (unsigned int i = 0; i < len; i++) {
        out[i*2]   = hex[data[i] >> 4];
        out[i*2+1] = hex[data[i] & 0xf];
    }
    out[len*2] = '\0';
    return out;
}

/* Get current UTC time in ISO8601 format: YYYYMMDDTHHMMSSZ */
static void get_iso8601_time(char *buf, size_t buf_size) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, buf_size, "%Y%m%dT%H%M%SZ", &tm);
}

/* Get date stamp from ISO time: YYYYMMDD */
static void get_date_stamp(const char *iso_time, char *buf, size_t buf_size) {
    (void)buf_size;
    memcpy(buf, iso_time, 8);
    buf[8] = '\0';
}

/* URI encode a string for SigV4 canonical request */
static char *uri_encode(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = c;
        } else if (c == '/') {
            out[j++] = c;
        } else {
            snprintf(out + j, 4, "%%%02X", c);
            j += 3;
        }
    }
    out[j] = '\0';
    return out;
}

/* Build SigV4 Authorization header value and return full header string */
static char *sigv4_sign(const char *method, const char *host, const char *path,
                         const char *query_string, const char *payload,
                         const char *access_key, const char *secret_key,
                         const char *region, const char *service,
                         char *amz_date_out, size_t amz_date_sz) {
    char amz_date[17];
    get_iso8601_time(amz_date, sizeof(amz_date));
    if (amz_date_out) snprintf(amz_date_out, amz_date_sz, "%s", amz_date);

    char date_stamp[9];
    get_date_stamp(amz_date, date_stamp, sizeof(date_stamp));

    /* Payload hash */
    char *payload_hash = sha256_hex(payload, strlen(payload));
    if (!payload_hash) return NULL;

    /* Canonical URI — must be URI-encoded path */
    char *canonical_uri = uri_encode(path);

    /* Canonical query string */
    const char *canonical_qs = query_string ? query_string : "";

    /* Canonical headers: host;x-amz-date */
    char host_header[512];
    snprintf(host_header, sizeof(host_header), "host:%s", host);

    char amz_date_header[64];
    snprintf(amz_date_header, sizeof(amz_date_header), "x-amz-date:%s", amz_date);

    const char *signed_headers = "host;x-amz-date";

    /* Build canonical request */
    size_t cr_size = strlen(method) + strlen(canonical_uri) + strlen(canonical_qs)
                     + strlen(host_header) + strlen(amz_date_header)
                     + strlen(signed_headers) + strlen(payload_hash) + 32;
    char *canonical_request = (char *)malloc(cr_size);
    if (!canonical_request) { free(payload_hash); free(canonical_uri); return NULL; }

    snprintf(canonical_request, cr_size,
        "%s\n%s\n%s\n%s\n%s\n\n%s\n%s",
        method, canonical_uri, canonical_qs,
        host_header, amz_date_header,
        signed_headers, payload_hash);

    free(payload_hash);
    free(canonical_uri);

    /* String to sign */
    char *cr_hash = sha256_hex(canonical_request, strlen(canonical_request));
    free(canonical_request);
    if (!cr_hash) return NULL;

    char scope[128];
    snprintf(scope, sizeof(scope), "%s/%s/%s/aws4_request", date_stamp, region, service);

    size_t sts_size = strlen(cr_hash) + strlen(scope) + 64;
    char *string_to_sign = (char *)malloc(sts_size);
    if (!string_to_sign) { free(cr_hash); return NULL; }

    snprintf(string_to_sign, sts_size,
        "AWS4-HMAC-SHA256\n%s\n%s\n%s",
        amz_date, scope, cr_hash);
    free(cr_hash);

    /* Signing key */
    char kSecret[64];
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", secret_key);

    unsigned int hash_len = 0;
    unsigned char *kDate = hmac_sha256((unsigned char *)kSecret, strlen(kSecret),
                                        date_stamp, strlen(date_stamp), &hash_len);
    if (!kDate) { free(string_to_sign); return NULL; }

    unsigned char *kRegion = hmac_sha256(kDate, hash_len, region, strlen(region), &hash_len);
    free(kDate);
    if (!kRegion) { free(string_to_sign); return NULL; }

    unsigned char *kService = hmac_sha256(kRegion, hash_len, service, strlen(service), &hash_len);
    free(kRegion);
    if (!kService) { free(string_to_sign); return NULL; }

    unsigned char *kSigning = hmac_sha256(kService, hash_len, "aws4_request", 12, &hash_len);
    free(kService);
    if (!kSigning) { free(string_to_sign); return NULL; }

    /* Signature */
    unsigned char *signature_bin = hmac_sha256(kSigning, hash_len,
                                                string_to_sign, strlen(string_to_sign),
                                                &hash_len);
    free(kSigning);
    free(string_to_sign);
    if (!signature_bin) return NULL;

    char *signature_hex = hex_encode(signature_bin, hash_len);
    free(signature_bin);
    if (!signature_hex) return NULL;

    /* Build full Authorization header value */
    char *auth_header = (char *)malloc(1024);
    if (!auth_header) { free(signature_hex); return NULL; }

    snprintf(auth_header, 1024,
        "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s",
        access_key, scope, signed_headers, signature_hex);
    free(signature_hex);

    /* Build full header string for http_request */
    char *full_headers = (char *)malloc(2048);
    if (!full_headers) { free(auth_header); return NULL; }

    snprintf(full_headers, 2048,
        "Content-Type: application/json\r\n"
        "X-Amz-Date: %s\r\n"
        "Authorization: %s\r\n"
        "Accept: application/json",
        amz_date, auth_header);
    free(auth_header);

    return full_headers;
}

/* Extract region from base_url or default */
static const char *bedrock_get_region(const char *base_url) {
    if (!base_url || !*base_url) {
        const char *env = getenv("AWS_REGION");
        if (env && *env) return env;
        env = getenv("AWS_DEFAULT_REGION");
        if (env && *env) return env;
        return "us-east-1";
    }
    /* Try to extract from URL: bedrock-runtime.{region}.amazonaws.com */
    const char *r = strstr(base_url, "bedrock-runtime.");
    if (r) {
        r += 16; /* skip "bedrock-runtime." */
        const char *dot = strchr(r, '.');
        if (dot) {
            static char region_buf[64];
            size_t len = dot - r;
            if (len < sizeof(region_buf)) {
                strncpy(region_buf, r, len);
                region_buf[len] = '\0';
                return region_buf;
            }
        }
    }
    return "us-east-1";
}

/* ================================================================
 *  Bedrock provider implementation
 * ================================================================ */

static char *bedrock_build_url(const provider_t *p, const char *base_url) {
    (void)base_url;
    const char *region = bedrock_get_region(NULL);
    const char *model = p->model[0] ? p->model : "anthropic.claude-3-sonnet-20240229-v1:0";

    size_t url_len = strlen(region) + strlen(model) + 128;
    char *url = (char *)malloc(url_len);
    if (!url) return NULL;

    snprintf(url, url_len,
             "https://bedrock-runtime.%s.amazonaws.com/model/%s/converse",
             region, model);
    return url;
}

/* Bedrock uses AWS credentials from env, not api_key field */
static char *bedrock_build_headers(const provider_t *p, const char *api_key) {
    (void)p;
    (void)api_key;

    const char *ak = getenv("AWS_ACCESS_KEY_ID");
    const char *sk = getenv("AWS_SECRET_ACCESS_KEY");
    if (!ak || !sk) {
        char *h = (char *)malloc(128);
        if (h) snprintf(h, 128, "Content-Type: application/json\\r\\nAccept: application/json");
        return h;
    }

    const char *region = bedrock_get_region(NULL);
    const char *model = p->model[0] ? p->model : "anthropic.claude-3-sonnet-20240229-v1:0";

    char path[512];
    snprintf(path, sizeof(path), "/model/%s/converse", model);

    char host[256];
    snprintf(host, sizeof(host), "bedrock-runtime.%s.amazonaws.com", region);

    char amz_date[17];
    return sigv4_sign("POST", host, path, "", "",
                      ak, sk, region, "bedrock",
                      amz_date, sizeof(amz_date));
}

/* Bedrock Converse API body format
 * {
 *   "system": [{ "text": "..." }],
 *   "messages": [{ "role": "user", "content": [{ "text": "..." }] }],
 *   "inferenceConfig": { "maxTokens": 4096, "temperature": 0.7 },
 *   "toolConfig": { "tools": [...] }
 * }
 */
static char *bedrock_build_request_body(const provider_t *p,
                                         const message_t **messages, size_t msg_count,
                                         json_node_t *tools_json, bool streaming) {
    (void)streaming; /* Bedrock Converse doesn't support SSE streaming */
    (void)p;

    json_t *root = json_new_object();
    if (!root) return NULL;

    /* System prompt goes in system array, not messages */
    json_t *sys_prompts = NULL;
    json_t *msgs = json_new_array();
    if (!msgs) { json_free(root); return NULL; }

    for (size_t i = 0; i < msg_count; i++) {
        if (messages[i]->role == MSG_SYSTEM) {
            if (!sys_prompts) {
                sys_prompts = json_new_array();
                if (!sys_prompts) { json_free(root); json_free(msgs); return NULL; }
            }
            json_t *sys_entry = json_new_object();
            json_object_set(sys_entry, "text",
                json_new_string(messages[i]->content ? messages[i]->content : ""));
            json_array_append(sys_prompts, sys_entry);
            continue;
        }

        json_t *msg = json_new_object();
        const char *role_str;
        switch (messages[i]->role) {
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "assistant"; break; /* Bedrock uses assistant for tool results */
            default:            role_str = "user";      break;
        }
        json_object_set(msg, "role", json_new_string(role_str));

        /* Content blocks array */
        json_t *content = json_new_array();
        if (messages[i]->role == MSG_TOOL) {
            /* Tool result */
            json_t *tr = json_new_object();
            json_object_set(tr, "toolResult", json_new_object());
            json_t *tr_content = json_new_array();
            json_t *tr_text = json_new_object();
            json_object_set(tr_text, "text",
                json_new_string(messages[i]->content ? messages[i]->content : ""));
            json_array_append(tr_content, tr_text);
            /* Bedrock toolResult doesn't have a direct toolUseId field here
               — in practice, tool results come inline */
            json_array_append(content, tr);
        } else {
            json_t *text_block = json_new_object();
            json_object_set(text_block, "text",
                json_new_string(messages[i]->content ? messages[i]->content : ""));
            json_array_append(content, text_block);
        }

        /* Tool calls from assistant */
        if (messages[i]->role == MSG_ASSISTANT && messages[i]->tool_calls_count > 0) {
            for (int j = 0; j < messages[i]->tool_calls_count && j < 64; j++) {
                json_t *tc_block = json_new_object();
                json_t *tool_use = json_new_object();
                json_object_set(tool_use, "toolUseId",
                    json_new_string(messages[i]->tool_calls[j].id));
                json_object_set(tool_use, "name",
                    json_new_string(messages[i]->tool_calls[j].name));
                json_object_set(tool_use, "input",
                    json_new_string(messages[i]->tool_calls[j].arguments));
                json_object_set(tc_block, "toolUse", tool_use);
                json_array_append(content, tc_block);
            }
        }

        json_object_set(msg, "content", content);
        json_array_append(msgs, msg);
    }

    json_object_set(root, "messages", msgs);
    if (sys_prompts)
        json_object_set(root, "system", sys_prompts);

    /* Inference config */
    json_t *inf_config = json_new_object();
    int max_tok = p->config.max_tokens > 0 ? p->config.max_tokens : 4096;
    json_object_set(inf_config, "maxTokens", json_new_number(max_tok));
    float temp = p->config.temperature >= 0.0f ? p->config.temperature : 0.7f;
    json_object_set(inf_config, "temperature", json_new_number(temp));
    if (p->config.top_p > 0.0f && p->config.top_p < 1.0f)
        json_object_set(inf_config, "topP", json_new_number(p->config.top_p));
    if (p->config.stop_count > 0) {
        json_t *stop_arr = json_new_array();
        for (int i = 0; i < p->config.stop_count && i < HERMES_STOP_SEQUENCES_MAX; i++)
            if (p->config.stop_sequences[i][0])
                json_array_append(stop_arr, json_new_string(p->config.stop_sequences[i]));
        if (json_array_count(stop_arr) > 0) json_object_set(inf_config, "stopSequences", stop_arr);
        else json_free(stop_arr);
    }
    json_object_set(root, "inferenceConfig", inf_config);

    /* B39: Bedrock inference profile */
    if (p->config.bedrock_inference_profile[0])
        json_object_set(root, "inferenceProfile", json_new_string(p->config.bedrock_inference_profile));

    /* B40: Bedrock guardrail config */
    if (p->config.bedrock_guardrail_config[0]) {
        json_t *gc = json_parse(p->config.bedrock_guardrail_config, NULL);
        if (gc && gc->type == JSON_OBJECT) {
            json_object_set(root, "guardrailConfig", gc);
        } else {
            json_free(gc);
        }
    }

    /* B41: Bedrock trace (enableTrace in request body) */
    if (p->config.bedrock_trace_enabled)
        json_object_set(root, "enableTrace", json_new_bool(true));

    /* response_format + metadata */
    if (p->config.response_format[0]) {
        json_t *rf = json_parse(p->config.response_format, NULL);
        if (rf) { json_object_set(root, "response_format", json_copy(rf)); json_free(rf); }
    } else if (p->config.json_mode) {
        json_t *rf = json_new_object();
        json_object_set(rf, "type", json_new_string("json_object"));
        json_object_set(root, "response_format", rf);
    }
    if (p->config.metadata[0]) {
        json_t *md = json_parse(p->config.metadata, NULL);
        if (md) { json_object_set(root, "metadata", json_copy(md)); json_free(md); }
    }

    /* tool_choice + parallel_tool_calls */
    if (p->config.tool_choice[0]) {
        json_t *tc = json_parse(p->config.tool_choice, NULL);
        if (tc) { json_object_set(root, "tool_choice", json_copy(tc)); json_free(tc); }
        else { json_object_set(root, "tool_choice", json_new_string(p->config.tool_choice)); }
    }
    if (!p->config.parallel_tool_calls)
        json_object_set(root, "parallel_tool_calls", json_new_bool(false));

    /* Tool config */
    if (tools_json && json_array_count(tools_json) > 0) {
        json_t *tool_config = json_new_object();
        json_object_set(tool_config, "tools", json_copy(tools_json));
        json_object_set(root, "toolConfig", tool_config);
    }

    char *body = json_serialize(root);
    json_free(root);
    return body;
}

/* Parse Bedrock Converse response
 * {
 *   "output": {
 *     "message": {
 *       "role": "assistant",
 *       "content": [{ "text": "...", "toolUse": { ... } }]
 *     }
 *   },
 *   "stopReason": "end_turn",
 *   "usage": { "inputTokens": N, "outputTokens": N }
 * }
 */
static provider_response_t *bedrock_parse_response(const provider_t *p,
                                                    const char *response_body) {
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;

    char *err = NULL;
    json_t *root = json_parse(response_body, &err);
    if (!root) {
        /* Check for error response */
        if (response_body) {
            resp->content = strdup(response_body);
        } else {
            resp->content = strdup("empty response");
        }
        free(err);
        return resp;
    }

    /* Usage */
    json_t *usage = json_object_get(root, "usage");
    if (usage) {
        resp->input_tokens = (int)json_get_num(usage, "inputTokens", 0);
        resp->output_tokens = (int)json_get_num(usage, "outputTokens", 0);
    }
    /* Check for Bedrock API error response (no "output" key = error) */
    json_t *output = json_object_get(root, "output");
    if (!output) {
        /* Check for raw message field (Bedrock API error format) */
        const char *msg = json_get_str(root, "message", NULL);
        if (msg) {
            resp->content = strdup(msg);
        }
        /* Check for wrapped error object (e.g. {"error":{"message":"..."}}) */
        json_t *err_obj = json_object_get(root, "error");
        if (err_obj) {
            const char *err_msg = json_get_str(err_obj, "message", NULL);
            if (err_msg) resp->content = strdup(err_msg);
        }
        json_free(root);
        return resp;
    }
    if (output) {
        json_t *message = json_object_get(output, "message");
        if (message) {
            /* Extract and map stopReason to OpenAI finish_reason */
            const char *stop_reason = json_get_str(root, "stopReason", NULL);
            if (stop_reason) {
                if (strcmp(stop_reason, "end_turn") == 0 || strcmp(stop_reason, "stop_sequence") == 0)
                    snprintf(resp->finish_reason, sizeof(resp->finish_reason), "stop");
                else if (strcmp(stop_reason, "tool_use") == 0)
                    snprintf(resp->finish_reason, sizeof(resp->finish_reason), "tool_calls");
                else if (strcmp(stop_reason, "max_tokens") == 0)
                    snprintf(resp->finish_reason, sizeof(resp->finish_reason), "length");
                else if (strcmp(stop_reason, "content_filtered") == 0 || strcmp(stop_reason, "guardrail_intervened") == 0)
                    snprintf(resp->finish_reason, sizeof(resp->finish_reason), "content_filter");
                else
                    snprintf(resp->finish_reason, sizeof(resp->finish_reason), "stop");
            }
            json_t *content = json_object_get(message, "content");
            if (content && json_len(content) > 0) {
                /* Concatenate all text blocks */
                size_t total = 0;
                for (int i = 0; i < (int)json_len(content); i++) {
                    json_t *block = json_get(content, i);
                    const char *text = json_get_str(block, "text", NULL);
                    if (text) total += strlen(text);
                    /* Count tool uses */
                    json_t *tu = json_object_get(block, "toolUse");
                    if (tu && resp->tool_calls_count < 64) {
                        int idx = resp->tool_calls_count++;
                        snprintf(resp->tool_calls[idx].id,
                                 sizeof(resp->tool_calls[idx].id), "%s",
                                 json_get_str(tu, "toolUseId", ""));
                        snprintf(resp->tool_calls[idx].name,
                                 sizeof(resp->tool_calls[idx].name), "%s",
                                 json_get_str(tu, "name", ""));
                        /* Serialize input object to JSON string */
                        json_t *input = json_obj_get(tu, "input");
                        if (input) {
                            char *args_str = json_serialize(input);
                            if (args_str) {
                                snprintf(resp->tool_calls[idx].arguments,
                                         sizeof(resp->tool_calls[idx].arguments), "%s", args_str);
                                free(args_str);
                            } else {
                                snprintf(resp->tool_calls[idx].arguments,
                                         sizeof(resp->tool_calls[idx].arguments), "{}");
                            }
                        } else {
                            snprintf(resp->tool_calls[idx].arguments,
                                     sizeof(resp->tool_calls[idx].arguments), "{}");
                        }
                    }
                }
                if (total > 0) {
                    resp->content = (char *)calloc(1, total + 1);
                    if (resp->content) {
                        size_t pos = 0;
                        for (int i = 0; i < (int)json_len(content); i++) {
                            json_t *block = json_get(content, i);
                            const char *text = json_get_str(block, "text", NULL);
                            if (text) {
                                size_t len = strlen(text);
                                memcpy(resp->content + pos, text, len);
                                pos += len;
                            }
                        }
                    }
                } else {
                    resp->content = strdup("");
                }
            }
        }
    }

    /* Error handling */
    json_t *error = json_object_get(root, "error");
    if (error && !resp->content) {
        resp->content = strdup(json_get_str(error, "message", "Bedrock API error"));
    }

    json_free(root);
    return resp;
}

static provider_response_t *bedrock_parse_stream_chunk(const provider_t *p,
                                                        const char *chunk) {
    /* Bedrock Converse API does not support SSE streaming natively.
     * For streaming, Bedrock uses response streaming via the InvokeModelWithResponseStream
     * endpoint, which returns chunks in a different format.
     * For now, return as plain text for non-streaming fallback. */
    (void)p;
    provider_response_t *resp = (provider_response_t *)calloc(1, sizeof(*resp));
    if (!resp) return NULL;
    resp->content = strdup(chunk ? chunk : "");
    return resp;
}

static void bedrock_free_response(provider_response_t *resp) {
    if (!resp) return;
    free(resp->content);
    free(resp->reasoning);
    free(resp);
}

/* ================================================================
 *  Bedrock utility functions — ported from Python bedrock_adapter.py
 * ================================================================ */

/* Context overflow patterns — check if error indicates input too long */
bool bedrock_is_context_overflow(const char *error_message) {
    if (!error_message) return false;

    /* Pattern 1: ValidationException + input is too long / max input token / input token exceed */
    bool has_validation = (strstr(error_message, "ValidationException") != NULL ||
                          strstr(error_message, "validation error") != NULL);
    if (has_validation) {
        bool has_too_long = (strstr(error_message, "input is too long") != NULL);
        bool has_max_input = (strstr(error_message, "max input token") != NULL ||
                             strstr(error_message, "maximum input token") != NULL);
        bool has_input_exceed = (strstr(error_message, "input token") != NULL &&
                                strstr(error_message, "exceed") != NULL);
        if (has_too_long || has_max_input || has_input_exceed)
            return true;

        /* Pattern 2: ValidationException + exceeds the maximum/max tokens */
        bool has_exceed = (strstr(error_message, "exceed") != NULL);
        bool has_max_tok = (strstr(error_message, "maximum token") != NULL ||
                           strstr(error_message, "max token") != NULL ||
                           strstr(error_message, "maximum number of token") != NULL ||
                           strstr(error_message, "maximum input token") != NULL);
        if (has_exceed && has_max_tok)
            return true;
    }

    /* Pattern 3: ModelStreamErrorException + Input is too long / too many input tokens */
    bool has_stream_exc = (strstr(error_message, "ModelStreamErrorException") != NULL ||
                          strstr(error_message, "stream error") != NULL);
    if (has_stream_exc) {
        if (strstr(error_message, "Input is too long") != NULL ||
            strstr(error_message, "too many input token") != NULL)
            return true;
    }

    return false;
}

/* Static patterns for throttling */
static bool has_throttle_pattern(const char *msg) {
    return (strstr(msg, "ThrottlingException") != NULL ||
            strstr(msg, "Too many concurrent requests") != NULL ||
            strstr(msg, "ServiceQuotaExceededException") != NULL);
}

/* Static patterns for overload */
static bool has_overload_pattern(const char *msg) {
    return (strstr(msg, "ModelNotReadyException") != NULL ||
            strstr(msg, "ModelTimeoutException") != NULL ||
            strstr(msg, "InternalServerException") != NULL ||
            strstr(msg, "ServiceUnavailableException") != NULL);
}

/* Classify a Bedrock error for retry/failover decisions */
const char *bedrock_classify_error(const char *error_message) {
    if (!error_message) return "unknown";

    if (bedrock_is_context_overflow(error_message))
        return "context_overflow";
    if (has_throttle_pattern(error_message))
        return "rate_limit";
    if (has_overload_pattern(error_message))
        return "overloaded";
    return "unknown";
}

/* Extract provider name from a Bedrock model ARN.
 * Example: "arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-v2" → "anthropic" */
char *bedrock_extract_provider_from_arn(const char *arn) {
    if (!arn) return NULL;
    const char *prefix = "foundation-model/";
    const char *start = strstr(arn, prefix);
    if (!start) return NULL;
    start += strlen(prefix);
    const char *dot = strchr(start, '.');
    if (!dot || dot == start) return NULL;
    return strndup(start, (size_t)(dot - start));
}

/* Bedrock model context length table — ported from Python bedrock_adapter.py */
static const struct {
    const char *key;
    int length;
} BEDROCK_CONTEXT_TABLE[] = {
    {"anthropic.claude-opus-4-6",      200000},
    {"anthropic.claude-sonnet-4-6",    200000},
    {"anthropic.claude-sonnet-4-5",    200000},
    {"anthropic.claude-haiku-4-5",     200000},
    {"anthropic.claude-opus-4",        200000},
    {"anthropic.claude-sonnet-4",      200000},
    {"anthropic.claude-3-5-sonnet",    200000},
    {"anthropic.claude-3-5-haiku",     200000},
    {"anthropic.claude-3-opus",        200000},
    {"anthropic.claude-3-sonnet",      200000},
    {"amazon.nova-pro",                300000},
    {"amazon.nova-lite",               300000},
    {"amazon.nova-micro",              128000},
    {"meta.llama4-maverick",           128000},
    {"meta.llama4-scout",              128000},
    {"meta.llama3-3-70b-instruct",     128000},
    {"mistral.mistral-large",          128000},
    {"deepseek.v3",                    128000},
    {NULL, 128000} /* sentinel — default */
};

/* Look up the context window size for a Bedrock model using substring matching.
 * Longest matching key wins (handles version suffixes like -v1:0). */
int bedrock_get_context_length(const char *model_id) {
    if (!model_id) return 128000;
    size_t best_len = 0;
    int best_val = 128000;
    for (int i = 0; BEDROCK_CONTEXT_TABLE[i].key != NULL; i++) {
        if (strstr(model_id, BEDROCK_CONTEXT_TABLE[i].key)) {
            size_t klen = strlen(BEDROCK_CONTEXT_TABLE[i].key);
            if (klen > best_len) {
                best_len = klen;
                best_val = BEDROCK_CONTEXT_TABLE[i].length;
            }
        }
    }
    return best_val;
}

/* ---- bedrock_is_anthropic_model ---- */
/* Port of Python bedrock_adapter.is_anthropic_bedrock_model().
 * Checks if model_id is an Anthropic Claude model on Bedrock.
 * Strips regional prefixes (us., global., eu., ap., jp.) before matching. */
bool bedrock_is_anthropic_model(const char *model_id) {
    if (!model_id) return false;
    size_t len = strlen(model_id);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++)
        lower[i] = tolower((unsigned char)model_id[i]);
    lower[len] = '\0';
    const char *prefixes[] = {"us.", "global.", "eu.", "ap.", "jp.", NULL};
    const char *p = lower;
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(p, prefixes[i], plen) == 0) { p += plen; break; }
    }
    bool result = (strncmp(p, "anthropic.claude", 16) == 0);
    free(lower);
    return result;
}

/* ---- bedrock_model_supports_tool_use ---- */
/* Port of Python bedrock_adapter._model_supports_tool_use().
 * Checks 5 denylist patterns; unknown models default to True. */
bool bedrock_model_supports_tool_use(const char *model_id) {
    if (!model_id) return false;
    size_t len = strlen(model_id);
    char *lower = malloc(len + 1);
    if (!lower) return true;
    for (size_t i = 0; i < len; i++)
        lower[i] = tolower((unsigned char)model_id[i]);
    lower[len] = '\0';
    const char *denylist[] = {
        "deepseek.r1", "deepseek-r1", "stability.",
        "cohere.embed", "amazon.titan-embed", NULL
    };
    bool blocked = false;
    for (int i = 0; denylist[i]; i++) {
        if (strstr(lower, denylist[i])) { blocked = true; break; }
    }
    free(lower);
    return !blocked;
}

/* ---- bedrock_resolve_auth_env_var ---- */
/* Port of Python bedrock_adapter.resolve_aws_auth_env_var().
 * Checks 5 AWS credential env vars in priority order.
 * Returns the env var name, or NULL if none found. */
const char *bedrock_resolve_auth_env_var(void) {
    const char *v;
    v = getenv("AWS_BEARER_TOKEN_BEDROCK");
    if (v && v[0]) return "AWS_BEARER_TOKEN_BEDROCK";
    v = getenv("AWS_ACCESS_KEY_ID");
    if (v && v[0] && getenv("AWS_SECRET_ACCESS_KEY") && getenv("AWS_SECRET_ACCESS_KEY")[0])
        return "AWS_ACCESS_KEY_ID";
    v = getenv("AWS_PROFILE");
    if (v && v[0]) return "AWS_PROFILE";
    v = getenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
    if (v && v[0]) return "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI";
    v = getenv("AWS_WEB_IDENTITY_TOKEN_FILE");
    if (v && v[0]) return "AWS_WEB_IDENTITY_TOKEN_FILE";
    return NULL;
}

/* ---- bedrock_has_credentials ---- */
/* Port of Python bedrock_adapter.has_aws_credentials().
 * Returns true if any AWS credential env var is set.
 * Note: unlike Python, this does NOT fall back to boto3/IMDS. */
bool bedrock_has_credentials(void) {
    return bedrock_resolve_auth_env_var() != NULL;
}

/* ---- bedrock_resolve_region ---- */
/* Port of Python bedrock_adapter.resolve_bedrock_region().
 * Reads AWS_REGION or AWS_DEFAULT_REGION, falls back to "us-east-1".
 * Note: unlike Python, does NOT fall back to boto3 session config. */
const char *bedrock_resolve_region(void) {
    const char *v = getenv("AWS_REGION");
    if (v && v[0]) return v;
    v = getenv("AWS_DEFAULT_REGION");
    if (v && v[0]) return v;
    return "us-east-1";
}

/* ---- bedrock_convert_tools_to_converse ---- */
/* Port of Python bedrock_adapter.convert_tools_to_converse().
 * Converts OpenAI-format tool definitions array to Bedrock Converse
 * toolSpec format: {function: {name, description, parameters}}
 * becomes {toolSpec: {name, description, inputSchema: {json: params}}}.
 * Returns json_t* array suitable for toolConfig.tools. Caller must free(). */
json_t *bedrock_convert_tools_to_converse(const json_t *tools) {
    if (!tools || tools->type != JSON_ARRAY || tools->c.count == 0)
        return json_array();

    json_t *result = json_array();
    if (!result) return NULL;

    for (size_t i = 0; i < tools->c.count; i++) {
        json_t *t = tools->c.items[i];
        if (!t || t->type != JSON_OBJECT) continue;

        /* Extract function sub-object */
        json_t *fn = json_obj_get(t, "function");
        if (!fn || fn->type != JSON_OBJECT) continue;

        const char *name = json_get_str(fn, "name", "");
        const char *description = json_get_str(fn, "description", "");
        json_t *parameters = json_obj_get(fn, "parameters");

        /* Build toolSpec */
        json_t *input_schema = json_object();
        json_t *inner_json = NULL;
        if (parameters && parameters->type == JSON_OBJECT) {
            inner_json = json_copy(parameters);
        } else {
            inner_json = json_object();
            json_set(inner_json, "type", json_string("object"));
            json_set(inner_json, "properties", json_object());
        }
        json_set(input_schema, "json", inner_json);

        json_t *tool_spec = json_object();
        json_set(tool_spec, "name", json_string(name));
        if (description && *description)
            json_set(tool_spec, "description", json_string(description));
        json_set(tool_spec, "inputSchema", input_schema);

        json_t *wrapper = json_object();
        json_set(wrapper, "toolSpec", tool_spec);
        json_append(result, wrapper);
    }
    return result;
}

/* ---- bedrock_convert_content_to_converse ---- */
/* Port of Python bedrock_adapter._convert_content_to_converse().
 * Converts OpenAI message content to Bedrock Converse content blocks.
 * Handles: NULL→[{"text":" "}], plain string→[{"text":"..."}],
 * content arrays with text/image_url parts, data: URI base64 images. */
json_t *bedrock_convert_content_to_converse(const json_t *content) {
    json_t *blocks = json_array();
    if (!blocks) return NULL;

    /* NULL or JSON_NULL → single space text block */
    if (!content || content->type == JSON_NULL) {
        json_t *block = json_object();
        json_set(block, "text", json_string(" "));
        json_append(blocks, block);
        return blocks;
    }

    /* Plain string → text block */
    if (content->type == JSON_STRING) {
        const char *text = content->str_val ? content->str_val : "";
        /* Skip leading whitespace for empty-check */
        while (*text == ' ' || *text == '\t' || *text == '\n') text++;
        json_t *block = json_object();
        json_set(block, "text", json_string(*text ? content->str_val : " "));
        json_append(blocks, block);
        return blocks;
    }

    /* JSON array → iterate parts */
    if (content->type == JSON_ARRAY) {
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *part = content->c.items[i];
            if (!part) continue;

            /* Plain string inside array */
            if (part->type == JSON_STRING) {
                json_t *block = json_object();
                json_set(block, "text", json_string(part->str_val ? part->str_val : " "));
                json_append(blocks, block);
                continue;
            }

            if (part->type != JSON_OBJECT) continue;

            const char *part_type = json_get_str(part, "type", "");

            if (strcmp(part_type, "text") == 0) {
                const char *text = json_get_str(part, "text", "");
                if (!text || !*text) text = " ";
                json_t *block = json_object();
                json_set(block, "text", json_string(text));
                json_append(blocks, block);

            } else if (strcmp(part_type, "image_url") == 0) {
                json_t *image_url_obj = json_obj_get(part, "image_url");
                const char *url = image_url_obj ?
                    json_get_str(image_url_obj, "url", "") : "";

                if (url && strncmp(url, "data:", 5) == 0) {
                    /* Parse data: URI: data:image/jpeg;base64,/9j/... */
                    const char *comma = strchr(url, ',');
                    const char *header = url + 5; /* skip "data:" */
                    const char *data = comma ? comma + 1 : "";
                    size_t header_len = comma ? (size_t)(comma - url - 5) : 0;

                    /* Extract MIME type from header */
                    const char *mime_type = "image/jpeg";
                    char mime_buf[64] = "";
                    if (header_len > 0 && header_len < sizeof(mime_buf)) {
                        memcpy(mime_buf, header, header_len);
                        mime_buf[header_len] = '\0';
                        /* Split on ';' to get the MIME type */
                        char *semi = strchr(mime_buf, ';');
                        if (semi) *semi = '\0';
                        if (mime_buf[0]) mime_type = mime_buf;
                    }

                    /* Extract format from MIME type (part after '/') */
                    const char *format = strchr(mime_type, '/');
                    format = format ? format + 1 : "jpeg";

                    json_t *source = json_object();
                    json_set(source, "bytes", json_string(data));

                    json_t *image = json_object();
                    json_set(image, "format", json_string(format));
                    json_set(image, "source", source);

                    json_t *block = json_object();
                    json_set(block, "image", image);
                    json_append(blocks, block);
                } else {
                    /* Remote URL → text reference */
                    char ref[1024];
                    snprintf(ref, sizeof(ref), "[Image: %s]", url ? url : "");
                    json_t *block = json_object();
                    json_set(block, "text", json_string(ref));
                    json_append(blocks, block);
                }
            }
        }

        /* If no blocks were added, return [{"text":" "}] */
        if (blocks->c.count == 0) {
            json_t *block = json_object();
            json_set(block, "text", json_string(" "));
            json_append(blocks, block);
        }
        return blocks;
    }

    /* Fallback: convert to string representation */
    /* For non-string/non-array content, return [{"text": " "}] */
    json_t *block = json_object();
    json_set(block, "text", json_string(" "));
    json_append(blocks, block);
    return blocks;
}

/* ---- bedrock_converse_stop_reason_to_openai ---- */
/* Port of Python bedrock_adapter._converse_stop_reason_to_openai().
 * Maps Bedrock Converse stop reasons to OpenAI-compatible finish_reason strings. */
static const char *bedrock_converse_stop_reason_to_openai(const char *stop_reason) {
    if (!stop_reason || !*stop_reason) return "stop";
    if (strcmp(stop_reason, "end_turn") == 0 || strcmp(stop_reason, "stop_sequence") == 0)
        return "stop";
    if (strcmp(stop_reason, "tool_use") == 0)
        return "tool_calls";
    if (strcmp(stop_reason, "max_tokens") == 0)
        return "length";
    if (strcmp(stop_reason, "content_filtered") == 0 || strcmp(stop_reason, "guardrail_intervened") == 0)
        return "content_filter";
    return "stop";
}

/* ---- bedrock_convert_messages_to_converse ---- */
/* Port of Python bedrock_adapter.convert_messages_to_converse().
 * Converts OpenAI-format messages array to Bedrock Converse format.
 * Returns json_t* with {system: [blocks]?, messages: [{role, content: [blocks]}]}.
 * Handles: system→system prompt extraction, user/assistant→role+content,
 * tool_calls→toolUse blocks, tool results→toolResult in user role.
 * Enforces strict user/assistant alternation per Converse API requirements.
 * Caller must free() the result. */
json_t *bedrock_convert_messages_to_converse(const json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY)
        return NULL;

    json_t *system_blocks = json_array();
    json_t *converse_msgs = json_array();
    if (!system_blocks || !converse_msgs) {
        json_free(system_blocks);
        json_free(converse_msgs);
        return NULL;
    }

    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (!msg || msg->type != JSON_OBJECT) continue;

        const char *role = json_get_str(msg, "role", "");
        json_t *content = json_obj_get(msg, "content");

        if (strcmp(role, "system") == 0) {
            /* System messages become the system prompt */
            if (content && content->type == JSON_STRING) {
                const char *text = content->str_val;
                while (*text == ' ') text++;
                if (*text) {
                    json_t *block = json_object();
                    json_set(block, "text", json_string(content->str_val));
                    json_append(system_blocks, block);
                }
            } else if (content && content->type == JSON_ARRAY) {
                for (size_t j = 0; j < content->c.count; j++) {
                    json_t *part = content->c.items[j];
                    if (!part) continue;
                    if (part->type == JSON_STRING) {
                        json_t *block = json_object();
                        json_set(block, "text", json_string(part->str_val ? part->str_val : ""));
                        json_append(system_blocks, block);
                    } else if (part->type == JSON_OBJECT) {
                        const char *type_s = json_get_str(part, "type", "");
                        if (strcmp(type_s, "text") == 0) {
                            const char *text = json_get_str(part, "text", "");
                            if (*text) {
                                json_t *block = json_object();
                                json_set(block, "text", json_string(text));
                                json_append(system_blocks, block);
                            }
                        }
                    }
                }
            }
            continue;
        }

        if (strcmp(role, "tool") == 0) {
            /* Tool result messages → merge into preceding user turn */
            const char *tool_call_id = json_get_str(msg, "tool_call_id", "");
            json_t *tool_result_content = NULL;
            if (content && content->type == JSON_STRING) {
                tool_result_content = json_array();
                json_t *tc = json_object();
                json_set(tc, "text", json_string(content->str_val));
                json_append(tool_result_content, tc);
            } else if (content && content->type == JSON_OBJECT) {
                char *serialized = json_serialize(content);
                tool_result_content = json_array();
                json_t *tc = json_object();
                json_set(tc, "text", json_string(serialized ? serialized : "{}"));
                json_append(tool_result_content, tc);
                free(serialized);
            } else {
                tool_result_content = json_array();
                json_t *tc = json_object();
                json_set(tc, "text", json_string("{}"));
                json_append(tool_result_content, tc);
            }

            json_t *tool_result_block = json_object();
            json_t *tr = json_object();
            json_set(tr, "toolUseId", json_string(tool_call_id));
            if (tool_result_content)
                json_set(tr, "content", tool_result_content);
            json_set(tool_result_block, "toolResult", tr);

            /* Merge with last user message if possible */
            size_t last_idx = converse_msgs->c.count;
            if (last_idx > 0) {
                json_t *last = converse_msgs->c.items[last_idx - 1];
                const char *last_role = json_get_str(last, "role", "");
                if (strcmp(last_role, "user") == 0) {
                    json_t *blocks = json_obj_get(last, "content");
                    if (blocks && blocks->type == JSON_ARRAY) {
                        json_append(blocks, tool_result_block);
                        continue;
                    }
                }
            }
            /* No user message to merge with — create new user message */
            json_t *new_msg = json_object();
            json_set(new_msg, "role", json_string("user"));
            json_t *new_blocks = json_array();
            json_append(new_blocks, tool_result_block);
            json_set(new_msg, "content", new_blocks);
            json_append(converse_msgs, new_msg);
            continue;
        }

        if (strcmp(role, "assistant") == 0) {
            json_t *content_blocks = json_array();

            /* Convert text content */
            if (content && content->type == JSON_STRING) {
                const char *text = content->str_val;
                while (*text == ' ') text++;
                if (*text) {
                    json_t *block = json_object();
                    json_set(block, "text", json_string(content->str_val));
                    json_append(content_blocks, block);
                }
            } else if (content && content->type == JSON_ARRAY) {
                json_t *converted = bedrock_convert_content_to_converse(content);
                if (converted) {
                    for (size_t j = 0; j < converted->c.count; j++) {
                        json_t *cb = converted->c.items[j];
                        if (cb) json_append(content_blocks, json_copy(cb));
                    }
                    json_free(converted);
                }
            }

            /* Convert tool_calls */
            json_t *tool_calls = json_obj_get(msg, "tool_calls");
            if (tool_calls && tool_calls->type == JSON_ARRAY) {
                for (size_t j = 0; j < tool_calls->c.count; j++) {
                    json_t *tc = tool_calls->c.items[j];
                    if (!tc || tc->type != JSON_OBJECT) continue;

                    const char *tc_id = json_get_str(tc, "id", "");
                    json_t *fn_obj = json_obj_get(tc, "function");
                    const char *fn_name = fn_obj ? json_get_str(fn_obj, "name", "") : "";
                    json_t *fn_args = fn_obj ? json_obj_get(fn_obj, "arguments") : NULL;

                    json_t *input = NULL;
                    if (fn_args && fn_args->type == JSON_OBJECT) {
                        input = json_copy(fn_args);
                    } else if (fn_args && fn_args->type == JSON_STRING) {
                        char *parsed = NULL;
                        json_t *parsed_args = json_parse(fn_args->str_val, &parsed);
                        if (parsed_args) {
                            input = parsed_args;
                            free(parsed);
                        }
                    }
                    if (!input) {
                        input = json_object();
                    }

                    json_t *tool_use = json_object();
                    json_set(tool_use, "toolUseId", json_string(tc_id));
                    json_set(tool_use, "name", json_string(fn_name));
                    json_set(tool_use, "input", input);

                    json_t *block = json_object();
                    json_set(block, "toolUse", tool_use);
                    json_append(content_blocks, block);
                }
            }

            /* Ensure at least one content block */
            if (content_blocks->c.count == 0) {
                json_t *block = json_object();
                json_set(block, "text", json_string(" "));
                json_append(content_blocks, block);
            }

            /* Merge with previous assistant message if needed */
            size_t last_idx = converse_msgs->c.count;
            if (last_idx > 0) {
                json_t *last = converse_msgs->c.items[last_idx - 1];
                const char *last_role = json_get_str(last, "role", "");
                if (strcmp(last_role, "assistant") == 0) {
                    json_t *last_content = json_obj_get(last, "content");
                    if (last_content && last_content->type == JSON_ARRAY) {
                        for (size_t j = 0; j < content_blocks->c.count; j++) {
                            json_append(last_content, json_copy(content_blocks->c.items[j]));
                        }
                        json_free(content_blocks);
                        continue;
                    }
                }
            }

            json_t *new_msg = json_object();
            json_set(new_msg, "role", json_string("assistant"));
            json_set(new_msg, "content", content_blocks);
            json_append(converse_msgs, new_msg);
            continue;
        }

        if (strcmp(role, "user") == 0) {
            json_t *content_blocks = bedrock_convert_content_to_converse(content);

            /* Merge with previous user message if needed */
            size_t last_idx = converse_msgs->c.count;
            if (last_idx > 0) {
                json_t *last = converse_msgs->c.items[last_idx - 1];
                const char *last_role = json_get_str(last, "role", "");
                if (strcmp(last_role, "user") == 0) {
                    json_t *last_content = json_obj_get(last, "content");
                    if (last_content && last_content->type == JSON_ARRAY && content_blocks) {
                        for (size_t j = 0; j < content_blocks->c.count; j++) {
                            json_append(last_content, json_copy(content_blocks->c.items[j]));
                        }
                        json_free(content_blocks);
                        continue;
                    }
                }
            }

            json_t *new_msg = json_object();
            json_set(new_msg, "role", json_string("user"));
            json_set(new_msg, "content", content_blocks ? content_blocks : json_array());
            json_append(converse_msgs, new_msg);
            continue;
        }
    }

    /* Enforce Converse alternation rules */
    /* First message must be from user */
    if (converse_msgs->c.count > 0) {
        json_t *first = converse_msgs->c.items[0];
        const char *first_role = json_get_str(first, "role", "");
        if (strcmp(first_role, "user") != 0) {
            json_t *padding = json_object();
            json_set(padding, "role", json_string("user"));
            json_t *pad_content = json_array();
            json_t *pad_block = json_object();
            json_set(pad_block, "text", json_string(" "));
            json_append(pad_content, pad_block);
            json_set(padding, "content", pad_content);
            /* Insert at front */
            json_t **new_items = realloc(converse_msgs->c.items,
                (converse_msgs->c.count + 1) * sizeof(json_t*));
            if (new_items) {
                memmove(new_items + 1, new_items,
                    converse_msgs->c.count * sizeof(json_t*));
                new_items[0] = padding;
                converse_msgs->c.items = new_items;
                converse_msgs->c.count++;
            }
        }
    }

    /* Last message must be from user */
    if (converse_msgs->c.count > 0) {
        json_t *last = converse_msgs->c.items[converse_msgs->c.count - 1];
        const char *last_role = json_get_str(last, "role", "");
        if (strcmp(last_role, "user") != 0) {
            json_t *padding = json_object();
            json_set(padding, "role", json_string("user"));
            json_t *pad_content = json_array();
            json_t *pad_block = json_object();
            json_set(pad_block, "text", json_string(" "));
            json_append(pad_content, pad_block);
            json_set(padding, "content", pad_content);
            json_append(converse_msgs, padding);
        }
    }

    /* Build result */
    json_t *result = json_object();
    if (system_blocks->c.count > 0)
        json_set(result, "system", system_blocks);
    else
        json_free(system_blocks);
    json_set(result, "messages", converse_msgs);
    return result;
}

/* ---- bedrock_normalize_converse_response ---- */
/* Port of Python bedrock_adapter.normalize_converse_response().
 * Converts a Bedrock Converse API response JSON to a normalized format
 * matching the shape used internally: {choices: [...], usage: {...}, model: "..."}.
 * Returns json_t* that the caller must free(). */
json_t *bedrock_normalize_converse_response(const json_t *response) {
    if (!response || response->type != JSON_OBJECT)
        return NULL;

    json_t *output = json_obj_get(response, "output");
    json_t *message = NULL;
    json_t *content_blocks = NULL;
    if (output) message = json_obj_get(output, "message");
    if (message) content_blocks = json_obj_get(message, "content");

    const char *stop_reason_str = json_get_str(response, "stopReason", "end_turn");
    const char *finish_reason = bedrock_converse_stop_reason_to_openai(stop_reason_str);

    /* Extract content blocks */
    json_t *text_parts = json_array();
    json_t *reasoning_parts = json_array();
    json_t *tool_calls = json_array();
    bool has_tool_use = false;

    if (content_blocks && content_blocks->type == JSON_ARRAY) {
        for (size_t i = 0; i < content_blocks->c.count; i++) {
            json_t *block = content_blocks->c.items[i];
            if (!block || block->type != JSON_OBJECT) continue;

            /* Text block */
            const char *text = json_get_str(block, "text", NULL);
            if (text) {
                json_append(text_parts, json_string(text));
                continue;
            }

            /* Reasoning content block */
            json_t *reasoning = json_obj_get(block, "reasoningContent");
            if (reasoning) {
                const char *thinking = json_get_str(reasoning, "text", NULL);
                if (thinking && *thinking) {
                    json_append(reasoning_parts, json_string(thinking));
                }
                continue;
            }

            /* Tool use block */
            json_t *tu = json_obj_get(block, "toolUse");
            if (tu) {
                has_tool_use = true;
                json_t *tc = json_object();
                json_set(tc, "id", json_string(json_get_str(tu, "toolUseId", "")));
                json_set(tc, "type", json_string("function"));
                json_t *fn = json_object();
                json_set(fn, "name", json_string(json_get_str(tu, "name", "")));
                json_t *input = json_obj_get(tu, "input");
                if (input) {
                    char *args_str = json_serialize(input);
                    json_set(fn, "arguments", json_string(args_str ? args_str : "{}"));
                    free(args_str);
                } else {
                    json_set(fn, "arguments", json_string("{}"));
                }
                json_set(tc, "function", fn);
                json_append(tool_calls, tc);
            }
        }
    }

    /* Build content string from text parts */
    char *content_str = NULL;
    if (text_parts->c.count > 0) {
        size_t total = 0;
        for (size_t i = 0; i < text_parts->c.count; i++) {
            json_t *tp = text_parts->c.items[i];
            if (tp && tp->type == JSON_STRING && tp->str_val)
                total += strlen(tp->str_val) + 1; /* \n separator or \0 */
        }
        content_str = (char *)calloc(1, total + 1);
        if (content_str) {
            size_t pos = 0;
            for (size_t i = 0; i < text_parts->c.count; i++) {
                json_t *tp = text_parts->c.items[i];
                if (tp && tp->type == JSON_STRING && tp->str_val) {
                    size_t len = strlen(tp->str_val);
                    if (pos > 0) content_str[pos++] = '\n';
                    memcpy(content_str + pos, tp->str_val, len);
                    pos += len;
                }
            }
        }
    }

    /* Build reasoning string */
    char *reasoning_str = NULL;
    if (reasoning_parts->c.count > 0) {
        size_t total = 0;
        for (size_t i = 0; i < reasoning_parts->c.count; i++) {
            json_t *rp = reasoning_parts->c.items[i];
            if (rp && rp->type == JSON_STRING && rp->str_val)
                total += strlen(rp->str_val) + 2; /* \n\n separator */
        }
        reasoning_str = (char *)calloc(1, total + 1);
        if (reasoning_str) {
            size_t pos = 0;
            for (size_t i = 0; i < reasoning_parts->c.count; i++) {
                json_t *rp = reasoning_parts->c.items[i];
                if (rp && rp->type == JSON_STRING && rp->str_val) {
                    size_t len = strlen(rp->str_val);
                    if (pos > 0) { reasoning_str[pos++] = '\n'; reasoning_str[pos++] = '\n'; }
                    memcpy(reasoning_str + pos, rp->str_val, len);
                    pos += len;
                }
            }
        }
    }

    /* Adjust finish_reason: tool_calls override stop */
    if (has_tool_use && strcmp(finish_reason, "stop") == 0)
        finish_reason = "tool_calls";

    /* Build usage */
    json_t *usage_data = json_obj_get(response, "usage");
    int input_tokens = usage_data ? (int)json_get_num(usage_data, "inputTokens", 0) : 0;
    int output_tokens = usage_data ? (int)json_get_num(usage_data, "outputTokens", 0) : 0;

    /* Build message object */
    json_t *msg_obj = json_object();
    json_set(msg_obj, "role", json_string("assistant"));
    if (content_str) {
        json_set(msg_obj, "content", json_string(content_str));
        free(content_str);
    } else {
        json_set(msg_obj, "content", json_null());
    }
    if (tool_calls->c.count > 0)
        json_set(msg_obj, "tool_calls", tool_calls);
    else
        json_free(tool_calls);
    if (reasoning_str) {
        json_set(msg_obj, "reasoning_content", json_string(reasoning_str));
        free(reasoning_str);
    }

    /* Build choice */
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", msg_obj);
    json_set(choice, "finish_reason", json_string(finish_reason));

    json_t *choices = json_array();
    json_append(choices, choice);

    /* Build usage */
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(input_tokens));
    json_set(usage, "completion_tokens", json_number(output_tokens));
    json_set(usage, "total_tokens", json_number(input_tokens + output_tokens));

    /* Build result */
    json_t *result = json_object();
    const char *model_id = json_get_str(response, "modelId", "");
    json_set(result, "choices", choices);
    json_set(result, "usage", usage);
    json_set(result, "model", json_string(model_id));

    json_free(text_parts);
    json_free(reasoning_parts);

    return result;
}

const provider_ops_t PROVIDER_OPS_BEDROCK = {
    .build_url = bedrock_build_url,
    .build_headers = bedrock_build_headers,
    .build_request_body = bedrock_build_request_body,
    .parse_response = bedrock_parse_response,
    .parse_stream_chunk = bedrock_parse_stream_chunk,
    .free_response = bedrock_free_response,
    .name = "bedrock"
};
