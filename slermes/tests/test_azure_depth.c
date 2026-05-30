/*
 * test_azure_depth.c — B37/B38/B43: Azure provider depth tests
 *
 * Tests: build_url (deployment_id, api_version), build_headers (API key),
 * build_request_body (generationConfig params, tools, messages),
 * parse_response (text, tool calls, error), parse_stream_chunk.
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
static const message_t *msgs1[] = {&user_msg};

int main(void) {
    printf("=== B37/B38/B43: Azure provider depth ===\n\n");

    /* ── build_url ───────────────────────────────────────────────── */
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

    /* B43: build_url edge cases */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_AZURE.build_url(&p, NULL);
        TEST("NULL base uses default", url && strstr(url, "openai.azure.com") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "");
        TEST("empty base uses default", url && strstr(url, "openai.azure.com") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o");
        char *url = PROVIDER_OPS_AZURE.build_url(&p, "https://res.openai.azure.com/");
        TEST("trailing slash stripped", url && strstr(url, "res.openai.azure.com/openai") != NULL);
        TEST("no double slash", url && strstr(url, "//openai") == NULL);
        free(url);
    }

    /* ── build_headers ───────────────────────────────────────────── */
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_AZURE.build_headers(&p, "sk-azure-test");
        TEST("headers api-key present", h && strstr(h, "api-key: sk-azure-test") != NULL);
        TEST("headers Content-Type", h && strstr(h, "Content-Type: application/json") != NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_AZURE.build_headers(&p, "");
        TEST("headers empty key no api-key", h && strstr(h, "api-key:") == NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_AZURE.build_headers(&p, NULL);
        TEST("headers NULL key valid", h != NULL);
        free(h);
    }

    /* ── build_request_body: streaming flag ───────────────────────── */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, true);
        TEST("streaming body stream=true", body && strstr(body, "\"stream\":true") != NULL);
        free(body);
    }
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("non-streaming body stream=false", body && strstr(body, "\"stream\":false") != NULL);
        free(body);
    }

    /* ── build_request_body: generation config ──────────────────────── */
    {
        provider_t p = {0};
        p.config.max_tokens = 0;
        p.config.temperature = -1.0f;
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("max_tokens default 4096", body && strstr(body, "\"max_tokens\":4096") != NULL);
        TEST("no temperature when negative", body && strstr(body, "\"temperature\"" )== NULL);
        free(body);
    }
    {
        provider_t p = {0};
        p.config.max_tokens = 8192;
        p.config.temperature = 0.5f;
        p.config.top_p = 0.9f;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "END");
        p.config.stop_count = 1;
        p.config.service_tier[0] = 'd'; /* "default" */
        p.config.service_tier[1] = '\0';
        p.config.presence_penalty = 0.1f;
        p.config.frequency_penalty = 0.2f;
        p.config.seed = 42;
        p.config.user[0] = 'u'; p.config.user[1] = '1'; p.config.user[2] = '\0';
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("max_tokens 8192", body && strstr(body, "\"max_tokens\":8192") != NULL);
        TEST("temperature 0.5", body && strstr(body, "\"temperature\":0.5") != NULL);
        TEST("top_p present", body && strstr(body, "\"top_p\":0.89") != NULL);
        TEST("stop sequences present", body && strstr(body, "\"stop\"") != NULL);
        TEST("stop END present", body && strstr(body, "\"END\"") != NULL);
        TEST("service_tier present", body && strstr(body, "\"service_tier\"") != NULL);
        TEST("presence_penalty present", body && strstr(body, "\"presence_penalty\"") != NULL);
        TEST("frequency_penalty present", body && strstr(body, "\"frequency_penalty\"") != NULL);
        TEST("seed 42", body && strstr(body, "\"seed\":42") != NULL);
        TEST("user present", body && strstr(body, "\"user\"") != NULL);
        free(body);
    }
    {
        provider_t p = {0};
        p.config.logprobs = true;
        p.config.top_logprobs = 5;
        p.config.n = 2;
        p.config.max_tool_calls = 10;
        p.config.reasoning_effort[0] = 'h'; p.config.reasoning_effort[1] = '\0';
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("logprobs true", body && strstr(body, "\"logprobs\":true") != NULL);
        TEST("top_logprobs 5", body && strstr(body, "\"top_logprobs\":5") != NULL);
        TEST("n 2", body && strstr(body, "\"n\":2") != NULL);
        TEST("max_tool_calls 10", body && strstr(body, "\"max_tool_calls\":10") != NULL);
        TEST("reasoning_effort present", body && strstr(body, "\"reasoning_effort\"") != NULL);
        free(body);
    }

    /* ── build_request_body: tools and messages ───────────────────── */
    {
        provider_t p = {0};
        json_t *tools = json_new_array();
        json_t *tool = json_new_object();
        json_t *func = json_new_object();
        json_object_set(func, "name", json_new_string("web_search"));
        json_object_set(func, "description", json_new_string("Search the web"));
        json_object_set(tool, "type", json_new_string("function"));
        json_object_set(tool, "function", func);
        json_array_append(tools, tool);
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, tools, false);
        TEST("tools array present", body && strstr(body, "\"tools\"") != NULL);
        TEST("tool name in body", body && strstr(body, "web_search") != NULL);
        TEST("tool type function", body && strstr(body, "\"type\":\"function\"") != NULL);
        json_free(tools);
        free(body);
    }
    {
        provider_t p = {0};
        message_t assist = {.role = MSG_ASSISTANT, .content = (char*)"I'll help", .tool_calls_count = 1};
        snprintf(assist.tool_calls[0].id, sizeof(assist.tool_calls[0].id), "call_123");
        snprintf(assist.tool_calls[0].name, sizeof(assist.tool_calls[0].name), "web_search");
        snprintf(assist.tool_calls[0].arguments, sizeof(assist.tool_calls[0].arguments), "{\"q\":\"test\"}");
        message_t tool_res = {.role = MSG_TOOL, .content = (char*)"result", .tool_call_id = (char*)"call_123"};
        const message_t *msgs_tc[] = {&user_msg, &assist, &tool_res};
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs_tc, 3, NULL, false);
        TEST("tool calls in body", body && strstr(body, "\"tool_calls\"") != NULL);
        TEST("tool call id", body && strstr(body, "call_123") != NULL);
        TEST("tool call name", body && strstr(body, "web_search") != NULL);
        TEST("tool call role", body && strstr(body, "\"role\":\"assistant\"") != NULL);
        TEST("tool result role", body && strstr(body, "\"role\":\"tool\"") != NULL);
        TEST("tool result content", body && strstr(body, "result") != NULL);
        TEST("tool_call_id in tool message", body && strstr(body, "\"tool_call_id\"") != NULL);
        free(body);
    }
    {
        provider_t p = {0};
        /* extra_body merge */
        p.config.extra_body[0] = '{';
        snprintf(p.config.extra_body, sizeof(p.config.extra_body), "{\"custom_field\":\"value\"}");
        char *body = PROVIDER_OPS_AZURE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("extra_body merged", body && strstr(body, "custom_field") != NULL);
        TEST("extra_body value present", body && strstr(body, "\"value\"") != NULL);
        free(body);
    }

    /* ── parse_response ──────────────────────────────────────────── */
    {
        /* Normal text response */
        const char *resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Hello Azure\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, resp);
        TEST("parse text content", r && r->content && strcmp(r->content, "Hello Azure") == 0);
        TEST("parse input tokens", r && r->input_tokens == 5);
        TEST("parse output tokens", r && r->output_tokens == 3);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Tool calls in response */
        const char *resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"call_abc\",\"type\":\"function\",\"function\":{\"name\":\"web_search\",\"arguments\":\"{\\\"q\\\":\\\"test\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, resp);
        TEST("parse tool calls count", r && r->tool_calls_count == 1);
        TEST("parse tool call name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("parse tool call id", r && strcmp(r->tool_calls[0].id, "call_abc") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Error response */
        const char *resp = "{\"error\":{\"message\":\"Deployment not found\",\"code\":\"DeploymentNotFound\"}}";
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, resp);
        TEST("parse error message", r && r->content && strstr(r->content, "Deployment not found") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Empty body */
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, "");
        TEST("parse empty body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Null body */
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, NULL);
        TEST("parse null body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Non-JSON body */
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_response(NULL, "not json");
        TEST("parse invalid json", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* ── parse_stream_chunk ──────────────────────────────────────── */
    {
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(NULL, NULL);
        TEST("stream null chunk", r && r->content && strlen(r->content) == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(NULL, "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}");
        TEST("stream data: delta", r && r->content && strcmp(r->content, "Hello") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(NULL, "data: [DONE]");
        TEST("stream done", r && r->content && strlen(r->content) == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(NULL, "data: {\"choices\":[{\"delta\":{\"content\":\"\"},\"finish_reason\":\"stop\"}]}");
        TEST("stream finish_reason present", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
