/*
 * provider.c — Provider registry and factory for Hermes C.
 * Manages provider implementations and instance creation.
 */

#include "hermes.h"
#include "provider.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Registry
 * ================================================================ */

#define MAX_PROVIDERS 32
static struct {
    provider_type_t type;
    const provider_ops_t *ops;
} g_provider_registry[MAX_PROVIDERS];
static int g_provider_count = 0;

void provider_register(provider_type_t type, const provider_ops_t *ops) {
    if (g_provider_count >= MAX_PROVIDERS) return;
    g_provider_registry[g_provider_count].type = type;
    g_provider_registry[g_provider_count].ops = ops;
    g_provider_count++;
}

void provider_register_builtins(void) {
    provider_register(PROVIDER_OPENAI, &PROVIDER_OPS_OPENAI);
    provider_register(PROVIDER_ANTHROPIC, &PROVIDER_OPS_ANTHROPIC);
    provider_register(PROVIDER_GOOGLE, &PROVIDER_OPS_GOOGLE);
    provider_register(PROVIDER_OPENROUTER, &PROVIDER_OPS_OPENROUTER);
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_register(PROVIDER_XAI, &PROVIDER_OPS_XAI);
    provider_register(PROVIDER_AZURE, &PROVIDER_OPS_AZURE);
    provider_register(PROVIDER_BEDROCK, &PROVIDER_OPS_BEDROCK);
    provider_register(PROVIDER_CUSTOM, &PROVIDER_OPS_CUSTOM);
}

/* Find provider ops by name (case-insensitive) */
static const provider_ops_t *find_provider_ops(const char *name) {
    if (!name) return &PROVIDER_OPS_OPENAI; /* Default */

    /* Check by type name in registry */
    for (int i = 0; i < g_provider_count; i++) {
        const char *reg_name = g_provider_registry[i].ops->name;
        if (strcasecmp(name, reg_name) == 0)
            return g_provider_registry[i].ops;
    }

    /* Check by well-known provider name */
    struct { const char *name; provider_type_t type; } providers[] = {
        {"openai",      PROVIDER_OPENAI},
        {"deepseek",    PROVIDER_DEEPSEEK},
        {"openrouter",  PROVIDER_OPENROUTER},
        {"groq",        PROVIDER_OPENAI},
        {"together",    PROVIDER_OPENAI},
        {"xai",         PROVIDER_XAI},
        {"anthropic",   PROVIDER_ANTHROPIC},
        {"claude",      PROVIDER_ANTHROPIC},
        {"google",      PROVIDER_GOOGLE},
        {"gemini",      PROVIDER_GOOGLE},
        {"azure",       PROVIDER_AZURE},
        {"bedrock",     PROVIDER_BEDROCK},
        {"aws",         PROVIDER_BEDROCK},
        {"custom",      PROVIDER_CUSTOM},
        /* OpenAI-compat providers (use base_url from config) */
        {"nous",        PROVIDER_OPENAI},
        {"alibaba",     PROVIDER_OPENAI},
        {"stepfun",     PROVIDER_OPENAI},
        {"minimax",     PROVIDER_OPENAI},
        {"novita",      PROVIDER_OPENAI},
        {"zai",         PROVIDER_OPENAI},
        /* More OpenAI-compat providers (G41-G51) */
        {"huggingface", PROVIDER_OPENAI},
        {"arcee",       PROVIDER_OPENAI},
        {"ollama_cloud",PROVIDER_OPENAI},
        {"nvidia",      PROVIDER_OPENAI},
        {"gmi",         PROVIDER_OPENAI},
        {"kilocode",    PROVIDER_OPENAI},
        {"kimi",        PROVIDER_OPENAI},
        {"ai_gateway",  PROVIDER_OPENAI},
        {"azure_foundry",PROVIDER_OPENAI},
        {"xiaomi",      PROVIDER_OPENAI},
        {"qwen_oauth",  PROVIDER_OPENAI},
        {NULL, 0}
    };
    for (int i = 0; providers[i].name; i++) {
        if (strcasecmp(name, providers[i].name) == 0) {
            for (int j = 0; j < g_provider_count; j++) {
                if (g_provider_registry[j].type == providers[i].type)
                    return g_provider_registry[j].ops;
            }
        }
    }

    /* Fallback to OpenAI */
    return &PROVIDER_OPS_OPENAI;
}

/* ================================================================
 *  Provider Instance
 * ================================================================ */

provider_t *provider_create(const char *provider_name,
                             const char *model,
                             const char *api_key,
                             const char *base_url) {
    const provider_ops_t *ops = find_provider_ops(provider_name);
    if (!ops) return NULL;

    provider_t *p = (provider_t *)calloc(1, sizeof(provider_t));
    if (!p) return NULL;

    p->ops = ops;
    p->type = PROVIDER_OPENAI; /* Will be resolved properly later */

    if (provider_name)
        snprintf(p->name, sizeof(p->name), "%s", provider_name);
    else
        snprintf(p->name, sizeof(p->name), "openai");

    if (model)
        snprintf(p->model, sizeof(p->model), "%s", model);
    if (api_key)
        snprintf(p->api_key, sizeof(p->api_key), "%s", api_key);
    if (base_url)
        snprintf(p->base_url, sizeof(p->base_url), "%s", base_url);

    return p;
}

void provider_free(provider_t *p) {
    if (!p) return;
    free(p->data);
    /* Note: pool is NOT owned — caller frees it */
    free(p);
}

void provider_set_credential_pool(provider_t *p, credential_pool_t *pool) {
    if (p) p->pool = pool;
}

credential_pool_t *provider_get_credential_pool(const provider_t *p) {
    return p ? p->pool : NULL;
}

/* B32: FIM dispatch — calls build_fim_url → HTTP POST → parse_fim_response */
provider_response_t *provider_fim(provider_t *p,
                                   const char *prompt,
                                   const char *suffix,
                                   int max_tokens) {
    if (!p || !p->ops || !prompt) return NULL;
    if (!p->ops->build_fim_body || !p->ops->parse_fim_response) return NULL;

    /* Build FIM URL */
    const char *base = p->base_url[0] ? p->base_url : "https://api.deepseek.com/v1";
    char *url = p->ops->build_fim_url
        ? p->ops->build_fim_url(p, base)
        : p->ops->build_url(p, base);
    if (!url) return NULL;

    /* Build headers */
    char *headers = p->ops->build_headers(p, p->api_key);
    if (!headers) { free(url); return NULL; }

    /* Build FIM body */
    char *body = p->ops->build_fim_body(p, prompt, suffix, max_tokens);
    if (!body) { free(url); free(headers); return NULL; }

    /* HTTP POST via libhttp */
    http_client_t *client = http_client_new(30);
    if (!client) { free(url); free(headers); free(body); return NULL; }

    http_response_t *http_resp = http_request(client, HTTP_POST, url, headers, body, strlen(body));
    free(url); free(headers); free(body);

    provider_response_t *resp = NULL;
    if (http_resp) {
        resp = p->ops->parse_fim_response(p, http_resp->body);
        http_response_free(http_resp);
    }
    http_client_free(client);
    return resp;
}

bool provider_has_fim(const provider_t *p) {
    return p && p->ops && p->ops->build_fim_body && p->ops->parse_fim_response;
}
