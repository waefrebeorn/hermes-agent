/* Anthropic provider tests — verify response parsing, URL building, headers.
 *
 * Tests Anthropic-specific JSON formats: content blocks, thinking blocks,
 * tool_use blocks, SSE streaming event types.
 */

#include "hermes.h"
#include "provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

static void test_url_building(void)
{
    provider_register(PROVIDER_ANTHROPIC, &PROVIDER_OPS_ANTHROPIC);

    provider_t *p = provider_create("anthropic", "claude-3-5-sonnet-20240620",
                                    "sk-ant-test", "https://api.anthropic.com/v1");
    TEST("anth provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Standard base URL */
    char *url = ops->build_url(p, "https://api.anthropic.com/v1");
    TEST("anth url built", url != NULL);
    if (url) {
        TEST("anth url has /messages", strstr(url, "/messages") != NULL);
        free(url);
    }

    /* URL already containing /messages */
    char *url2 = ops->build_url(p, "https://api.anthropic.com/v1/messages");
    TEST("anth url with /messages already", url2 != NULL);
    if (url2) {
        TEST("already has /messages", strstr(url2, "/messages") != NULL);
        free(url2);
    }

    /* NULL base URL */
    char *url3 = ops->build_url(p, NULL);
    TEST("anth url NULL base", url3 != NULL);
    if (url3) {
        TEST("NULL base uses default", strstr(url3, "api.anthropic.com") != NULL);
        free(url3);
    }

    /* Trailing slash */
    char *url4 = ops->build_url(p, "https://api.anthropic.com/v1/");
    TEST("anth url trailing slash", url4 != NULL);
    if (url4) free(url4);

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-ant-test-key", "");
    TEST("anth header provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_headers) { provider_free(p); return; }

    char *h = ops->build_headers(p, "sk-ant-test-key");
    TEST("anth headers built", h != NULL);
    if (h) {
        TEST("has x-api-key", strstr(h, "x-api-key") != NULL);
        TEST("has test key", strstr(h, "sk-ant-test-key") != NULL);
        TEST("has anthropic-version", strstr(h, "anthropic-version") != NULL);
        TEST("has Content-Type", strstr(h, "application/json") != NULL);
        free(h);
    }

    /* NULL api_key */
    char *h2 = ops->build_headers(p, NULL);
    TEST("anth headers NULL key", h2 != NULL);
    if (h2) {
        /* Should not have x-api-key line */
        TEST("no x-api-key when NULL", strstr(h2, "x-api-key") == NULL);
        free(h2);
    }

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Standard Anthropic response with text content blocks */
    const char *json =
        "{"
        "\"id\":\"msg_01abc123\","
        "\"type\":\"message\","
        "\"role\":\"assistant\","
        "\"content\":["
          "{\"type\":\"text\",\"text\":\"Hello! I am Claude.\"}"
        "],"
        "\"usage\":{\"input_tokens\":12,\"output_tokens\":7}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("anth response parsed", resp != NULL);
    if (resp) {
        TEST("anth content", resp->content != NULL);
        if (resp->content) TEST("anth content correct", strcmp(resp->content, "Hello! I am Claude.") == 0);
        TEST("anth input_tokens", resp->input_tokens == 12);
        TEST("anth output_tokens", resp->output_tokens == 7);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_multiple_text_blocks(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth multi provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Anthropic response with multiple text blocks (concatenated) */
    const char *json =
        "{"
        "\"id\":\"msg_multi\","
        "\"content\":["
          "{\"type\":\"text\",\"text\":\"First block. \"},"
          "{\"type\":\"text\",\"text\":\"Second block.\"}"
        "],"
        "\"usage\":{\"input_tokens\":10,\"output_tokens\":6}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("anth multi block parsed", resp != NULL);
    if (resp) {
        TEST("anth multi content", resp->content != NULL);
        if (resp->content) {
            TEST("multi blocks concatenated", strcmp(resp->content, "First block. Second block.") == 0);
        }
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_thinking(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth thinking provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Anthropic response with thinking + text blocks */
    const char *json =
        "{"
        "\"id\":\"msg_think\","
        "\"content\":["
          "{\"type\":\"thinking\",\"thinking\":\"Let me reason about this...\"},"
          "{\"type\":\"text\",\"text\":\"The answer is 42.\"}"
        "],"
        "\"usage\":{\"input_tokens\":15,\"output_tokens\":20}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("anth thinking parsed", resp != NULL);
    if (resp) {
        TEST("anth thinking content", resp->content != NULL);
        if (resp->content) TEST("thinking content text", strcmp(resp->content, "The answer is 42.") == 0);
        TEST("anth reasoning present", resp->reasoning != NULL);
        if (resp->reasoning) TEST("reasoning matches", strstr(resp->reasoning, "Let me reason") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"error\":{"
          "\"type\":\"authentication_error\","
          "\"message\":\"Invalid API key provided.\""
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("anth error parsed", resp != NULL);
    if (resp) {
        TEST("anth error msg", strstr(resp->content, "Invalid API key") != NULL);
        TEST("anth error type", strstr(resp->content, "authentication_error") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty JSON */
    provider_response_t *r1 = ops->parse_response(p, "{}");
    TEST("anth empty json", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Invalid JSON */
    provider_response_t *r2 = ops->parse_response(p, "not json");
    TEST("anth invalid json", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* NULL */
    provider_response_t *r3 = ops->parse_response(p, NULL);
    TEST("anth NULL body", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    provider_free(p);
}

static void test_parse_stream_chunk_content_block_delta(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* content_block_delta with text_delta */
    const char *chunk =
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}";

    provider_response_t *r1 = ops->parse_stream_chunk(p, chunk);
    TEST("anth delta chunk", r1 != NULL);
    if (r1) {
        TEST("anth delta content", r1->content != NULL);
        if (r1->content) TEST("anth delta text", strcmp(r1->content, "Hello") == 0);
        ops->free_response(r1);
    }

    /* Raw JSON (HTTP parser stripped framing) */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"World\"}}");
    TEST("anth raw json chunk", r2 != NULL);
    if (r2) {
        TEST("anth raw json text", r2->content != NULL && strcmp(r2->content, "World") == 0);
        ops->free_response(r2);
    }

    /* Simple "data: " prefix format */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"!\"}}");
    TEST("anth data prefix chunk", r3 != NULL);
    if (r3) {
        TEST("anth data prefix text", r3->content != NULL && strcmp(r3->content, "!") == 0);
        ops->free_response(r3);
    }

    provider_free(p);
}

static void test_parse_stream_thinking_delta(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth thinking stream", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    const char *chunk =
        "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"thinking_delta\",\"thinking\":\"I'm thinking...\"}}";

    provider_response_t *r = ops->parse_stream_chunk(p, chunk);
    TEST("anth thinking delta", r != NULL);
    if (r) {
        TEST("anth thinking reasoning", r->reasoning != NULL);
        if (r->reasoning) TEST("anth thinking text", strcmp(r->reasoning, "I'm thinking...") == 0);
        ops->free_response(r);
    }

    provider_free(p);
}

static void test_parse_stream_content_block_start(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth start provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    const char *chunk =
        "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"Initial\"}}";

    provider_response_t *r = ops->parse_stream_chunk(p, chunk);
    TEST("anth start chunk", r != NULL);
    if (r) {
        TEST("anth start text", r->content != NULL && strcmp(r->content, "Initial") == 0);
        ops->free_response(r);
    }

    provider_free(p);
}

static void test_parse_stream_message_start(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth msg start provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    const char *chunk =
        "{\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"usage\":{\"input_tokens\":15,\"output_tokens\":0}}}";

    provider_response_t *r = ops->parse_stream_chunk(p, chunk);
    TEST("anth msg start", r != NULL);
    if (r) {
        TEST("anth msg input_tokens", r->input_tokens == 15);
        TEST("anth msg empty content", r->content != NULL && strlen(r->content) == 0);
        ops->free_response(r);
    }

    provider_free(p);
}

static void test_parse_stream_message_delta(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth msg delta provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    const char *chunk =
        "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":25}}";

    provider_response_t *r = ops->parse_stream_chunk(p, chunk);
    TEST("anth msg delta", r != NULL);
    if (r) {
        TEST("anth delta output_tokens", r->output_tokens == 25);
        TEST("anth delta finish_reason", strcmp(r->finish_reason, "end_turn") == 0);
        ops->free_response(r);
    }

    /* tool_use stop_reason */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\"},\"usage\":{\"output_tokens\":30}}");
    TEST("anth msg delta tool_use", r2 != NULL);
    if (r2) {
        TEST("anth delta tool_use reason", strcmp(r2->finish_reason, "tool_use") == 0);
        ops->free_response(r2);
    }

    provider_free(p);
}

static void test_parse_stream_unknown(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth unknown provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Ping event */
    provider_response_t *r = ops->parse_stream_chunk(p,
        "{\"type\":\"ping\"}");
    TEST("anth ping", r != NULL);
    if (r) {
        TEST("anth ping empty", r->content != NULL && strlen(r->content) == 0);
        ops->free_response(r);
    }

    /* [DONE] sentinel */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("anth DONE", r2 != NULL);
    if (r2) {
        TEST("anth DONE empty", r2->content != NULL && strlen(r2->content) == 0);
        ops->free_response(r2);
    }

    /* NULL */
    provider_response_t *r3 = ops->parse_stream_chunk(p, NULL);
    TEST("anth NULL chunk", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    /* Empty string */
    provider_response_t *r4 = ops->parse_stream_chunk(p, "");
    TEST("anth empty chunk", r4 != NULL);
    if (r4) { ops->free_response(r4); }

    provider_free(p);
}

static void test_free_response_null(void)
{
    provider_t *p = provider_create("anthropic", "claude-3-5", "sk-test", "");
    TEST("anth free provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->free_response) { provider_free(p); return; }

    ops->free_response(NULL);
    TEST("anth free_response(NULL) safe", 1);

    provider_free(p);
}

int main(void)
{
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_multiple_text_blocks();
    test_parse_response_with_thinking();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk_content_block_delta();
    test_parse_stream_thinking_delta();
    test_parse_stream_content_block_start();
    test_parse_stream_message_start();
    test_parse_stream_message_delta();
    test_parse_stream_unknown();
    test_free_response_null();

    printf("provider_anthropic: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
