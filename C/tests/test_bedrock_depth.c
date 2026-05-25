/*
 * test_bedrock_depth.c — B39-B41: Bedrock provider depth tests
 *
 * Tests: inferenceProfile, guardrailConfig, enableTrace in request body.
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
    printf("=== B39-B41: Bedrock provider depth ===\n\n");

    /* B39: inferenceProfile in request body */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile),
                 "arn:aws:bedrock:us-east-1:123456:inference-profile/us.anthropic.claude-3-5-sonnet-20241022-v2:0");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("inferenceProfile present", body && strstr(body, "\"inferenceProfile\""));
        TEST("inferenceProfile has ARN", body && strstr(body, "us.anthropic.claude-3-5-sonnet"));
        free(body);
    }

    /* B39: empty inferenceProfile omitted */
    {
        provider_t p = {0};
        p.config.bedrock_inference_profile[0] = '\0';
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("empty inferenceProfile omitted", body && !strstr(body, "\"inferenceProfile\""));
        free(body);
    }

    /* B40: guardrailConfig JSON object */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"abc123\",\"guardrailVersion\":\"DRAFT\"}");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("guardrailConfig present", body && strstr(body, "\"guardrailConfig\""));
        TEST("guardrailIdentifier in body", body && strstr(body, "guardrailIdentifier"));
        TEST("guardrailVersion in body", body && strstr(body, "\"DRAFT\""));
        free(body);
    }

    /* B40: empty guardrailConfig omitted */
    {
        provider_t p = {0};
        p.config.bedrock_guardrail_config[0] = '\0';
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("empty guardrailConfig omitted", body && !strstr(body, "\"guardrailConfig\""));
        free(body);
    }

    /* B40: non-JSON guardrailConfig silently dropped */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config), "not-json");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("non-JSON guardrailConfig dropped", body && !strstr(body, "not-json"));
        free(body);
    }

    /* B41: enableTrace true when trace_enabled */
    {
        provider_t p = {0};
        p.config.bedrock_trace_enabled = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("enableTrace true", body && strstr(body, "\"enableTrace\":true"));
        free(body);
    }

    /* B41: enableTrace omitted when trace_enabled false */
    {
        provider_t p = {0};
        p.config.bedrock_trace_enabled = false;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("enableTrace omitted when false", body && !strstr(body, "\"enableTrace\""));
        free(body);
    }

    /* B39+B40+B41: all combined */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile), "us.my-profile");
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"xyz\"}");
        p.config.bedrock_trace_enabled = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("combined: inferenceProfile", body && strstr(body, "\"inferenceProfile\""));
        TEST("combined: guardrailConfig", body && strstr(body, "\"guardrailConfig\""));
        TEST("combined: enableTrace", body && strstr(body, "\"enableTrace\":true"));
        free(body);
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
