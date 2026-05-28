/*
 * test_azure_depth.c — B37/B38: Azure provider depth tests
 *
 * Tests: deployment_id routing, api_version override in URL builder.
 */
#include "hermes.h"
#include "hermes_json.h"
#include "provider.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== B37/B38: Azure provider depth ===\n\n");

    /* B37: default URL — uses model name as deployment, default API version */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o-mini");
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://my-resource.openai.azure.com");
        TEST("default url uses model as deploy", url && strstr(url, "/deployments/gpt-4o-mini/"));
        TEST("default url uses default api-version", url && strstr(url, "api-version=2024-10-01-preview"));
        free(url);
    }

    /* B37: custom deployment_id overrides model name */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o-mini");
        snprintf(p.config.azure_deployment_id, sizeof(p.config.azure_deployment_id), "my-gpt4-deploy");
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://my-resource.openai.azure.com");
        TEST("deployment_id overrides model", url && strstr(url, "/deployments/my-gpt4-deploy/"));
        TEST("deployment_id does not contain model", url && !strstr(url, "/deployments/gpt-4o-mini/"));
        free(url);
    }

    /* B38: custom api_version overrides default */
    {
        provider_t p = {0};
        snprintf(p.config.azure_api_version, sizeof(p.config.azure_api_version), "2025-01-01");
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://my-resource.openai.azure.com");
        TEST("api_version overrides default", url && strstr(url, "api-version=2025-01-01"));
        free(url);
    }

    /* B37+B38: both custom fields together */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o");
        snprintf(p.config.azure_deployment_id, sizeof(p.config.azure_deployment_id), "custom-deploy");
        snprintf(p.config.azure_api_version, sizeof(p.config.azure_api_version), "2024-12-01");
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://res.openai.azure.com");
        TEST("combined: custom deploy", url && strstr(url, "/deployments/custom-deploy/"));
        TEST("combined: custom api-version", url && strstr(url, "api-version=2024-12-01"));
        free(url);
    }

    /* B37: empty model uses default gpt-4o when no deployment_id */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://res.openai.azure.com");
        TEST("empty model falls back to gpt-4o", url && strstr(url, "/deployments/gpt-4o/"));
        free(url);
    }

    /* B38: empty api_version uses default */
    {
        provider_t p = {0};
        p.config.azure_api_version[0] = '\0';
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://res.openai.azure.com");
        TEST("empty api_version falls back to default", url && strstr(url, "api-version=2024-10-01-preview"));
        free(url);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
