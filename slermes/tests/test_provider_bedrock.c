/* AWS Bedrock provider tests — URL building, Converse API response parsing.
 *
 * Bedrock differs from OpenAI:
 * - URL: https://bedrock-runtime.{region}.amazonaws.com/model/{model}/converse
 * - Auth: AWS SigV4 (needs env AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY)
 * - Response: Bedrock Converse API format (not OpenAI-compatible)
 *   output.message.content[].text, usage.inputTokens/outputTokens
 * - No SSE streaming (passthrough fallback)
 *
 * Tests WITHOUT AWS credentials — pure structural/parsing tests.
 * build_headers requires OpenSSL SigV4 and env vars, so skipped in this unit test.
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
    provider_register(PROVIDER_BEDROCK, &PROVIDER_OPS_BEDROCK);

    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "https://bedrock-runtime.us-east-1.amazonaws.com");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Default URL with model */
    char *url = ops->build_url(p, "https://bedrock-runtime.us-east-1.amazonaws.com");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url starts with bedrock-runtime", strstr(url, "bedrock-runtime") != NULL);
        TEST("url has model path", strstr(url, "/model/") != NULL);
        TEST("url has model id", strstr(url, "anthropic.claude-3-sonnet") != NULL);
        TEST("url has converse", strstr(url, "/converse") != NULL);
        free(url);
    }

    /* NULL base → uses default region */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    if (url2) {
        TEST("default url has bedrock-runtime", strstr(url2, "bedrock-runtime") != NULL);
        TEST("default url has converse", strstr(url2, "/converse") != NULL);
        free(url2);
    }

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Bedrock Converse API response (NOT OpenAI format) */
    const char *json =
        "{"
        "\"output\":{"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":[{"
              "\"text\":\"Hello from Bedrock!\""
            "}]"
          "}"
        "},"
        "\"stopReason\":\"end_turn\","
        "\"usage\":{"
          "\"inputTokens\":15,"
          "\"outputTokens\":25"
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("response parsed", resp != NULL);
    if (resp) {
        TEST("content extracted", resp->content != NULL);
        TEST("content correct", strcmp(resp->content, "Hello from Bedrock!") == 0);
        TEST("input_tokens (Bedrock inputTokens)", resp->input_tokens == 15);
        TEST("output_tokens (Bedrock outputTokens)", resp->output_tokens == 25);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_multiple_text_blocks(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("multi-block provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Response with multiple text blocks */
    const char *json =
        "{"
        "\"output\":{"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":["
              "{\"text\":\"Hello!\"},"
              "{\"text\":\" How can I help?\"}"
            "]"
          "}"
        "},"
        "\"stopReason\":\"end_turn\""
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("multi-block parsed", resp != NULL);
    if (resp) {
        TEST("multi-block concatenated", resp->content != NULL);
        TEST("both blocks present", strcmp(resp->content, "Hello! How can I help?") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("tool call provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Bedrock response with toolUse content block */
    const char *json =
        "{"
        "\"output\":{"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":[{"
              "\"text\":\"Let me check the weather.\""
            "},{"
              "\"toolUse\":{"
                "\"toolUseId\":\"tooluse_abc123\","
                "\"name\":\"get_weather\","
                "\"input\":{\"location\":\"NYC\"}"
              "}"
            "}]"
          "}"
        "},"
        "\"stopReason\":\"tool_use\""
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("tool call response parsed", resp != NULL);
    if (resp) {
        TEST("tool_calls_count", resp->tool_calls_count == 1);
        TEST("tool call id", strcmp(resp->tool_calls[0].id, "tooluse_abc123") == 0);
        TEST("tool call name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("tool call args contains NYC", strstr(resp->tool_calls[0].arguments, "NYC") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error_format(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Bedrock error format 1: {"message":"..."} */
    const char *err1 = "{\"message\":\"Access denied: insufficient permissions\"}";
    provider_response_t *r1 = ops->parse_response(p, err1);
    TEST("error format 1 parsed", r1 != NULL);
    if (r1) {
        TEST("error message captured", strstr(r1->content, "Access denied") != NULL);
        ops->free_response(r1);
    }

    /* Bedrock error format 2: {"error":{"message":"..."}} */
    const char *err2 = "{\"error\":{\"message\":\"Model not found\"}}";
    provider_response_t *r2 = ops->parse_response(p, err2);
    TEST("error format 2 parsed", r2 != NULL);
    if (r2) {
        TEST("error message captured", strstr(r2->content, "Model not found") != NULL);
        ops->free_response(r2);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Malformed JSON — Bedrock returns raw string, not prefixed error */
    provider_response_t *r1 = ops->parse_response(p, "{bad}");
    TEST("malformed not null", r1 != NULL);
    if (r1) {
        TEST("raw string returned", strstr(r1->content, "{bad}") != NULL);
        ops->free_response(r1);
    }

    /* Empty response */
    provider_response_t *r2 = ops->parse_response(p, "");
    TEST("empty response", r2 != NULL);
    if (r2) ops->free_response(r2);

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0",
                                     "", "");
    TEST("stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Bedrock stream is passthrough (Converse doesn't support SSE) */
    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("null chunk", r1 != NULL);
    if (r1) { TEST("null passes empty", strcmp(r1->content, "") == 0); ops->free_response(r1); }

    provider_response_t *r2 = ops->parse_stream_chunk(p, "raw data");
    TEST("passthrough chunk", r2 != NULL);
    if (r2) {
        TEST("passthrough content", strcmp(r2->content, "raw data") == 0);
        ops->free_response(r2);
    }

    provider_free(p);
}

int main(void)
{
    /* Register Bedrock provider */
    provider_register(PROVIDER_BEDROCK, &PROVIDER_OPS_BEDROCK);
    test_url_building();
    test_parse_response_basic();
    test_parse_response_multiple_text_blocks();
    test_parse_response_with_tool_calls();
    test_parse_response_error_format();
    test_parse_response_malformed();
    test_parse_stream_chunk();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
