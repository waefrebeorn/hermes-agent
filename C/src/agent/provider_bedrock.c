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
    strncpy(buf, iso_time, 8);
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
    float temp = p->config.temperature > 0.0f ? p->config.temperature : 0.7f;
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

    /* Output → message → content */
    json_t *output = json_object_get(root, "output");
    if (output) {
        json_t *message = json_object_get(output, "message");
        if (message) {
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
                        snprintf(resp->tool_calls[idx].arguments,
                                 sizeof(resp->tool_calls[idx].arguments), "%s",
                                 json_get_str(tu, "input", "{}"));
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

const provider_ops_t PROVIDER_OPS_BEDROCK = {
    .build_url = bedrock_build_url,
    .build_headers = bedrock_build_headers,
    .build_request_body = bedrock_build_request_body,
    .parse_response = bedrock_parse_response,
    .parse_stream_chunk = bedrock_parse_stream_chunk,
    .free_response = bedrock_free_response,
    .name = "bedrock"
};
