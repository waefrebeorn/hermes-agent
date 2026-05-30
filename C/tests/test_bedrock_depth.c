/*
 * test_bedrock_depth.c — B39-B42: Bedrock provider depth tests
 *
 * Tests: build_url, build_request_body (inferenceProfile, guardrailConfig,
 * enableTrace, inferenceConfig defaults, system messages, tool config),
 * parse_response (normal, error, tool_use).
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

static const message_t user_msg = {.role = MSG_USER, .content = (char*)"hello"};
static const message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"system prompt"};
static const message_t *msgs1[] = {&user_msg};
static const message_t *msgs_with_sys[] = {&sys_msg, &user_msg};

int main(void) {
    printf("=== B39-B42: Bedrock provider depth ===\n\n");

    /* ── build_url edge cases ────────────────────────────────────── */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "anthropic.claude-3-sonnet-20240229-v1:0");
        char *url = PROVIDER_OPS_BEDROCK.build_url(&p, NULL);
        TEST("build_url contains model", url && strstr(url, "claude-3-sonnet") != NULL);
        TEST("build_url contains bedrock-runtime", url && strstr(url, "bedrock-runtime") != NULL);
        TEST("build_url ends with /converse", url && strstr(url, "/converse") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        /* Empty model should use default */
        char *url = PROVIDER_OPS_BEDROCK.build_url(&p, NULL);
        TEST("build_url empty model uses default", url && strstr(url, "/model/") != NULL);
        free(url);
    }

    /* ── build_url: region from env ──────────────────────────────── */
    {
        setenv("AWS_REGION", "eu-west-1", 1);
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "test-model");
        char *url = PROVIDER_OPS_BEDROCK.build_url(&p, NULL);
        TEST("build_url uses AWS_REGION env", url && strstr(url, "eu-west-1") != NULL);
        free(url);
        unsetenv("AWS_REGION");
    }

    /* B39: inferenceProfile in request body */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile),
                 "arn:aws:bedrock:us-east-1:123456:inference-profile/us.anthropic.claude-3-5-sonnet-20241022-v2:0");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("inferenceProfile present", body && strstr(body, "\"inferenceProfile\""));
        TEST("inferenceProfile has ARN", body && strstr(body, "us.anthropic.claude-3-5-sonnet"));
        free(body);
    }

    /* B39: empty inferenceProfile omitted */
    {
        provider_t p = {0};
        p.config.bedrock_inference_profile[0] = '\0';
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("empty inferenceProfile omitted", body && !strstr(body, "\"inferenceProfile\""));
        free(body);
    }

    /* B40: guardrailConfig JSON object */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"abc123\",\"guardrailVersion\":\"DRAFT\"}");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("guardrailConfig present", body && strstr(body, "\"guardrailConfig\""));
        TEST("guardrailIdentifier in body", body && strstr(body, "guardrailIdentifier"));
        TEST("guardrailVersion in body", body && strstr(body, "\"DRAFT\""));
        free(body);
    }

    /* B40: empty guardrailConfig omitted */
    {
        provider_t p = {0};
        p.config.bedrock_guardrail_config[0] = '\0';
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("empty guardrailConfig omitted", body && !strstr(body, "\"guardrailConfig\""));
        free(body);
    }

    /* B40: non-JSON guardrailConfig silently dropped */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config), "not-json");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("non-JSON guardrailConfig dropped", body && !strstr(body, "not-json"));
        free(body);
    }

    /* B41: enableTrace true when trace_enabled */
    {
        provider_t p = {0};
        p.config.bedrock_trace_enabled = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("enableTrace true", body && strstr(body, "\"enableTrace\":true"));
        free(body);
    }

    /* B41: enableTrace omitted when trace_enabled false */
    {
        provider_t p = {0};
        p.config.bedrock_trace_enabled = false;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("enableTrace omitted when false", body && !strstr(body, "\"enableTrace\""));
        free(body);
    }

    /* B42: inferenceConfig defaults (no config) */
    {
        provider_t p = {0};
        p.config.max_tokens = 0; /* should use 4096 default */
        p.config.temperature = -1.0f; /* should use 0.7 default */
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("inferenceConfig present", body && strstr(body, "\"inferenceConfig\""));
        TEST("maxTokens default 4096", body && strstr(body, "\"maxTokens\":4096"));
        TEST("temperature default 0.7", body && strstr(body, "\"temperature\":0.699") != NULL);
        free(body);
    }

    /* B42: inferenceConfig with custom values */
    {
        provider_t p = {0};
        p.config.max_tokens = 8192;
        p.config.temperature = 1.0f;
        p.config.top_p = 0.95f;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("maxTokens 8192", body && strstr(body, "\"maxTokens\":8192"));
        TEST("temperature 1.0", body && strstr(body, "\"temperature\":1"));
        TEST("topP 0.95", body && strstr(body, "\"topP\":0.949") != NULL);
        free(body);
    }

    /* B42: system message extraction */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs_with_sys, 2, NULL, false);
        TEST("system array present", body && strstr(body, "\"system\""));
        TEST("system text present", body && strstr(body, "system prompt"));
        TEST("messages has user role", body && strstr(body, "\"role\":\"user\""));
        TEST("messages has text content", body && strstr(body, "\"text\":\"hello\""));
        free(body);
    }

    /* B42: stop sequences in inferenceConfig */
    {
        provider_t p = {0};
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "STOP");
        snprintf(p.config.stop_sequences[1], sizeof(p.config.stop_sequences[1]), "END");
        p.config.stop_count = 2;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("stopSequences array present", body && strstr(body, "\"stopSequences\""));
        TEST("STOP in stopSequences", body && strstr(body, "\"STOP\""));
        TEST("END in stopSequences", body && strstr(body, "\"END\""));
        free(body);
    }

    /* B39+B40+B41+B42: all combined */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile), "us.my-profile");
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"xyz\"}");
        p.config.bedrock_trace_enabled = true;
        p.config.max_tokens = 16384;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("combined: inferenceProfile", body && strstr(body, "\"inferenceProfile\""));
        TEST("combined: guardrailConfig", body && strstr(body, "\"guardrailConfig\""));
        TEST("combined: enableTrace", body && strstr(body, "\"enableTrace\":true"));
        TEST("combined: maxTokens 16384", body && strstr(body, "\"maxTokens\":16384"));
        free(body);
    }

    /* ── parse_response edge cases ─────────────────────────────── */
    {
        /* Normal text response */
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"Hello world\"}]}},\"stopReason\":\"end_turn\",\"usage\":{\"inputTokens\":10,\"outputTokens\":20}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response content", r && r->content && strcmp(r->content, "Hello world") == 0);
        TEST("parse_response input_tokens", r && r->input_tokens == 10);
        TEST("parse_response output_tokens", r && r->output_tokens == 20);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Response with toolUse */
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"Let me search\",\"toolUse\":{\"toolUseId\":\"tu1\",\"name\":\"web_search\",\"input\":{\"query\":\"test\"}}}]}},\"stopReason\":\"tool_use\",\"usage\":{\"inputTokens\":5,\"outputTokens\":3}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response tool_use text", r && r->content && strcmp(r->content, "Let me search") == 0);
        TEST("parse_response tool_call count", r && r->tool_calls_count == 1);
        TEST("parse_response tool_call name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("parse_response tool_call id", r && strcmp(r->tool_calls[0].id, "tu1") == 0);
        TEST("parse_response tool_call args has query", r && strstr(r->tool_calls[0].arguments, "test") != NULL);
        TEST("parse_response finish_reason = tool_calls", r && strcmp(r->finish_reason, "tool_calls") == 0);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }
    /* stopReason = end_turn */
    {
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"Hello!\"}]}},\"stopReason\":\"end_turn\",\"usage\":{\"inputTokens\":5,\"outputTokens\":3}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response end_turn text", r && r->content && strcmp(r->content, "Hello!") == 0);
        TEST("parse_response finish_reason = stop", r && strcmp(r->finish_reason, "stop") == 0);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }
    /* stopReason = max_tokens */
    {
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"Partial response\"}]}},\"stopReason\":\"max_tokens\",\"usage\":{\"inputTokens\":5,\"outputTokens\":128}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response max_tokens text", r && r->content && strcmp(r->content, "Partial response") == 0);
        TEST("parse_response finish_reason = length", r && strcmp(r->finish_reason, "length") == 0);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }
    /* stopReason = content_filtered */
    {
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"\"}]}},\"stopReason\":\"content_filtered\",\"usage\":{\"inputTokens\":5,\"outputTokens\":1}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response content_filtered finish_reason = content_filter", r && strcmp(r->finish_reason, "content_filter") == 0);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }
    /* stopReason = guardrail_intervened */
    {
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[]}},\"stopReason\":\"guardrail_intervened\",\"usage\":{\"inputTokens\":5,\"outputTokens\":0}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response guardrail_intervened finish_reason = content_filter", r && strcmp(r->finish_reason, "content_filter") == 0);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }
    {
        /* Error response (no output key, message field) */
        const char *resp_json = "{\"message\":\"Access denied: insufficient permissions\"}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response error message", r && r->content && strstr(r->content, "Access denied") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Nested error object */
        const char *resp_json = "{\"error\":{\"message\":\"Model not available\",\"code\":\"ModelNotFound\"}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response nested error", r && r->content && strstr(r->content, "Model not available") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Multiple text blocks */
        const char *resp_json = "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[{\"text\":\"First\"},{\"text\":\"Second\"}]}},\"stopReason\":\"end_turn\"}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, resp_json);
        TEST("parse_response multi-block", r && r->content && strcmp(r->content, "FirstSecond") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* NULL body */
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, NULL);
        TEST("parse_response null body", r && r->content && strcmp(r->content, "empty response") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Empty body */
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(NULL, "");
        TEST("parse_response empty body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
