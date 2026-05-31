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

/* === URL edge cases === */
static void test_url_edge_cases(void)
{
    provider_register(PROVIDER_BEDROCK, &PROVIDER_OPS_BEDROCK);

    /* NULL base → default region */
    provider_t *p1 = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0", "", "");
    TEST("url edge p1", p1 != NULL);
    if (p1) {
        const provider_ops_t *ops = provider_ops(p1);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p1, NULL);
            TEST("url null base", url != NULL);
            if (url) {
                TEST("null base has bedrock-runtime", strstr(url, "bedrock-runtime") != NULL);
                TEST("null base has model", strstr(url, "claude-3-sonnet") != NULL);
                free(url);
            }
        }
        provider_free(p1);
    }

    /* Empty model → default model */
    provider_t *p2 = provider_create("bedrock", "", "", "");
    TEST("url edge p2", p2 != NULL);
    if (p2) {
        const provider_ops_t *ops = provider_ops(p2);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p2, NULL);
            TEST("url empty model", url != NULL);
            if (url) {
                TEST("default model in url", strstr(url, "anthropic.claude-3-sonnet-20240229-v1:0") != NULL);
                free(url);
            }
        }
        provider_free(p2);
    }
}

/* === Stop reason mapping tests === */
static void test_stop_reason_mapping(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0", "", "");
    TEST("stop reason p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    struct { const char *stop_reason; const char *expected; } cases[] = {
        {"end_turn", "stop"},
        {"stop_sequence", "stop"},
        {"tool_use", "tool_calls"},
        {"max_tokens", "length"},
        {"content_filtered", "content_filter"},
        {"guardrail_intervened", "content_filter"},
        {"unknown_reason", "stop"},
        {NULL, ""},  /* missing stopReason → empty string (not set) */
    };

    for (int i = 0; i < 8; i++) {
        char json[512];
        if (cases[i].stop_reason) {
            snprintf(json, sizeof(json),
                "{\"output\":{\"message\":{\"content\":[{\"text\":\"hello\"}]}},"
                "\"stopReason\":\"%s\"}",
                cases[i].stop_reason);
        } else {
            snprintf(json, sizeof(json),
                "{\"output\":{\"message\":{\"content\":[{\"text\":\"hello\"}]}}}");
        }
        provider_response_t *r = ops->parse_response(p, json);
        TEST("stop reason parsed", r != NULL);
        if (r) {
            TEST("finish_reason matches", strcmp(r->finish_reason, cases[i].expected) == 0);
            ops->free_response(r);
        }
    }

    provider_free(p);
}

/* === Response edge cases === */
static void test_parse_response_edge_cases(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0", "", "");
    TEST("edge p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* No output key */
    provider_response_t *r1 = ops->parse_response(p,
        "{\"usage\":{\"inputTokens\":5,\"outputTokens\":3}}");
    TEST("no output", r1 != NULL);
    if (r1) {
        TEST("no output tokens", r1->input_tokens == 5 && r1->output_tokens == 3);
        ops->free_response(r1);
    }

    /* Output with no message */
    provider_response_t *r2 = ops->parse_response(p,
        "{\"output\":{},\"usage\":{\"inputTokens\":7,\"outputTokens\":2}}");
    TEST("no message", r2 != NULL);
    if (r2) {
        TEST("no message tokens", r2->input_tokens == 7 && r2->output_tokens == 2);
        ops->free_response(r2);
    }

    /* Empty content array (content stays NULL) */
    provider_response_t *r3 = ops->parse_response(p,
        "{\"output\":{\"message\":{\"content\":[]}},\"stopReason\":\"end_turn\"}");
    TEST("empty content", r3 != NULL);
    if (r3) {
        TEST("empty content null", r3->content == NULL);
        ops->free_response(r3);
    }

    /* Content block with no text (toolUse only) */
    provider_response_t *r4 = ops->parse_response(p,
        "{\"output\":{\"message\":{\"content\":[{\"toolUse\":{\"toolUseId\":\"tu_1\",\"name\":\"fn\",\"input\":{}}}]}},\"stopReason\":\"tool_use\"}");
    TEST("tool only content", r4 != NULL);
    if (r4) {
        TEST("tool only tool count", r4->tool_calls_count == 1);
        TEST("tool only empty text", r4->content && strlen(r4->content) == 0);
        ops->free_response(r4);
    }

    provider_free(p);
}

/* === bedrock_is_context_overflow tests === */
static void test_bedrock_is_context_overflow(void)
{
    /* Pattern 1: ValidationException + input is too long */
    TEST("ctx overflow pattern1",
        bedrock_is_context_overflow("ValidationException: input is too long for this model") == true);
    TEST("ctx overflow pattern1b",
        bedrock_is_context_overflow("validation error: input is too long") == true);
    TEST("ctx overflow pattern1 max input",
        bedrock_is_context_overflow("validation error: max input token exceeded") == true);
    TEST("ctx overflow pattern1 exceed",
        bedrock_is_context_overflow("validation error: input token exceeds limit") == true);

    /* Pattern 2: ValidationException + exceeds + max tokens */
    TEST("ctx overflow pattern2",
        bedrock_is_context_overflow("ValidationException: exceeds the maximum number of tokens") == true);
    TEST("ctx overflow pattern2b",
        bedrock_is_context_overflow("validation error: exceeds max token limit") == true);

    /* Pattern 3: ModelStreamErrorException + too many input tokens */
    TEST("ctx overflow pattern3",
        bedrock_is_context_overflow("ModelStreamErrorException: Input is too long") == true);
    TEST("ctx overflow pattern3b",
        bedrock_is_context_overflow("ModelStreamErrorException: too many input tokens") == true);
    TEST("ctx overflow stream error",
        bedrock_is_context_overflow("stream error: too many input tokens") == true);

    /* Non-overflow errors */
    TEST("ctx overflow non-match",
        bedrock_is_context_overflow("AccessDeniedException: Not authorized") == false);
    TEST("ctx overflow NULL",
        bedrock_is_context_overflow(NULL) == false);
    TEST("ctx overflow empty",
        bedrock_is_context_overflow("") == false);
    TEST("ctx overflow validation only",
        bedrock_is_context_overflow("ValidationException: Some other error") == false);
}

/* === bedrock_classify_error tests === */
static void test_bedrock_classify_error(void)
{
    TEST("classify context overflow",
        strcmp(bedrock_classify_error("validation error: input is too long"), "context_overflow") == 0);
    TEST("classify throttle",
        strcmp(bedrock_classify_error("ThrottlingException: Too many requests"), "rate_limit") == 0);
    TEST("classify throttle2",
        strcmp(bedrock_classify_error("Too many concurrent requests"), "rate_limit") == 0);
    TEST("classify throttle3",
        strcmp(bedrock_classify_error("ServiceQuotaExceededException"), "rate_limit") == 0);
    TEST("classify overload",
        strcmp(bedrock_classify_error("ModelNotReadyException"), "overloaded") == 0);
    TEST("classify overload2",
        strcmp(bedrock_classify_error("ModelTimeoutException"), "overloaded") == 0);
    TEST("classify overload3",
        strcmp(bedrock_classify_error("InternalServerException"), "overloaded") == 0);
    TEST("classify overload4",
        strcmp(bedrock_classify_error("ServiceUnavailableException"), "overloaded") == 0);
    TEST("classify unknown",
        strcmp(bedrock_classify_error("AccessDeniedException: Not authorized"), "unknown") == 0);
    TEST("classify NULL",
        strcmp(bedrock_classify_error(NULL), "unknown") == 0);
}

/* === Streaming passthrough tests === */
static void test_parse_stream_edge_depth(void)
{
    provider_t *p = provider_create("bedrock", "anthropic.claude-3-sonnet-20240229-v1:0", "", "");
    TEST("stream depth p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* NULL */
    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("stream null", r1 != NULL);
    if (r1) {
        TEST("stream null empty", strcmp(r1->content, "") == 0);
        ops->free_response(r1);
    }

    /* Empty */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "");
    TEST("stream empty", r2 != NULL);
    if (r2) {
        TEST("stream empty content", r2->content && strlen(r2->content) == 0);
        ops->free_response(r2);
    }

    provider_free(p);
}

int main(void)
{
    /* Register Bedrock provider */
    provider_register(PROVIDER_BEDROCK, &PROVIDER_OPS_BEDROCK);
    test_url_building();
    test_url_edge_cases();
    test_parse_response_basic();
    test_parse_response_multiple_text_blocks();
    test_parse_response_with_tool_calls();
    test_parse_response_error_format();
    test_parse_response_malformed();
    test_stop_reason_mapping();
    test_parse_response_edge_cases();
    test_bedrock_is_context_overflow();
    test_bedrock_classify_error();
    test_parse_stream_chunk();
    test_parse_stream_edge_depth();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
