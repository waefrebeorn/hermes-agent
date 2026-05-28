/*
 * provider_metadata.h — Provider/model metadata for Hermes C (P85).
 *
 * Defines model capabilities, context windows, and pricing.
 * Used by budget tracking, model selection, and agent loop.
 */

#ifndef PROVIDER_METADATA_H
#define PROVIDER_METADATA_H

#include "hermes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Model Capabilities — bitmask flags
 * ================================================================ */

typedef enum {
    MODEL_CAP_NONE               = 0,
    MODEL_CAP_VISION             = 1 << 0,  /* can process images */
    MODEL_CAP_FUNCTION_CALLING   = 1 << 1,  /* supports tool/function calling */
    MODEL_CAP_STREAMING          = 1 << 2,  /* supports response streaming */
    MODEL_CAP_THINKING           = 1 << 3,  /* extended thinking/reasoning */
    MODEL_CAP_STRUCTURED_OUTPUT  = 1 << 4,  /* JSON mode / structured output */
    MODEL_CAP_CODE_EXECUTION     = 1 << 5,  /* code interpreter / sandbox */
    MODEL_CAP_CONTEXT_CACHING    = 1 << 6,  /* supports prompt caching */
} model_capability_t;

/* ================================================================
 *  Model Metadata Record
 * ================================================================ */

typedef struct {
    const char *model_prefix;    /* matched by prefix (longer first) */
    const char *family;          /* model family name */
    int         context_window;  /* max context tokens */
    int         max_output;      /* max output tokens */
    model_capability_t caps;     /* capability bitmask */
    double      input_per_1m;    /* USD per 1M input tokens */
    double      output_per_1m;   /* USD per 1M output tokens */
} model_metadata_t;

/* ================================================================
 *  Provider Metadata Record
 * ================================================================ */

typedef struct {
    const char *provider_name;   /* "openai", "anthropic", etc. */
    const char *display_name;    /* "OpenAI", "Anthropic", etc. */
    const char *base_url;        /* default API endpoint */
    bool        supports_streaming;
    bool        supports_thinking;
    bool        supports_tool_calling;
} provider_metadata_t;

/* ================================================================
 *  API
 * ================================================================ */

/* Find model metadata by model name (prefix match). Returns NULL if not found. */
const model_metadata_t *model_metadata_find(const char *model_name);

/* Get provider metadata by provider name (case-insensitive). Returns NULL if not found. */
const provider_metadata_t *provider_metadata_find(const char *provider_name);

/* Check if a model has a specific capability. */
static inline bool model_has_capability(const model_metadata_t *meta, model_capability_t cap) {
    return meta && (meta->caps & cap) != 0;
}

/* Check if a model name has a specific capability (convenience). */
bool model_name_has_capability(const char *model_name, model_capability_t cap);

/* Get context window for a model. Returns -1 if unknown. */
int model_context_window(const char *model_name);

/* Get max output tokens for a model. Returns -1 if unknown. */
int model_max_output(const char *model_name);

/* Compute estimated USD cost from token counts. Wraps the pricing table. */
double model_estimate_cost(const char *model, long long input_tokens, long long output_tokens);

/* List all known models as JSON string (malloc'd). Caller must free(). */
char *model_metadata_list_json(void);

/* Get all known providers as JSON string (malloc'd). Caller must free(). */
char *provider_metadata_list_json(void);

/* ================================================================
 *  P158: API Key Security
 * ================================================================ */

/* Check if a URL is trusted to receive this provider's API key.
 * Compares URL host against the provider's known authoritative hostname.
 * Returns true if host matches (or is subdomain of) the provider's known host.
 * Falls back to true if provider not found in metadata (defensive). */
bool provider_url_is_trusted(const char *provider_name, const char *url);

/* Derive <VENDOR>_API_KEY env var name from provider name or base_url.
 * When no explicit API key is set, this derives the likely env var name
 * from the provider's hostname (e.g. "api.deepseek.com" → "DEEPSEEK_API_KEY").
 * Returns malloc'd env var name string, or NULL if undetermined.
 * Caller must free(). */
char *provider_derive_api_key_name(const char *provider_name, const char *base_url);

/* ================================================================
 *  L06: supports_vision config override helper
 * ================================================================ */

/* Check if a model supports vision, respecting the config override.
 * If supports_vision override is set (true), returns true regardless of metadata.
 * If supports_vision override is unset (false), delegates to model_metadata.
 * The override comes from model.supports_vision YAML key or HERMES_SUPPORTS_VISION env var.
 * provider_cfg can be NULL (no override). */
bool model_supports_vision(const char *model_name, const provider_config_t *provider_cfg);

#ifdef __cplusplus
}
#endif

#endif /* PROVIDER_METADATA_H */
