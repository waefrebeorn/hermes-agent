/*
 * test_bedrock_full.c — Bedrock provider comprehensive tests
 *
 * Covers: URL building, request body (inference config, system array,
 * tool config, B39-B41), response parsing (text, toolUse, error),
 * streaming, null safety.
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
static const message_t *msgs[] = {&user_msg};

int main(void) {
    printf("=== Bedrock provider comprehensive tests ===\n\n");

    /* ──────────── URL building ──────────── */

    /* Default URL with us-east-1 region */
    {
        provider_t p = {0};
        /* Unset AWS_REGION to guarantee default */
        unsetenv("AWS_REGION");
        unsetenv("AWS_DEFAULT_REGION");
        char *url = PROVIDER_OPS_BEDROCK.build_url(&p, NULL);
        TEST("default URL contains bedrock-runtime",
             url && strstr(url, "bedrock-runtime.us-east-1.amazonaws.com"));
        TEST("default URL contains /model/", url && strstr(url, "/model/"));
        TEST("default URL contains /converse", url && strstr(url, "/converse"));
        TEST("default model is claude-3-sonnet",
             url && strstr(url, "claude-3-sonnet"));
        free(url);
    }

    /* Custom model in URL */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "anthropic.claude-4");
        char *url = PROVIDER_OPS_BEDROCK.build_url(&p, NULL);
        TEST("custom model in URL",
             url && strstr(url, "anthropic.claude-4"));
        free(url);
    }

    /* ──────────── Headers ──────────── */

    /* Headers without AWS creds (minimal output) */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_BEDROCK.build_headers(&p, NULL);
        TEST("headers without creds contain Content-Type",
             hdr && strstr(hdr, "Content-Type"));
        free(hdr);
    }

    /* ──────────── Request body — inference config ──────────── */

    /* inferenceConfig with maxTokens */
    {
        provider_t p = {0};
        p.config.max_tokens = 8192;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("inferenceConfig present", body && strstr(body, "\"inferenceConfig\""));
        TEST("maxTokens in inferenceConfig", body && strstr(body, "\"maxTokens\":8192"));
        free(body);
    }

    /* temperature in inferenceConfig */
    {
        provider_t p = {0};
        p.config.temperature = 0.7f;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature in body", body && strstr(body, "\"temperature\":0.699"));
        free(body);
    }

    /* temperature=0.0 included */
    {
        provider_t p = {0};
        p.config.temperature = 0.0f;
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature=0.0 included", body && strstr(body, "\"temperature\":0"));
        free(body);
    }

    /* topP in inferenceConfig */
    {
        provider_t p = {0};
        p.config.top_p = 0.9f;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("topP in body", body && strstr(body, "\"topP\":0.899"));
        free(body);
    }

    /* stopSequences in inferenceConfig */
    {
        provider_t p = {0};
        p.config.stop_count = 2;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "\\n");
        snprintf(p.config.stop_sequences[1], sizeof(p.config.stop_sequences[1]), "END");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("stopSequences in inferenceConfig", body && strstr(body, "\"stopSequences\""));
        free(body);
    }

    /* ──────────── System prompt ──────────── */

    /* System message in system array */
    {
        provider_t p = {0};
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are a helpful assistant."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("system array present", body && strstr(body, "\"system\""));
        TEST("system has text entry",
             body && strstr(body, "\"text\":\"You are a helpful assistant.\""));
        TEST("system not in messages",
             body && !strstr(body, "\"role\":\"system\""));
        free(body);
    }

    /* Multiple system messages produce multiple system entries */
    {
        provider_t p = {0};
        message_t sys1 = {.role = MSG_SYSTEM, .content = (char*)"Rule 1."};
        message_t sys2 = {.role = MSG_SYSTEM, .content = (char*)"Rule 2."};
        const message_t *with_sys[] = {&sys1, &sys2, &user_msg};
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, with_sys, 3, NULL, false);
        TEST("multiple system entries in array",
             body && strstr(body, "Rule 1.") && strstr(body, "Rule 2."));
        free(body);
    }

    /* ──────────── Messages ──────────── */

    /* User message in messages array */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("messages array present", body && strstr(body, "\"messages\""));
        TEST("user role in messages",
             body && strstr(body, "\"role\":\"user\""));
        TEST("user content text",
             body && strstr(body, "\"text\":\"hello\""));
        free(body);
    }

    /* Assistant message */
    {
        provider_t p = {0};
        message_t asst_msg = {.role = MSG_ASSISTANT, .content = (char*)"Sure!"};
        const message_t *with_asst[] = {&asst_msg};
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, with_asst, 1, NULL, false);
        TEST("assistant role in messages",
             body && strstr(body, "\"role\":\"assistant\""));
        free(body);
    }

    /* ──────────── B39-B41: inferenceProfile + guardrails + trace ──────────── */

    /* inferenceProfile in root */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile),
                 "arn:aws:bedrock:us-east-1:123:profile/test");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("inferenceProfile in root", body && strstr(body, "\"inferenceProfile\""));
        TEST("inferenceProfile ARN value", body && strstr(body, "arn:aws:bedrock"));
        free(body);
    }

    /* guardrailConfig in root */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"abc\"}");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("guardrailConfig in root", body && strstr(body, "\"guardrailConfig\""));
        TEST("guardrailIdentifier in config", body && strstr(body, "guardrailIdentifier"));
        free(body);
    }

    /* enableTrace when enabled */
    {
        provider_t p = {0};
        p.config.bedrock_trace_enabled = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("enableTrace:true when enabled",
             body && strstr(body, "\"enableTrace\":true"));
        free(body);
    }

    /* All B39-B41 combined */
    {
        provider_t p = {0};
        snprintf(p.config.bedrock_inference_profile, sizeof(p.config.bedrock_inference_profile), "us.profile");
        snprintf(p.config.bedrock_guardrail_config, sizeof(p.config.bedrock_guardrail_config),
                 "{\"guardrailIdentifier\":\"xyz\",\"guardrailVersion\":\"1\"}");
        p.config.bedrock_trace_enabled = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("combined inferenceProfile", body && strstr(body, "\"inferenceProfile\""));
        TEST("combined guardrailConfig", body && strstr(body, "\"guardrailConfig\""));
        TEST("combined enableTrace", body && strstr(body, "\"enableTrace\":true"));
        free(body);
    }

    /* ──────────── response_format + json_mode ──────────── */

    /* response_format in body */
    {
        provider_t p = {0};
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_object\"}");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("response_format in body", body && strstr(body, "\"response_format\""));
        free(body);
    }

    /* json_mode sets json_object */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("json_mode response_format", body && strstr(body, "\"type\":\"json_object\""));
        free(body);
    }

    /* ──────────── tool_choice + parallel_tool_calls ──────────── */

    /* tool_choice in body */
    {
        provider_t p = {0};
        snprintf(p.config.tool_choice, sizeof(p.config.tool_choice), "auto");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("tool_choice in body", body && strstr(body, "\"tool_choice\":\"auto\""));
        free(body);
    }

    /* parallel_tool_calls disabled */
    {
        provider_t p = {0};
        p.config.parallel_tool_calls = false;
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("parallel_tool_calls=false in body",
             body && strstr(body, "\"parallel_tool_calls\":false"));
        free(body);
    }

    /* ──────────── metadata + extra_body ──────────── */

    /* metadata in body */
    {
        provider_t p = {0};
        snprintf(p.config.metadata, sizeof(p.config.metadata),
                 "{\"user_id\":\"abc\"}");
        char *body = PROVIDER_OPS_BEDROCK.build_request_body(&p, msgs, 1, NULL, false);
        TEST("metadata in body", body && strstr(body, "\"metadata\""));
        TEST("metadata user_id", body && strstr(body, "\"user_id\":\"abc\""));
        free(body);
    }

    /* ──────────── Response parsing ──────────── */

    /* Text-only response */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":["
            "{\"text\":\"Hello!\"}]}},\"stopReason\":\"end_turn\","
            "\"usage\":{\"inputTokens\":10,\"outputTokens\":5}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, resp_json);
        TEST("text response parsed", r != NULL);
        TEST("text response content",
             r && r->content && strcmp(r->content, "Hello!") == 0);
        TEST("text response inputTokens", r && r->input_tokens == 10);
        TEST("text response outputTokens", r && r->output_tokens == 5);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Response with toolUse */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":["
            "{\"text\":\"Searching...\"},"
            "{\"toolUse\":{\"toolUseId\":\"tu123\",\"name\":\"web_search\",\"input\":{\"q\":\"weather\"}}}]}},"
            "\"stopReason\":\"end_turn\"}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, resp_json);
        TEST("toolUse response parsed", r != NULL);
        TEST("toolUse has text content",
             r && r->content && strcmp(r->content, "Searching...") == 0);
        TEST("toolUse has tool calls", r && r->tool_calls_count == 1);
        TEST("toolUse id", r && strcmp(r->tool_calls[0].id, "tu123") == 0);
        TEST("toolUse name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("toolUse args has q", r && strstr(r->tool_calls[0].arguments, "\"weather\""));
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Empty content blocks */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"output\":{\"message\":{\"role\":\"assistant\",\"content\":[]}},\"stopReason\":\"end_turn\"}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, resp_json);
        TEST("empty content parsed", r != NULL);
        TEST("empty content NULL or empty",
             r && (!r->content || r->content[0] == '\0'));
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* No output field */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, "{}");
        TEST("empty response parsed", r != NULL);
        TEST("empty response NULL content",
             r && (!r->content || r->content[0] == '\0'));
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Error in response */
    {
        provider_t p = {0};
        const char *err_json =
            "{\"error\":{\"message\":\"Access denied\"}}";
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, err_json);
        TEST("error response parsed", r != NULL);
        TEST("error message in content",
             r && r->content && strstr(r->content, "Access denied"));
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* ──────────── Streaming ──────────── */

    /* Stream chunk (Bedrock doesn't use SSE, just raw text) */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_stream_chunk(&p, "data: {\"text\":\"hello\"}\n");
        TEST("stream chunk parsed", r != NULL);
        TEST("stream chunk passed through",
             r && r->content && strstr(r->content, "hello"));
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* ──────────── Null safety ──────────── */

    /* free_response with NULL */
    {
        PROVIDER_OPS_BEDROCK.free_response(NULL);
        TEST("free_response(NULL) no crash", 1);
    }

    /* Parse NULL response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, NULL);
        TEST("parse_response(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Parse empty response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, "");
        TEST("parse_response(empty) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Parse invalid JSON */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, "{invalid}");
        TEST("parse_response(invalid) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    /* Stream NULL chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_stream_chunk(&p, NULL);
        TEST("parse_stream_chunk(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_BEDROCK.free_response(r);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}
