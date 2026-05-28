/*
 * test_openrouter_depth.c — B43-B46: OpenRouter provider depth tests
 *
 * Tests: provider preferences JSON in request body (order, allow_fallbacks,
 * data_control, ignore).
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

static const message_t msg = {.role = MSG_USER, .content = (char*)"hello"};
static const message_t *msgs[] = {&msg};

int main(void) {
    printf("=== B43-B46: OpenRouter provider depth ===\n\n");

    /* B43-B46: empty provider field — omitted from body */
    {
        provider_t p = {0};
        p.config.openrouter_provider[0] = '\0';
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("empty provider field omitted", body && !strstr(body, "\"provider\""));
        free(body);
    }

    /* B43: provider.preferences — order list */
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"order\":[\"Anthropic\",\"OpenAI\"],\"allow_fallbacks\":true}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("provider order present", body && strstr(body, "\"order\""));
        TEST("provider Anthropic in order", body && strstr(body, "Anthropic"));
        TEST("provider OpenAI in order", body && strstr(body, "OpenAI"));
        TEST("allow_fallbacks true", body && strstr(body, "allow_fallbacks"));
        free(body);
    }

    /* B46: data_controls */
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"data_control\":\"allow\"}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("data_control allow", body && strstr(body, "\"data_control\":\"allow\""));
        free(body);
    }

    /* B44: route fixed */
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"route\":\"fixed\"}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("route fixed", body && strstr(body, "\"route\":\"fixed\""));
        free(body);
    }

    /* B45: ignore list */
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"ignore\":[\"Amazon Bedrock\",\"Mancer\"]}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("ignore list", body && strstr(body, "\"ignore\""));
        TEST("ignore Bedrock", body && strstr(body, "Amazon Bedrock"));
        TEST("ignore Mancer", body && strstr(body, "Mancer"));
        free(body);
    }

    /* non-JSON provider value silently dropped */
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "not-json");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs, 1, NULL, false);
        TEST("non-JSON provider dropped", body && !strstr(body, "not-json"));
        free(body);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
