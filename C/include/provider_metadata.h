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

/* List models filtered by required capabilities as JSON string (malloc'd).
 * required_caps: bitmask of capabilities the model must have (all must match).
 * Pass 0 to list all models (equivalent to model_metadata_list_json). */
char *model_metadata_list_filtered_json(model_capability_t required_caps);

/* Parse a capability name string (e.g. "vision", "streaming") into a bitmask.
 * Returns 0 on unknown/empty input. Accepts comma or space-separated. */
model_capability_t model_capability_parse(const char *name);

/* Get the short name for a capability flag. Returns "" for unknown. */
const char *model_capability_name(model_capability_t cap);

/* Format capability bitmask as comma-separated string into buf. */
void model_capability_format(model_capability_t caps, char *buf, size_t bufsz);

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

/* ================================================================
 *  A18: models.dev Integration
 * ================================================================ */

/* Fetch models.dev data with 3-tier cache (in-memory -> disk -> network).
 * force_refresh=true skips cache, always hits network.
 * Returns parsed JSON object keyed by provider ID, or NULL on failure. */
json_t *models_dev_fetch(bool force_refresh);

/* Look up context window for a specific provider/model from models.dev data.
 * Returns context window, or -1 if not found / unavailable. */
int models_dev_lookup_context(const char *provider, const char *model);

/* Convert models.dev data to flat JSON array string (same format as
 * model_metadata_list_json). Returns malloc'd string, caller must free(). */
char *models_dev_list_json(void);

/* ================================================================
 *  R10: Provider utility functions — ported from model_metadata.py
 * ================================================================ */

/* Normalize a base URL: strip whitespace and trailing slash.
 * Port of Python _normalize_base_url().
 * Returns malloc'd string, caller must free(). */
char *provider_normalize_base_url(const char *base_url);

/* Strip a recognized provider prefix from a model name.
 * Handles "provider/" and "provider:" prefix formats.
 * Preserves model:tag format (e.g. "qwen3.5:27b").
 * Returns malloc'd string, caller must free(). */
char *provider_strip_prefix(const char *model);

/* Check if a URL points to a local or private endpoint.
 * Port of Python model_metadata.is_local_endpoint().
 * Recognises loopback, container DNS (host.docker.internal),
 * RFC-1918 private ranges, link-local, and Tailscale CGNAT. */
bool provider_is_local_endpoint(const char *base_url);

/* Infer provider name from a base URL by matching against known provider hosts.
 * Port of Python model_metadata._infer_provider_from_url().
 * Returns malloc'd provider name string, or NULL if unknown. Caller must free(). */
char *provider_infer_from_url(const char *base_url);

/* Parse a context length limit from an API error message.
 * Port of Python model_metadata.parse_context_limit_from_error().
 * Returns limit, or -1 if not found. */
int provider_parse_context_limit_from_error(const char *error_msg);

/* Parse available output tokens from a max_tokens-too-large error message.
 * Port of Python model_metadata.parse_available_output_tokens_from_error().
 * Returns available tokens, or -1 if not a max_tokens-too-large error. */
int provider_parse_available_output_tokens_from_error(const char *error_msg);

/* Check if a candidate model ID matches a lookup model string.
 * Port of Python model_metadata._model_id_matches().
 * Supports exact match and slug match (part after last '/' equals lookup). */
bool provider_model_id_matches(const char *candidate_id, const char *lookup_model);

/* Check if a model name looks like a Kimi-family model.
 * Port of Python model_metadata._model_name_suggests_kimi().
 * Checks for 'kimi' prefix or 'moonshot' in the name (case-insensitive). */
bool provider_model_suggests_kimi(const char *model);

/* Normalize version separators: replace '.' with '-'.
 * Port of Python model_metadata._normalize_model_version().
 * Returns malloc'd string, caller must free(). */
char *provider_normalize_model_version(const char *model);

/* Check if Grok model supports reasoning.effort parameter.
 * Port of Python model_metadata.grok_supports_reasoning_effort(). */
bool model_grok_supports_reasoning_effort(const char *model);

/* Check if a URL is an OpenRouter base URL (contains openrouter.ai).
 * Port of Python model_metadata._is_openrouter_base_url(). */
bool provider_is_openrouter_base_url(const char *base_url);

/* Check if a URL is a custom (non-OpenRouter) endpoint.
 * Port of Python model_metadata._is_custom_endpoint(). */
bool provider_is_custom_endpoint(const char *base_url);

/* Check if a URL is a known provider base URL.
 * Port of Python model_metadata._is_known_provider_base_url(). */
bool provider_is_known_base_url(const char *base_url);

/* Build Authorization header dict as json_t {Authorization: Bearer <key>}.
 * Port of Python model_metadata._auth_headers().
 * Returns NULL when api_key is empty/NULL/whitespace-only. */
json_t *provider_auth_headers(const char *api_key);

/* Coerce a string value to an int within [minimum, maximum].
 * Port of Python model_metadata._coerce_reasonable_int().
 * Returns -1 on failure (not a valid int, or out of range). */
int provider_coerce_reasonable_int(const char *value, int minimum, int maximum);

/* Estimate tokens from text length (~4 chars/token, ceiling division).
 * Port of Python model_metadata.estimate_tokens_rough(). */
int estimate_tokens_rough(const char *text);

/* Resolve HERMES_VERIFY_SSL env var for HTTP client verification.
 * Port of Python model_metadata._resolve_requests_verify().
 * Returns 1 (verify), 0 (skip verify), or -1 (custom CA bundle path). */
int provider_resolve_requests_verify(void);

/* Return custom CA bundle path from HERMES_VERIFY_SSL env var, or NULL. */
const char *provider_requests_verify_path(void);

/* Extract context length from a model metadata payload JSON object.
 * Port of Python model_metadata._extract_context_length().
 * Searches nested dicts for known context-length keys.
 * Returns the value or -1 if not found. */
int provider_extract_context_length(const json_t *payload);

/* Extract max completion tokens from a model metadata payload JSON object.
 * Port of Python model_metadata._extract_max_completion_tokens().
 * Searches nested dicts for known max-completion-token keys.
 * Returns the value or -1 if not found. */
int provider_extract_max_completion_tokens(const json_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* PROVIDER_METADATA_H */
