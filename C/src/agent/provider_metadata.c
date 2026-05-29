/*
 * provider_metadata.c — Provider/model metadata lookup (P85).
 *
 * Central registry of model capabilities, context windows, and pricing.
 * Longer prefixes must come before shorter ones to avoid false matches.
 */

#include "provider_metadata.h"
#include "hermes_json.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Parse a capability name string (e.g. "vision", "streaming") into a bitmask.
 * Returns 0 on unknown. Comma-separated or space-separated multiple values. */
model_capability_t model_capability_parse(const char *name) {
    if (!name || !*name) return 0;
    model_capability_t caps = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", name);
    char *save = NULL;
    const char *tok = strtok_r(buf, " ,", &save);
    while (tok) {
        if (strcasecmp(tok, "vision") == 0) caps |= MODEL_CAP_VISION;
        else if (strcasecmp(tok, "streaming") == 0) caps |= MODEL_CAP_STREAMING;
        else if (strcasecmp(tok, "thinking") == 0 ||
                 strcasecmp(tok, "reasoning") == 0) caps |= MODEL_CAP_THINKING;
        else if (strcasecmp(tok, "fc") == 0 ||
                 strcasecmp(tok, "function_calling") == 0 ||
                 strcasecmp(tok, "tool_calling") == 0 ||
                 strcasecmp(tok, "tools") == 0) caps |= MODEL_CAP_FUNCTION_CALLING;
        else if (strcasecmp(tok, "structured_output") == 0 ||
                 strcasecmp(tok, "json") == 0) caps |= MODEL_CAP_STRUCTURED_OUTPUT;
        else if (strcasecmp(tok, "code") == 0 ||
                 strcasecmp(tok, "code_execution") == 0) caps |= MODEL_CAP_CODE_EXECUTION;
        else if (strcasecmp(tok, "caching") == 0 ||
                 strcasecmp(tok, "context_caching") == 0) caps |= MODEL_CAP_CONTEXT_CACHING;
        tok = strtok_r(NULL, " ,", &save);
    }
    return caps;
}

/* Capability name lookup (returns static ptr, not thread-safe) */
const char *model_capability_name(model_capability_t cap) {
    switch (cap) {
        case MODEL_CAP_VISION: return "vision";
        case MODEL_CAP_FUNCTION_CALLING: return "fc";
        case MODEL_CAP_STREAMING: return "streaming";
        case MODEL_CAP_THINKING: return "thinking";
        case MODEL_CAP_STRUCTURED_OUTPUT: return "json";
        case MODEL_CAP_CODE_EXECUTION: return "code";
        case MODEL_CAP_CONTEXT_CACHING: return "caching";
        default: return "";
    }
}

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
    {"nous",        "Nous Research","https://inference.nousresearch.com/v1",  true,  false, true},
    {"alibaba",     "Alibaba Qwen", "https://dashscope.aliyuncs.com/compatible-mode/v1", true, false, true},
    {"stepfun",     "StepFun",      "https://api.stepfun.com/v1",          true,  false, true},
    {"minimax",     "MiniMax",      "https://api.minimax.chat/v1",         true,  false, true},
    {"novita",      "Novita AI",    "https://api.novita.ai/v3/openai",     true,  false, true},
    {"zai",         "Zhipu AI",     "https://open.bigmodel.cn/api/paas/v4",true,  false, true},
    /* More OpenAI-compat providers (G41-G51) */
    {"huggingface", "Hugging Face", "https://huggingface.co/api/inference/v1", true, false, true},
    {"arcee",       "Arcee AI",     "https://api.arcee.ai/v1",                true, false, true},
    {"ollama_cloud","Ollama Cloud", "https://api.ollama.cloud/v1",            true, false, true},
    {"nvidia",      "Nvidia NIM",   "https://api.nvcf.nvidia.com/v1",         true, false, true},
    {"gmi",         "GMI",          "https://api.gmi.com/v1",                 true, false, true},
    {"kilocode",    "KiloCode",     "https://api.kilocode.ai/v1",             true, false, true},
    {"kimi",        "Kimi Coding",  "https://api.moonshot.cn/v1",             true, false, true},
    {"ai_gateway",  "AI Gateway",   "https://gateway.ai.cloudflare.com/v1",   true, false, true},
    {"azure_foundry","Azure AI Foundry","https://YOUR_PROJECT.openai.azure.com", true, true, true},
    {"xiaomi",      "Xiaomi",       "https://api.xiaomi.com/v1",              true, false, true},
    {"qwen_oauth",  "Qwen OAuth",   "https://dashscope.aliyuncs.com/v1",      true, false, true},
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
    {"deepseek-v4",      "DeepSeek v4",      65536, 8192, MODEL_CAP_FUNCTION_CALLING|MODEL_CAP_STREAMING|MODEL_CAP_CONTEXT_CACHING,   0.27,  1.10},
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

/* List models filtered by required capabilities */
char *model_metadata_list_filtered_json(model_capability_t required_caps) {
    json_node_t *root = json_array();
    for (int i = 0; MODELS[i].model_prefix; i++) {
        if (required_caps != 0 && (MODELS[i].caps & required_caps) != required_caps)
            continue;
        json_node_t *entry = json_object();
        json_set(entry, "model", json_string(MODELS[i].model_prefix));
        json_set(entry, "family", json_string(MODELS[i].family));
        json_set(entry, "context_window", json_number(MODELS[i].context_window));
        json_set(entry, "max_output", json_number(MODELS[i].max_output));
        json_set(entry, "caps", json_number((double)MODELS[i].caps));
        json_append(root, entry);
    }
    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

/* Format capability bitmask as comma-separated string */
void model_capability_format(model_capability_t caps, char *buf, size_t bufsz) {
    if (!buf || bufsz == 0) return;
    buf[0] = '\0';
    size_t pos = 0;
    #define APPEND_CAP(flag, name) do { \
        if (caps & flag) { \
            size_t nlen = strlen(name); \
            if (pos + nlen + 2 < bufsz) { \
                if (pos > 0) { buf[pos++] = ','; buf[pos] = '\0'; } \
                memcpy(buf + pos, name, nlen); \
                pos += nlen; \
                buf[pos] = '\0'; \
            } \
        } \
    } while(0)
    APPEND_CAP(MODEL_CAP_VISION, "vision");
    APPEND_CAP(MODEL_CAP_FUNCTION_CALLING, "fc");
    APPEND_CAP(MODEL_CAP_STREAMING, "streaming");
    APPEND_CAP(MODEL_CAP_THINKING, "thinking");
    APPEND_CAP(MODEL_CAP_STRUCTURED_OUTPUT, "json");
    APPEND_CAP(MODEL_CAP_CODE_EXECUTION, "code");
    APPEND_CAP(MODEL_CAP_CONTEXT_CACHING, "caching");
    #undef APPEND_CAP
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

/* ================================================================
 *  P158: API Key Security
 * ================================================================ */

bool provider_url_is_trusted(const char *provider_name, const char *url) {
    if (!provider_name || !url) return false;
    if (!url_has_valid_scheme(url)) return false;

    /* Look up provider metadata */
    const provider_metadata_t *meta = provider_metadata_find(provider_name);
    if (!meta || !meta->base_url || !*meta->base_url) {
        /* Unknown provider — trust by default (defensive) */
        return true;
    }

    /* Extract hostname from the provider's known base_url */
    char *expected_host = url_extract_hostname(meta->base_url);
    if (!expected_host) {
        /* Can't extract from metadata — trust by default */
        return true;
    }

    /* Check if URL host matches provider's authoritative host */
    bool trusted = url_host_matches(url, expected_host);
    free(expected_host);

    if (!trusted) {
        fprintf(stderr, "[provider-security] WARNING: URL %s does not match "
                "provider %s's known endpoint %s — not sending API key\n",
                url, provider_name, meta->base_url);
    }

    return trusted;
}

char *provider_derive_api_key_name(const char *provider_name, const char *base_url) {
    if (!provider_name && !base_url) return NULL;

    const char *src = NULL;

    /* First try: extract hostname from base_url */
    if (base_url && *base_url) {
        char *host = url_extract_hostname(base_url);
        if (host) {
            char host_lower[256];
            size_t hl = 0;
            for (const char *p = host; *p && hl < sizeof(host_lower) - 1; p++) {
                host_lower[hl++] = tolower((unsigned char)*p);
            }
            host_lower[hl] = '\0';

            /* Common API subdomains to skip */
            static const char *prefixes[] = {
                "api.", "api-", "openapi.", "rest.", "ws.", "gateway.",
                "generativelanguage.", NULL
            };

            char stripped[256] = "";
            for (int pi = 0; prefixes[pi]; pi++) {
                size_t plen = strlen(prefixes[pi]);
                if (strncmp(host_lower, prefixes[pi], plen) == 0) {
                    const char *rest = host_lower + plen;
                    const char *dot = strchr(rest, '.');
                    if (dot) {
                        size_t root_len = (size_t)(dot - rest);
                        if (root_len > 0 && root_len < sizeof(stripped)) {
                            memcpy(stripped, rest, root_len);
                            stripped[root_len] = '\0';
                            src = stripped;
                        }
                    }
                    break;
                }
            }

            /* If no prefix stripped, use the first label */
            if (!src && hl > 0) {
                const char *dot = strchr(host_lower, '.');
                if (dot) {
                    size_t first_len = (size_t)(dot - host_lower);
                    if (first_len > 0 && first_len < sizeof(stripped)) {
                        memcpy(stripped, host_lower, first_len);
                        stripped[first_len] = '\0';
                        src = stripped;
                    }
                }
            }

            free(host);
        }
    }

    /* Second try: use provider name directly */
    if (!src && provider_name) {
        const provider_metadata_t *meta = provider_metadata_find(provider_name);
        if (meta && meta->provider_name) {
            src = meta->provider_name;
        } else {
            src = provider_name;
        }
    }

    if (!src) return NULL;

    /* Build env var name: VENDOR_API_KEY (uppercase, underscores for hyphens) */
    char result[128];
    int pos = 0;
    for (const char *p = src; *p && pos < (int)sizeof(result) - 12; p++) {
        if (*p == '-' || *p == ' ') {
            result[pos++] = '_';
        } else {
            result[pos++] = toupper((unsigned char)*p);
        }
    }
    const char *suffix = "_API_KEY";
    size_t slen = strlen(suffix);
    if ((size_t)pos + slen < sizeof(result)) {
        memcpy(result + pos, suffix, slen + 1);
    } else {
        return NULL;
    }

    return strdup(result);
}

/* ================================================================
 *  L06: supports_vision config override
 * ================================================================ */

bool model_supports_vision(const char *model_name, const provider_config_t *provider_cfg) {
    if (!model_name) return false;

    /* 1. Config override takes precedence */
    if (provider_cfg && provider_cfg->supports_vision) {
        return true;
    }

    /* 2. S06: Check per-model vision overrides (comma-separated prefixes) */
    if (provider_cfg && provider_cfg->vision_overrides[0]) {
        char buf[1024];
        strncpy(buf, provider_cfg->vision_overrides, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = buf;
        while (tok && *tok) {
            while (*tok == ' ' || *tok == ',') tok++;
            if (!*tok) break;
            char *end = tok;
            while (*end && *end != ',') end++;
            int is_last = (*end == '\0');
            *end = '\0';
            /* Trim trailing whitespace */
            char *trim = end - 1;
            while (trim >= tok && *trim == ' ') { *trim = '\0'; trim--; }
            if (*tok && strncasecmp(model_name, tok, strlen(tok)) == 0) {
                return true;
            }
            tok = is_last ? NULL : end + 1;
        }
    }

    /* 3. Fall back to metadata lookup */
    return model_name_has_capability(model_name, MODEL_CAP_VISION);
}
