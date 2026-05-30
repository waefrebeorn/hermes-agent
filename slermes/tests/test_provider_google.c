/* Google provider tests — verify response parsing, URL building, headers.
 *
 * Tests Google-specific formats: parts array, functionCall,
 * usageMetadata, SSE streaming.
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
    provider_register(PROVIDER_GOOGLE, &PROVIDER_OPS_GOOGLE);

    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Standard base URL with model */
    char *url = ops->build_url(p, "https://generativelanguage.googleapis.com/v1beta");
    TEST("google url built", url != NULL);
    if (url) {
        TEST("google url has :generateContent", strstr(url, ":generateContent") != NULL);
        TEST("google url has model", strstr(url, "gemini-2.0-flash") != NULL);
        free(url);
    }

    /* NULL base URL (uses default) */
    char *url2 = ops->build_url(p, NULL);
    TEST("google url NULL base", url2 != NULL);
    if (url2) {
        TEST("NULL base uses default", strstr(url2, "generativelanguage.googleapis.com") != NULL);
        free(url2);
    }

    /* URL with trailing slash */
    char *url3 = ops->build_url(p, "https://generativelanguage.googleapis.com/v1beta/");
    TEST("google url trailing slash", url3 != NULL);
    if (url3) {
        TEST("no double slash", strstr(url3, "//models") == NULL);
        free(url3);
    }

    /* URL already containing :generateContent */
    char *url4 = ops->build_url(p, "https://custom.example.com/v1/models/gemini-pro:generateContent");
    TEST("google url already has :generateContent", url4 != NULL);
    if (url4) {
        TEST("url used as-is", strcmp(url4, "https://custom.example.com/v1/models/gemini-pro:generateContent") == 0);
        free(url4);
    }

    /* Different model name */
    provider_t *p2 = provider_create("google", "gemini-1.5-pro", "AIza-test", "https://api.example.com");
    if (p2) {
        char *url5 = ops->build_url(p2, "https://api.example.com");
        TEST("google different model", url5 != NULL);
        if (url5) {
            TEST("model name in url", strstr(url5, "gemini-1.5-pro") != NULL);
            free(url5);
        }
        provider_free(p2);
    }

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test-key", "");
    TEST("google header provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_headers) { provider_free(p); return; }

    char *h = ops->build_headers(p, "AIza-test-key");
    TEST("google headers built", h != NULL);
    if (h) {
        TEST("has Content-Type", strstr(h, "Content-Type") != NULL);
        TEST("has x-goog-api-key", strstr(h, "x-goog-api-key") != NULL);
        TEST("has test key", strstr(h, "AIza-test-key") != NULL);
        free(h);
    }

    /* NULL api_key */
    char *h2 = ops->build_headers(p, NULL);
    TEST("google headers NULL key", h2 != NULL);
    free(h2);

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"candidates\":[{"
          "\"content\":{"
            "\"parts\":[{\"text\":\"Hello from Gemini!\"}],"
            "\"role\":\"model\""
          "},"
          "\"finishReason\":\"STOP\""
        "}],"
        "\"usageMetadata\":{"
          "\"promptTokenCount\":10,"
          "\"candidatesTokenCount\":5"
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("google response parsed", resp != NULL);
    if (resp) {
        TEST("google content", resp->content != NULL);
        if (resp->content) TEST("google content correct", strcmp(resp->content, "Hello from Gemini!") == 0);
        TEST("google input_tokens", resp->input_tokens == 10);
        TEST("google output_tokens", resp->output_tokens == 5);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_multiple_parts(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google multi provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Multiple text parts concatenated */
    const char *json =
        "{"
        "\"candidates\":[{"
          "\"content\":{"
            "\"parts\":["
              "{\"text\":\"First part. \"},"
              "{\"text\":\"Second part.\"}"
            "]"
          "},"
          "\"finishReason\":\"STOP\""
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("google multi part parsed", resp != NULL);
    if (resp) {
        TEST("google multi content", resp->content != NULL);
        if (resp->content) TEST("parts concatenated", strcmp(resp->content, "First part. Second part.") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_function_call(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google fc provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"candidates\":[{"
          "\"content\":{"
            "\"parts\":[{"
              "\"functionCall\":{"
                "\"name\":\"get_weather\","
                "\"args\":{\"location\":\"Tokyo\",\"unit\":\"celsius\"}"
              "}"
            "}]"
          "},"
          "\"finishReason\":\"STOP\""
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("google fc parsed", resp != NULL);
    if (resp) {
        TEST("google fc count", resp->tool_calls_count == 1);
        TEST("google fc name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("google fc args has location", strstr(resp->tool_calls[0].arguments, "Tokyo") != NULL);
        TEST("google fc args has unit", strstr(resp->tool_calls[0].arguments, "celsius") != NULL);
        TEST("google fc id generated", strlen(resp->tool_calls[0].id) > 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"error\":{"
          "\"code\":400,"
          "\"message\":\"API key not valid.\","
          "\"status\":\"INVALID_ARGUMENT\""
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("google error parsed", resp != NULL);
    if (resp) {
        TEST("google error msg", strstr(resp->content, "API key not valid") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty JSON */
    provider_response_t *r1 = ops->parse_response(p, "{}");
    TEST("google empty json", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Invalid JSON */
    provider_response_t *r2 = ops->parse_response(p, "not json");
    TEST("google invalid json", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    provider_free(p);
}

static void test_parse_stream_chunk_text(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Standard Google streaming chunk with text */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";

    provider_response_t *r1 = ops->parse_stream_chunk(p, chunk);
    TEST("google stream chunk", r1 != NULL);
    if (r1) {
        TEST("google stream text", r1->content != NULL && strcmp(r1->content, "Hello") == 0);
        ops->free_response(r1);
    }

    /* With data: prefix */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"World\"}]}}]}");
    TEST("google data prefix", r2 != NULL);
    if (r2) {
        TEST("google data text", r2->content != NULL && strcmp(r2->content, "World") == 0);
        ops->free_response(r2);
    }

    /* [DONE] */
    provider_response_t *r3 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("google DONE", r3 != NULL);
    if (r3) {
        TEST("google DONE empty", r3->content != NULL && strlen(r3->content) == 0);
        ops->free_response(r3);
    }

    provider_free(p);
}

static void test_parse_stream_finish_reason(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google finish provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Chunk with finishReason only */
    const char *chunk =
        "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";

    provider_response_t *r1 = ops->parse_stream_chunk(p, chunk);
    TEST("google finish chunk", r1 != NULL);
    if (r1) {
        TEST("google finish reason", strcmp(r1->finish_reason, "stop") == 0);
        ops->free_response(r1);
    }

    /* Chunk with both finishReason and text */
    const char *chunk2 =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Final\"}]},\"finishReason\":\"STOP\"}]}";

    provider_response_t *r2 = ops->parse_stream_chunk(p, chunk2);
    TEST("google final chunk", r2 != NULL);
    if (r2) {
        TEST("google final text", r2->content != NULL && strcmp(r2->content, "Final") == 0);
        TEST("google final reason", strcmp(r2->finish_reason, "stop") == 0);
        ops->free_response(r2);
    }

    provider_free(p);
}

static void test_parse_stream_usage(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google usage provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Usage metadata chunk (no candidates) */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":15,\"candidatesTokenCount\":10}}";

    provider_response_t *r = ops->parse_stream_chunk(p, chunk);
    TEST("google usage chunk", r != NULL);
    if (r) {
        TEST("google usage input", r->input_tokens == 15);
        TEST("google usage output", r->output_tokens == 10);
        ops->free_response(r);
    }

    provider_free(p);
}

static void test_parse_stream_edge_cases(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* NULL */
    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("google NULL chunk", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Empty */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "");
    TEST("google empty chunk", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* Invalid JSON */
    provider_response_t *r3 = ops->parse_stream_chunk(p, "not json");
    TEST("google invalid JSON chunk", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    provider_free(p);
}

static void test_free_response_null(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("google free provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->free_response) { provider_free(p); return; }

    ops->free_response(NULL);
    TEST("google free_response(NULL) safe", 1);

    provider_free(p);
}

int main(void)
{
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_multiple_parts();
    test_parse_response_function_call();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk_text();
    test_parse_stream_finish_reason();
    test_parse_stream_usage();
    test_parse_stream_edge_cases();
    test_free_response_null();

    printf("provider_google: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
