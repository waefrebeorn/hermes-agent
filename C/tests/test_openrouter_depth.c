/*
 * test_openrouter_depth.c — B43-B48: OpenRouter provider depth tests
 *
 * Tests: build_url, build_headers, build_request_body (provider preferences,
 * genConfig, tools, tool_calls, extra_body), parse_response (text, error,
 * tool_calls, reasoning), parse_stream_chunk (delta, done).
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
static const message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are helpful"};

int main(void) {
    printf("=== B43-B48: OpenRouter provider depth ===\n\n");

    /* ── build_url edge cases ────────────────────────────────────── */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_OPENROUTER.build_url(&p, NULL);
        TEST("default url", url && strstr(url, "openrouter.ai") != NULL);
        TEST("default url has /chat/completions", url && strstr(url, "/chat/completions") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_OPENROUTER.build_url(&p, "https://custom.or.com/v1");
        TEST("custom base", url && strstr(url, "custom.or.com") != NULL);
        TEST("custom base has /chat/completions", url && strstr(url, "/chat/completions") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_OPENROUTER.build_url(&p, "https://custom.or.com/");
        TEST("trailing slash base", url && strstr(url, "/chat/completions") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_OPENROUTER.build_url(&p, "https://custom.or.com/v1/chat/completions");
        TEST("existing /chat/completions preserved", url && strcmp(url, "https://custom.or.com/v1/chat/completions") == 0);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_OPENROUTER.build_url(&p, "");
        TEST("empty base uses default", url && strstr(url, "openrouter.ai") != NULL);
        free(url);
    }

    /* ── build_headers ───────────────────────────────────────────── */
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_OPENROUTER.build_headers(&p, "sk-or-v1-test");
        TEST("headers Bearer token", h && strstr(h, "Authorization: Bearer sk-or-v1-test") != NULL);
        TEST("headers HTTP-Referer", h && strstr(h, "HTTP-Referer") != NULL);
        TEST("headers X-Title", h && strstr(h, "X-Title: Hermes-C") != NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_OPENROUTER.build_headers(&p, "");
        TEST("headers no key still has Referer", h && strstr(h, "HTTP-Referer") != NULL);
        TEST("headers no key still has X-Title", h && strstr(h, "X-Title: Hermes-C") != NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_OPENROUTER.build_headers(&p, NULL);
        TEST("headers NULL key valid", h != NULL);
        free(h);
    }

    /* ── build_request_body: model + stream ──────────────────────── */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "anthropic/claude-3.5-sonnet");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, true);
        TEST("model in body", body && strstr(body, "claude-3.5-sonnet") != NULL);
        TEST("stream true", body && strstr(body, "\"stream\":true") != NULL);
        free(body);
    }
    {
        provider_t p = {0};
        p.model[0] = '\0';
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("empty model uses gpt-4o-mini", body && strstr(body, "gpt-4o-mini") != NULL);
        TEST("stream false", body && strstr(body, "\"stream\":false") != NULL);
        free(body);
    }

    /* ── build_request_body: genConfig ──────────────────────────── */
    {
        provider_t p = {0};
        p.config.max_tokens = 0;
        p.config.temperature = -1.0f;
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("max_tokens default 4096", body && strstr(body, "\"max_tokens\":4096") != NULL);
        TEST("no temperature when negative", body && strstr(body, "\"temperature\"" )== NULL);
        free(body);
    }
    {
        provider_t p = {0};
        p.config.max_tokens = 16384;
        p.config.temperature = 0.7f;
        p.config.top_p = 0.95f;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "END");
        p.config.stop_count = 1;
        p.config.presence_penalty = 0.1f;
        p.config.frequency_penalty = 0.2f;
        p.config.seed = 42;
        p.config.service_tier[0] = 'd';
        p.config.reasoning_effort[0] = 'h';
        p.config.user[0] = 'u'; p.config.user[1] = '1';
        p.config.logprobs = true;
        p.config.top_logprobs = 5;
        p.config.n = 2;
        p.config.max_tool_calls = 10;
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("max_tokens 16384", body && strstr(body, "\"max_tokens\":16384") != NULL);
        TEST("temperature 0.7", body && strstr(body, "\"temperature\":0.69") != NULL);
        TEST("top_p present", body && strstr(body, "\"top_p\":0.94") != NULL);
        TEST("stop sequences present", body && strstr(body, "\"stop\"") != NULL);
        TEST("presence_penalty", body && strstr(body, "\"presence_penalty\"") != NULL);
        TEST("frequency_penalty", body && strstr(body, "\"frequency_penalty\"") != NULL);
        TEST("seed 42", body && strstr(body, "\"seed\":42") != NULL);
        TEST("service_tier", body && strstr(body, "\"service_tier\"") != NULL);
        TEST("reasoning_effort", body && strstr(body, "\"reasoning_effort\"") != NULL);
        TEST("user", body && strstr(body, "\"user\"") != NULL);
        TEST("logprobs true", body && strstr(body, "\"logprobs\":true") != NULL);
        TEST("top_logprobs 5", body && strstr(body, "\"top_logprobs\":5") != NULL);
        TEST("n 2", body && strstr(body, "\"n\":2") != NULL);
        TEST("max_tool_calls 10", body && strstr(body, "\"max_tool_calls\":10") != NULL);
        free(body);
    }

    /* ── build_request_body: system message ──────────────────────── */
    {
        provider_t p = {0};
        const message_t *msgs_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs_sys, 2, NULL, false);
        TEST("system message role", body && strstr(body, "\"role\":\"system\"") != NULL);
        TEST("system message content", body && strstr(body, "You are helpful") != NULL);
        free(body);
    }

    /* ── build_request_body: tools + tool_calls ──────────────────── */
    {
        provider_t p = {0};
        json_t *tools = json_new_array();
        json_t *tool = json_new_object();
        json_t *func = json_new_object();
        json_object_set(func, "name", json_new_string("web_search"));
        json_object_set(tool, "type", json_new_string("function"));
        json_object_set(tool, "function", func);
        json_array_append(tools, tool);
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, tools, false);
        TEST("tools array", body && strstr(body, "\"tools\"") != NULL);
        TEST("tool type function", body && strstr(body, "\"type\":\"function\"") != NULL);
        TEST("tool name", body && strstr(body, "web_search") != NULL);
        json_free(tools);
        free(body);
    }
    {
        provider_t p = {0};
        message_t assist = {.role = MSG_ASSISTANT, .content = (char*)"Let me search", .tool_calls_count = 1};
        snprintf(assist.tool_calls[0].id, sizeof(assist.tool_calls[0].id), "call_1");
        snprintf(assist.tool_calls[0].name, sizeof(assist.tool_calls[0].name), "web_search");
        snprintf(assist.tool_calls[0].arguments, sizeof(assist.tool_calls[0].arguments), "{\"q\":\"test\"}");
        message_t tool_res = {.role = MSG_TOOL, .content = (char*)"results", .tool_call_id = (char*)"call_1"};
        const message_t *msgs_tc[] = {&user_msg, &assist, &tool_res};
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs_tc, 3, NULL, false);
        TEST("tool_calls in body", body && strstr(body, "\"tool_calls\"") != NULL);
        TEST("tool_call_id", body && strstr(body, "call_1") != NULL);
        TEST("tool name in body", body && strstr(body, "web_search") != NULL);
        TEST("tool result role", body && strstr(body, "\"role\":\"tool\"") != NULL);
        free(body);
    }

    /* B43-B46: provider preferences — existing tests preserved */
    {
        provider_t p = {0};
        p.config.openrouter_provider[0] = '\0';
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("empty provider field omitted", body && !strstr(body, "\"provider\""));
        free(body);
    }
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"order\":[\"Anthropic\",\"OpenAI\"],\"allow_fallbacks\":true}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("provider order present", body && strstr(body, "\"order\""));
        TEST("provider Anthropic in order", body && strstr(body, "Anthropic"));
        TEST("allow_fallbacks true", body && strstr(body, "allow_fallbacks"));
        free(body);
    }
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"data_control\":\"allow\"}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("data_control allow", body && strstr(body, "\"data_control\":\"allow\""));
        free(body);
    }
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"route\":\"fixed\"}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("route fixed", body && strstr(body, "\"route\":\"fixed\""));
        free(body);
    }
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider),
                 "{\"ignore\":[\"Amazon Bedrock\",\"Mancer\"]}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("ignore list", body && strstr(body, "\"ignore\""));
        TEST("ignore Bedrock", body && strstr(body, "Amazon Bedrock"));
        TEST("ignore Mancer", body && strstr(body, "Mancer"));
        free(body);
    }
    {
        provider_t p = {0};
        snprintf(p.config.openrouter_provider, sizeof(p.config.openrouter_provider), "not-json");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("non-JSON provider dropped", body && !strstr(body, "not-json"));
        free(body);
    }

    /* B48: extra_body merge */
    {
        provider_t p = {0};
        snprintf(p.config.extra_body, sizeof(p.config.extra_body), "{\"custom_field\":\"value\"}");
        char *body = PROVIDER_OPS_OPENROUTER.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("extra_body merged", body && strstr(body, "custom_field") != NULL);
        TEST("extra_body value", body && strstr(body, "\"value\"") != NULL);
        free(body);
    }

    /* ── parse_response ──────────────────────────────────────────── */
    {
        const char *resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Hello from OR\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, resp);
        TEST("parse text content", r && r->content && strcmp(r->content, "Hello from OR") == 0);
        TEST("parse input tokens", r && r->input_tokens == 5);
        TEST("parse output tokens", r && r->output_tokens == 3);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        const char *resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"call_abc\",\"type\":\"function\",\"function\":{\"name\":\"web_search\",\"arguments\":\"{\\\"q\\\":\\\"test\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, resp);
        TEST("parse tool calls count", r && r->tool_calls_count == 1);
        TEST("parse tool call name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("parse tool call id", r && strcmp(r->tool_calls[0].id, "call_abc") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        const char *resp = "{\"error\":{\"message\":\"Insufficient credits\",\"code\":\"429\"}}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, resp);
        TEST("parse error", r && r->content && strstr(r->content, "Insufficient credits") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        const char *resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"With reasoning\",\"reasoning_content\":\"Step by step...\"},\"finish_reason\":\"stop\"}]}";
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, resp);
        TEST("parse reasoning", r && r->reasoning && strcmp(r->reasoning, "Step by step...") == 0);
        TEST("parse content with reasoning", r && r->content && strcmp(r->content, "With reasoning") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, NULL);
        TEST("parse null body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, "");
        TEST("parse empty body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_response(NULL, "not json");
        TEST("parse invalid json", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* ── parse_stream_chunk ──────────────────────────────────────── */
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(NULL, NULL);
        TEST("stream null chunk", r && r->content && strlen(r->content) == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(NULL, "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}");
        TEST("stream delta", r && r->content && strcmp(r->content, "Hello") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(NULL, "data: [DONE]");
        TEST("stream done", r && r->content && strlen(r->content) == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(NULL, "[DONE]");
        TEST("stream raw DONE", r && r->content && strlen(r->content) == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        provider_response_t *r = PROVIDER_OPS_OPENROUTER.parse_stream_chunk(NULL, "data: {\"choices\":[{\"delta\":{\"content\":\"\"},\"finish_reason\":\"stop\"}]}");
        TEST("stream finish reason", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
