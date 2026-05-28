/*
 * test_provider_smoke.c — Quick smoke test for provider registration (P71).
 *
 * Verifies all built-in providers register correctly and basic
 * provider_create/create pattern works for each type.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin -I lib/libhttp \
 *       test_provider_smoke.c \
 *       src/agent/provider.c \
 *       src/agent/provider_openai.c src/agent/provider_openrouter.c \
 *       src/agent/provider_deepseek.c src/agent/provider_xai.c \
 *       src/agent/provider_anthropic.c src/agent/provider_google.c \
 *       src/agent/provider_azure.c src/agent/provider_bedrock.c \
 *       src/agent/provider_custom.c \
 *       lib/libjson/json.c lib/libhttp/http.c \
 *       -o /tmp/test_provsmoke -lm -lssl -lcrypto
 *
 * Run:
 *   /tmp/test_provsmoke
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "provider.h"

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        printf("  \xe2\x9c\x93 %s\n", name); \
        passed++; \
    } else { \
        printf("  \xe2\x9c\x97 %s (FAILED)\n", name); \
        failed++; \
    } \
} while(0)

int main(void) {
    printf("=== Provider Registration Smoke Test (P71) ===\n\n");

    /* Register all built-in providers */
    provider_register_builtins();

    /* Test each provider creates without crash */
    /* 9 registered providers + 17 OpenAI-compat aliases = 26 total */
    const char *providers[] = {
        "openai", "anthropic", "google", "openrouter",
        "deepseek", "xai", "azure", "bedrock", "custom",
        /* G35-G40: OpenAI-compat aliases */
        "nous", "alibaba", "stepfun", "minimax", "novita", "zai",
        /* G41-G51: More OpenAI-compat aliases */
        "huggingface", "arcee", "ollama_cloud", "nvidia",
        "gmi", "kilocode", "kimi", "ai_gateway",
        "azure_foundry", "xiaomi", "qwen_oauth",
        /* Well-known aliases */
        "groq", "together", "claude", "gemini", "aws",
        NULL
    };

    printf("[P71] Provider create/free smoke test:\n");
    for (int i = 0; providers[i]; i++) {
        provider_t *p = provider_create(providers[i], "test-model", "sk-test-key", "https://test.example.com");
        TEST(providers[i], p != NULL);
        TEST("  name matches", p && strcmp(p->name, providers[i]) == 0);
        TEST("  ops non-null", p && p->ops != NULL);
        TEST("  build_url non-null", p && p->ops->build_url != NULL);
        TEST("  build_headers non-null", p && p->ops->build_headers != NULL);
        TEST("  build_request_body non-null", p && p->ops->build_request_body != NULL);
        TEST("  parse_response non-null", p && p->ops->parse_response != NULL);
        TEST("  parse_stream_chunk non-null", p && p->ops->parse_stream_chunk != NULL);
        TEST("  free_response non-null", p && p->ops->free_response != NULL);
        provider_free(p);
    }

    /* Test default provider (NULL name → openai) */
    printf("\n[P71] Default/edge case:\n");
    provider_t *def = provider_create(NULL, "gpt-4o", NULL, NULL);
    TEST("default provider (NULL name) creates", def != NULL);
    TEST("default name is openai", def && strcmp(def->name, "openai") == 0);
    TEST("default model set", def && strcmp(def->model, "gpt-4o") == 0);
    provider_free(def);

    /* Test credential pool attachment */
    printf("\n[P71] Credential pool integration:\n");
    provider_t *p = provider_create("openai", "gpt-4o", "sk-key", NULL);
    TEST("provider creates for pool test", p != NULL);
    /* NULL pool is valid — just detached */
    provider_set_credential_pool(p, NULL);
    TEST("NULL pool set/get", provider_get_credential_pool(p) == NULL);
    provider_free(p);

    /* Test URL building for each provider */
    printf("\n[P71] URL building smoke test:\n");
    for (int i = 0; providers[i]; i++) {
        provider_t *prov = provider_create(providers[i], "test-model", NULL, NULL);
        if (prov && prov->ops && prov->ops->build_url) {
            char *url = prov->ops->build_url(prov, NULL);
            TEST(providers[i], url != NULL);
            free(url);
        }
        provider_free(prov);
    }

    /* Test header building for each provider */
    printf("\n[P71] Header building smoke test:\n");
    for (int i = 0; providers[i]; i++) {
        provider_t *prov = provider_create(providers[i], "test-model", "sk-test-key", NULL);
        if (prov && prov->ops && prov->ops->build_headers) {
            char *headers = prov->ops->build_headers(prov, "sk-test-key");
            TEST(providers[i], headers != NULL);
            TEST("  contains Content-Type", headers && strstr(headers, "application/json") != NULL);
            free(headers);
        }
        provider_free(prov);
    }

    /* Test response parsing for error responses (should not crash) */
    printf("\n[P71] Error response resilience:\n");
    for (int i = 0; providers[i]; i++) {
        provider_t *prov = provider_create(providers[i], "test-model", NULL, NULL);
        if (prov && prov->ops && prov->ops->parse_response) {
            provider_response_t *resp = prov->ops->parse_response(prov,
                "{\"error\":{\"message\":\"Invalid API key\",\"type\":\"auth_error\"}}");
            TEST(providers[i], resp != NULL);
            TEST("  no crash", 1);
            prov->ops->free_response(resp);
        }
        provider_free(prov);
    }

    /* Summary */
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
