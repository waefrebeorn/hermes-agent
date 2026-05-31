/*
 * test_anthropic_depth.c — B26-B28: Anthropic provider depth tests
 *
 * Tests: URL/headers building, request body (thinking, cache control,
 * tool conversion, system message extraction), response parsing,
 * streaming, error handling.
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

/* Helper: build a simple tools_json with one function */
static json_t *make_tools(void) {
    json_t *tools = json_new_array();
    json_t *tool = json_new_object();
    json_t *func = json_new_object();
    json_object_set(func, "name", json_new_string("test_tool"));
    json_object_set(func, "description", json_new_string("A test tool"));
    json_t *params = json_new_object();
    json_object_set(params, "type", json_new_string("object"));
    json_t *props = json_new_object();
    json_t *arg = json_new_object();
    json_object_set(arg, "type", json_new_string("string"));
    json_object_set(props, "input", arg);
    json_object_set(params, "properties", props);
    json_object_set(func, "parameters", params);
    json_object_set(tool, "type", json_new_string("function"));
    json_object_set(tool, "function", func);
    json_array_append(tools, tool);
    return tools;
}

static int test_anthropic_depth_main(void) {
    printf("=== Anthropic provider depth tests ===\n\n");

    /* ──────────── URL building ──────────── */

    /* Default URL */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(&p, NULL);
        TEST("default URL is api.anthropic.com/v1/messages",
             url && strstr(url, "api.anthropic.com/v1/messages"));
        free(url);
    }

    /* Custom base URL without /messages */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(&p, "https://custom.anthropic.com/v1");
        TEST("custom URL appends /messages",
             url && strcmp(url, "https://custom.anthropic.com/v1/messages") == 0);
        free(url);
    }

    /* Base URL with trailing slash */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(&p, "https://custom.anthropic.com/v1/");
        TEST("trailing slash URL appends messages",
             url && strcmp(url, "https://custom.anthropic.com/v1/messages") == 0);
        free(url);
    }

    /* URL already includes /messages */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_ANTHROPIC.build_url(&p, "https://custom.anthropic.com/v1/messages");
        TEST("URL with /messages used as-is",
             url && strcmp(url, "https://custom.anthropic.com/v1/messages") == 0);
        free(url);
    }

    /* ──────────── Headers ──────────── */

    /* Headers with API key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_ANTHROPIC.build_headers(&p, "sk-ant-test123");
        TEST("headers contain x-api-key", hdr && strstr(hdr, "x-api-key: sk-ant-test123"));
        TEST("headers contain anthropic-version", hdr && strstr(hdr, "anthropic-version: 2023-06-01"));
        TEST("headers contain Content-Type", hdr && strstr(hdr, "Content-Type: application/json"));
        free(hdr);
    }

    /* Headers without API key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_ANTHROPIC.build_headers(&p, NULL);
        TEST("headers without API key still have version",
             hdr && strstr(hdr, "anthropic-version: 2023-06-01"));
        TEST("no Authorization when no key", hdr && !strstr(hdr, "Authorization"));
        free(hdr);
    }

    /* ──────────── Request body — basic params ──────────── */

    /* Default model */
    {
        provider_t p = {0};
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("default model is claude-sonnet-4",
             body && strstr(body, "claude-sonnet-4-20250514"));
        free(body);
    }

    /* Custom model */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "claude-opus-4-20250514");
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("custom model used",
             body && strstr(body, "claude-opus-4-20250514"));
        free(body);
    }

    /* max_tokens in body */
    {
        provider_t p = {0};
        p.config.max_tokens = 8192;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("max_tokens in body", body && strstr(body, "\"max_tokens\":8192"));
        free(body);
    }

    /* temperature */
    {
        provider_t p = {0};
        p.config.temperature = 0.7f;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature in body", body && strstr(body, "\"temperature\":0.699"));
        free(body);
    }

    /* temperature=0.0 included */
    {
        provider_t p = {0};
        p.config.temperature = 0.0f;
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature=0.0 included", body && strstr(body, "\"temperature\":0"));
        free(body);
    }

    /* top_p */
    {
        provider_t p = {0};
        p.config.top_p = 0.9f;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("top_p in body", body && strstr(body, "\"top_p\":0.899"));
        free(body);
    }

    /* stop sequences */
    {
        provider_t p = {0};
        p.config.stop_count = 2;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "\\n\\n");
        snprintf(p.config.stop_sequences[1], sizeof(p.config.stop_sequences[1]), "Human:");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("stop_sequences in body",
             body && strstr(body, "\"stop_sequences\""));
        free(body);
    }

    /* ──────────── B26: Thinking blocks ──────────── */

    /* reasoning_effort = low → budget_tokens 8192 */
    {
        provider_t p = {0};
        snprintf(p.config.reasoning_effort, sizeof(p.config.reasoning_effort), "low");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("thinking enabled for low effort",
             body && strstr(body, "\"thinking\""));
        TEST("thinking budget 8192 for low",
             body && strstr(body, "\"budget_tokens\":8192"));
        free(body);
    }

    /* reasoning_effort = high → budget_tokens 32768 */
    {
        provider_t p = {0};
        snprintf(p.config.reasoning_effort, sizeof(p.config.reasoning_effort), "high");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("thinking budget 32768 for high",
             body && strstr(body, "\"budget_tokens\":32768"));
        free(body);
    }

    /* Direct number budget */
    {
        provider_t p = {0};
        snprintf(p.config.reasoning_effort, sizeof(p.config.reasoning_effort), "50000");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("thinking budget 50000 for direct number",
             body && strstr(body, "\"budget_tokens\":50000"));
        free(body);
    }

    /* Empty reasoning_effort → no thinking field */
    {
        provider_t p = {0};
        p.config.reasoning_effort[0] = '\0';
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("no thinking when effort empty",
             body && !strstr(body, "\"thinking\""));
        free(body);
    }

    /* ──────────── B27: Cache control ──────────── */

    /* Cache control on last message when tools present */
    {
        provider_t p = {0};
        json_t *tools = make_tools();
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, tools, false);
        TEST("cache_control on last user content block when tools present",
             body && strstr(body, "\"cache_control\""));
        TEST("cache_control type ephemeral",
             body && strstr(body, "\"type\":\"ephemeral\""));
        json_free(tools);
        free(body);
    }

    /* Cache control on system prompt on first call */
    {
        provider_t p = {0};
        p.system_cached = false; /* first call */
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are a helpful assistant."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("system cache_control on first call",
             body && strstr(body, "\"cache_control\""));
        free(body);
    }

    /* No cache_control on system prompt when already cached */
    {
        provider_t p = {0};
        p.system_cached = true; /* subsequent call */
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are a helpful assistant."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("plain system string when already cached",
             body && strstr(body, "\"system\":\"You are a helpful assistant.\""));
        TEST("system is not a cache_control array",
             body && !strstr(body, "\"system\":["));
        free(body);
    }

    /* ──────────── System message extraction ──────────── */

    /* System messages extracted to top-level "system" */
    {
        provider_t p = {0};
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are helpful."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("system message extracted to top-level",
             body && strstr(body, "\"system\"") && strstr(body, "\"You are helpful.\""));
        TEST("user message still in messages array",
             body && strstr(body, "\"role\":\"user\""));
        TEST("no system role in messages",
             body && !strstr(body, "\"role\":\"system\""));
        free(body);
    }

    /* Multiple system messages concatenated */
    {
        provider_t p = {0};
        message_t sys1 = {.role = MSG_SYSTEM, .content = (char*)"Part1."};
        message_t sys2 = {.role = MSG_SYSTEM, .content = (char*)"Part2."};
        const message_t *with_sys[] = {&sys1, &sys2, &user_msg};
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, with_sys, 3, NULL, false);
        TEST("concatenated system messages",
             body && strstr(body, "Part1.Part2."));
        free(body);
    }

    /* ──────────── Tool format conversion ──────────── */

    /* Tools converted from OpenAI to Anthropic format */
    {
        provider_t p = {0};
        json_t *tools = make_tools();
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, tools, false);
        TEST("tools in body", body && strstr(body, "\"tools\""));
        TEST("has input_schema (not parameters)", body && strstr(body, "\"input_schema\""));
        TEST("no type:function wrapper", body && !strstr(body, "\"type\":\"function\""));
        TEST("tool name preserved", body && strstr(body, "\"test_tool\""));
        json_free(tools);
        free(body);
    }

    /* ──────────── Streaming ──────────── */

    /* message_start event */
    {
        provider_t p = {0};
        const char *chunk =
            "event: message_start\n"
            "data: {\"type\":\"message_start\",\"message\":{\"usage\":{\"input_tokens\":15}}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("message_start parsed", r != NULL);
        TEST("message_start input_tokens", r && r->input_tokens == 15);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* content_block_delta — text_delta */
    {
        provider_t p = {0};
        const char *chunk =
            "event: content_block_delta\n"
            "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("text_delta parsed", r != NULL);
        TEST("text_delta content", r && r->content && strcmp(r->content, "Hello") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* content_block_delta — thinking_delta (B26) */
    {
        provider_t p = {0};
        const char *chunk =
            "event: content_block_delta\n"
            "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\",\"thinking\":\"Let me think...\"}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("thinking_delta parsed", r != NULL);
        TEST("thinking_delta reasoning content",
             r && r->reasoning && strcmp(r->reasoning, "Let me think...") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* content_block_start — text block */
    {
        provider_t p = {0};
        const char *chunk =
            "event: content_block_start\n"
            "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"Initial\"}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("content_block_start text parsed", r != NULL);
        TEST("content_block_start text content",
             r && r->content && strcmp(r->content, "Initial") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* message_delta — stop_reason */
    {
        provider_t p = {0};
        const char *chunk =
            "event: message_delta\n"
            "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":42}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("message_delta parsed", r != NULL);
        TEST("message_delta output_tokens", r && r->output_tokens == 42);
        TEST("message_delta finish_reason", r && strcmp(r->finish_reason, "end_turn") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Direct data: format (simplified proxy) */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Direct\"}}\n";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, chunk);
        TEST("direct data: format", r != NULL);
        TEST("direct data: content", r && r->content && strcmp(r->content, "Direct") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* ──────────── Response parsing ──────────── */

    /* Text-only response */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"content\":[{\"type\":\"text\",\"text\":\"Hello! How can I help?\"}],"
            "\"id\":\"msg_123\",\"model\":\"claude-sonnet-4-20250514\","
            "\"role\":\"assistant\",\"stop_reason\":\"end_turn\","
            "\"type\":\"message\","
            "\"usage\":{\"input_tokens\":10,\"output_tokens\":8}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, resp_json);
        TEST("text response parsed", r != NULL);
        TEST("text response content",
             r && r->content && strcmp(r->content, "Hello! How can I help?") == 0);
        TEST("text response input_tokens", r && r->input_tokens == 10);
        TEST("text response output_tokens", r && r->output_tokens == 8);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Response with thinking/reasoning */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"content\":["
            "{\"type\":\"thinking\",\"thinking\":\"Let me analyze this.\"},"
            "{\"type\":\"text\",\"text\":\"The answer is 42.\"}],"
            "\"id\":\"msg_456\",\"model\":\"claude-sonnet-4-20250514\","
            "\"role\":\"assistant\",\"stop_reason\":\"end_turn\","
            "\"type\":\"message\","
            "\"usage\":{\"input_tokens\":15,\"output_tokens\":50}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, resp_json);
        TEST("thinking response parsed", r != NULL);
        TEST("thinking response has content",
             r && r->content && strcmp(r->content, "The answer is 42.") == 0);
        TEST("thinking response has reasoning",
             r && r->reasoning && strcmp(r->reasoning, "Let me analyze this.") == 0);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Response with tool_use */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"content\":["
            "{\"type\":\"text\",\"text\":\"Let me search.\"},"
            "{\"type\":\"tool_use\",\"id\":\"toolu_123\",\"name\":\"web_search\",\"input\":{\"query\":\"weather\"}}],"
            "\"id\":\"msg_789\",\"model\":\"claude-sonnet-4-20250514\","
            "\"role\":\"assistant\",\"stop_reason\":\"tool_use\","
            "\"type\":\"message\","
            "\"usage\":{\"input_tokens\":20,\"output_tokens\":15}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, resp_json);
        TEST("tool_use response parsed", r != NULL);
        TEST("tool_use has text content",
             r && r->content && strcmp(r->content, "Let me search.") == 0);
        TEST("tool_use has tool calls", r && r->tool_calls_count == 1);
        TEST("tool_use call id", r && strcmp(r->tool_calls[0].id, "toolu_123") == 0);
        TEST("tool_use call name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("tool_use call arguments has query",
             r && strstr(r->tool_calls[0].arguments, "\"weather\""));
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Error response */
    {
        provider_t p = {0};
        const char *err_json =
            "{\"error\":{\"type\":\"authentication_error\",\"message\":\"Invalid API key\"}}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, err_json);
        TEST("error response parsed", r != NULL);
        TEST("error message includes type",
             r && r->content && strstr(r->content, "authentication_error"));
        TEST("error message includes text",
             r && r->content && strstr(r->content, "Invalid API key"));
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Empty content blocks */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"content\":[],\"id\":\"msg_empty\",\"model\":\"claude-sonnet-4-20250514\","
            "\"role\":\"assistant\",\"stop_reason\":\"end_turn\",\"type\":\"message\"}";
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, resp_json);
        TEST("empty content parsed", r != NULL);
        TEST("empty content is NULL or empty",
             r && (!r->content || r->content[0] == '\0'));
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* ──────────── Null safety ──────────── */

    /* free_response with NULL */
    {
        PROVIDER_OPS_ANTHROPIC.free_response(NULL);
        TEST("free_response(NULL) no crash", 1);
    }

    /* Parse NULL response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, NULL);
        TEST("parse_response(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Parse empty response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, "");
        TEST("parse_response(empty) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Parse invalid JSON */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, "{invalid}");
        TEST("parse_response(invalid JSON) returns non-NULL", r != NULL);
        TEST("parse_response invalid has empty content",
             r && r->content && r->content[0] == '\0');
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Stream NULL chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, NULL);
        TEST("parse_stream_chunk(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* Stream empty chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, "");
        TEST("parse_stream_chunk(empty) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
    }

    /* ──────────── Response format + json_mode ──────────── */

    /* response_format in body */
    {
        provider_t p = {0};
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_object\"}");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("response_format in body", body && strstr(body, "\"response_format\""));
        TEST("response_format type json_object",
             body && strstr(body, "\"type\":\"json_object\""));
        free(body);
    }

    /* json_mode sets json_object */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("json_mode sets response_format",
             body && strstr(body, "\"type\":\"json_object\""));
        free(body);
    }

    /* tool_choice in body */
    {
        provider_t p = {0};
        snprintf(p.config.tool_choice, sizeof(p.config.tool_choice), "any");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("tool_choice in body", body && strstr(body, "\"tool_choice\":\"any\""));
        free(body);
    }

    /* parallel_tool_calls → disable_parallel_tool_use */
    {
        provider_t p = {0};
        p.config.parallel_tool_calls = false;
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("parallel_tool_calls=false sets disable_parallel_tool_use",
             body && strstr(body, "\"disable_parallel_tool_use\":true"));
        free(body);
    }

    /* extra_body merged */
    {
        provider_t p = {0};
        snprintf(p.config.extra_body, sizeof(p.config.extra_body),
                 "{\"metadata\":{\"user_id\":\"abc\"},\"custom_field\":42}");
        char *body = PROVIDER_OPS_ANTHROPIC.build_request_body(&p, msgs, 1, NULL, false);
        TEST("extra_body merged into root",
             body && strstr(body, "\"user_id\":\"abc\""));
        TEST("extra_body custom_field",
             body && strstr(body, "\"custom_field\":42"));
        free(body);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}

/* ──────────── Anthropic endpoint detection utility tests ──────────── */

static int test_endpoint_detection(void) {
    int f = 0;
#define TEST_ED(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        f++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

    printf("\n=== Anthropic endpoint detection tests ===\n\n");

    /* anthropic_is_oauth_token */
    {
        TEST_ED("oauth_token: NULL returns false", !anthropic_is_oauth_token(NULL));
        TEST_ED("oauth_token: empty returns false", !anthropic_is_oauth_token(""));
        TEST_ED("oauth_token: sk-ant-api key returns false", !anthropic_is_oauth_token("sk-ant-api03-***"));
        TEST_ED("oauth_token: sk-ant-oa setup token returns true", anthropic_is_oauth_token("sk-ant-oat-***"));
        TEST_ED("oauth_token: sk-ant-managed returns true", anthropic_is_oauth_token("sk-ant-managed-***"));
        TEST_ED("oauth_token: JWT returns true", anthropic_is_oauth_token("eyJhbGciOiJSUzI1NiJ9.***"));
        TEST_ED("oauth_token: cc- prefix returns true", anthropic_is_oauth_token("cc-***"));
        TEST_ED("oauth_token: random string returns false", !anthropic_is_oauth_token("random"));
    }

    /* anthropic_normalize_base_url_text */
    {
        char *r;
        r = anthropic_normalize_base_url_text(NULL);
        TEST_ED("norm_base_url: NULL returns empty", r && *r == '\0');
        free(r);
        r = anthropic_normalize_base_url_text("");
        TEST_ED("norm_base_url: empty returns empty", r && *r == '\0');
        free(r);
        r = anthropic_normalize_base_url_text("  https://api.anthropic.com  ");
        TEST_ED("norm_base_url: strips whitespace", r && strcmp(r, "https://api.anthropic.com") == 0);
        free(r);
        r = anthropic_normalize_base_url_text("https://api.anthropic.com");
        TEST_ED("norm_base_url: passthrough", r && strcmp(r, "https://api.anthropic.com") == 0);
        free(r);
    }

    /* anthropic_is_third_party_endpoint */
    {
        TEST_ED("third_party: NULL returns false", !anthropic_is_third_party_endpoint(NULL));
        TEST_ED("third_party: anthropic.com returns false", !anthropic_is_third_party_endpoint("https://api.anthropic.com"));
        TEST_ED("third_party: custom URL returns true", anthropic_is_third_party_endpoint("https://custom.proxy.com"));
        TEST_ED("third_party: minimax returns true", anthropic_is_third_party_endpoint("https://api.minimax.io/anthropic"));
    }

    /* anthropic_is_kimi_coding_endpoint */
    {
        TEST_ED("kimi_coding: NULL returns false", !anthropic_is_kimi_coding_endpoint(NULL));
        TEST_ED("kimi_coding: matching URL", anthropic_is_kimi_coding_endpoint("https://api.kimi.com/coding"));
        TEST_ED("kimi_coding: with trailing slash", anthropic_is_kimi_coding_endpoint("https://api.kimi.com/coding/"));
        TEST_ED("kimi_coding: non-matching URL", !anthropic_is_kimi_coding_endpoint("https://api.anthropic.com"));
    }

    /* anthropic_model_name_is_kimi_family */
    {
        TEST_ED("kimi_model: NULL returns false", !anthropic_model_name_is_kimi_family(NULL));
        TEST_ED("kimi_model: empty returns false", !anthropic_model_name_is_kimi_family(""));
        TEST_ED("kimi_model: kimi-k2.5", anthropic_model_name_is_kimi_family("kimi-k2.5"));
        TEST_ED("kimi_model: moonshot-v1-8k", anthropic_model_name_is_kimi_family("moonshot-v1-8k"));
        TEST_ED("kimi_model: k2-thinking", anthropic_model_name_is_kimi_family("k2-thinking"));
        TEST_ED("kimi_model: with vendor prefix", anthropic_model_name_is_kimi_family("moonshotai/kimi-k2.5"));
        TEST_ED("kimi_model: claude not kimi", !anthropic_model_name_is_kimi_family("claude-sonnet-4"));
        TEST_ED("kimi_model: llama not kimi", !anthropic_model_name_is_kimi_family("llama-3-70b"));
    }

    /* anthropic_is_kimi_family_endpoint */
    {
        TEST_ED("kimi_endpoint: NULL base NULL model", !anthropic_is_kimi_family_endpoint(NULL, NULL));
        TEST_ED("kimi_endpoint: coding URL", anthropic_is_kimi_family_endpoint("https://api.kimi.com/coding", NULL));
        TEST_ED("kimi_endpoint: api.kimi.com domain", anthropic_is_kimi_family_endpoint("https://api.kimi.com/v1/chat", NULL));
        TEST_ED("kimi_endpoint: model name fallback", anthropic_is_kimi_family_endpoint("https://custom.gateway.com", "kimi-k2.5"));
        TEST_ED("kimi_endpoint: non-kimi", !anthropic_is_kimi_family_endpoint("https://api.anthropic.com", "claude-sonnet-4"));
    }

    /* anthropic_is_deepseek_endpoint */
    {
        TEST_ED("deepseek: NULL returns false", !anthropic_is_deepseek_endpoint(NULL));
        TEST_ED("deepseek: matching URL", anthropic_is_deepseek_endpoint("https://api.deepseek.com/anthropic"));
        TEST_ED("deepseek: with trailing slash", anthropic_is_deepseek_endpoint("https://api.deepseek.com/anthropic/"));
        TEST_ED("deepseek: wrong host", !anthropic_is_deepseek_endpoint("https://api.anthropic.com"));
        TEST_ED("deepseek: wrong path", !anthropic_is_deepseek_endpoint("https://api.deepseek.com/chat"));
    }

    /* anthropic_requires_bearer_auth */
    {
        TEST_ED("bearer: NULL returns false", !anthropic_requires_bearer_auth(NULL));
        TEST_ED("bearer: minimax.io", anthropic_requires_bearer_auth("https://api.minimax.io/anthropic"));
        TEST_ED("bearer: minimaxi.com", anthropic_requires_bearer_auth("https://api.minimaxi.com/anthropic"));
        TEST_ED("bearer: azure.com", anthropic_requires_bearer_auth("https://my-account.services.ai.azure.com/anthropic"));
        TEST_ED("bearer: native anthropic", !anthropic_requires_bearer_auth("https://api.anthropic.com"));
    }

    /* anthropic_base_url_needs_1m_beta */
    {
        TEST_ED("1m_beta: NULL returns false", !anthropic_base_url_needs_1m_beta(NULL));
        TEST_ED("1m_beta: azure.com returns true", anthropic_base_url_needs_1m_beta("https://my-account.services.ai.azure.com"));
        TEST_ED("1m_beta: native anthropic returns false", !anthropic_base_url_needs_1m_beta("https://api.anthropic.com"));
    }

    /* anthropic_is_minimax_endpoint */
    {
        TEST_ED("minimax: NULL returns false", !anthropic_is_minimax_endpoint(NULL));
        TEST_ED("minimax: minimax.io", anthropic_is_minimax_endpoint("https://api.minimax.io/anthropic"));
        TEST_ED("minimax: minimaxi.com", anthropic_is_minimax_endpoint("https://api.minimaxi.com/anthropic"));
        TEST_ED("minimax: azure not minimax", !anthropic_is_minimax_endpoint("https://my-account.openai.azure.com"));
    }

    /* anthropic_is_azure_anthropic_endpoint */
    {
        TEST_ED("azure: NULL returns false", !anthropic_is_azure_anthropic_endpoint(NULL));
        TEST_ED("azure: foundry host + /anthropic", anthropic_is_azure_anthropic_endpoint(
            "https://my-account.services.ai.azure.com/anthropic"));
        TEST_ED("azure: legacy azoai + /anthropic", anthropic_is_azure_anthropic_endpoint(
            "https://my-account.openai.azure.com/anthropic"));
        TEST_ED("azure: no /anthropic path", !anthropic_is_azure_anthropic_endpoint(
            "https://my-account.services.ai.azure.com/chat"));
        TEST_ED("azure: wrong host", !anthropic_is_azure_anthropic_endpoint(
            "https://api.anthropic.com"));
    }

    /* anthropic_common_betas_for_base_url */
    {
        json_t *betas;
        /* Native Anthropic — no 1m beta by default */
        betas = anthropic_common_betas_for_base_url("https://api.anthropic.com", false);
        TEST_ED("betas: native has interleaved-thinking",
                betas && strstr(json_serialize(betas), "interleaved-thinking-2025-05-14"));
        TEST_ED("betas: native no 1m-beta",
                betas && !strstr(json_serialize(betas), "context-1m-2025-08-07"));
        json_free(betas);

        /* Azure — includes 1m beta */
        betas = anthropic_common_betas_for_base_url("https://my-account.services.ai.azure.com", false);
        TEST_ED("betas: azure has 1m-beta",
                betas && strstr(json_serialize(betas), "context-1m-2025-08-07"));
        json_free(betas);

        /* MiniMax — excludes tool-streaming */
        betas = anthropic_common_betas_for_base_url("https://api.minimax.io/anthropic", false);
        TEST_ED("betas: minimax no tool-streaming",
                betas && !strstr(json_serialize(betas), "fine-grained-tool-streaming-2025-05-14"));
        TEST_ED("betas: minimax has interleaved-thinking",
                betas && strstr(json_serialize(betas), "interleaved-thinking-2025-05-14"));
        json_free(betas);

        /* drop_context_1m_beta=true */
        betas = anthropic_common_betas_for_base_url("https://my-account.services.ai.azure.com", true);
        TEST_ED("betas: drop 1m-beta when drop flag set",
                betas && !strstr(json_serialize(betas), "context-1m-2025-08-07"));
        json_free(betas);
    }

    /* anthropic_is_bedrock_model_id */
    {
        TEST_ED("bedrock_id: NULL returns false", !anthropic_is_bedrock_model_id(NULL));
        TEST_ED("bedrock_id: empty returns false", !anthropic_is_bedrock_model_id(""));
        TEST_ED("bedrock_id: anthropic.claude- prefix", anthropic_is_bedrock_model_id("anthropic.claude-sonnet-4-v1"));
        TEST_ED("bedrock_id: ARN format", anthropic_is_bedrock_model_id("arn:aws:bedrock:us-east-1::foundation-model/anthropic.claude-v2"));
        TEST_ED("bedrock_id: openai not bedrock", !anthropic_is_bedrock_model_id("gpt-4o"));
    }

    /* anthropic_resolve_positive_max_tokens */
    {
        TEST_ED("pos_tokens: positive returns value", anthropic_resolve_positive_max_tokens(8192) == 8192);
        TEST_ED("pos_tokens: zero returns 0", anthropic_resolve_positive_max_tokens(0) == 0);
        TEST_ED("pos_tokens: negative returns 0", anthropic_resolve_positive_max_tokens(-1) == 0);
    }

    printf("\n=== Endpoint detection: %s ===\n", f == 0 ? "ALL PASSED" : "SOME FAILED");
    return f;
}

int main(void) {
    int total = 0;
    total += test_anthropic_depth_main();
    total += test_endpoint_detection();
    printf("\n=== Overall: %s ===\n", total == 0 ? "ALL PASSED" : "SOME FAILED");
    return total ? 1 : 0;
}
