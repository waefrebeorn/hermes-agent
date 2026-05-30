/*
 * provider.h — Abstract provider interface for Hermes C.
 * Phase 101-110: Multi-provider support.
 *
 * Each provider implements:
 *  - build_url: construct the API endpoint URL
 *  - build_headers: construct auth + content-type headers
 *  - parse_response: extract content, reasoning, tool_calls from response JSON
 *  - build_message_body: construct the JSON message body (provider-specific format)
 */

/**
 * @defgroup providers Provider System
 * @brief Provider operations tables and registration.
 *
 * Defines provider_ops_t interface (build_url, build_headers,
 * build_request_body, parse_response, parse_stream_chunk, free_response)
 * and the provider registration system (max 32 providers).
 *
 * Implementations: OpenAI, OpenRouter, DeepSeek, xAI, Anthropic,
 * Google, Azure, Bedrock, Custom.
 *
 * @{
 */
#ifndef PROVIDER_H
#define PROVIDER_H

#include "hermes.h"
#include "hermes_json.h"

#include "credential_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Provider Interface
 * ================================================================ */

/* Provider identification */
typedef enum {
    PROVIDER_OPENAI,       /* OpenAI-compatible (OpenAI, Groq, Together, etc.) */
    PROVIDER_ANTHROPIC,    /* Anthropic API format */
    PROVIDER_GOOGLE,       /* Google AI / Gemini */
    PROVIDER_OPENROUTER,   /* OpenRouter — model routing with preferences */
    PROVIDER_DEEPSEEK,     /* DeepSeek — context caching, FIM */
    PROVIDER_XAI,          /* xAI — Grok API */
    PROVIDER_AZURE,        /* Azure OpenAI — api-key auth, deployment URL */
    PROVIDER_BEDROCK,      /* AWS Bedrock — SigV4 auth, Converse API */
    PROVIDER_CUSTOM,       /* Custom provider */
} provider_type_t;

/* Opaque provider handle */
typedef struct provider_t provider_t;

/* Response parsed from provider-specific format */
typedef struct {
    char *content;
    char *reasoning;
    char *encrypted_content;  /* L07: xAI encrypted reasoning content */
    int   input_tokens;
    int   output_tokens;
    int   tool_calls_count;
    tool_call_t tool_calls[64];
    char  finish_reason[32];   /* B22: "stop", "length", "tool_calls", "content_filter" */
} provider_response_t;

/* Provider operations (function pointers) */
typedef struct {
    /* Build the API URL. Returns malloc'd string or NULL. */
    char *(*build_url)(const provider_t *p, const char *base_url);

    /* Build HTTP headers. Returns malloc'd string or NULL. */
    char *(*build_headers)(const provider_t *p, const char *api_key);

    /* Build the JSON request body. Returns malloc'd string or NULL. */
    char *(*build_request_body)(const provider_t *p,
                                const message_t **messages, size_t msg_count,
                                json_node_t *tools_json,
                                bool streaming);

    /* Parse API response into common format. Returns provider_response. */
    provider_response_t *(*parse_response)(const provider_t *p,
                                            const char *response_body);

    /* Parse streaming chunk. Returns provider_response with partial content. */
    provider_response_t *(*parse_stream_chunk)(const provider_t *p,
                                                const char *chunk);

    /* Free a provider response */
    void (*free_response)(provider_response_t *resp);

    /* B32: FIM (Fill-in-the-Middle) — optional, NULL = not supported.
     * Build FIM request body with prompt + suffix for code completion.
     * Returns malloc'd JSON string or NULL. */
    char *(*build_fim_body)(const provider_t *p,
                            const char *prompt,
                            const char *suffix,
                            int max_tokens);

    /* B32: Parse FIM response (returns text in content field, not message.content).
     * Returns provider_response with content set to FIM completion text. */
    provider_response_t *(*parse_fim_response)(const provider_t *p,
                                                const char *response_body);

    /* B32: Build FIM endpoint URL (e.g., /beta/completions instead of /chat/completions).
     * Returns malloc'd URL or NULL. Defaults to build_url if NULL. */
    char *(*build_fim_url)(const provider_t *p, const char *base_url);

    /* Provider name */
    const char *name;
} provider_ops_t;

/* Provider instance */
struct provider_t {
    provider_type_t type;
    const provider_ops_t *ops;
    char name[64];
    char model[128];
    char api_key[512];
    char base_url[512];
    void *data; /* Provider-specific data */
    credential_pool_t *pool; /* P82: optional credential pool for multi-key rotation */
    bool  system_cached; /* P91: system prompt cache primed flag */
    /* LLM request params wired from config */
    provider_config_t config;
};

/* ================================================================
 *  Provider Registry
 * ================================================================ */

/* Register a provider implementation */
void provider_register(provider_type_t type, const provider_ops_t *ops);

/* Create a provider instance from config */
provider_t *provider_create(const char *provider_name,
                             const char *model,
                             const char *api_key,
                             const char *base_url);

/* Free a provider instance */
void provider_free(provider_t *p);

/* Attach a credential pool to a provider instance.
 * The pool is NOT owned by the provider — caller must free it separately
 * after provider_free(). Use NULL to detach. */
void provider_set_credential_pool(provider_t *p, credential_pool_t *pool);

/* Get the credential pool attached to this provider (may be NULL). */
credential_pool_t *provider_get_credential_pool(const provider_t *p);

/* B32: FIM (Fill-in-the-Middle) code completion call.
 * Returns provider_response with content set to FIM completion text.
 * Returns NULL if provider does not support FIM.
 * Caller must free response with provider_free_response(). */
provider_response_t *provider_fim(provider_t *p,
                                   const char *prompt,
                                   const char *suffix,
                                   int max_tokens);

/* Check if provider supports FIM */
bool provider_has_fim(const provider_t *p);

/* Get provider operations (convenience) */
static inline const provider_ops_t *provider_ops(const provider_t *p) {
    return p ? p->ops : NULL;
}

/* P91: System prompt caching — set whether cache has been primed */
static inline void provider_set_system_cached(provider_t *p, bool cached) {
    if (p) p->system_cached = cached;
}

/* P91: Check if system prompt cache is primed */
static inline bool provider_get_system_cached(const provider_t *p) {
    return p ? p->system_cached : false;
}

/* ================================================================
 *  Built-in Provider Implementations
 * ================================================================ */

/* OpenAI-compatible (covers OpenAI, DeepSeek, OpenRouter, Groq, etc.) */
extern const provider_ops_t PROVIDER_OPS_OPENAI;

/* OpenRouter with model routing and provider preferences */
extern const provider_ops_t PROVIDER_OPS_OPENROUTER;

/* DeepSeek with context caching and FIM support */
extern const provider_ops_t PROVIDER_OPS_DEEPSEEK;

/* xAI (Grok) with native API support */
extern const provider_ops_t PROVIDER_OPS_XAI;

/* Anthropic API format */
extern const provider_ops_t PROVIDER_OPS_ANTHROPIC;

/* Google Gemini API format */
extern const provider_ops_t PROVIDER_OPS_GOOGLE;

/* Azure OpenAI API format */
extern const provider_ops_t PROVIDER_OPS_AZURE;

/* AWS Bedrock Converse API */
extern const provider_ops_t PROVIDER_OPS_BEDROCK;

/* Bedrock utility functions — ported from Python bedrock_adapter.py */
bool bedrock_is_context_overflow(const char *error_message);
const char *bedrock_classify_error(const char *error_message);
char *bedrock_extract_provider_from_arn(const char *arn);
int  bedrock_get_context_length(const char *model_id);

/* Google provider utility functions */
bool google_is_native_base_url(const char *base_url);
char *google_coerce_content_to_text(const json_t *content);

/* Custom (user-defined) provider */
extern const provider_ops_t PROVIDER_OPS_CUSTOM;

/* Register all built-in providers */
void provider_register_builtins(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of providers group */

#endif /* PROVIDER_H */
