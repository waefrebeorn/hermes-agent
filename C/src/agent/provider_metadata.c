/*
 * provider_metadata.c — Provider/model metadata lookup (P85).
 *
 * Central registry of model capabilities, context windows, and pricing.
 * Longer prefixes must come before shorter ones to avoid false matches.
 */

#include "provider_metadata.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Provider metadata
 * ================================================================ */

static const provider_metadata_t PROVIDERS[] = {
    {"openai",      "OpenAI",       "https://api.openai.com/v1",         true,  true,  true},
    {"anthropic",   "Anthropic",    "https://api.anthropic.com/v1",      true,  true,  true},
    {"google",      "Google AI",    "https://generativelanguage.googleapis.com/v1beta", true, false, true},
    {"deepseek",    "DeepSeek",     "https://api.deepseek.com/v1",       true,  true,  true},
    {"openrouter",  "OpenRouter",   "https://openrouter.ai/api/v1",      true,  false, true},
    {"groq",        "Groq",         "https://api.groq.com/openai/v1",    true,  false, true},
    {"together",    "Together AI",  "https://api.together.xyz/v1",       true,  false, true},
    {"xai",         "xAI",          "https://api.x.ai/v1",               true,  false, true},
    {"azure",       "Azure OpenAI", "https://YOUR_RESOURCE.openai.azure.com", true, true, true},
    {"bedrock",     "AWS Bedrock",  "",                                   true,  true,  true},
    {NULL, NULL, NULL, false, false, false},
};

/* ================================================================
 *  Model metadata — longer prefixes first
 * ================================================================ */

static const model_metadata_t MODELS[] = {
    /* OpenAI */
    {"gpt-4.1",        "GPT-4.1",      1048576, 32768, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_STRUCTURED_OUTPUT|MODEL_CAP_CONTEXT_CACHING,   2.00,  8.00},
    {"gpt-4o-mini",    "GPT-4o Mini",   131072, 16384, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_STRUCTURED_OUTPUT,   0.15,  0.60},
    {"gpt-4o",         "GPT-4o",        131072, 16384, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_STRUCTURED_OUTPUT|MODEL_CAP_CONTEXT_CACHING,   2.50, 10.00},
    {"gpt-4-turbo",    "GPT-4 Turbo",   131072,  4096, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING,  10.00, 30.00},
    {"gpt-4",          "GPT-4",          32768,  4096, MODEL_CAP_FUNCTION_CALLING,  30.00, 60.00},
    {"gpt-3.5",        "GPT-3.5 Turbo",  16384,  4096, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING,   0.50,  1.50},
    {"o1",             "o1",             131072, 32768, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_THINKING,  15.00, 60.00},
    {"o3-mini",        "o3-mini",        131072, 32768, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_THINKING,   1.10,  4.40},
    /* Anthropic */
    {"claude-3.5-sonnet", "Claude 3.5 Sonnet", 131072, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_THINKING|MODEL_CAP_CONTEXT_CACHING,   3.00, 15.00},
    {"claude-3.5-haiku",  "Claude 3.5 Haiku",  131072, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   0.80,  4.00},
    {"claude-3-opus",     "Claude 3 Opus",      65536, 4096, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING,  15.00, 75.00},
    {"claude-4",       "Claude 4",       131072, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_THINKING|MODEL_CAP_CONTEXT_CACHING,   3.00, 15.00},
    /* Google */
    {"gemini-2.0-pro",  "Gemini 2.0 Pro",  1048576, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   2.00,  8.00},
    {"gemini-2.0-flash","Gemini 2.0 Flash", 1048576, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   0.10,  0.40},
    {"gemini-1.5-pro",  "Gemini 1.5 Pro",   2097152, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   3.50, 10.50},
    {"gemini-1.5-flash","Gemini 1.5 Flash", 1048576, 8192, MODEL_CAP_VISION|MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   0.075, 0.30},
    /* DeepSeek */
    {"deepseek-chat",    "DeepSeek Chat",    65536, 8192, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   0.27,  1.10},
    {"deepseek-reasoner","DeepSeek Reasoner",65536, 8192, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_THINKING|MODEL_CAP_CONTEXT_CACHING,   0.55,  2.19},
    /* Default fallback */
    {NULL,               "Unknown",         32768, 4096, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING,   1.00,  4.00},
};

/* ================================================================
 *  Implementation
 * ================================================================ */

const model_metadata_t *model_metadata_find(const char *model_name) {
    if (!model_name) return NULL;
    for (int i = 0; MODELS[i].model_prefix; i++) {
        if (strncasecmp(model_name, MODELS[i].model_prefix, strlen(MODELS[i].model_prefix)) == 0)
            return &MODELS[i];
    }
    return NULL; /* No match */
}

const provider_metadata_t *provider_metadata_find(const char *provider_name) {
    if (!provider_name) return NULL;
    for (int i = 0; PROVIDERS[i].provider_name; i++) {
        if (strcasecmp(provider_name, PROVIDERS[i].provider_name) == 0)
            return &PROVIDERS[i];
    }
    return NULL;
}

bool model_name_has_capability(const char *model_name, model_capability_t cap) {
    const model_metadata_t *meta = model_metadata_find(model_name);
    return model_has_capability(meta, cap);
}

int model_context_window(const char *model_name) {
    const model_metadata_t *meta = model_metadata_find(model_name);
    return meta ? meta->context_window : -1;
}

int model_max_output(const char *model_name) {
    const model_metadata_t *meta = model_metadata_find(model_name);
    return meta ? meta->max_output : -1;
}

double model_estimate_cost(const char *model, long long input_tokens, long long output_tokens) {
    const model_metadata_t *meta = model_metadata_find(model);
    if (!meta) {
        /* Use fallback */
        meta = &MODELS[sizeof(MODELS)/sizeof(MODELS[0]) - 1];
    }
    double input_cost = (double)input_tokens / 1000000.0 * meta->input_per_1m;
    double output_cost = (double)output_tokens / 1000000.0 * meta->output_per_1m;
    return input_cost + output_cost;
}

char *model_metadata_list_json(void) {
    json_node_t *root = json_array();
    for (int i = 0; MODELS[i].model_prefix; i++) {
        json_node_t *entry = json_object();
        json_set(entry, "model", json_string(MODELS[i].model_prefix));
        json_set(entry, "family", json_string(MODELS[i].family));
        json_set(entry, "context_window", json_number(MODELS[i].context_window));
        json_set(entry, "max_output", json_number(MODELS[i].max_output));
        json_append(root, entry);
    }
    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

char *provider_metadata_list_json(void) {
    json_node_t *root = json_array();
    for (int i = 0; PROVIDERS[i].provider_name; i++) {
        json_node_t *entry = json_object();
        json_set(entry, "name", json_string(PROVIDERS[i].provider_name));
        json_set(entry, "display_name", json_string(PROVIDERS[i].display_name));
        json_set(entry, "base_url", json_string(PROVIDERS[i].base_url));
        json_set(entry, "supports_streaming", json_bool(PROVIDERS[i].supports_streaming));
        json_set(entry, "supports_thinking", json_bool(PROVIDERS[i].supports_thinking));
        json_set(entry, "supports_tool_calling", json_bool(PROVIDERS[i].supports_tool_calling));
        json_append(root, entry);
    }
    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}
