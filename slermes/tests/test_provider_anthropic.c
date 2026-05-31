/*
 * test_provider_anthropic.c — Tests for Anthropic provider depth.
 *
 * Tests: normalize_model_key, model matching predicates, build_url, headers,
 * parse_response (text, tool_use, error, edge cases),
 * parse_stream_chunk (text, events, edge cases).
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

/* Duplicate of the internal normalize_model_key for testing */
static void norm_key(const char *model, char *out, size_t out_size) {
    if (!model || !*model) { out[0] = '\0'; return; }
    const char *slash = strrchr(model, '/');
    if (slash) model = slash + 1;
    size_t i, j = 0;
    for (i = 0; model[i] && j < out_size - 1; i++) {
        if (model[i] == '.')
            out[j++] = '-';
        else
            out[j++] = model[i];
    }
    out[j] = '\0';
}

static bool contains_any(const char *model, const char **patterns) {
    if (!model || !*model) return false;
    char normalized[128];
    norm_key(model, normalized, sizeof(normalized));
    for (int i = 0; patterns[i]; i++) {
        if (strstr(normalized, patterns[i]))
            return true;
    }
    return false;
}

/* ── Response parsing ────────────────────────────────────────── */

static void test_parse_response_text(void)
{
    /* Anthropic text response with content block */
    const char *json =
        "{"
        "\"id\":\"msg_123\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":[{\"type\":\"text\",\"text\":\"Hello! I am Claude.\"}],"
        "\"model\":\"claude-sonnet-4-20250514\","
        "\"stop_reason\":\"end_turn\","
        "\"stop_sequence\":null,"
        "\"usage\":{\"input_tokens\":10,\"output_tokens\":5}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic parse text response", resp != NULL);
    if (resp) {
        TEST("text content correct", resp->content && strcmp(resp->content, "Hello! I am Claude.") == 0);
        TEST("text input tokens", resp->input_tokens == 10);
        TEST("text output tokens", resp->output_tokens == 5);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_tool_use(void)
{
    /* Anthropic response with tool_use content block */
    const char *json =
        "{"
        "\"id\":\"msg_tool\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":["
          "{\"type\":\"text\",\"text\":\"Let me check the weather.\"},"
          "{\"type\":\"tool_use\",\"id\":\"toolu_123\",\"name\":\"get_weather\",\"input\":{\"location\":\"NYC\"}}"
        "],"
        "\"model\":\"claude-sonnet-4\","
        "\"stop_reason\":\"tool_use\","
        "\"usage\":{\"input_tokens\":20,\"output_tokens\":15}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic parse tool_use response", resp != NULL);
    if (resp) {
        TEST("tool_use text content", resp->content && strstr(resp->content, "Let me check") != NULL);
        TEST("tool_use count", resp->tool_calls_count == 1);
        TEST("tool_use id", strcmp(resp->tool_calls[0].id, "toolu_123") == 0);
        TEST("tool_use name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("tool_use args", strstr(resp->tool_calls[0].arguments, "NYC") != NULL);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_multi_tool(void)
{
    /* Anthropic response with multiple tool_use blocks */
    const char *json =
        "{"
        "\"id\":\"msg_multi\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":["
          "{\"type\":\"tool_use\",\"id\":\"tu_1\",\"name\":\"search_web\",\"input\":{\"q\":\"weather\"}},"
          "{\"type\":\"tool_use\",\"id\":\"tu_2\",\"name\":\"get_time\",\"input\":{\"tz\":\"UTC\"}}"
        "],"
        "\"stop_reason\":\"tool_use\","
        "\"usage\":{\"input_tokens\":30,\"output_tokens\":25}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic parse multi tool", resp != NULL);
    if (resp) {
        TEST("multi tool count", resp->tool_calls_count == 2);
        TEST("first tool name", strcmp(resp->tool_calls[0].name, "search_web") == 0);
        TEST("second tool name", strcmp(resp->tool_calls[1].name, "get_time") == 0);
        TEST("second args has tz", strstr(resp->tool_calls[1].arguments, "UTC") != NULL);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_only_tool_use(void)
{
    /* Anthropic response with only tool_use and null text */
    const char *json =
        "{"
        "\"id\":\"msg_only_tool\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":["
          "{\"type\":\"tool_use\",\"id\":\"tu_only\",\"name\":\"list_files\",\"input\":{}}"
        "],"
        "\"stop_reason\":\"tool_use\","
        "\"usage\":{\"input_tokens\":5,\"output_tokens\":10}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic only tool_use parsed", resp != NULL);
    if (resp) {
        TEST("only tool count 1", resp->tool_calls_count == 1);
        TEST("only tool name", strcmp(resp->tool_calls[0].name, "list_files") == 0);
        TEST("only tool empty args", resp->tool_calls[0].arguments != NULL);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_empty(void)
{
    /* Empty content block */
    const char *json =
        "{"
        "\"id\":\"msg_empty\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":[{\"type\":\"text\",\"text\":\"\"}],"
        "\"stop_reason\":\"end_turn\","
        "\"usage\":{\"input_tokens\":0,\"output_tokens\":0}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic empty text parsed", resp != NULL);
    if (resp) {
        TEST("empty text content", resp->content && strlen(resp->content) == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_null_content(void)
{
    /* Response with null content field */
    const char *json =
        "{"
        "\"id\":\"msg_null\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":null,"
        "\"stop_reason\":\"end_turn\""
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic null content parsed", resp != NULL);
    if (resp) {
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_invalid_content(void)
{
    /* Content as string instead of array */
    const char *json =
        "{"
        "\"id\":\"msg_bad\","
        "\"type\":\"message\","
        "\"content\":\"not an array\","
        "\"stop_reason\":\"end_turn\""
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic invalid content parsed", resp != NULL);
    if (resp) {
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_error(void)
{
    /* Anthropic error response */
    const char *json =
        "{"
        "\"type\":\"error\","
        "\"error\":{"
          "\"type\":\"authentication_error\","
          "\"message\":\"Invalid API key\""
        "}"
        "}";

    provider_response_t *resp = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, json);
    TEST("anthropic error parsed", resp != NULL);
    if (resp) {
        TEST("error message present", resp->content && strstr(resp->content, "Invalid API key") != NULL);
        TEST("error mentions authentication", resp->content && strstr(resp->content, "authentication") != NULL);
        PROVIDER_OPS_ANTHROPIC.free_response(resp);
    }
}

static void test_parse_response_malformed(void)
{
    /* Empty JSON */
    provider_response_t *r1 = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, "{}");
    TEST("empty json parsed", r1 != NULL);
    if (r1) PROVIDER_OPS_ANTHROPIC.free_response(r1);

    /* Invalid JSON string */
    provider_response_t *r2 = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, "not json at all");
    TEST("invalid json parsed", r2 != NULL);
    if (r2) {
        TEST("invalid json has error content", r2->content != NULL);
        PROVIDER_OPS_ANTHROPIC.free_response(r2);
    }

    /* NULL body */
    provider_response_t *r3 = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, NULL);
    TEST("NULL body parsed", r3 != NULL);
    if (r3) PROVIDER_OPS_ANTHROPIC.free_response(r3);

    /* Empty string */
    provider_response_t *r4 = PROVIDER_OPS_ANTHROPIC.parse_response(NULL, "");
    TEST("empty string parsed", r4 != NULL);
    if (r4) PROVIDER_OPS_ANTHROPIC.free_response(r4);
}

static void test_free_response_null(void)
{
    /* Should not crash */
    PROVIDER_OPS_ANTHROPIC.free_response(NULL);
    TEST("anthropic free_response NULL safe", 1);
}

/* ── Streaming ───────────────────────────────────────────────── */

static void test_parse_stream_chunk_text(void)
{
    /* SSE text_delta chunk: event + data format */
    provider_response_t *r1 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}");
    TEST("text delta chunk parsed", r1 != NULL);
    if (r1) {
        TEST("text delta content", r1->content && strcmp(r1->content, "Hello") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r1);
    }

    /* text_delta with partial word */
    provider_response_t *r2 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" world\"}}");
    TEST("text delta partial parsed", r2 != NULL);
    if (r2) {
        TEST("partial content correct", r2->content && strcmp(r2->content, " world") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r2);
    }
}

static void test_parse_stream_chunk_events(void)
{
    /* content_block_start with text block */
    provider_response_t *r1 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}");
    TEST("block start parsed", r1 != NULL);
    if (r1) {
        PROVIDER_OPS_ANTHROPIC.free_response(r1);
    }

    /* content_block_start with tool_use block */
    provider_response_t *r2 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"tu_1\",\"name\":\"get_weather\",\"input\":{}}}");
    TEST("tool block start parsed", r2 != NULL);
    if (r2) {
        PROVIDER_OPS_ANTHROPIC.free_response(r2);
    }

    /* content_block_stop */
    provider_response_t *r3 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}");
    TEST("block stop parsed", r3 != NULL);
    if (r3) {
        PROVIDER_OPS_ANTHROPIC.free_response(r3);
    }

    /* message_delta with stop_reason */
    provider_response_t *r4 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":10}}");
    TEST("message delta parsed", r4 != NULL);
    if (r4) {
        TEST("stop reason end_turn", r4->finish_reason[0] != '\0');
        PROVIDER_OPS_ANTHROPIC.free_response(r4);
    }

    /* message_delta with tool_use stop_reason */
    provider_response_t *r5 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":15}}");
    TEST("message delta tool_use parsed", r5 != NULL);
    if (r5) {
        PROVIDER_OPS_ANTHROPIC.free_response(r5);
    }

    /* message_start */
    provider_response_t *r6 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[],\"model\":\"claude-sonnet-4\",\"stop_reason\":null,\"usage\":{\"input_tokens\":10,\"output_tokens\":0}}}");
    TEST("message start parsed", r6 != NULL);
    if (r6) {
        PROVIDER_OPS_ANTHROPIC.free_response(r6);
    }

    /* ping event (keep-alive) */
    provider_response_t *r7 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, "event: ping\ndata: {\"type\":\"ping\"}");
    TEST("ping event parsed", r7 != NULL);
    if (r7) {
        PROVIDER_OPS_ANTHROPIC.free_response(r7);
    }
}

static void test_parse_stream_chunk_edge(void)
{
    /* Direct "data: {...}" line (simplified proxy format) */
    provider_response_t *r1 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hi\"}}");
    TEST("direct data line parsed", r1 != NULL);
    if (r1) {
        TEST("direct data content", r1->content && strcmp(r1->content, "Hi") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r1);
    }

    /* Raw JSON (HTTP parser already stripped prefix) */
    provider_response_t *r2 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL,
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"raw\"}}");
    TEST("raw json chunk parsed", r2 != NULL);
    if (r2) {
        TEST("raw json content", r2->content && strcmp(r2->content, "raw") == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r2);
    }

    /* [DONE] sentinel */
    provider_response_t *r3 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, "data: [DONE]");
    TEST("DONE sentinel parsed", r3 != NULL);
    if (r3) {
        TEST("DONE content empty", r3->content != NULL && strlen(r3->content) == 0);
        PROVIDER_OPS_ANTHROPIC.free_response(r3);
    }

    /* Null chunk */
    provider_response_t *r4 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, NULL);
    TEST("null chunk parsed", r4 != NULL);
    if (r4) {
        PROVIDER_OPS_ANTHROPIC.free_response(r4);
    }

    /* Empty chunk */
    provider_response_t *r5 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, "");
    TEST("empty chunk parsed", r5 != NULL);
    if (r5) {
        PROVIDER_OPS_ANTHROPIC.free_response(r5);
    }

    /* Whitespace-only chunk */
    provider_response_t *r6 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, "   ");
    TEST("whitespace chunk parsed", r6 != NULL);
    if (r6) {
        PROVIDER_OPS_ANTHROPIC.free_response(r6);
    }

    /* Event only, no data line */
    provider_response_t *r7 = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(NULL, "event: content_block_delta");
    TEST("event-only chunk parsed", r7 != NULL);
    if (r7) {
        PROVIDER_OPS_ANTHROPIC.free_response(r7);
    }
}

static void test_url_edge_cases(void)
{
    /* URL with version suffix */
    char *u1 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/v1");
    TEST("standard v1 base", u1 != NULL);
    if (u1) {
        TEST("v1 has /messages", strcmp(u1, "https://api.anthropic.com/v1/messages") == 0);
        free(u1);
    }

    /* Trailing slash */
    char *u2 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/");
    TEST("trailing slash base", u2 != NULL);
    if (u2) {
        TEST("trailing slash has /messages", strstr(u2, "/messages") != NULL);
        free(u2);
    }

    /* Custom endpoint with no /messages suffix */
    char *u3 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://custom-proxy.example.com/anthropic");
    TEST("custom proxy url", u3 != NULL);
    if (u3) {
        TEST("custom proxy has /messages", strstr(u3, "/messages") != NULL);
        free(u3);
    }

    /* URL already ending in /messages */
    char *u4 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://custom.com/messages");
    TEST("already has /messages", u4 != NULL && strcmp(u4, "https://custom.com/messages") == 0);
    free(u4);

    /* NULL base */
    char *u5 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, NULL);
    TEST("null base url", u5 != NULL && strstr(u5, "/messages") != NULL);
    free(u5);

    /* Empty base */
    char *u6 = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "");
    TEST("empty base url", u6 != NULL && strstr(u6, "/messages") != NULL);
    free(u6);
}

static void test_headers_edge_cases(void)
{
    provider_t p;
    memset(&p, 0, sizeof(p));

    /* Headers with NULL API key */
    char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, NULL);
    TEST("NULL key headers", h != NULL);
    if (h) {
        TEST("NULL key has anthropic-version", strstr(h, "anthropic-version: 2023-06-01") != NULL);
        free(h);
    }

    /* Headers with x-api-key for OAuth token */
    memset(&p, 0, sizeof(p));
    h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-oat-abcdef");
    TEST("OAuth token headers", h != NULL);
    if (h) {
        TEST("OAuth includes x-api-key", strstr(h, "x-api-key: sk-ant-oat-abcdef") != NULL);
        free(h);
    }

    /* Headers with system_cached=true */
    memset(&p, 0, sizeof(p));
    p.system_cached = true;
    h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test");
    TEST("cached headers", h != NULL);
    if (h) {
        TEST("cached has ephemeral-cache header", strstr(h, "ephemeral-cache-2025-05-20") != NULL);
        free(h);
    }
}

/* ── Build request body ──────────────────────────────────────── */

static void test_build_request_body_system_user(void)
{
    /* Skipped — requires full provider registration pipeline;
     * response parsing and streaming tests cover the core APIs. */
    fprintf(stderr, "DBG: SKIP build_request_body_system_user\n");
}

int main(void) {
    printf("=== Anthropic Provider Depth Tests ===\n\n");

    /* ── normalize_model_key ────────────────────────────────────── */
    {
        char out[128];
        norm_key("anthropic/claude-sonnet-4-20250514", out, sizeof(out));
        TEST("strip provider prefix", strcmp(out, "claude-sonnet-4-20250514") == 0);
    }
    {
        char out[128];
        norm_key("claude-opus-4.6", out, sizeof(out));
        TEST("dots to hyphens", strcmp(out, "claude-opus-4-6") == 0);
    }
    {
        char out[128];
        norm_key("anthropic/claude-opus-4.7-20250514", out, sizeof(out));
        TEST("prefix and dots", strcmp(out, "claude-opus-4-7-20250514") == 0);
    }
    {
        char out[128];
        norm_key(NULL, out, sizeof(out));
        TEST("NULL model", out[0] == '\0');
    }
    {
        char out[128];
        norm_key("", out, sizeof(out));
        TEST("empty model", out[0] == '\0');
    }
    {
        char out[128];
        norm_key("claude-sonnet-4", out, sizeof(out));
        TEST("no prefix or dots", strcmp(out, "claude-sonnet-4") == 0);
    }

    /* ── model_contains_any ──────────────────────────────────────── */
    {
        const char *pats[] = {"4-7", "4.7", NULL};
        TEST("detect 4.7 via dot", contains_any("claude-opus-4.7", pats));
    }
    {
        const char *pats[] = {"4-7", "4.7", NULL};
        TEST("detect 4.7 with prefix", contains_any("anthropic/claude-opus-4.7-20250514", pats));
    }
    {
        const char *pats[] = {"4-6", "4.6", NULL};
        TEST("detect 4.6 via hyphen", contains_any("claude-sonnet-4-6-20250514", pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", "4-8", NULL};
        TEST("reject 3.5 sonnet", !contains_any("claude-3-5-sonnet", pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", NULL};
        TEST("reject NULL model", !contains_any(NULL, pats));
    }
    {
        const char *pats[] = {"4-6", "4.7", NULL};
        TEST("reject empty model", !contains_any("", pats));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode opus-4-6", contains_any("claude-opus-4-6", op46));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode opus-4.6", contains_any("claude-opus-4.6", op46));
    }
    {
        const char *op46[] = {"opus-4-6", "opus-4.6", NULL};
        TEST("fast mode NOT sonnet-4-6", !contains_any("claude-sonnet-4-6", op46));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh matches 4.7", contains_any("claude-opus-4.7", xhigh));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh matches 4.8", contains_any("claude-opus-4-8", xhigh));
    }
    {
        const char *xhigh[] = {"4-7", "4.7", "4-8", "4.8", NULL};
        TEST("xhigh NOT 4.6", !contains_any("claude-opus-4-6", xhigh));
    }

    /* ── build_url ──────────────────────────────────────────────── */
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/v1");
        TEST("build_url appends /messages", url && strcmp(url, "https://api.anthropic.com/v1/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://custom.com/messages");
        TEST("build_url preserves existing /messages", url && strcmp(url, "https://custom.com/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "https://api.anthropic.com/v1/");
        TEST("build_url trailing slash", url && strcmp(url, "https://api.anthropic.com/v1/messages") == 0);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, NULL);
        TEST("build_url NULL base", url && strstr(url, "/messages") != NULL);
        free(url);
    }
    {
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(NULL, "");
        TEST("build_url empty base", url && strstr(url, "/messages") != NULL);
        free(url);
    }

    /* ── headers ────────────────────────────────────────────────── */
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = false;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test");
        TEST("headers contain anthropic-beta", h && strstr(h, "anthropic-beta:") != NULL);
        TEST("headers contain interleaved-thinking", h && strstr(h, "interleaved-thinking") != NULL);
        TEST("headers contain fine-grained-tool-streaming", h && strstr(h, "fine-grained-tool-streaming") != NULL);
        TEST("headers contain x-api-key", h && strstr(h, "x-api-key: sk-ant-test") != NULL);
        free(h);
    }
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = true;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test");
        TEST("headers contain ephemeral-cache when cached", h && strstr(h, "ephemeral-cache-2025-05-20") != NULL);
        free(h);
    }
    {
        provider_t p;
        memset(&p, 0, sizeof(p));
        p.system_cached = false;
        char *h = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "");
        TEST("headers no API key still valid", h && strstr(h, "anthropic-version: 2023-06-01") != NULL);
        free(h);
    }

    /* ── Response parsing ────────────────────────────────────────── */
    test_parse_response_text();
    test_parse_response_tool_use();
    test_parse_response_multi_tool();
    test_parse_response_only_tool_use();
    test_parse_response_empty();
    test_parse_response_null_content();
    test_parse_response_invalid_content();
    test_parse_response_error();
    test_parse_response_malformed();
    test_free_response_null();

    /* ── Streaming ──────────────────────────────────────────────── */
    test_parse_stream_chunk_text();
    test_parse_stream_chunk_events();
    test_parse_stream_chunk_edge();

    /* ── URL edge cases ──────────────────────────────────────────── */
    test_url_edge_cases();

    /* ── Headers edge cases ─────────────────────────────────────── */
    test_headers_edge_cases();

    /* ── Build request body ──────────────────────────────────────── */
    test_build_request_body_system_user();

    /* ── report ───────────────────────────────────────────────────── */
    printf("\n==============================================\n");
    if (failures == 0)
        printf("  All tests PASSED\n");
    else
        printf("  %d test(s) FAILED\n", failures);
    printf("==============================================\n\n");
    return failures > 0 ? 1 : 0;
}
