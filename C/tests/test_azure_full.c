/*
 * test_azure_full.c — Azure provider comprehensive tests
 *
 * Covers: URL building (deployment_id, api_version), headers,
 * request body (LLM params, response_format, json_mode, metadata,
 * tool_choice, extra_body, messages), response parsing (text,
 * tool_calls, error, usage), streaming, null safety.
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
    printf("=== Azure provider comprehensive tests ===\n\n");

    /* ──────────── URL building ──────────── */

    /* Default URL with model as deployment */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o-mini");
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://my-resource.openai.azure.com");
        TEST("URL uses model as deployment",
             url && strstr(url, "/deployments/gpt-4o-mini/"));
        TEST("URL has default api-version",
             url && strstr(url, "api-version=2024-10-01-preview"));
        TEST("URL has chat/completions",
             url && strstr(url, "/chat/completions"));
        free(url);
    }

    /* B37: deployment_id overrides model */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o-mini");
        snprintf(p.config.azure_deployment_id, sizeof(p.config.azure_deployment_id), "my-deploy");
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://res.openai.azure.com");
        TEST("deployment_id overrides model",
             url && strstr(url, "/deployments/my-deploy/"));
        TEST("model name not in URL path",
             url && !strstr(url, "/deployments/gpt-4o-mini/"));
        free(url);
    }

    /* B38: custom api_version */
    {
        provider_t p = {0};
        snprintf(p.config.azure_api_version, sizeof(p.config.azure_api_version), "2025-03-01");
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://res.openai.azure.com");
        TEST("custom api_version", url && strstr(url, "api-version=2025-03-01"));
        free(url);
    }

    /* Empty model falls back to gpt-4o */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://res.openai.azure.com");
        TEST("empty model falls back to gpt-4o",
             url && strstr(url, "/deployments/gpt-4o/"));
        free(url);
    }

    /* Trailing slash stripped */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4");
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://res.openai.azure.com/");
        TEST("trailing slash stripped, no //",
             url && !strstr(url, "//openai") && strstr(url, "/openai/deployments/gpt-4/"));
        free(url);
    }

    /* B37+B38: both custom fields */
    {
        provider_t p = {0};
        snprintf(p.config.azure_deployment_id, sizeof(p.config.azure_deployment_id), "custom-dep");
        snprintf(p.config.azure_api_version, sizeof(p.config.azure_api_version), "2025-01-01");
        char *url = PROVIDER_OPS_AZURE.build_url(&p,
            "https://my-resource.openai.azure.com");
        TEST("combined custom deploy and api-version",
             url && strstr(url, "/deployments/custom-dep/") &&
             strstr(url, "api-version=2025-01-01"));
        free(url);
    }

    /* ──────────── Headers ──────────── */

    /* Headers with api-key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_AZURE.build_headers(&p, "sk-azure-test");
        TEST("headers contain api-key", hdr && strstr(hdr, "api-key: sk-azure-test"));
        TEST("headers contain Content-Type", hdr && strstr(hdr, "Content-Type: application/json"));
        free(hdr);
    }

    /* Headers without API key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_AZURE.build_headers(&p, NULL);
        TEST("headers without key have no Authorization",
             hdr && !strstr(hdr, "Authorization"));
        free(hdr);
    }

    /* ──────────── Request body — LLM params ──────────── */

    /* max_tokens */
    {
        provider_t p = {0};
        p.config.max_tokens = 4096;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("max_tokens in body", body && strstr(body, "\"max_tokens\":4096"));
        free(body);
    }

    /* temperature */
    {
        provider_t p = {0};
        p.config.temperature = 0.7f;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature in body", body && strstr(body, "\"temperature\":0.699"));
        free(body);
    }

    /* temperature=0.0 included */
    {
        provider_t p = {0};
        p.config.temperature = 0.0f;
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature=0.0 included", body && strstr(body, "\"temperature\":0"));
        free(body);
    }

    /* top_p */
    {
        provider_t p = {0};
        p.config.top_p = 0.9f;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("top_p in body", body && strstr(body, "\"top_p\":0.899"));
        free(body);
    }

    /* stop sequences */
    {
        provider_t p = {0};
        p.config.stop_count = 1;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "END");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("stop sequences in body", body && strstr(body, "\"stop\""));
        free(body);
    }

    /* service_tier */
    {
        provider_t p = {0};
        snprintf(p.config.service_tier, sizeof(p.config.service_tier), "default");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("service_tier in body", body && strstr(body, "\"service_tier\":\"default\""));
        free(body);
    }

    /* presence_penalty */
    {
        provider_t p = {0};
        p.config.presence_penalty = 0.5f;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("presence_penalty in body", body && strstr(body, "\"presence_penalty\":0.5"));
        free(body);
    }

    /* frequency_penalty */
    {
        provider_t p = {0};
        p.config.frequency_penalty = 0.3f;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("frequency_penalty in body", body && strstr(body, "\"frequency_penalty\":0.3"));
        free(body);
    }

    /* seed */
    {
        provider_t p = {0};
        p.config.seed = 42;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("seed in body", body && strstr(body, "\"seed\":42"));
        free(body);
    }

    /* logprobs + top_logprobs */
    {
        provider_t p = {0};
        p.config.logprobs = true;
        p.config.top_logprobs = 5;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("logprobs=true in body", body && strstr(body, "\"logprobs\":true"));
        TEST("top_logprobs in body", body && strstr(body, "\"top_logprobs\":5"));
        free(body);
    }

    /* user */
    {
        provider_t p = {0};
        snprintf(p.config.user, sizeof(p.config.user), "user-abc");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("user in body", body && strstr(body, "\"user\":\"user-abc\""));
        free(body);
    }

    /* ──────────── response_format + json_mode ──────────── */

    /* response_format with strict */
    {
        provider_t p = {0};
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_object\"}");
        p.config.response_format_strict = true;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("response_format in body", body && strstr(body, "\"response_format\""));
        TEST("strict in response_format", body && strstr(body, "\"strict\":true"));
        free(body);
    }

    /* json_mode convenience */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("json_mode response_format", body && strstr(body, "\"type\":\"json_object\""));
        free(body);
    }

    /* ──────────── metadata, tool_choice, extra_body ──────────── */

    /* metadata */
    {
        provider_t p = {0};
        snprintf(p.config.metadata, sizeof(p.config.metadata),
                 "{\"user_id\":\"abc\"}");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("metadata in body", body && strstr(body, "\"metadata\""));
        free(body);
    }

    /* tool_choice */
    {
        provider_t p = {0};
        snprintf(p.config.tool_choice, sizeof(p.config.tool_choice), "auto");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("tool_choice in body", body && strstr(body, "\"tool_choice\":\"auto\""));
        free(body);
    }

    /* parallel_tool_calls disabled */
    {
        provider_t p = {0};
        p.config.parallel_tool_calls = false;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("parallel_tool_calls=false",
             body && strstr(body, "\"parallel_tool_calls\":false"));
        free(body);
    }

    /* max_tool_calls */
    {
        provider_t p = {0};
        p.config.max_tool_calls = 5;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("max_tool_calls in body", body && strstr(body, "\"max_tool_calls\":5"));
        free(body);
    }

    /* n > 1 */
    {
        provider_t p = {0};
        p.config.n = 3;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("n in body", body && strstr(body, "\"n\":3"));
        free(body);
    }

    /* extra_body merged */
    {
        provider_t p = {0};
        snprintf(p.config.extra_body, sizeof(p.config.extra_body),
                 "{\"custom\":\"val\"}");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("extra_body merged", body && strstr(body, "\"custom\":\"val\""));
        free(body);
    }

    /* ──────────── Messages ──────────── */

    /* System message */
    {
        provider_t p = {0};
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are helpful."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("system role in messages",
             body && strstr(body, "\"role\":\"system\""));
        TEST("system content in messages",
             body && strstr(body, "\"content\":\"You are helpful.\""));
        free(body);
    }

    /* Tool call in assistant message */
    {
        provider_t p = {0};
        message_t asst_msg;
        memset(&asst_msg, 0, sizeof(asst_msg));
        asst_msg.role = MSG_ASSISTANT;
        asst_msg.content = (char*)"Searching...";
        tool_call_t tc;
        memset(&tc, 0, sizeof(tc));
        snprintf(tc.id, sizeof(tc.id), "call_123");
        snprintf(tc.name, sizeof(tc.name), "search");
        snprintf(tc.arguments, sizeof(tc.arguments), "{\"q\":\"test\"}");
        memcpy(&asst_msg.tool_calls[0], &tc, sizeof(tc));
        asst_msg.tool_calls_count = 1;
        const message_t *with_tc[] = {&asst_msg};
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, with_tc, 1, NULL, false);
        TEST("tool_calls in assistant message",
             body && strstr(body, "\"tool_calls\""));
        TEST("tool_call id", body && strstr(body, "\"call_123\""));
        TEST("tool_call function name",
             body && strstr(body, "\"name\":\"search\""));
        free(body);
    }

    /* Tools JSON in request */
    {
        provider_t p = {0};
        json_t *tools = json_new_array();
        json_t *tool = json_new_object();
        json_object_set(tool, "type", json_new_string("function"));
        json_t *func = json_new_object();
        json_object_set(func, "name", json_new_string("my_tool"));
        json_object_set(tool, "function", func);
        json_array_append(tools, tool);

        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs, 1, tools, false);
        TEST("tools in request body", body && strstr(body, "\"tools\""));
        TEST("tool function name in body",
             body && strstr(body, "\"my_tool\""));
        json_free(tools);
        free(body);
    }

    /* ──────────── Response parsing ──────────── */

    /* Text response */
    {
        provider_t p = {0};
        const char *resp =
            "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"Hello!\"}}],"
            "\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5}}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, resp);
        TEST("text response parsed", r != NULL);
        TEST("text response content",
             r && r->content && strcmp(r->content, "Hello!") == 0);
        TEST("text response input_tokens", r && r->input_tokens == 10);
        TEST("text response output_tokens", r && r->output_tokens == 5);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Tool calls response */
    {
        provider_t p = {0};
        const char *resp =
            "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\","
            "\"content\":\"Let me check.\","
            "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\","
            "\"function\":{\"name\":\"search\",\"arguments\":\"{\\\"q\\\":\\\"weather\\\"}\"}}]}}],"
            "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":10}}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, resp);
        TEST("tool_calls response parsed", r != NULL);
        TEST("tool_calls text content",
             r && r->content && strcmp(r->content, "Let me check.") == 0);
        TEST("tool_calls count", r && r->tool_calls_count == 1);
        TEST("tool_calls id",
             r && strcmp(r->tool_calls[0].id, "call_1") == 0);
        TEST("tool_calls name",
             r && strcmp(r->tool_calls[0].name, "search") == 0);
        TEST("tool_calls arguments has q",
             r && strstr(r->tool_calls[0].arguments, "\"weather\""));
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Error response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, "{\"error\":{\"message\":\"Rate limit\"}}");
        TEST("error response parsed", r != NULL);
        TEST("error content has Azure API error prefix",
             r && r->content && strstr(r->content, "Azure API error"));
        TEST("error content has rate limit text",
             r && r->content && strstr(r->content, "Rate limit"));
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* ──────────── Streaming ──────────── */

    /* Text delta */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, chunk);
        TEST("stream text delta parsed", r != NULL);
        TEST("stream text content",
             r && r->content && strcmp(r->content, "Hello") == 0);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* [DONE] marker */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, "data: [DONE]\n");
        TEST("stream [DONE] parsed", r != NULL);
        TEST("stream [DONE] empty content",
             r && r->content && strcmp(r->content, "") == 0);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Finish reason in stream */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, chunk);
        TEST("stream finish_reason parsed", r != NULL);
        TEST("stream finish_reason value",
             r && strcmp(r->finish_reason, "stop") == 0);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Non-data prefix passthrough */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, "event: ping\n");
        TEST("non-data prefix passthrough", r != NULL);
        TEST("non-data content passthrough",
             r && r->content && strstr(r->content, "event: ping"));
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* ──────────── Null safety ──────────── */

    /* free_response(NULL) */
    {
        PROVIDER_OPS_AZURE.free_response(NULL);
        TEST("free_response(NULL) no crash", 1);
    }

    /* Parse NULL response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, NULL);
        TEST("parse_response(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Parse empty response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, "");
        TEST("parse_response(empty) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Parse invalid JSON */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(&p, "{invalid}");
        TEST("parse_response(invalid JSON) returns non-NULL", r != NULL);
        TEST("parse_response invalid has error text",
             r && r->content && strstr(r->content, "JSON parse error"));
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    /* Stream NULL chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, NULL);
        TEST("parse_stream_chunk(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_AZURE.free_response(r);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}
