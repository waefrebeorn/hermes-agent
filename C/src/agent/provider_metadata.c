/*
 * provider_metadata.c — Provider/model metadata lookup (P85).
 *
 * Central registry of model capabilities, context windows, and pricing.
 * Longer prefixes must come before shorter ones to avoid false matches.
 *
 * A18: models.dev integration — HTTP fetch + 3-tier cache.
 */

#include "provider_metadata.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

/* models.dev fetch constants */
#define MODELS_DEV_URL       "https://models.dev/api.json"
#define MODELS_DEV_CACHE_TTL 3600  /* 1 hour */
#define MODELS_DEV_TIMEOUT   10    /* 10s HTTP timeout */
#define MODELS_DEV_CACHE_FILE "models_dev_cache.json"

/* In-memory cache */
static json_t *g_models_dev_cache = NULL;
static time_t  g_models_dev_cache_time = 0;

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


/* ================================================================
 *  A18: models.dev Integration — HTTP fetch + 3-tier cache
 * ================================================================ */

/* Build path to models.dev disk cache: ~/.hermes/models_dev_cache.json */
static char *get_models_dev_cache_path(void) {
    const char *home = getenv("XDG_CONFIG_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home) return NULL;
    size_t len = strlen(home) + 64;
    char *path = (char *)malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/.hermes/%s", home, MODELS_DEV_CACHE_FILE);
    return path;
}

/* Load models.dev cache from disk. Returns parsed JSON or NULL. */
static json_t *models_dev_load_disk_cache(void) {
    char *path = get_models_dev_cache_path();
    if (!path) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) { free(path); return NULL; }
    time_t now = time(NULL);
    if (now - st.st_mtime > MODELS_DEV_CACHE_TTL * 2) {
        free(path);
        return NULL;
    }

    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[nread] = '\0';

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    free(err);
    return root;
}

/* Save models.dev data to disk cache. */
static bool models_dev_save_disk_cache(json_t *data) {
    char *path = get_models_dev_cache_path();
    if (!path) return false;

    char *json_str = json_serialize(data);
    if (!json_str) { free(path); return false; }

    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(json_str); free(path); return false; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);

    rename(tmp_path, path);
    free(path);
    return true;
}

/* Fetch models.dev data from network. Returns parsed JSON or NULL. */
static json_t *models_dev_fetch_network(void) {
    http_t *h = http_new(MODELS_DEV_TIMEOUT);
    if (!h) return NULL;

    http_resp_t *resp = http_get(h, MODELS_DEV_URL, NULL);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    char *err = NULL;
    json_t *root = json_parse(resp->body, &err);
    http_resp_free(resp);
    http_free(h);
    free(err);
    return root;
}

/* Fetch models.dev data with 3-tier cache: in-memory -> disk -> network.
 * Returns parsed JSON (root is a JSON object keyed by provider ID),
 * or NULL if all sources fail. */
json_t *models_dev_fetch(bool force_refresh) {
    time_t now = time(NULL);

    /* Stage 1: fresh in-memory cache */
    if (!force_refresh && g_models_dev_cache &&
        (now - g_models_dev_cache_time) < MODELS_DEV_CACHE_TTL) {
        return g_models_dev_cache;
    }

    /* Stage 2: fresh disk cache (load into memory) */
    if (!force_refresh) {
        json_t *disk = models_dev_load_disk_cache();
        if (disk) {
            struct stat st;
            char *path = get_models_dev_cache_path();
            if (path) {
                if (stat(path, &st) == 0 &&
                    (now - st.st_mtime) < MODELS_DEV_CACHE_TTL) {
                    if (g_models_dev_cache) json_free(g_models_dev_cache);
                    g_models_dev_cache = disk;
                    g_models_dev_cache_time = now;
                    free(path);
                    return g_models_dev_cache;
                }
                free(path);
            }
            json_free(disk);
        }
    }

    /* Stage 3: network fetch */
    json_t *net = models_dev_fetch_network();
    if (net) {
        models_dev_save_disk_cache(net);
        if (g_models_dev_cache) json_free(g_models_dev_cache);
        g_models_dev_cache = net;
        g_models_dev_cache_time = now;
        return g_models_dev_cache;
    }

    /* Stage 4: fallback to any disk cache (even stale) */
    json_t *stale = models_dev_load_disk_cache();
    if (stale) {
        if (g_models_dev_cache) json_free(g_models_dev_cache);
        g_models_dev_cache = stale;
        g_models_dev_cache_time = now;
        return g_models_dev_cache;
    }

    return NULL;
}

/* Look up context window from models.dev data.
 * Returns context window, or -1 if not found. */
int models_dev_lookup_context(const char *provider, const char *model) {
    if (!provider || !model) return -1;
    json_t *data = models_dev_fetch(false);
    if (!data) return -1;

    json_t *prov_node = json_obj_get(data, provider);
    if (!prov_node) return -1;

    json_t *models_node = json_obj_get(prov_node, "models");
    if (!models_node) return -1;

    json_t *model_node = json_obj_get(models_node, model);
    if (!model_node) return -1;

    json_t *ctx_node = json_obj_get(model_node, "context");
    if (!ctx_node || ctx_node->type != JSON_NUMBER) return -1;
    return (int)ctx_node->num_val;
}

/* Convert models.dev data to a flat JSON array string for /model list.
 * Returns malloc'd JSON string, caller must free(). */
char *models_dev_list_json(void) {
    json_t *data = models_dev_fetch(false);
    if (!data) return NULL;

    json_t *arr = json_array();
    if (!arr) return NULL;

    /* Iterate all provider keys */
    for (size_t i = 0; i < data->c.count; i++) {
        const char *prov_name = data->c.keys[i];
        json_t *prov = data->c.items[i];
        if (!prov_name || !prov) continue;

        json_t *models = json_obj_get(prov, "models");
        if (!models || models->type != JSON_OBJECT) continue;

        for (size_t mi = 0; mi < models->c.count; mi++) {
            const char *model_name = models->c.keys[mi];
            json_t *md = models->c.items[mi];
            if (!model_name || !md) continue;

            json_t *entry = json_object();
            json_set(entry, "id", json_string(model_name));
            json_set(entry, "provider", json_string(prov_name));

            json_t *ctx = json_obj_get(md, "context");
            if (ctx && ctx->type == JSON_NUMBER)
                json_set(entry, "context_window", json_copy(ctx));

            json_t *out = json_obj_get(md, "max_output");
            if (out && out->type == JSON_NUMBER)
                json_set(entry, "max_output", json_copy(out));

            json_t *desc = json_obj_get(md, "description");
            if (desc && desc->type == JSON_STRING)
                json_set(entry, "description", json_copy(desc));

            json_append(arr, entry);
        }
    }

    char *result = json_serialize(arr);
    json_free(arr);
    return result;
}

/* ================================================================
 *  R10: Provider utility functions — ported from model_metadata.py
 * ================================================================ */

/* Normalize a base URL: strip whitespace and trailing slash.
 * Port of Python _normalize_base_url().
 * Returns malloc'd string, caller must free(). */
char *provider_normalize_base_url(const char *base_url) {
    if (!base_url || !*base_url) return NULL;

    /* Skip leading whitespace */
    const char *start = base_url;
    while (*start && isspace((unsigned char)*start)) start++;
    if (!*start) return NULL;

    /* Find end (before trailing whitespace) */
    const char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    /* Strip trailing slash */
    if (*end == '/') end--;

    size_t len = (size_t)(end - start + 1);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/* Strip a recognized provider prefix from a model name.
 * Handles "provider/" prefix format: "openrouter/gpt-4" → "gpt-4".
 * Handles "provider:" prefix format: "openrouter:gpt-4" → "gpt-4".
 * Preserves model:tag format like "qwen3.5:27b" (not a provider prefix).
 * Returns malloc'd string, caller must free(). Returns NULL on error. */
char *provider_strip_prefix(const char *model) {
    if (!model || !*model) return NULL;

    /* Skip if starts with http (URL, not model name) */
    if (strncmp(model, "http", 4) == 0)
        return strdup(model);

    /* Check for "provider/" format (last slash) */
    const char *slash = strrchr(model, '/');
    if (slash && slash != model) {
        /* Check if prefix looks like a provider name (no dots, no digits at start) */
        size_t prefix_len = (size_t)(slash - model);
        if (prefix_len > 0 && prefix_len < 64) {
            return strdup(slash + 1);
        }
    }

    /* Check for "provider:" format (first colon, but not model:tag) */
    const char *colon = strchr(model, ':');
    if (colon && colon != model) {
        size_t prefix_len = (size_t)(colon - model);
        if (prefix_len > 0 && prefix_len < 64) {
            /* Check if suffix looks like a tag (e.g. "7b", "latest", "q4_0") */
            const char *suffix = colon + 1;
            /* Simple heuristic: if suffix starts with a digit or is short, it's a tag */
            bool looks_like_tag = false;
            if (isdigit((unsigned char)*suffix))
                looks_like_tag = true;
            else if (strncmp(suffix, "latest", 6) == 0 ||
                     strncmp(suffix, "stable", 6) == 0 ||
                     strncmp(suffix, "instruct", 8) == 0 ||
                     strncmp(suffix, "text", 4) == 0)
                looks_like_tag = true;

            if (!looks_like_tag)
                return strdup(suffix);
        }
    }

    /* No prefix detected — return copy */
    return strdup(model);
}

/* Check if a URL points to a local or private endpoint.
 * Port of Python model_metadata.is_local_endpoint().
 * Recognises loopback, container DNS (host.docker.internal),
 * RFC-1918 private ranges, link-local, and Tailscale CGNAT.
 * Returns true if the host is local/private. */
bool provider_is_local_endpoint(const char *base_url) {
    if (!base_url || !*base_url) return false;

    /* Normalize */
    char *normalized = provider_normalize_base_url(base_url);
    if (!normalized) return false;

    /* Add scheme if missing */
    char url_buf[1024];
    if (strstr(normalized, "://")) {
        snprintf(url_buf, sizeof(url_buf), "%s", normalized);
    } else {
        snprintf(url_buf, sizeof(url_buf), "http://%s", normalized);
    }
    free(normalized);

    /* Extract hostname — handle IPv6 literal URLs separately */
    char host_buf[256];
    const char *host = NULL;

    /* Check for IPv6 bracketed address */
    const char *scheme_end = strstr(url_buf, "://");
    if (scheme_end) {
        const char *host_start = scheme_end + 3;
        if (*host_start == '[') {
            /* IPv6 literal: extract between brackets */
            const char *close_bracket = strchr(host_start + 1, ']');
            if (close_bracket) {
                size_t ipv6_len = (size_t)(close_bracket - host_start - 1);
                if (ipv6_len < sizeof(host_buf) - 1) {
                    memcpy(host_buf, host_start + 1, ipv6_len);
                    host_buf[ipv6_len] = '\0';
                    host = host_buf;
                }
            }
        }
    }

    /* Fallback: use url_extract_hostname for non-IPv6 */
    if (!host) {
        char *h = url_extract_hostname(url_buf);
        if (!h || !*h) return false;
        snprintf(host_buf, sizeof(host_buf), "%s", h);
        host = host_buf;
        free(h);
    }

    if (!host || !*host) return false;

    bool result = false;

    /* Check known local hosts */
    static const char *local_hosts[] = {
        "localhost", "127.0.0.1", "::1", "0.0.0.0", "127.0.1.1", NULL
    };
    for (int i = 0; local_hosts[i]; i++) {
        if (strcasecmp(host, local_hosts[i]) == 0) {
            result = true;
            goto done;
        }
    }

    /* Check container DNS suffixes */
    static const char *container_suffixes[] = {
        "host.docker.internal", "host.podman.internal",
        "host.lima.internal", ".host.docker.internal",
        ".host.podman.internal", ".host.lima.internal", NULL
    };
    size_t hlen = strlen(host);
    for (int i = 0; container_suffixes[i]; i++) {
        size_t slen = strlen(container_suffixes[i]);
        if (hlen >= slen && strcasecmp(host + hlen - slen, container_suffixes[i]) == 0) {
            result = true;
            goto done;
        }
    }

    /* Check IP ranges */
    struct in_addr addr4;
    if (inet_pton(AF_INET, host, &addr4) == 1) {
        unsigned long ip = ntohl(addr4.s_addr);
        /* 127.0.0.0/8 — loopback */
        if ((ip & 0xFF000000) == 0x7F000000) { result = true; goto done; }
        /* 10.0.0.0/8 — RFC-1918 */
        if ((ip & 0xFF000000) == 0x0A000000) { result = true; goto done; }
        /* 172.16.0.0/12 — RFC-1918 */
        if ((ip & 0xFFF00000) == 0xAC100000) { result = true; goto done; }
        /* 192.168.0.0/16 — RFC-1918 */
        if ((ip & 0xFFFF0000) == 0xC0A80000) { result = true; goto done; }
        /* 169.254.0.0/16 — link-local */
        if ((ip & 0xFFFF0000) == 0xA9FE0000) { result = true; goto done; }
        /* 100.64.0.0/10 — Tailscale CGNAT (RFC 6598) */
        if ((ip & 0xFFC00000) == 0x64400000) { result = true; goto done; }
        /* 0.0.0.0/8 — current network */
        if ((ip & 0xFF000000) == 0x00000000) { result = true; goto done; }
    }

    /* Check IPv6 loopback and private */
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, host, &addr6) == 1) {
        static const unsigned char loopback[16] =
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
        static const unsigned char ipv4_mapped_prefix[12] =
            {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};
        if (memcmp(&addr6, loopback, 16) == 0) { result = true; goto done; }
        /* IPv4-mapped IPv6 — extract embedded IPv4 and recheck */
        if (memcmp(&addr6, ipv4_mapped_prefix, 12) == 0) {
            struct in_addr embedded;
            memcpy(&embedded, ((const unsigned char *)&addr6) + 12, 4);
            unsigned long ip = ntohl(embedded.s_addr);
            if ((ip & 0xFF000000) == 0x7F000000) { result = true; goto done; }
            if ((ip & 0xFF000000) == 0x0A000000) { result = true; goto done; }
            if ((ip & 0xFFF00000) == 0xAC100000) { result = true; goto done; }
            if ((ip & 0xFFFF0000) == 0xC0A80000) { result = true; goto done; }
            if ((ip & 0xFFFF0000) == 0xA9FE0000) { result = true; goto done; }
            if ((ip & 0xFFC00000) == 0x64400000) { result = true; goto done; }
        }
        /* Unique local address (fc00::/7) */
        if (((const unsigned char *)&addr6)[0] & 0x02) { /* fec0::/10 not fc00::/7; check fd00::/8 */
            if (((const unsigned char *)&addr6)[0] == 0xfd) { result = true; goto done; }
        }
        /* Link-local (fe80::/10) */
        if (((const unsigned char *)&addr6)[0] == 0xfe &&
            (((const unsigned char *)&addr6)[1] & 0xc0) == 0x80) { result = true; goto done; }
    }

done:
    return result;
}

/* Infer provider name from a base URL by matching against known provider hosts.
 * Port of Python model_metadata._infer_provider_from_url().
 * Returns malloc'd provider name string, or NULL if unknown. Caller must free(). */
char *provider_infer_from_url(const char *base_url) {
    if (!base_url || !*base_url) return NULL;

    /* Normalize */
    char *normalized = provider_normalize_base_url(base_url);
    if (!normalized) return NULL;

    /* Add scheme if missing */
    char url_buf[1024];
    if (strstr(normalized, "://")) {
        snprintf(url_buf, sizeof(url_buf), "%s", normalized);
    } else {
        snprintf(url_buf, sizeof(url_buf), "https://%s", normalized);
    }
    free(normalized);

    /* Extract hostname from the URL */
    char *host = url_extract_hostname(url_buf);
    if (!host || !*host) {
        free(host);
        return NULL;
    }

    /* Lowercase the hostname for comparison */
    for (char *p = host; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* Check each known provider's base_url hostname against our URL */
    char *result = NULL;
    for (int i = 0; PROVIDERS[i].provider_name; i++) {
        const char *prov_base_url = PROVIDERS[i].base_url;
        if (!prov_base_url || !*prov_base_url) continue;

        /* Extract hostname from provider's base_url */
        char *prov_host = url_extract_hostname(prov_base_url);
        if (!prov_host || !*prov_host) {
            free(prov_host);
            continue;
        }

        /* Lowercase the provider hostname */
        for (char *p = prov_host; *p; p++) *p = (char)tolower((unsigned char)*p);

        /* Check if provider hostname appears in our URL's host */
        if (strstr(host, prov_host) || strstr(prov_host, host)) {
            result = strdup(PROVIDERS[i].provider_name);
            free(prov_host);
            break;
        }
        free(prov_host);
    }

    /* Check OpenAI-compat aliases that may not be in PROVIDERS table */
    if (!result) {
        static const struct { const char *host_part; const char *provider; } aliases[] = {
            {"api.fireworks.ai", "fireworks"},
            {"opencode.ai", "opencode-go"},
            {"api.arcee.ai", "arcee"},
            {"api.minimax", "minimax"},
            {"xiaomimimo.com", "xiaomi"},
            {"api.gmi-serving.com", "gmi"},
            {"api.novita.ai", "novita"},
            {"tokenhub.tencentmaas.com", "tencent-tokenhub"},
            {"api.anthropic.com", "anthropic"},
            {"api.deepseek.com", "deepseek"},
            {"generativelanguage.googleapis.com", "gemini"},
            {"inference-api.nousresearch.com", "nous"},
            {"chatgpt.com", "openai"},
            {"localhost", "local"},
            {"127.0.0.1", "local"},
            {NULL, NULL}
        };
        for (int i = 0; aliases[i].host_part; i++) {
            if (strstr(host, aliases[i].host_part)) {
                result = strdup(aliases[i].provider);
                break;
            }
        }
    }

    free(host);
    return result;
}

/* Parse a context length limit from an API error message.
 * Port of Python model_metadata.parse_context_limit_from_error().
 * Extracts numbers near context-related keywords. Returns limit,
 * or -1 if not found / not parseable. */
int provider_parse_context_limit_from_error(const char *error_msg) {
    if (!error_msg || !*error_msg) return -1;

    /* Work on a lowercase copy */
    char buf[2048];
    size_t len = strlen(error_msg);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)error_msg[i]);
    buf[len] = '\0';

    /* Pattern 1: "maximum context length is N" or "max context size is N" */
    const char *p = strstr(buf, "maximum");
    if (!p) p = strstr(buf, "max");
    if (p) {
        /* Scan around this area for a 4+ digit number */
        const char *scan_start = p > buf ? p - 20 : buf;
        const char *scan_end = p + 40;
        if (scan_end > buf + len) scan_end = buf + len;
        if (scan_start < buf) scan_start = buf;

        /* Look for number near context/length keywords */
        const char *keywords[] = {"context", "length", "size", "window", "limit", "token", NULL};
        for (int ki = 0; keywords[ki]; ki++) {
            const char *kw = strstr(scan_start, keywords[ki]);
            if (kw && kw < scan_end) {
                /* Search for digits before and after the keyword */
                const char *num_search_start = (kw - scan_start) > 20 ? kw - 20 : scan_start;
                const char *num_search_end = kw + 20;
                if (num_search_end > scan_end) num_search_end = scan_end;

                for (const char *cp = num_search_start; cp < num_search_end; cp++) {
                    if (cp >= buf + len) break;
                    if (cp >= num_search_end) break;
                    if (cp < scan_start) continue;
                    if (*cp >= '0' && *cp <= '9') {
                        long val = strtol(cp, NULL, 10);
                        if (val >= 1024 && val <= 10000000)
                            return (int)val;
                        /* Skip this number */
                        while (cp < num_search_end && *cp >= '0' && *cp <= '9') cp++;
                    }
                }
            }
        }
    }

    /* Pattern 2: "context_length_exceeded: NNNNN" */
    const char *cl = strstr(buf, "context_length_exceeded");
    if (cl) {
        const char *num = cl + 24; /* skip past "context_length_exceeded" */
        while (*num && (*num == ':' || *num == ' ' || *num == ',')) num++;
        if (*num >= '0' && *num <= '9') {
            long val = strtol(num, NULL, 10);
            if (val >= 1024 && val <= 10000000) return (int)val;
        }
    }

    /* Pattern 3: "context length is N" or "context size N" */
    p = strstr(buf, "context length");
    if (!p) p = strstr(buf, "context size");
    if (!p) p = strstr(buf, "context window");
    if (p) {
        /* Look for a number within 30 chars after the keyword */
        const char *end = p + 30;
        if (end > buf + len) end = buf + len;
        for (const char *cp = p; cp < end; cp++) {
            if (*cp >= '0' && *cp <= '9') {
                long val = strtol(cp, NULL, 10);
                if (val >= 1024 && val <= 10000000) return (int)val;
                while (cp < end && *cp >= '0' && *cp <= '9') cp++;
            }
        }
    }

    return -1;
}

/* Parse available output tokens from a max_tokens-too-large error message.
 * Port of Python model_metadata.parse_available_output_tokens_from_error().
 * Returns available tokens, or -1 if not a max_tokens-too-large error. */
int provider_parse_available_output_tokens_from_error(const char *error_msg) {
    if (!error_msg || !*error_msg) return -1;

    /* Work on a lowercase copy */
    char buf[2048];
    size_t len = strlen(error_msg);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)error_msg[i]);
    buf[len] = '\0';

    /* Must contain "max_tokens" and "available_(output_)?tokens" */
    if (!strstr(buf, "max_tokens")) return -1;
    if (!strstr(buf, "available_tokens") && !strstr(buf, "available tokens")
        && !strstr(buf, "available_output_tokens")) return -1;

    /* Extract the available tokens figure */
    const char *at = strstr(buf, "available_output_tokens");
    if (!at) at = strstr(buf, "available_tokens");
    if (!at) at = strstr(buf, "available tokens");
    if (at) {
        const char *num = at;
        /* Skip past the keyword */
        while (*num && *num != ':' && *num != '=' && *num != ' ') num++;
        while (*num && (*num == ':' || *num == ' ' || *num == '=')) num++;
        if (*num >= '0' && *num <= '9') {
            long val = strtol(num, NULL, 10);
            if (val >= 1) return (int)val;
        }
    }

    /* Fallback: look for "= N" at the end of the error */
    const char *eq = strrchr(buf, '=');
    if (eq) {
        const char *num = eq + 1;
        while (*num && *num == ' ') num++;
        if (*num >= '0' && *num <= '9') {
            long val = strtol(num, NULL, 10);
            if (val >= 1) return (int)val;
        }
    }

    return -1;
}

/* ---- provider_model_id_matches ---- */
bool provider_model_id_matches(const char *candidate_id, const char *lookup_model) {
    if (!candidate_id || !lookup_model) return false;

    /* Exact match */
    if (strcmp(candidate_id, lookup_model) == 0) return true;

    /* Slug match: part after last '/' equals lookup_model */
    const char *slash = strrchr(candidate_id, '/');
    if (slash) {
        const char *slug = slash + 1;
        if (strcmp(slug, lookup_model) == 0) return true;
    }

    return false;
}

/* ---- provider_model_suggests_kimi ---- */
bool provider_model_suggests_kimi(const char *model) {
    if (!model) return false;

    /* Case-insensitive check */
    size_t len = strlen(model);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';

    /* Check for 'kimi' prefix or 'moonshot' substring */
    bool result = (strncmp(lower, "kimi", 4) == 0) || (strstr(lower, "moonshot") != NULL);

    free(lower);
    return result;
}

/* ---- provider_normalize_model_version ---- */
char *provider_normalize_model_version(const char *model) {
    if (!model) return NULL;

    size_t len = strlen(model);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    for (size_t i = 0; i < len; i++) {
        result[i] = (model[i] == '.') ? '-' : model[i];
    }
    result[len] = '\0';

    return result;
}

/* ---- model_grok_supports_reasoning_effort ---- */
/* Port of Python model_metadata.grok_supports_reasoning_effort().
 * Checks if model name starts with a known Grok-effort-capable prefix. */
bool model_grok_supports_reasoning_effort(const char *model) {
    if (!model) return false;
    size_t len = strlen(model);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++)
        lower[i] = tolower((unsigned char)model[i]);
    lower[len] = '\0';
    /* Strip aggregator prefix after last '/' */
    const char *name = lower;
    const char *slash = strrchr(lower, '/');
    if (slash) name = slash + 1;
    /* Check against known capable prefixes */
    const char *prefixes[] = {"grok-3-mini", "grok-4.20-multi-agent", "grok-4.3", NULL};
    bool result = false;
    for (int i = 0; prefixes[i]; i++) {
        if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0) {
            result = true;
            break;
        }
    }
    free(lower);
    return result;
}

/* ---- provider_is_openrouter_base_url ---- */
/* Port of Python model_metadata._is_openrouter_base_url().
 * Checks if URL contains "openrouter.ai". */
bool provider_is_openrouter_base_url(const char *base_url) {
    return base_url && strstr(base_url, "openrouter.ai") != NULL;
}

/* ---- provider_is_custom_endpoint ---- */
/* Port of Python model_metadata._is_custom_endpoint().
 * URL is valid (non-empty) and not an OpenRouter endpoint. */
bool provider_is_custom_endpoint(const char *base_url) {
    return base_url && *base_url && !provider_is_openrouter_base_url(base_url);
}

/* ---- provider_is_known_base_url ---- */
/* Port of Python model_metadata._is_known_provider_base_url().
 * Wraps provider_infer_from_url(). */
bool provider_is_known_base_url(const char *base_url) {
    char *provider = provider_infer_from_url(base_url);
    if (provider) {
        free(provider);
        return true;
    }
    return false;
}

/* ---- provider_auth_headers ---- */
/* Port of Python model_metadata._auth_headers().
 * Returns json_t dict {Authorization: Bearer <key>} or NULL if key empty. */
json_t *provider_auth_headers(const char *api_key) {
    if (!api_key) return NULL;
    while (*api_key == ' ' || *api_key == '\t') api_key++;
    if (!*api_key) return NULL;
    json_t *obj = json_object();
    if (!obj) return NULL;
    size_t klen = strlen(api_key);
    char *bearer = malloc(klen + 8);
    if (!bearer) { json_free(obj); return NULL; }
    memcpy(bearer, "Bearer ", 7);
    memcpy(bearer + 7, api_key, klen + 1);
    json_set(obj, "Authorization", json_string(bearer));
    free(bearer);
    return obj;
}

/* ---- provider_coerce_reasonable_int ---- */
/* Port of Python model_metadata._coerce_reasonable_int().
 * Converts string to int, checks range [minimum, maximum].
 * Returns -1 on failure (overflow, non-numeric, out of range). */
int provider_coerce_reasonable_int(const char *value, int minimum, int maximum) {
    if (!value) return -1;
    while (*value == ' ' || *value == '\t') value++;
    if (!*value) return -1;
    size_t len = strlen(value);
    char *buf = malloc(len + 1);
    if (!buf) return -1;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (value[i] != ',') buf[j++] = value[i];
    }
    buf[j] = '\0';
    char *endptr = NULL;
    long val = strtol(buf, &endptr, 10);
    /* Save whether endptr consumed all non-whitespace BEFORE freeing buf */
    bool endptr_ok = (endptr && (*endptr == '\0' || *endptr == ' ' || *endptr == '\t'));
    free(buf);
    if (!endptr_ok) return -1;
    if (val < (long)minimum || val > (long)maximum) return -1;
    return (int)val;
}

/* ---- estimate_tokens_rough ---- */
/* Port of Python model_metadata.estimate_tokens_rough().
 * Rough token estimate using ceiling division: (len + 3) / 4. */
int estimate_tokens_rough(const char *text) {
    if (!text) return 0;
    size_t len = strlen(text);
    return (int)((len + 3) / 4);
}

/* ---- provider_resolve_requests_verify ---- */
/* Port of Python model_metadata._resolve_requests_verify().
 * Returns 1 for verify, 0 for skip verify, -1 for custom CA path. */
int provider_resolve_requests_verify(void) {
    const char *value = getenv("HERMES_VERIFY_SSL");
    if (!value || !*value) return 1; /* Default: verify */
    /* Lowercase the value for comparison */
    size_t len = strlen(value);
    char *lower = malloc(len + 1);
    if (!lower) return 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = tolower((unsigned char)value[i]);
    lower[len] = '\0';
    int result;
    if (strcmp(lower, "0") == 0 || strcmp(lower, "false") == 0 ||
        strcmp(lower, "no") == 0 || strcmp(lower, "off") == 0) {
        result = 0;
    } else if (strcmp(lower, "1") == 0 || strcmp(lower, "true") == 0 ||
               strcmp(lower, "yes") == 0 || strcmp(lower, "on") == 0) {
        result = 1;
    } else {
        result = -1; /* Custom CA bundle path */
    }
    free(lower);
    return result;
}

/* ---- provider_requests_verify_path ---- */
/* Return the custom CA bundle path from HERMES_VERIFY_SSL, or NULL if
 * the env var is a boolean keyword or not set. */
const char *provider_requests_verify_path(void) {
    if (provider_resolve_requests_verify() != -1) return NULL;
    return getenv("HERMES_VERIFY_SSL");
}

/* ---- provider_extract_context_length ---- */
/* Port of Python model_metadata._extract_context_length().
 * Iterates nested JSON objects looking for known context-length keys.
 * Uses provider_coerce_reasonable_int for value validation. */
int provider_extract_context_length(const json_t *payload) {
    if (!payload) return -1;
    const char *keys[] = {
        "context_length", "context_window", "context_size",
        "max_context_length", "max_position_embeddings", "max_model_len",
        "max_input_tokens", "max_sequence_length", "max_seq_len",
        "n_ctx_train", "n_ctx", "ctx_size", NULL
    };
    /* Walk the JSON tree looking for matching keys */
    /* Simple flat + one-level deep scan */
    if (payload->type == JSON_OBJECT) {
        for (size_t i = 0; i < payload->c.count; i++) {
            const char *key = payload->c.keys ? payload->c.keys[i] : "";
            if (!key) continue;
            size_t klen = strlen(key);
            char *klower = malloc(klen + 1);
            if (!klower) continue;
            for (size_t j = 0; j < klen; j++)
                klower[j] = tolower((unsigned char)key[j]);
            klower[klen] = '\0';
            for (int k = 0; keys[k]; k++) {
                if (strcmp(klower, keys[k]) == 0) {
                    json_t *val = payload->c.items[i];
                    if (val->type == JSON_NUMBER) {
                        int n = (int)val->num_val;
                        free(klower);
                        if (n >= 1024 && n <= 10000000) return n;
                        goto next_key_outer;
                    }
                    if (val->type == JSON_STRING) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s", val->str_val);
                        free(klower);
                        int n = provider_coerce_reasonable_int(buf, 1024, 10000000);
                        if (n >= 1024) return n;
                        goto next_key_outer;
                    }
                    free(klower);
                    goto next_key_outer;
                }
            }
            /* Recursively check nested objects */
            if (payload->c.items[i]->type == JSON_OBJECT) {
                int n = provider_extract_context_length(payload->c.items[i]);
                free(klower);
                if (n >= 0) return n;
            } else {
                free(klower);
            }
            next_key_outer:;
        }
    }
    return -1;
}

/* ---- provider_extract_max_completion_tokens ---- */
/* Port of Python model_metadata._extract_max_completion_tokens().
 * Iterates nested JSON objects looking for max-completion-token keys. */
int provider_extract_max_completion_tokens(const json_t *payload) {
    if (!payload) return -1;
    const char *keys[] = {
        "max_completion_tokens", "max_output_tokens", "max_tokens", NULL
    };
    if (payload->type == JSON_OBJECT) {
        for (size_t i = 0; i < payload->c.count; i++) {
            const char *key = payload->c.keys ? payload->c.keys[i] : "";
            if (!key) continue;
            size_t klen = strlen(key);
            char *klower = malloc(klen + 1);
            if (!klower) continue;
            for (size_t j = 0; j < klen; j++)
                klower[j] = tolower((unsigned char)key[j]);
            klower[klen] = '\0';
            for (int k = 0; keys[k]; k++) {
                if (strcmp(klower, keys[k]) == 0) {
                    json_t *val = payload->c.items[i];
                    if (val->type == JSON_NUMBER) {
                        int n = (int)val->num_val;
                        free(klower);
                        if (n >= 1024 && n <= 10000000) return n;
                        goto next_key_mct;
                    }
                    if (val->type == JSON_STRING) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s", val->str_val);
                        free(klower);
                        int n = provider_coerce_reasonable_int(buf, 1024, 10000000);
                        if (n >= 1024) return n;
                        goto next_key_mct;
                    }
                    free(klower);
                    goto next_key_mct;
                }
            }
            if (payload->c.items[i]->type == JSON_OBJECT) {
                int n = provider_extract_max_completion_tokens(payload->c.items[i]);
                free(klower);
                if (n >= 0) return n;
            } else {
                free(klower);
            }
            next_key_mct:;
        }
    }
    return -1;
}

/* ---- provider_extract_pricing ---- */
/* Port of Python model_metadata._extract_pricing().
 * Extracts pricing from model metadata payload.
 * First checks for novita-specific keys, then iterates nested dicts
 * using alias maps for prompt/completion/request/cache_read/cache_write. */
json_t *provider_extract_pricing(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;

    /* First check for novita-specific pricing keys */
    json_t *novita_input = json_obj_get(payload, "input_token_price_per_m");
    json_t *novita_output = json_obj_get(payload, "output_token_price_per_m");
    if (novita_input || novita_output) {
        json_t *pricing = json_object();
        if (novita_input && novita_input->type == JSON_NUMBER) {
            /* Convert: input_token_price_per_m / 10_000 / 1_000_000 */
            double val = novita_input->num_val / 10000000000.0;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.12g", val);
            json_set(pricing, "prompt", json_string(buf));
        }
        if (novita_output && novita_output->type == JSON_NUMBER) {
            double val = novita_output->num_val / 10000000000.0;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.12g", val);
            json_set(pricing, "completion", json_string(buf));
        }
        return pricing;
    }

    /* Alias maps for pricing keys */
    typedef struct {
        const char *target;
        const char *aliases[6];
    } pricing_alias_group_t;

    pricing_alias_group_t groups[] = {
        {"prompt",     {"prompt", "input", "input_cost_per_token", "prompt_token_cost", NULL}},
        {"completion", {"completion", "output", "output_cost_per_token", "completion_token_cost", NULL}},
        {"request",    {"request", "request_cost", NULL}},
        {"cache_read", {"cache_read", "cached_prompt", "input_cache_read", "cache_read_cost_per_token", NULL}},
        {"cache_write",{"cache_write", "cache_creation", "input_cache_write", "cache_write_cost_per_token", NULL}},
    };
    int num_groups = sizeof(groups) / sizeof(groups[0]);

    /* Iterate over nested dicts in the payload recursively */
    /* We use a simple 2-level recursion: payload and any immediate child dicts */
    const json_t *dicts[32];
    int num_dicts = 0;
    dicts[num_dicts++] = payload;

    /* Add first-level child dicts */
    for (size_t i = 0; i < payload->c.count && num_dicts < 32; i++) {
        if (payload->c.items[i]->type == JSON_OBJECT)
            dicts[num_dicts++] = payload->c.items[i];
    }

    for (int d = 0; d < num_dicts; d++) {
        const json_t *mapping = dicts[d];

        /* Quick check: does this dict have ANY pricing-related keys? */
        bool has_pricing_key = false;
        for (size_t i = 0; i < mapping->c.count && !has_pricing_key; i++) {
            const char *key = mapping->c.keys ? mapping->c.keys[i] : "";
            if (!key) continue;
            size_t klen = strlen(key);
            char *klower = malloc(klen + 1);
            if (!klower) continue;
            for (size_t j = 0; j < klen; j++)
                klower[j] = tolower((unsigned char)key[j]);
            klower[klen] = '\0';
            for (int g = 0; g < num_groups && !has_pricing_key; g++) {
                for (int a = 0; groups[g].aliases[a]; a++) {
                    if (strcmp(klower, groups[g].aliases[a]) == 0) {
                        has_pricing_key = true;
                        break;
                    }
                }
            }
            free(klower);
        }

        if (!has_pricing_key) continue;

        /* Build pricing dict from this mapping */
        json_t *pricing = json_object();
        int found = 0;

        for (int g = 0; g < num_groups; g++) {
            for (int a = 0; groups[g].aliases[a]; a++) {
                for (size_t i = 0; i < mapping->c.count; i++) {
                    const char *key = mapping->c.keys ? mapping->c.keys[i] : "";
                    if (!key) continue;
                    size_t klen = strlen(key);
                    char *klower = malloc(klen + 1);
                    if (!klower) continue;
                    for (size_t j = 0; j < klen; j++)
                        klower[j] = tolower((unsigned char)key[j]);
                    klower[klen] = '\0';
                    bool match = (strcmp(klower, groups[g].aliases[a]) == 0);
                    free(klower);
                    if (!match) continue;

                    json_t *val = mapping->c.items[i];
                    if (val->type == JSON_NULL) continue;
                    if (val->type == JSON_STRING && (!val->str_val || !*val->str_val))
                        continue;
                    if (val->type == JSON_NUMBER) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%.12g", val->num_val);
                        json_set(pricing, groups[g].target, json_string(buf));
                    } else if (val->type == JSON_STRING) {
                        json_set(pricing, groups[g].target, json_string(val->str_val));
                    } else {
                        /* Other types: skip */
                        continue;
                    }
                    found++;
                    break;
                }
            }
        }

        if (found > 0) return pricing;
        json_free(pricing);
    }

    return NULL;
}
