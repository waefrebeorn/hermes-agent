/*
 * error_classifier.c — API error classification for smart failover/recovery.
 * Port of Python agent/error_classifier.py.
 *
 * Priority-ordered classification pipeline:
 *   1. Provider-specific patterns (thinking sigs, tier gates, llama.cpp grammar)
 *   2. HTTP status code + message-aware refinement
 *   3. Error code classification (from body JSON)
 *   4. Message pattern matching (billing vs rate_limit vs context vs auth)
 *   5. SSL/TLS transient alert patterns → retry as timeout
 *   6. Server disconnect + large session → context overflow
 *   7. Generic transport error heuristics
 *   8. Fallback: unknown (retryable with backoff)
 *
 * MIT License — WuBu Hermes Project
 */

#include "error_classifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ════════════════════════════════════════════════════════════════════════
 *  Internal helpers
 * ════════════════════════════════════════════════════════════════════════ */

/* Case-insensitive substring search */
static bool contains_i(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return false;
    const char *p = haystack;
    size_t nlen = strlen(needle);
    while (*p) {
        if (strncasecmp(p, needle, nlen) == 0) return true;
        p++;
    }
    return false;
}

/* Check if haystack contains any of the patterns (case-insensitive). */
static bool contains_any_i(const char *haystack, const char *patterns[], int count) {
    if (!haystack || !patterns || count <= 0) return false;
    for (int i = 0; i < count; i++) {
        if (contains_i(haystack, patterns[i])) return true;
    }
    return false;
}

/* Safe string copy with truncation */
static void safe_copy(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dstsz) len = dstsz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* Build a classified_error_t with defaults. */
static void make_result(classified_error_t *r, failover_reason_t reason,
                        int status_code, const char *provider,
                        const char *model, const char *message,
                        bool retryable, bool compress,
                        bool rotate_cred, bool fallback)
{
    memset(r, 0, sizeof(*r));
    r->reason = reason;
    r->status_code = status_code;
    safe_copy(r->provider, sizeof(r->provider), provider);
    safe_copy(r->model, sizeof(r->model), model);
    safe_copy(r->message, sizeof(r->message), message);
    r->retryable = retryable;
    r->should_compress = compress;
    r->should_rotate_credential = rotate_cred;
    r->should_fallback = fallback;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Pattern lists
 * ════════════════════════════════════════════════════════════════════════ */

static const char *BILLING_PATTERNS[] = {
    "insufficient credits", "insufficient_quota", "insufficient balance",
    "credit balance", "credits have been exhausted", "top up your credits",
    "payment required", "billing hard limit", "exceeded your current quota",
    "account is deactivated", "plan does not include",
};

static const char *RATE_LIMIT_PATTERNS[] = {
    "rate limit", "rate_limit", "too many requests", "throttled",
    "requests per minute", "tokens per minute", "requests per day",
    "try again in", "please retry after", "resource_exhausted",
    "rate increased too quickly", "throttlingexception",
    "too many concurrent requests", "servicequotaexceededexception",
};

static const char *USAGE_LIMIT_PATTERNS[] = {
    "usage limit", "quota", "limit exceeded", "key limit exceeded",
};

static const char *USAGE_LIMIT_TRANSIENT[] = {
    "try again", "retry", "resets at", "reset in", "wait",
    "requests remaining", "periodic", "window",
};

static const char *PAYLOAD_TOO_LARGE_PATTERNS[] = {
    "request entity too large", "payload too large", "error code: 413",
};

static const char *IMAGE_TOO_LARGE_PATTERNS[] = {
    "image exceeds", "image too large", "image_too_large", "image size exceeds",
};

static const char *CONTEXT_OVERFLOW_PATTERNS[] = {
    "context length", "context size", "maximum context", "token limit",
    "too many tokens", "reduce the length", "exceeds the limit",
    "context window", "prompt is too long", "prompt exceeds max length",
    "max_tokens", "maximum number of tokens",
    "exceeds the max_model_len", "max_model_len",
    "prompt length", "input is too long",
    "maximum model length", "context length exceeded",
    "truncating input", "slot context", "n_ctx_slot",
    /* Chinese patterns */
    "\xe8\xb6\x85\xe8\xbf\x87\xe6\x9c\x80\xe5\xa4\xa7\xe9\x95\xbf\xe5\xba\xa6", /* 超过最大长度 */
    "\xe4\xb8\x8a\xe4\xb8\x8b\xe6\x96\x87\xe9\x95\xbf\xe5\xba\xa6", /* 上下文长度 */
    /* Bedrock patterns */
    "input is too long for", "max input token", "input token",
    "exceeds the maximum number of input tokens",
};

static const char *MODEL_NOT_FOUND_PATTERNS[] = {
    "is not a valid model", "invalid model", "model not found",
    "model_not_found", "does not exist", "no such model",
    "unknown model", "unsupported model",
};

static const char *PROVIDER_POLICY_BLOCKED_PATTERNS[] = {
    "no endpoints available matching your guardrail",
    "no endpoints available matching your data policy",
    "no endpoints found matching your data policy",
};

static const char *AUTH_PATTERNS[] = {
    "invalid api key", "invalid_api_key", "authentication",
    "unauthorized", "forbidden", "invalid token", "token expired",
    "token revoked", "access denied",
};

static const char *TIMEOUT_MESSAGE_PATTERNS[] = {
    "timed out", "turn timed out", "request timed out",
    "deadline exceeded", "operation timed out", "upstream timed out",
};

static const char *SERVER_DISCONNECT_PATTERNS[] = {
    "server disconnected", "peer closed connection", "connection reset by peer",
    "connection was closed", "network connection lost", "unexpected eof",
    "incomplete chunked read",
};

static const char *SSL_TRANSIENT_PATTERNS[] = {
    "bad record mac", "ssl alert", "tls alert", "ssl handshake failure",
    "tlsv1 alert", "sslv3 alert",
    "bad_record_mac", "ssl_alert", "tls_alert", "tls_alert_internal_error",
    "[ssl:",
};

static const char *MULTIMODAL_TOOL_CONTENT_PATTERNS[] = {
    "text is not set",
    "tool message content must be a string",
    "tool content must be a string",
    "tool message must be a string",
    "expected string, got list",
    "expected string, got array",
    "tool_call.content must be string",
};

/* ════════════════════════════════════════════════════════════════════════
 *  JSON field extraction helpers (inlined for simplicity)
 * ════════════════════════════════════════════════════════════════════════ */

/* Find a JSON string value by key in a simple JSON body.
   Scrubs whitespace between colon and value, handles quoted strings.
   NOT a general JSON parser — only for the shape: {"error": {"message": "..."}}
   or {"code": "..."}, {"message": "..."} at top level. */
static const char *json_extract_str(const char *body, const char *key) {
    if (!body || !key) return NULL;
    const char *k = strstr(body, key);
    if (!k) return NULL;
    k += strlen(key);
    while (*k && (*k == ' ' || *k == ':' || *k == '\t' || *k == '\n')) k++;
    if (*k != '"') return NULL;
    k++; /* skip opening quote */
    static char buf[1024];
    int i = 0;
    while (*k && *k != '"' && i < (int)sizeof(buf) - 1) {
        if (*k == '\\' && *(k+1)) { k++; buf[i++] = *k++; }
        else { buf[i++] = *k++; }
    }
    buf[i] = '\0';
    return buf;
}

static void json_extract_nested_msg(const char *body,
                                     char *buf, size_t bufsz)
{
    if (!body) { if (buf && bufsz) buf[0] = '\0'; return; }
    buf[0] = '\0';

    /* Try: body.error.message */
    const char *err = strstr(body, "\"error\"");
    if (err) {
        const char *start = err + 7;
        while (*start && *start != '{') start++;
        if (*start == '{') {
            /* Try param field — check first so it doesn't shadow message */
            const char *param_key = strstr(start, "\"param\"");
            const char *msg_key = strstr(start, "\"message\"");
            const char *code_key = strstr(start, "\"code\"");

            /* Build combined message from all available fields */
            size_t pos = 0;

            /* Add message field */
            if (msg_key) {
                const char *v = json_extract_str(msg_key, "\"message\"");
                if (v) {
                    size_t vlen = strlen(v);
                    size_t copy = (vlen < bufsz - pos - 1) ? vlen : (bufsz - pos - 1);
                    memcpy(buf + pos, v, copy);
                    pos += copy;
                    buf[pos] = '\0';
                }
            }

            /* Append param field if different from message */
            if (param_key) {
                const char *v = json_extract_str(param_key, "\"param\"");
                if (v && *v) {
                    size_t remaining = bufsz - pos;
                    if (remaining > 3) {
                        buf[pos++] = ' ';
                        size_t vlen = strlen(v);
                        size_t copy = (vlen < remaining - 2) ? vlen : (remaining - 2);
                        memcpy(buf + pos, v, copy);
                        pos += copy;
                        buf[pos] = '\0';
                    }
                }
            }

            if (pos > 0) return;

            /* Try code if nothing else found */
            if (code_key) {
                const char *v = json_extract_str(code_key, "\"code\"");
                if (v) { safe_copy(buf, bufsz, v); return; }
            }
        }
    }

    /* Try top-level: "message" */
    const char *v = json_extract_str(body, "\"message\"");
    if (v) { safe_copy(buf, bufsz, v); return; }

    /* Try top-level: "code" */
    v = json_extract_str(body, "\"code\"");
    if (v) { safe_copy(buf, bufsz, v); return; }
}

static const char *json_extract_error_code(const char *body) {
    if (!body) return "";
    /* Try error.code */
    const char *err = strstr(body, "\"error\"");
    if (err) {
        const char *code_v = json_extract_str(err, "\"code\"");
        if (code_v && *code_v) return code_v;
        const char *type_v = json_extract_str(err, "\"type\"");
        if (type_v && *type_v) return type_v;
    }
    /* Top-level code */
    const char *v = json_extract_str(body, "\"code\"");
    if (v && *v) return v;
    v = json_extract_str(body, "\"error_code\"");
    if (v && *v) return v;
    return "";
}

/* ════════════════════════════════════════════════════════════════════════
 *  Classification sub-functions
 * ════════════════════════════════════════════════════════════════════════ */

/* _classify_400 — classify 400 Bad Request */
static bool classify_400(const char *error_msg, const char *error_code,
                         const char *body,
                         const char *provider, const char *model,
                         int approx_tokens, int context_length,
                         classified_error_t *result)
{
    (void)error_code;
    (void)body;
    int bl_count = sizeof(MULTIMODAL_TOOL_CONTENT_PATTERNS) / sizeof(MULTIMODAL_TOOL_CONTENT_PATTERNS[0]);
    if (contains_any_i(error_msg, MULTIMODAL_TOOL_CONTENT_PATTERNS, bl_count)) {
        make_result(result, FAILOVER_MULTIMODAL_TOOL_UNSUPPORTED, 400, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    int il_count = sizeof(IMAGE_TOO_LARGE_PATTERNS) / sizeof(IMAGE_TOO_LARGE_PATTERNS[0]);
    if (contains_any_i(error_msg, IMAGE_TOO_LARGE_PATTERNS, il_count)) {
        make_result(result, FAILOVER_IMAGE_TOO_LARGE, 400, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    int co_count = sizeof(CONTEXT_OVERFLOW_PATTERNS) / sizeof(CONTEXT_OVERFLOW_PATTERNS[0]);
    if (contains_any_i(error_msg, CONTEXT_OVERFLOW_PATTERNS, co_count)) {
        make_result(result, FAILOVER_CONTEXT_OVERFLOW, 400, provider, model,
                    error_msg, true, true, false, false);
        return true;
    }

    int pb_count = sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS) / sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS[0]);
    if (contains_any_i(error_msg, PROVIDER_POLICY_BLOCKED_PATTERNS, pb_count)) {
        make_result(result, FAILOVER_PROVIDER_POLICY_BLOCKED, 400, provider, model,
                    error_msg, false, false, false, false);
        return true;
    }

    int mnf_count = sizeof(MODEL_NOT_FOUND_PATTERNS) / sizeof(MODEL_NOT_FOUND_PATTERNS[0]);
    if (contains_any_i(error_msg, MODEL_NOT_FOUND_PATTERNS, mnf_count)) {
        make_result(result, FAILOVER_MODEL_NOT_FOUND, 400, provider, model,
                    error_msg, false, false, false, true);
        return true;
    }

    int rl_count = sizeof(RATE_LIMIT_PATTERNS) / sizeof(RATE_LIMIT_PATTERNS[0]);
    if (contains_any_i(error_msg, RATE_LIMIT_PATTERNS, rl_count)) {
        make_result(result, FAILOVER_RATE_LIMIT, 400, provider, model,
                    error_msg, true, false, true, true);
        return true;
    }

    int bill_count = sizeof(BILLING_PATTERNS) / sizeof(BILLING_PATTERNS[0]);
    if (contains_any_i(error_msg, BILLING_PATTERNS, bill_count)) {
        make_result(result, FAILOVER_BILLING, 400, provider, model,
                    error_msg, false, false, true, true);
        return true;
    }

    /* Generic 400 + large session → probable context overflow */
    if (approx_tokens > 0 && context_length > 0 &&
        approx_tokens > context_length * 0.4) {
        make_result(result, FAILOVER_CONTEXT_OVERFLOW, 400, provider, model,
                    error_msg, true, true, false, false);
        return true;
    }

    /* Non-retryable format error */
    make_result(result, FAILOVER_FORMAT_ERROR, 400, provider, model,
                error_msg, false, false, false, true);
    return true;
}

/* _classify_402 — disambiguate billing vs transient usage limit */
static void classify_402(const char *error_msg, const char *provider,
                         const char *model, classified_error_t *result)
{
    int ul_count = sizeof(USAGE_LIMIT_PATTERNS) / sizeof(USAGE_LIMIT_PATTERNS[0]);
    int ts_count = sizeof(USAGE_LIMIT_TRANSIENT) / sizeof(USAGE_LIMIT_TRANSIENT[0]);
    bool has_usage = contains_any_i(error_msg, USAGE_LIMIT_PATTERNS, ul_count);
    bool has_transient = contains_any_i(error_msg, USAGE_LIMIT_TRANSIENT, ts_count);

    if (has_usage && has_transient) {
        make_result(result, FAILOVER_RATE_LIMIT, 402, provider, model,
                    error_msg, true, false, true, true);
    } else {
        make_result(result, FAILOVER_BILLING, 402, provider, model,
                    error_msg, false, false, true, true);
    }
}

/* _classify_by_status — classify based on HTTP status code */
static bool classify_by_status(int status_code, const char *error_msg,
                               const char *error_code, const char *body,
                               const char *provider, const char *model,
                               int approx_tokens, int context_length,
                               classified_error_t *result)
{
    if (status_code <= 0) return false;
    (void)approx_tokens;
    (void)context_length;

    switch (status_code) {
    case 401:
        make_result(result, FAILOVER_AUTH, 401, provider, model,
                    error_msg, false, false, true, true);
        return true;

    case 403: {
        int bill_count = sizeof(BILLING_PATTERNS) / sizeof(BILLING_PATTERNS[0]);
        if (contains_any_i(error_msg, BILLING_PATTERNS, bill_count) ||
            contains_i(error_msg, "key limit exceeded") ||
            contains_i(error_msg, "spending limit")) {
            make_result(result, FAILOVER_BILLING, 403, provider, model,
                        error_msg, false, false, true, true);
        } else {
            make_result(result, FAILOVER_AUTH, 403, provider, model,
                        error_msg, false, false, false, true);
        }
        return true;
    }

    case 402:
        classify_402(error_msg, provider, model, result);
        return true;

    case 404: {
        int pb_count = sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS) / sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS[0]);
        if (contains_any_i(error_msg, PROVIDER_POLICY_BLOCKED_PATTERNS, pb_count)) {
            make_result(result, FAILOVER_PROVIDER_POLICY_BLOCKED, 404, provider, model,
                        error_msg, false, false, false, false);
            return true;
        }
        int mnf_count = sizeof(MODEL_NOT_FOUND_PATTERNS) / sizeof(MODEL_NOT_FOUND_PATTERNS[0]);
        if (contains_any_i(error_msg, MODEL_NOT_FOUND_PATTERNS, mnf_count)) {
            make_result(result, FAILOVER_MODEL_NOT_FOUND, 404, provider, model,
                        error_msg, false, false, false, true);
            return true;
        }
        /* Generic 404 — unknown */
        make_result(result, FAILOVER_UNKNOWN, 404, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    case 413:
        make_result(result, FAILOVER_PAYLOAD_TOO_LARGE, 413, provider, model,
                    error_msg, true, true, false, false);
        return true;

    case 429:
        make_result(result, FAILOVER_RATE_LIMIT, 429, provider, model,
                    error_msg, true, false, true, true);
        return true;

    case 400:
        return classify_400(error_msg, error_code, body,
                           provider, model, approx_tokens, context_length,
                           result);

    case 500:
    case 502:
        make_result(result, FAILOVER_SERVER_ERROR, status_code, provider, model,
                    error_msg, true, false, false, false);
        return true;

    case 503:
    case 529:
        make_result(result, FAILOVER_OVERLOADED, status_code, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    /* Other 4xx — non-retryable */
    if (status_code >= 400 && status_code < 500) {
        make_result(result, FAILOVER_FORMAT_ERROR, status_code, provider, model,
                    error_msg, false, false, false, true);
        return true;
    }

    /* Other 5xx — retryable */
    if (status_code >= 500 && status_code < 600) {
        make_result(result, FAILOVER_SERVER_ERROR, status_code, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    return false;
}

/* _classify_by_error_code — classify by structured error codes */
static bool classify_by_error_code(const char *error_code,
                                   const char *error_msg,
                                   const char *provider, const char *model,
                                   classified_error_t *result)
{
    if (!error_code || !*error_code) return false;

    /* Normalize to lowercase for comparison */
    char code_lower[256];
    int i;
    for (i = 0; error_code[i] && i < 255; i++)
        code_lower[i] = tolower((unsigned char)error_code[i]);
    code_lower[i] = '\0';

    if (strcmp(code_lower, "resource_exhausted") == 0 ||
        strcmp(code_lower, "throttled") == 0 ||
        strcmp(code_lower, "rate_limit_exceeded") == 0) {
        make_result(result, FAILOVER_RATE_LIMIT, -1, provider, model,
                    error_msg, true, false, true, false);
        return true;
    }

    if (strcmp(code_lower, "insufficient_quota") == 0 ||
        strcmp(code_lower, "billing_not_active") == 0 ||
        strcmp(code_lower, "payment_required") == 0) {
        make_result(result, FAILOVER_BILLING, -1, provider, model,
                    error_msg, false, false, true, true);
        return true;
    }

    if (strcmp(code_lower, "model_not_found") == 0 ||
        strcmp(code_lower, "model_not_available") == 0 ||
        strcmp(code_lower, "invalid_model") == 0) {
        make_result(result, FAILOVER_MODEL_NOT_FOUND, -1, provider, model,
                    error_msg, false, false, false, true);
        return true;
    }

    if (strcmp(code_lower, "context_length_exceeded") == 0 ||
        strcmp(code_lower, "max_tokens_exceeded") == 0) {
        make_result(result, FAILOVER_CONTEXT_OVERFLOW, -1, provider, model,
                    error_msg, true, true, false, false);
        return true;
    }

    return false;
}

/* _classify_by_message — classify based on message text patterns */
static bool classify_by_message(const char *error_msg,
                                const char *provider, const char *model,
                                int approx_tokens, int context_length,
                                classified_error_t *result)
{
    (void)approx_tokens;
    (void)context_length;
    int count;

    /* Payload too large */
    count = sizeof(PAYLOAD_TOO_LARGE_PATTERNS) / sizeof(PAYLOAD_TOO_LARGE_PATTERNS[0]);
    if (contains_any_i(error_msg, PAYLOAD_TOO_LARGE_PATTERNS, count)) {
        make_result(result, FAILOVER_PAYLOAD_TOO_LARGE, -1, provider, model,
                    error_msg, true, true, false, false);
        return true;
    }

    /* Multimodal tool content */
    count = sizeof(MULTIMODAL_TOOL_CONTENT_PATTERNS) / sizeof(MULTIMODAL_TOOL_CONTENT_PATTERNS[0]);
    if (contains_any_i(error_msg, MULTIMODAL_TOOL_CONTENT_PATTERNS, count)) {
        make_result(result, FAILOVER_MULTIMODAL_TOOL_UNSUPPORTED, -1, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    /* Image too large */
    count = sizeof(IMAGE_TOO_LARGE_PATTERNS) / sizeof(IMAGE_TOO_LARGE_PATTERNS[0]);
    if (contains_any_i(error_msg, IMAGE_TOO_LARGE_PATTERNS, count)) {
        make_result(result, FAILOVER_IMAGE_TOO_LARGE, -1, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    /* Usage limit disambiguation */
    int ul_count = sizeof(USAGE_LIMIT_PATTERNS) / sizeof(USAGE_LIMIT_PATTERNS[0]);
    if (contains_any_i(error_msg, USAGE_LIMIT_PATTERNS, ul_count)) {
        int ts_count = sizeof(USAGE_LIMIT_TRANSIENT) / sizeof(USAGE_LIMIT_TRANSIENT[0]);
        if (contains_any_i(error_msg, USAGE_LIMIT_TRANSIENT, ts_count)) {
            make_result(result, FAILOVER_RATE_LIMIT, -1, provider, model,
                        error_msg, true, false, true, true);
        } else {
            make_result(result, FAILOVER_BILLING, -1, provider, model,
                        error_msg, false, false, true, true);
        }
        return true;
    }

    /* Billing patterns */
    count = sizeof(BILLING_PATTERNS) / sizeof(BILLING_PATTERNS[0]);
    if (contains_any_i(error_msg, BILLING_PATTERNS, count)) {
        make_result(result, FAILOVER_BILLING, -1, provider, model,
                    error_msg, false, false, true, true);
        return true;
    }

    /* Rate limit patterns */
    count = sizeof(RATE_LIMIT_PATTERNS) / sizeof(RATE_LIMIT_PATTERNS[0]);
    if (contains_any_i(error_msg, RATE_LIMIT_PATTERNS, count)) {
        make_result(result, FAILOVER_RATE_LIMIT, -1, provider, model,
                    error_msg, true, false, true, true);
        return true;
    }

    /* Context overflow patterns */
    count = sizeof(CONTEXT_OVERFLOW_PATTERNS) / sizeof(CONTEXT_OVERFLOW_PATTERNS[0]);
    if (contains_any_i(error_msg, CONTEXT_OVERFLOW_PATTERNS, count)) {
        make_result(result, FAILOVER_CONTEXT_OVERFLOW, -1, provider, model,
                    error_msg, true, true, false, false);
        return true;
    }

    /* Auth patterns */
    count = sizeof(AUTH_PATTERNS) / sizeof(AUTH_PATTERNS[0]);
    if (contains_any_i(error_msg, AUTH_PATTERNS, count)) {
        make_result(result, FAILOVER_AUTH, -1, provider, model,
                    error_msg, false, false, true, true);
        return true;
    }

    /* Provider policy blocked */
    count = sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS) / sizeof(PROVIDER_POLICY_BLOCKED_PATTERNS[0]);
    if (contains_any_i(error_msg, PROVIDER_POLICY_BLOCKED_PATTERNS, count)) {
        make_result(result, FAILOVER_PROVIDER_POLICY_BLOCKED, -1, provider, model,
                    error_msg, false, false, false, false);
        return true;
    }

    /* Model not found */
    count = sizeof(MODEL_NOT_FOUND_PATTERNS) / sizeof(MODEL_NOT_FOUND_PATTERNS[0]);
    if (contains_any_i(error_msg, MODEL_NOT_FOUND_PATTERNS, count)) {
        make_result(result, FAILOVER_MODEL_NOT_FOUND, -1, provider, model,
                    error_msg, false, false, false, true);
        return true;
    }

    /* Timeout message patterns */
    count = sizeof(TIMEOUT_MESSAGE_PATTERNS) / sizeof(TIMEOUT_MESSAGE_PATTERNS[0]);
    if (contains_any_i(error_msg, TIMEOUT_MESSAGE_PATTERNS, count)) {
        make_result(result, FAILOVER_TIMEOUT, -1, provider, model,
                    error_msg, true, false, false, false);
        return true;
    }

    return false;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Public API — main classification entry point
 * ════════════════════════════════════════════════════════════════════════ */

void error_classify(int status_code, const char *error_body,
                    const char *provider, const char *model,
                    int approx_tokens, int context_length,
                    classified_error_t *result)
{
    /* Ensure clean output */
    memset(result, 0, sizeof(*result));
    result->status_code = status_code;

    /* Build a combined lowercased error message string for pattern matching */
    char error_msg[4096] = "";
    char body_msg[1024] = "";

    /* Extract structured message from JSON body if present */
    if (error_body && *error_body) {
        json_extract_nested_msg(error_body, body_msg, sizeof(body_msg));
    }

    /* Combine: str(error) equivalent → use body_msg as primary.
       If no structured message found, fall back to raw body text for
       pattern matching (catches fields like "param" not in message). */
    safe_copy(error_msg, sizeof(error_msg), body_msg);
    if (!error_msg[0] && error_body) {
        size_t elen = strlen(error_body);
        if (elen > sizeof(error_msg) - 1) elen = sizeof(error_msg) - 1;
        memcpy(error_msg, error_body, elen);
        error_msg[elen] = '\0';
    }

    /* Also extract error_code */
    const char *error_code = error_body ? json_extract_error_code(error_body) : "";

    /* Normalize provider/model */
    char provider_lower[64] = "";
    char model_lower[128] = "";
    safe_copy(provider_lower, sizeof(provider_lower), provider);
    safe_copy(model_lower, sizeof(model_lower), model);
    for (char *p = provider_lower; *p; p++) *p = tolower((unsigned char)*p);
    for (char *p = model_lower; *p; p++) *p = tolower((unsigned char)*p);

    /* ── Step 1: Provider-specific patterns ──────────────────────── */

    /* Anthropic thinking block signature (400 + "signature" + "thinking") */
    if (status_code == 400 && contains_i(error_msg, "signature") &&
        contains_i(error_msg, "thinking")) {
        make_result(result, FAILOVER_THINKING_SIGNATURE, 400, provider, model,
                    error_msg, true, false, false, false);
        return;
    }

    /* Anthropic long-context tier gate (429 + "extra usage" + "long context") */
    if (status_code == 429 && contains_i(error_msg, "extra usage") &&
        contains_i(error_msg, "long context")) {
        make_result(result, FAILOVER_LONG_CONTEXT_TIER, 429, provider, model,
                    error_msg, true, true, false, false);
        return;
    }

    /* Anthropic OAuth 1M context beta forbidden (400 + "long context beta" + "not yet available") */
    if (status_code == 400 && contains_i(error_msg, "long context beta") &&
        contains_i(error_msg, "not yet available")) {
        make_result(result, FAILOVER_OAUTH_LONG_CONTEXT_BETA_FORBIDDEN, 400, provider, model,
                    error_msg, true, false, false, false);
        return;
    }

    /* llama.cpp grammar rejection (400 + "error parsing grammar" or "json-schema-to-grammar") */
    if (status_code == 400 &&
        (contains_i(error_msg, "error parsing grammar") ||
         contains_i(error_msg, "json-schema-to-grammar") ||
         (contains_i(error_msg, "unable to generate parser") &&
          contains_i(error_msg, "template")))) {
        make_result(result, FAILOVER_LLAMA_CPP_GRAMMAR, 400, provider, model,
                    error_msg, true, false, false, false);
        return;
    }

    /* xAI Grok entitlement errors */
    if (contains_i(error_msg, "do not have an active grok subscription") ||
        (contains_i(error_msg, "out of available resources") &&
         contains_i(error_msg, "grok"))) {
        make_result(result, FAILOVER_AUTH, status_code, provider, model,
                    error_msg, false, false, false, true);
        return;
    }

    /* ── Step 2: HTTP status code classification ─────────────────── */
    if (classify_by_status(status_code, error_msg, error_code, error_body,
                           provider, model, approx_tokens, context_length,
                           result)) {
        return;
    }

    /* ── Step 3: Error code classification ───────────────────────── */
    if (classify_by_error_code(error_code, error_msg,
                               provider, model, result)) {
        return;
    }

    /* ── Step 4: Message pattern matching ────────────────────────── */
    if (classify_by_message(error_msg, provider, model,
                            approx_tokens, context_length, result)) {
        return;
    }

    /* ── Step 5: SSL/TLS transient errors → retry as timeout ─────── */
    int ssl_count = sizeof(SSL_TRANSIENT_PATTERNS) / sizeof(SSL_TRANSIENT_PATTERNS[0]);
    if (contains_any_i(error_msg, SSL_TRANSIENT_PATTERNS, ssl_count)) {
        make_result(result, FAILOVER_TIMEOUT, status_code, provider, model,
                    error_msg, true, false, false, false);
        return;
    }

    /* ── Step 6: Server disconnect + large session → context overflow ── */
    int disc_count = sizeof(SERVER_DISCONNECT_PATTERNS) / sizeof(SERVER_DISCONNECT_PATTERNS[0]);
    bool is_disconnect = contains_any_i(error_msg, SERVER_DISCONNECT_PATTERNS, disc_count);
    if (is_disconnect && status_code <= 0) {
        bool is_large = false;
        if (context_length > 0 && approx_tokens > context_length * 0.6)
            is_large = true;
        else if (context_length <= 256000 && approx_tokens > 120000)
            is_large = true;

        if (is_large) {
            make_result(result, FAILOVER_CONTEXT_OVERFLOW, status_code, provider, model,
                        error_msg, true, true, false, false);
        } else {
            make_result(result, FAILOVER_TIMEOUT, status_code, provider, model,
                        error_msg, true, false, false, false);
        }
        return;
    }

    /* ── Step 8: Fallback: unknown ───────────────────────────────── */
    make_result(result, FAILOVER_UNKNOWN, status_code, provider, model,
                error_msg, true, false, false, false);
}

/* ════════════════════════════════════════════════════════════════════════
 *  Format helper
 * ════════════════════════════════════════════════════════════════════════ */

int error_format(const classified_error_t *err, char *buf, size_t bufsz) {
    if (!buf || bufsz == 0) return 0;

    static const char *reason_names[] = {
        "auth", "auth_permanent", "billing", "rate_limit",
        "overloaded", "server_error", "timeout",
        "context_overflow", "payload_too_large", "image_too_large",
        "model_not_found", "provider_policy_blocked",
        "format_error", "multimodal_tool_content_unsupported",
        "thinking_signature", "long_context_tier",
        "oauth_long_context_beta_forbidden", "llama_cpp_grammar_pattern",
        "unknown"
    };

    const char *rname = "?";
    if (err->reason >= 0 && err->reason < (int)(sizeof(reason_names)/sizeof(reason_names[0])))
        rname = reason_names[err->reason];

    return snprintf(buf, bufsz,
        "reason=%s status=%d provider=%s model=%s retryable=%s "
        "compress=%s rotate=%s fallback=%s msg=\"%s\"",
        rname, err->status_code, err->provider, err->model,
        err->retryable ? "yes" : "no",
        err->should_compress ? "yes" : "no",
        err->should_rotate_credential ? "yes" : "no",
        err->should_fallback ? "yes" : "no",
        err->message);
}
