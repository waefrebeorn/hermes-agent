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
        printf("  PASS: %s\n", name); \
    } \
} while(0)

/* Forward declarations for R02 utility test functions */
static void test_is_anthropic_model(void);
static void test_supports_tool_use(void);
static void test_resolve_auth_env_var(void);
static void test_has_credentials(void);
static void test_resolve_region(void);
static void test_convert_tools_to_converse(void);
static void test_convert_content_to_converse(void);
static void test_bedrock_edge_cases(void);
static void test_convert_messages_to_converse(void);
static void test_normalize_converse_response(void);

/* Test data */
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

    /* ── bedrock_is_context_overflow ────────────────────────────── */
    printf("\n--- bedrock_is_context_overflow ---\n");
    {
        TEST("is_context_overflow NULL", !bedrock_is_context_overflow(NULL));
    }
    {
        TEST("is_context_overflow empty", !bedrock_is_context_overflow(""));
    }
    {
        TEST("is_context_overflow unrelated", !bedrock_is_context_overflow("Some random error message"));
    }
    {
        /* Pattern 1: ValidationException + input is too long */
        const char *msg = "ValidationException: The input is too long for this model";
        TEST("is_context_overflow pattern1 input too long", bedrock_is_context_overflow(msg));
    }
    {
        /* Pattern 1b: validation error + max input token */
        const char *msg = "validation error: max input token exceeded";
        TEST("is_context_overflow pattern1 max input token", bedrock_is_context_overflow(msg));
    }
    {
        /* Pattern 2: ValidationException + exceeds maximum token */
        const char *msg = "ValidationException: Input exceeds the maximum number of input tokens";
        TEST("is_context_overflow pattern2 exceeds max token", bedrock_is_context_overflow(msg));
    }
    {
        /* Pattern 3: ModelStreamErrorException */
        const char *msg = "ModelStreamErrorException: Input is too long for the model";
        TEST("is_context_overflow pattern3 stream error", bedrock_is_context_overflow(msg));
    }
    {
        /* Pattern 3b: stream error + too many input tokens */
        const char *msg = "stream error: too many input tokens in request";
        TEST("is_context_overflow pattern3b too many tokens", bedrock_is_context_overflow(msg));
    }
    {
        /* ThrottlingException is NOT context overflow */
        const char *msg = "ThrottlingException: Rate exceeded";
        TEST("is_context_overflow throttling is not overflow", !bedrock_is_context_overflow(msg));
    }

    /* ── bedrock_classify_error ──────────────────────────────────── */
    printf("\n--- bedrock_classify_error ---\n");
    {
        TEST("classify NULL", strcmp(bedrock_classify_error(NULL), "unknown") == 0);
    }
    {
        TEST("classify context overflow",
             strcmp(bedrock_classify_error("ValidationException: input is too long"), "context_overflow") == 0);
    }
    {
        TEST("classify rate_limit ThrottlingException",
             strcmp(bedrock_classify_error("ThrottlingException"), "rate_limit") == 0);
    }
    {
        TEST("classify rate_limit too many concurrent",
             strcmp(bedrock_classify_error("Too many concurrent requests"), "rate_limit") == 0);
    }
    {
        TEST("classify rate_limit quota",
             strcmp(bedrock_classify_error("ServiceQuotaExceededException"), "rate_limit") == 0);
    }
    {
        TEST("classify overload ModelNotReady",
             strcmp(bedrock_classify_error("ModelNotReadyException"), "overloaded") == 0);
    }
    {
        TEST("classify overload ModelTimeout",
             strcmp(bedrock_classify_error("ModelTimeoutException"), "overloaded") == 0);
    }
    {
        TEST("classify overload InternalServer",
             strcmp(bedrock_classify_error("InternalServerException"), "overloaded") == 0);
    }
    {
        TEST("classify unknown",
             strcmp(bedrock_classify_error("Something else entirely"), "unknown") == 0);
    }

    /* ── bedrock_extract_provider_from_arn ───────────────────────── */
    printf("\n--- bedrock_extract_provider_from_arn ---\n");
    {
        TEST("extract_provider NULL", bedrock_extract_provider_from_arn(NULL) == NULL);
    }
    {
        char *r = bedrock_extract_provider_from_arn("arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-v2");
        TEST("extract_provider anthropic", r && strcmp(r, "anthropic") == 0);
        free(r);
    }
    {
        char *r = bedrock_extract_provider_from_arn("arn:aws:bedrock:us-west-2::foundation-model/meta.llama4-maverick-v1:0");
        TEST("extract_provider meta", r && strcmp(r, "meta") == 0);
        free(r);
    }
    {
        char *r = bedrock_extract_provider_from_arn("arn:aws:bedrock:eu-west-1::foundation-model/amazon.nova-pro");
        TEST("extract_provider amazon", r && strcmp(r, "amazon") == 0);
        free(r);
    }
    {
        /* No foundation-model/ prefix */
        TEST("extract_provider no prefix", bedrock_extract_provider_from_arn("some-random-string") == NULL);
    }
    {
        /* Empty after prefix (no dot) */
        const char *arn = "arn:aws:bedrock::foundation-model/nodot";
        char *r = bedrock_extract_provider_from_arn(arn);
        TEST("extract_provider no dot", r == NULL);
        free(r);
    }

    /* ── bedrock_get_context_length ─────────────────────────────── */
    printf("\n--- bedrock_get_context_length ---\n");
    {
        TEST("get_context NULL returns default", bedrock_get_context_length(NULL) == 128000);
    }
    {
        TEST("get_context unknown returns default", bedrock_get_context_length("unknown.model") == 128000);
    }
    {
        TEST("get_context claude-sonnet-4-6",
             bedrock_get_context_length("anthropic.claude-sonnet-4-6-20250514-v1:0") == 200000);
    }
    {
        TEST("get_context claude-opus-4",
             bedrock_get_context_length("anthropic.claude-opus-4-20250514") == 200000);
    }
    {
        TEST("get_context nova-pro",
             bedrock_get_context_length("amazon.nova-pro-v1:0") == 300000);
    }
    {
        TEST("get_context nova-micro",
             bedrock_get_context_length("amazon.nova-micro-v1:0") == 128000);
    }
    {
        /* Longest prefix match: sonnet-4-5 should match before sonnet-4 */
        TEST("get_context longest prefix match",
             bedrock_get_context_length("anthropic.claude-sonnet-4-5-v1:0") == 200000);
    }
    {
        TEST("get_context llama4-maverick",
             bedrock_get_context_length("meta.llama4-maverick-v1:0") == 128000);
    }

    /* Print summary */
    printf("\n=== Existing tests: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");

    /* ---- R02 utility function tests ---- */
    test_is_anthropic_model();
    test_supports_tool_use();
    test_resolve_auth_env_var();
    test_has_credentials();
    test_resolve_region();
    test_convert_tools_to_converse();
    test_convert_content_to_converse();
    test_bedrock_edge_cases();

    printf("\n=== R02 messages/converse tests ===\n");
    test_convert_messages_to_converse();
    test_normalize_converse_response();

    printf("\n=== Overall: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}

/* ---- bedrock_is_anthropic_model ---- */
static void test_is_anthropic_model(void) {
    printf("\n[R02] bedrock_is_anthropic_model:\n");
    TEST("null",         !bedrock_is_anthropic_model(NULL));
    TEST("anthropic",     bedrock_is_anthropic_model("anthropic.claude-sonnet-4"));
    TEST("us prefix",     bedrock_is_anthropic_model("us.anthropic.claude-sonnet-4"));
    TEST("global prefix", bedrock_is_anthropic_model("global.anthropic.claude-haiku"));
    TEST("eu prefix",     bedrock_is_anthropic_model("eu.anthropic.claude-opus"));
    TEST("ap prefix",     bedrock_is_anthropic_model("ap.anthropic.claude-3-5"));
    TEST("jp prefix",     bedrock_is_anthropic_model("jp.anthropic.claude-3"));
    TEST("non-anthropic", !bedrock_is_anthropic_model("meta.llama4-maverick"));
    TEST("aws nova",      !bedrock_is_anthropic_model("amazon.nova-pro"));
    TEST("deepseek",      !bedrock_is_anthropic_model("deepseek.r1-v1:0"));
    TEST("empty",         !bedrock_is_anthropic_model(""));
    TEST("case check",    bedrock_is_anthropic_model("ANTHROPIC.CLAUDE-SONNET"));
}

/* ---- bedrock_model_supports_tool_use ---- */
static void test_supports_tool_use(void) {
    printf("\n[R02] bedrock_model_supports_tool_use:\n");
    TEST("null",          !bedrock_model_supports_tool_use(NULL));
    TEST("claude",         bedrock_model_supports_tool_use("anthropic.claude-sonnet-4"));
    TEST("deepseek r1",   !bedrock_model_supports_tool_use("deepseek.r1-v1:0"));
    TEST("deepseek-r1",   !bedrock_model_supports_tool_use("deepseek-r1-v1:0"));
    TEST("stability",     !bedrock_model_supports_tool_use("stability.stable-diffusion-xl"));
    TEST("cohere embed",  !bedrock_model_supports_tool_use("cohere.embed-english-v3"));
    TEST("titan embed",   !bedrock_model_supports_tool_use("amazon.titan-embed-text-v2"));
    TEST("nova",           bedrock_model_supports_tool_use("amazon.nova-pro"));
    TEST("llama",          bedrock_model_supports_tool_use("meta.llama4-maverick"));
    TEST("case check",    !bedrock_model_supports_tool_use("DEEPSEEK.R1"));
}

/* ---- bedrock_resolve_auth_env_var ---- */
static void test_resolve_auth_env_var(void) {
    printf("\n[R02] bedrock_resolve_auth_env_var:\n");
    /* These tests rely on the current env state. Run before/after setenv. */
    const char *saved_bearer = getenv("AWS_BEARER_TOKEN_BEDROCK");
    const char *saved_key_id = getenv("AWS_ACCESS_KEY_ID");
    const char *saved_secret = getenv("AWS_SECRET_ACCESS_KEY");
    const char *saved_profile = getenv("AWS_PROFILE");

    /* Test with no AWS env vars */
    unsetenv("AWS_BEARER_TOKEN_BEDROCK");
    unsetenv("AWS_ACCESS_KEY_ID");
    unsetenv("AWS_SECRET_ACCESS_KEY");
    unsetenv("AWS_PROFILE");
    unsetenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
    unsetenv("AWS_WEB_IDENTITY_TOKEN_FILE");
    TEST("none set", bedrock_resolve_auth_env_var() == NULL);

    /* Test bearer token */
    setenv("AWS_BEARER_TOKEN_BEDROCK", "tok_123", 1);
    TEST("bearer token", bedrock_resolve_auth_env_var() != NULL &&
         strcmp(bedrock_resolve_auth_env_var(), "AWS_BEARER_TOKEN_BEDROCK") == 0);
    unsetenv("AWS_BEARER_TOKEN_BEDROCK");

    /* Test access key pair (both needed) */
    setenv("AWS_ACCESS_KEY_ID", "AKID123", 1);
    setenv("AWS_SECRET_ACCESS_KEY", "secret456", 1);
    TEST("access key pair", bedrock_resolve_auth_env_var() != NULL &&
         strcmp(bedrock_resolve_auth_env_var(), "AWS_ACCESS_KEY_ID") == 0);
    unsetenv("AWS_ACCESS_KEY_ID");
    unsetenv("AWS_SECRET_ACCESS_KEY");

    /* Test partial access key (secret only, no key id) */
    setenv("AWS_SECRET_ACCESS_KEY", "secret", 1);
    TEST("secret only no key", bedrock_resolve_auth_env_var() == NULL);
    unsetenv("AWS_SECRET_ACCESS_KEY");

    /* Test profile */
    setenv("AWS_PROFILE", "myprofile", 1);
    TEST("profile", bedrock_resolve_auth_env_var() != NULL &&
         strcmp(bedrock_resolve_auth_env_var(), "AWS_PROFILE") == 0);
    unsetenv("AWS_PROFILE");

    /* Restore saved env */
    if (saved_bearer) setenv("AWS_BEARER_TOKEN_BEDROCK", saved_bearer, 1);
    if (saved_key_id) setenv("AWS_ACCESS_KEY_ID", saved_key_id, 1);
    if (saved_secret) setenv("AWS_SECRET_ACCESS_KEY", saved_secret, 1);
    if (saved_profile) setenv("AWS_PROFILE", saved_profile, 1);
}

/* ---- bedrock_has_credentials ---- */
static void test_has_credentials(void) {
    printf("\n[R02] bedrock_has_credentials:\n");
    const char *saved_profile = getenv("AWS_PROFILE");

    unsetenv("AWS_PROFILE");
    unsetenv("AWS_BEARER_TOKEN_BEDROCK");
    unsetenv("AWS_ACCESS_KEY_ID");
    unsetenv("AWS_SECRET_ACCESS_KEY");
    TEST("no creds", !bedrock_has_credentials());

    setenv("AWS_PROFILE", "test", 1);
    TEST("has profile", bedrock_has_credentials());
    unsetenv("AWS_PROFILE");

    if (saved_profile) setenv("AWS_PROFILE", saved_profile, 1);
}

/* ---- bedrock_resolve_region ---- */
static void test_resolve_region(void) {
    printf("\n[R02] bedrock_resolve_region:\n");
    const char *saved_region = getenv("AWS_REGION");
    const char *saved_def = getenv("AWS_DEFAULT_REGION");

    unsetenv("AWS_REGION");
    unsetenv("AWS_DEFAULT_REGION");
    const char *r = bedrock_resolve_region();
    TEST("default us-east-1", r && strcmp(r, "us-east-1") == 0);

    setenv("AWS_REGION", "eu-west-1", 1);
    TEST("AWS_REGION", bedrock_resolve_region() &&
         strcmp(bedrock_resolve_region(), "eu-west-1") == 0);

    unsetenv("AWS_REGION");
    setenv("AWS_DEFAULT_REGION", "ap-southeast-1", 1);
    TEST("AWS_DEFAULT_REGION", bedrock_resolve_region() &&
         strcmp(bedrock_resolve_region(), "ap-southeast-1") == 0);

    /* AWS_REGION takes priority over AWS_DEFAULT_REGION */
    setenv("AWS_REGION", "us-west-2", 1);
    TEST("priority", bedrock_resolve_region() &&
         strcmp(bedrock_resolve_region(), "us-west-2") == 0);

    unsetenv("AWS_REGION");
    unsetenv("AWS_DEFAULT_REGION");
    if (saved_region) setenv("AWS_REGION", saved_region, 1);
    if (saved_def) setenv("AWS_DEFAULT_REGION", saved_def, 1);
}

/* ---- bedrock_convert_tools_to_converse ---- */
static void test_convert_tools_to_converse(void) {
    printf("\n[R02] bedrock_convert_tools_to_converse:\n");

    /* Test NULL/empty */
    {
        json_t *r = bedrock_convert_tools_to_converse(NULL);
        TEST("null tools returns array", r != NULL && r->type == JSON_ARRAY && r->c.count == 0);
        json_free(r);
    }
    {
        json_t *arr = json_array();
        json_t *r = bedrock_convert_tools_to_converse(arr);
        TEST("empty array", r != NULL && r->type == JSON_ARRAY && r->c.count == 0);
        json_free(r);
        json_free(arr);
    }

    /* Test single tool */
    {
        json_t *fn = json_object();
        json_set(fn, "name", json_string("get_weather"));
        json_set(fn, "description", json_string("Get weather for a location"));
        json_t *params = json_object();
        json_set(params, "type", json_string("object"));
        json_set(fn, "parameters", json_copy(params));
        json_free(params);

        json_t *tool = json_object();
        json_set(tool, "type", json_string("function"));
        json_set(tool, "function", fn);

        json_t *tools = json_array();
        json_append(tools, tool);

        json_t *r = bedrock_convert_tools_to_converse(tools);
        TEST("single tool returns array", r != NULL && r->type == JSON_ARRAY);
        TEST("one tool spec", r->c.count == 1);
        if (r && r->c.count > 0) {
            json_t *ts = r->c.items[0];
            json_t *spec = json_obj_get(ts, "toolSpec");
            TEST("has toolSpec", spec != NULL && spec->type == JSON_OBJECT);
            if (spec) {
                const char *n = json_get_str(spec, "name", "");
                TEST("name preserved", n && strcmp(n, "get_weather") == 0);
                const char *d = json_get_str(spec, "description", "");
                TEST("description preserved", d && strcmp(d, "Get weather for a location") == 0);
                json_t *schema = json_obj_get(spec, "inputSchema");
                TEST("has inputSchema", schema != NULL);
                if (schema) {
                    json_t *inner = json_obj_get(schema, "json");
                    TEST("inputSchema has json", inner != NULL && inner->type == JSON_OBJECT);
                }
            }
        }
        json_free(r);
        json_free(tools);
    }

    /* Test tool without description */
    {
        json_t *fn = json_object();
        json_set(fn, "name", json_string("search_web"));
        json_t *tool = json_object();
        json_set(tool, "type", json_string("function"));
        json_set(tool, "function", fn);
        json_t *tools = json_array();
        json_append(tools, tool);

        json_t *r = bedrock_convert_tools_to_converse(tools);
        TEST("no description", r != NULL && r->c.count == 1);
        if (r && r->c.count > 0) {
            json_t *spec = json_obj_get(r->c.items[0], "toolSpec");
            const char *d = json_get_str(spec, "description", "__missing__");
            TEST("description omitted", d && strcmp(d, "__missing__") == 0);
        }
        json_free(r);
        json_free(tools);
    }
}

/* ---- bedrock_convert_content_to_converse ---- */
static void test_convert_content_to_converse(void) {
    printf("\n[R02] bedrock_convert_content_to_converse:\n");
    json_t *blocks;

    /* NULL content */
    blocks = bedrock_convert_content_to_converse(NULL);
    TEST("null returns array", blocks != NULL && blocks->type == JSON_ARRAY);
    TEST("null has one block", blocks && blocks->c.count == 1);
    if (blocks && blocks->c.count > 0) {
        const char *t = json_get_str(blocks->c.items[0], "text", "");
        TEST("null yields space", t && strcmp(t, " ") == 0);
    }
    json_free(blocks);

    /* JSON_NULL */
    blocks = bedrock_convert_content_to_converse(json_null());
    TEST("json_null has block", blocks && blocks->c.count == 1);
    json_free(blocks);

    /* Plain string */
    {
        json_t *s = json_string("Hello world");
        blocks = bedrock_convert_content_to_converse(s);
        TEST("string has block", blocks && blocks->c.count == 1);
        if (blocks && blocks->c.count > 0) {
            const char *t = json_get_str(blocks->c.items[0], "text", "");
            TEST("text preserved", t && strcmp(t, "Hello world") == 0);
        }
        json_free(blocks);
        json_free(s);
    }

    /* Empty string → space */
    {
        json_t *s = json_string("");
        blocks = bedrock_convert_content_to_converse(s);
        TEST("empty becomes space", blocks && blocks->c.count == 1);
        if (blocks && blocks->c.count > 0) {
            const char *t = json_get_str(blocks->c.items[0], "text", "");
            TEST("space text", t && strcmp(t, " ") == 0);
        }
        json_free(blocks);
        json_free(s);
    }

    /* Content array with text parts */
    {
        json_t *part1 = json_object();
        json_set(part1, "type", json_string("text"));
        json_set(part1, "text", json_string("First part"));
        json_t *part2 = json_object();
        json_set(part2, "type", json_string("text"));
        json_set(part2, "text", json_string("Second part"));
        json_t *arr = json_array();
        json_append(arr, part1);
        json_append(arr, part2);

        blocks = bedrock_convert_content_to_converse(arr);
        TEST("array 2 parts", blocks && blocks->c.count == 2);
        if (blocks && blocks->c.count == 2) {
            const char *t1 = json_get_str(blocks->c.items[0], "text", "");
            TEST("first text", t1 && strcmp(t1, "First part") == 0);
            const char *t2 = json_get_str(blocks->c.items[1], "text", "");
            TEST("second text", t2 && strcmp(t2, "Second part") == 0);
        }
        json_free(blocks);
        json_free(arr);
    }

    /* Image with data: URI */
    {
        json_t *url_obj = json_object();
        json_set(url_obj, "url", json_string("data:image/png;base64,iVBORw0KGgoAAAANSUhEUg=="));
        json_t *part = json_object();
        json_set(part, "type", json_string("image_url"));
        json_set(part, "image_url", url_obj);
        json_t *arr = json_array();
        json_append(arr, part);

        blocks = bedrock_convert_content_to_converse(arr);
        TEST("image has block", blocks && blocks->c.count == 1);
        if (blocks && blocks->c.count > 0) {
            json_t *img = json_obj_get(blocks->c.items[0], "image");
            TEST("has image object", img != NULL);
            if (img) {
                const char *fmt = json_get_str(img, "format", "");
                TEST("png format", fmt && strcmp(fmt, "png") == 0);
                json_t *src = json_obj_get(img, "source");
                TEST("has source", src != NULL);
                if (src) {
                    const char *bytes = json_get_str(src, "bytes", "");
                    TEST("has bytes", bytes && strstr(bytes, "iVBOR") != NULL);
                }
            }
        }
        json_free(blocks);
        json_free(arr);
    }

    /* Remote URL image → text reference */
    {
        json_t *url_obj = json_object();
        json_set(url_obj, "url", json_string("https://example.com/image.jpg"));
        json_t *part = json_object();
        json_set(part, "type", json_string("image_url"));
        json_set(part, "image_url", url_obj);
        json_t *arr = json_array();
        json_append(arr, part);

        blocks = bedrock_convert_content_to_converse(arr);
        TEST("remote url has block", blocks && blocks->c.count == 1);
        if (blocks && blocks->c.count > 0) {
            const char *t = json_get_str(blocks->c.items[0], "text", "");
            TEST("text reference", t && strstr(t, "[Image:") != NULL);
        }
        json_free(blocks);
        json_free(arr);
    }

    /* Empty array → fallback space */
    {
        json_t *arr = json_array();
        blocks = bedrock_convert_content_to_converse(arr);
        TEST("empty array space", blocks && blocks->c.count == 1);
        if (blocks && blocks->c.count > 0) {
            const char *t = json_get_str(blocks->c.items[0], "text", "");
            TEST("fallback space", t && strcmp(t, " ") == 0);
        }
        json_free(blocks);
        json_free(arr);
    }
}

/* ---- Edge case expansion for R02 utility functions ---- */
static void test_bedrock_edge_cases(void) {
    printf("\n[R02] Edge case expansion:\n");

    /* bedrock_is_context_overflow — NULL/empty safety */
    TEST("overflow NULL", !bedrock_is_context_overflow(NULL));
    TEST("overflow empty", !bedrock_is_context_overflow(""));

    /* bedrock_is_context_overflow — Bedrock-specific patterns */
    TEST("overflow validation + too long", bedrock_is_context_overflow("ValidationException: input is too long"));
    TEST("overflow validation + max input", bedrock_is_context_overflow("ValidationException: max input token"));
    TEST("overflow validation + exceed tokens", bedrock_is_context_overflow("ValidationException: exceeds the maximum number of tokens"));
    TEST("overflow stream + too long", bedrock_is_context_overflow("ModelStreamErrorException: Input is too long"));
    TEST("overflow no match", !bedrock_is_context_overflow("SomeOtherError: something else"));

    /* bedrock_classify_error — NULL/empty */
    TEST("classify NULL", strcmp(bedrock_classify_error(NULL), "unknown") == 0);
    TEST("classify empty", strcmp(bedrock_classify_error(""), "unknown") == 0);

    /* bedrock_classify_error — category detection */
    TEST("classify rate_limit", strcmp(bedrock_classify_error("ThrottlingException: retry"), "rate_limit") == 0);
    TEST("classify overloaded", strcmp(bedrock_classify_error("ModelNotReadyException: model loading"), "overloaded") == 0);
    TEST("classify context_overflow", strcmp(bedrock_classify_error("ValidationException: input is too long"), "context_overflow") == 0);
    TEST("classify unknown", strcmp(bedrock_classify_error("random error"), "unknown") == 0);

    /* bedrock_extract_provider_from_arn — edge cases */
    const char *arn;
    arn = bedrock_extract_provider_from_arn("arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-3-sonnet");
    TEST("arn provider anthropic", arn && strcmp(arn, "anthropic") == 0);
    arn = bedrock_extract_provider_from_arn("arn:aws:bedrock:eu-west-1::foundation-model/meta.llama3-70b");
    TEST("arn provider meta", arn && strcmp(arn, "meta") == 0);
    arn = bedrock_extract_provider_from_arn("arn:aws:bedrock:us-west-2::foundation-model/cohere.command-r");
    TEST("arn provider cohere", arn && strcmp(arn, "cohere") == 0);
    arn = bedrock_extract_provider_from_arn("not-an-arn");
    TEST("arn invalid", arn == NULL);
    arn = bedrock_extract_provider_from_arn("");
    TEST("arn empty", arn == NULL);

    /* bedrock_get_context_length — known models */
    int ctx = bedrock_get_context_length("anthropic.claude-3-sonnet");
    TEST("context sonnet known", ctx > 0);
    ctx = bedrock_get_context_length("meta.llama3-70b-instruct-v1:0");
    TEST("context llama known", ctx > 0);

    /* Unknown/edge cases return default (128000) */
    ctx = bedrock_get_context_length("unknown.model.v1");
    TEST("context unknown returns default", ctx == 128000);
    ctx = bedrock_get_context_length(NULL);
    TEST("context NULL returns default", ctx == 128000);
    ctx = bedrock_get_context_length("");
    TEST("context empty returns default", ctx == 128000);
}

/* ---- bedrock_convert_messages_to_converse tests ---- */
static void test_convert_messages_to_converse(void) {
    printf("\n[R02] bedrock_convert_messages_to_converse:\n");

    /* NULL/empty input */
    {
        json_t *result = bedrock_convert_messages_to_converse(NULL);
        TEST("NULL messages returns NULL", result == NULL);

        json_t *empty = json_array();
        result = bedrock_convert_messages_to_converse(empty);
        TEST("empty messages returns result", result != NULL);
        if (result) {
            TEST("empty has messages array", json_obj_get(result, "messages") != NULL);
            TEST("empty no system", json_obj_get(result, "system") == NULL);
            json_free(result);
        }
        json_free(empty);
    }

    /* Simple user message */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("hello"));
        json_t *msgs = json_array();
        json_append(msgs, msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("simple user returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            TEST("has messages", conv_msgs != NULL);
            if (conv_msgs && conv_msgs->type == JSON_ARRAY) {
                TEST("1 converse message", conv_msgs->c.count == 1);
                if (conv_msgs->c.count > 0) {
                    json_t *cm = conv_msgs->c.items[0];
                    const char *role = json_get_str(cm, "role", "");
                    TEST("role is user", strcmp(role, "user") == 0);
                }
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* System message → system blocks */
    {
        json_t *sys_msg = json_object();
        json_set(sys_msg, "role", json_string("system"));
        json_set(sys_msg, "content", json_string("You are a helpful assistant."));
        json_t *user_msg = json_object();
        json_set(user_msg, "role", json_string("user"));
        json_set(user_msg, "content", json_string("hi"));
        json_t *msgs = json_array();
        json_append(msgs, sys_msg);
        json_append(msgs, user_msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("system msg returns result", result != NULL);
        if (result) {
            json_t *system = json_obj_get(result, "system");
            TEST("has system blocks", system != NULL);
            if (system && system->type == JSON_ARRAY && system->c.count > 0) {
                const char *text = json_get_str(system->c.items[0], "text", "");
                TEST("system text matches", strstr(text, "helpful") != NULL);
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* Assistant message with tool calls */
    {
        json_t *fn = json_object();
        json_set(fn, "name", json_string("get_weather"));
        json_set(fn, "arguments", json_string("{\"city\":\"London\"}"));

        json_t *tc = json_object();
        json_set(tc, "id", json_string("call_123"));
        json_set(tc, "type", json_string("function"));
        json_set(tc, "function", fn);

        json_t *tool_calls = json_array();
        json_append(tool_calls, tc);

        json_t *msg = json_object();
        json_set(msg, "role", json_string("assistant"));
        json_set(msg, "content", json_string("Let me check the weather."));
        json_set(msg, "tool_calls", tool_calls);

        json_t *msgs = json_array();
        json_append(msgs, msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("assistant with tool call returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            TEST("has messages", conv_msgs != NULL);
            if (conv_msgs && conv_msgs->type == JSON_ARRAY && conv_msgs->c.count > 1) {
                /* First message is assistant → padding inserts user at index 0,
                 * so assistant is at index 1 */
                json_t *am = conv_msgs->c.items[1];
                const char *role = json_get_str(am, "role", "");
                TEST("role is assistant", strcmp(role, "assistant") == 0);
                json_t *blocks = json_obj_get(am, "content");
                if (blocks && blocks->type == JSON_ARRAY) {
                    bool has_tool_use = false;
                    for (size_t i = 0; i < blocks->c.count; i++) {
                        json_t *tu = json_obj_get(blocks->c.items[i], "toolUse");
                        if (tu) { has_tool_use = true; break; }
                    }
                    TEST("has toolUse block", has_tool_use);
                }
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* Tool result → merges into user message */
    {
        json_t *tr_msg = json_object();
        json_set(tr_msg, "role", json_string("tool"));
        json_set(tr_msg, "tool_call_id", json_string("call_123"));
        json_set(tr_msg, "content", json_string("{\"temp\":22}"));

        json_t *msgs = json_array();
        json_append(msgs, tr_msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("tool result returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            TEST("has messages", conv_msgs != NULL);
            if (conv_msgs && conv_msgs->type == JSON_ARRAY && conv_msgs->c.count > 0) {
                json_t *um = conv_msgs->c.items[0];
                const char *role = json_get_str(um, "role", "");
                /* Tool result goes in user role, but if no preceding user msg,
                 * a new user message is created */
                TEST("role is user", strcmp(role, "user") == 0);
                json_t *blocks = json_obj_get(um, "content");
                if (blocks && blocks->type == JSON_ARRAY) {
                    bool has_tr = false;
                    for (size_t i = 0; i < blocks->c.count; i++) {
                        json_t *tr = json_obj_get(blocks->c.items[i], "toolResult");
                        if (tr) { has_tr = true; break; }
                    }
                    TEST("has toolResult block", has_tr);
                }
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* User/assistant alternation — first message not user → padding inserted */
    {
        json_t *msg = json_object();
        json_set(msg, "role", json_string("assistant"));
        json_set(msg, "content", json_string("hello"));
        json_t *msgs = json_array();
        json_append(msgs, msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("alternation padding returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            if (conv_msgs && conv_msgs->type == JSON_ARRAY) {
                TEST("padding inserted as first msg", conv_msgs->c.count >= 2);
                if (conv_msgs->c.count > 0) {
                    json_t *first = conv_msgs->c.items[0];
                    const char *role = json_get_str(first, "role", "");
                    TEST("first role is user after padding", strcmp(role, "user") == 0);
                }
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* Consecutive same-role messages should be merged */
    {
        json_t *m1 = json_object();
        json_set(m1, "role", json_string("user"));
        json_set(m1, "content", json_string("first"));
        json_t *m2 = json_object();
        json_set(m2, "role", json_string("user"));
        json_set(m2, "content", json_string("second"));
        json_t *msgs = json_array();
        json_append(msgs, m1);
        json_append(msgs, m2);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("merge consecutive returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            if (conv_msgs && conv_msgs->type == JSON_ARRAY) {
                TEST("merged into single message", conv_msgs->c.count == 1);
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* Last message not user → padding appended */
    {
        json_t *user = json_object();
        json_set(user, "role", json_string("user"));
        json_set(user, "content", json_string("hi"));
        json_t *assist = json_object();
        json_set(assist, "role", json_string("assistant"));
        json_set(assist, "content", json_string("hello"));
        json_t *msgs = json_array();
        json_append(msgs, user);
        json_append(msgs, assist);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("last message padding returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            if (conv_msgs && conv_msgs->type == JSON_ARRAY && conv_msgs->c.count > 0) {
                json_t *last = conv_msgs->c.items[conv_msgs->c.count - 1];
                const char *role = json_get_str(last, "role", "");
                TEST("last role is user after padding", strcmp(role, "user") == 0);
            }
            json_free(result);
        }
        json_free(msgs);
    }

    /* System message with array content */
    {
        json_t *text_part = json_object();
        json_set(text_part, "type", json_string("text"));
        json_set(text_part, "text", json_string("You are Claude."));
        json_t *content = json_array();
        json_append(content, text_part);
        json_t *msg = json_object();
        json_set(msg, "role", json_string("system"));
        json_set(msg, "content", content);
        json_t *user_msg = json_object();
        json_set(user_msg, "role", json_string("user"));
        json_set(user_msg, "content", json_string("ok"));
        json_t *msgs = json_array();
        json_append(msgs, msg);
        json_append(msgs, user_msg);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("system array content returns result", result != NULL);
        if (result) {
            json_t *system = json_obj_get(result, "system");
            TEST("has system from array", system != NULL);
            json_free(result);
        }
        json_free(msgs);
    }

    /* Empty assistant content → space placeholder */
    {
        json_t *user = json_object();
        json_set(user, "role", json_string("user"));
        json_set(user, "content", json_string("hi"));
        json_t *assist = json_object();
        json_set(assist, "role", json_string("assistant"));
        json_set(assist, "content", json_string(""));
        json_t *msgs = json_array();
        json_append(msgs, user);
        json_append(msgs, assist);

        json_t *result = bedrock_convert_messages_to_converse(msgs);
        TEST("empty assistant returns result", result != NULL);
        if (result) {
            json_t *conv_msgs = json_obj_get(result, "messages");
            if (conv_msgs && conv_msgs->type == JSON_ARRAY && conv_msgs->c.count > 1) {
                json_t *am = conv_msgs->c.items[1];
                const char *role = json_get_str(am, "role", "");
                TEST("assistant role preserved", strcmp(role, "assistant") == 0);
            }
            json_free(result);
        }
        json_free(msgs);
    }
}

/* ---- bedrock_normalize_converse_response tests ---- */
static void test_normalize_converse_response(void) {
    printf("\n[R02] bedrock_normalize_converse_response:\n");

    /* NULL/empty */
    {
        json_t *result = bedrock_normalize_converse_response(NULL);
        TEST("NULL response returns NULL", result == NULL);
    }

    /* Text-only response */
    {
        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(10));
        json_set(usage, "outputTokens", json_number(20));

        json_t *text_block = json_object();
        json_set(text_block, "text", json_string("Hello, world!"));

        json_t *content = json_array();
        json_append(content, text_block);

        json_t *message = json_object();
        json_set(message, "content", content);

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("end_turn"));
        json_set(response, "usage", usage);
        json_set(response, "modelId", json_string("anthropic.claude-v2"));

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("text response returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            TEST("has choices", choices != NULL && choices->type == JSON_ARRAY);
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                json_t *choice = choices->c.items[0];
                const char *fr = json_get_str(choice, "finish_reason", "");
                TEST("finish_reason is stop", strcmp(fr, "stop") == 0);

                json_t *msg = json_obj_get(choice, "message");
                TEST("has message", msg != NULL);
                if (msg) {
                    const char *content_s = json_get_str(msg, "content", "");
                    TEST("content matches", strcmp(content_s, "Hello, world!") == 0);
                    const char *role = json_get_str(msg, "role", "");
                    TEST("role is assistant", strcmp(role, "assistant") == 0);
                }
            }

            json_t *usage_out = json_obj_get(result, "usage");
            TEST("has usage", usage_out != NULL);
            if (usage_out) {
                TEST("prompt_tokens", (int)json_get_num(usage_out, "prompt_tokens", 0) == 10);
                TEST("completion_tokens", (int)json_get_num(usage_out, "completion_tokens", 0) == 20);
                TEST("total_tokens", (int)json_get_num(usage_out, "total_tokens", 0) == 30);
            }

            const char *model = json_get_str(result, "model", "");
            TEST("model matches", strstr(model, "claude") != NULL);

            json_free(result);
        }
        json_free(response);
    }

    /* Tool use response */
    {
        json_t *input_obj = json_object();
        json_set(input_obj, "city", json_string("London"));

        json_t *tool_use = json_object();
        json_set(tool_use, "toolUseId", json_string("call_456"));
        json_set(tool_use, "name", json_string("get_weather"));
        json_set(tool_use, "input", input_obj);

        json_t *tu_block = json_object();
        json_set(tu_block, "toolUse", tool_use);

        json_t *content = json_array();
        json_append(content, tu_block);

        json_t *message = json_object();
        json_set(message, "content", content);

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(5));
        json_set(usage, "outputTokens", json_number(15));

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("tool_use"));
        json_set(response, "usage", usage);

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("tool use response returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                json_t *choice = choices->c.items[0];
                const char *fr = json_get_str(choice, "finish_reason", "");
                TEST("finish_reason is tool_calls", strcmp(fr, "tool_calls") == 0);
                json_t *msg = json_obj_get(choice, "message");
                if (msg) {
                    json_t *tcs = json_obj_get(msg, "tool_calls");
                    TEST("has tool_calls array", tcs != NULL && tcs->type == JSON_ARRAY);
                    if (tcs && tcs->type == JSON_ARRAY && tcs->c.count > 0) {
                        json_t *tc = tcs->c.items[0];
                        const char *tc_id = json_get_str(tc, "id", "");
                        TEST("tool call id matches", strcmp(tc_id, "call_456") == 0);
                        json_t *fn = json_obj_get(tc, "function");
                        TEST("has function", fn != NULL);
                        if (fn) {
                            const char *name = json_get_str(fn, "name", "");
                            TEST("function name matches", strcmp(name, "get_weather") == 0);
                        }
                    }
                }
            }
            json_free(result);
        }
        json_free(response);
    }

    /* Reasoning content */
    {
        json_t *reasoning = json_object();
        json_set(reasoning, "text", json_string("I need to think step by step"));

        json_t *reasoning_block = json_object();
        json_set(reasoning_block, "reasoningContent", reasoning);

        json_t *content = json_array();
        json_append(content, reasoning_block);

        json_t *message = json_object();
        json_set(message, "content", content);

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(1));
        json_set(usage, "outputTokens", json_number(1));

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("end_turn"));
        json_set(response, "usage", usage);

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("reasoning response returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                json_t *msg = json_obj_get(choices->c.items[0], "message");
                if (msg) {
                    const char *rc = json_get_str(msg, "reasoning_content", "");
                    TEST("has reasoning_content", rc && strstr(rc, "step") != NULL);
                }
            }
            json_free(result);
        }
        json_free(response);
    }

    /* content_filter stop reason */
    {
        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(0));
        json_set(usage, "outputTokens", json_number(0));

        json_t *message = json_object();
        json_set(message, "content", json_array());

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("content_filtered"));
        json_set(response, "usage", usage);

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("content_filter returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                const char *fr = json_get_str(choices->c.items[0], "finish_reason", "");
                TEST("finish_reason is content_filter", strcmp(fr, "content_filter") == 0);
            }
            json_free(result);
        }
        json_free(response);
    }

    /* Empty output (no content blocks) */
    {
        json_t *message = json_object();
        json_set(message, "content", json_array());

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(0));
        json_set(usage, "outputTokens", json_number(0));

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("max_tokens"));
        json_set(response, "usage", usage);

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("empty output returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                const char *fr = json_get_str(choices->c.items[0], "finish_reason", "");
                TEST("finish_reason is length", strcmp(fr, "length") == 0);
            }
            json_free(result);
        }
        json_free(response);
    }

    /* Multiple text blocks joined with newline */
    {
        json_t *b1 = json_object();
        json_set(b1, "text", json_string("First line"));
        json_t *b2 = json_object();
        json_set(b2, "text", json_string("Second line"));
        json_t *content = json_array();
        json_append(content, b1);
        json_append(content, b2);

        json_t *message = json_object();
        json_set(message, "content", content);

        json_t *output = json_object();
        json_set(output, "message", message);

        json_t *usage = json_object();
        json_set(usage, "inputTokens", json_number(0));
        json_set(usage, "outputTokens", json_number(0));

        json_t *response = json_object();
        json_set(response, "output", output);
        json_set(response, "stopReason", json_string("end_turn"));
        json_set(response, "usage", usage);

        json_t *result = bedrock_normalize_converse_response(response);
        TEST("multi-block returns result", result != NULL);
        if (result) {
            json_t *choices = json_obj_get(result, "choices");
            if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
                json_t *msg = json_obj_get(choices->c.items[0], "message");
                if (msg) {
                    const char *content_s = json_get_str(msg, "content", "");
                    TEST("multi-block joined with newline",
                         content_s && strstr(content_s, "line\nSecond") != NULL);
                }
            }
            json_free(result);
        }
        json_free(response);
    }
}
