/* OpenRouter provider tests — URL, headers (HTTP-Referer/X-Title), parse, stream.
 *
 * OpenRouter is OpenAI-compatible with additions:
 * - Default base: https://openrouter.ai/api/v1
 * - HTTP-Referer and X-Title in headers
 * - Reasoning content in responses (reasoning_content or reasoning fields)
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
    provider_register(PROVIDER_OPENROUTER, &PROVIDER_OPS_OPENROUTER);

    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "https://openrouter.ai/api/v1");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Standard URL */
    char *url = ops->build_url(p, "https://openrouter.ai/api/v1");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url contains chat/completions", strstr(url, "chat/completions") != NULL);
        TEST("url starts with openrouter", strstr(url, "openrouter.ai") != NULL);
        free(url);
    }

    /* NULL base → default */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    if (url2) {
        TEST("default uses openrouter", strstr(url2, "openrouter.ai") != NULL);
        free(url2);
    }

    /* Already has /chat/completions → pass through */
    char *url3 = ops->build_url(p, "https://openrouter.ai/api/v1/chat/completions");
    TEST("url with full path", url3 != NULL);
    if (url3) {
        TEST("full path preserved", strcmp(url3, "https://openrouter.ai/api/v1/chat/completions") == 0);
        free(url3);
    }

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test-key", "https://openrouter.ai/api/v1");
    TEST("header provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_headers) { provider_free(p); return; }

    char *h = ops->build_headers(p, "sk-test-key");
    TEST("headers built", h != NULL);
    if (h) {
        TEST("has Bearer", strstr(h, "Bearer") != NULL);
        TEST("has test key", strstr(h, "sk-test-key") != NULL);
        TEST("has Content-Type", strstr(h, "application/json") != NULL);
        TEST("has HTTP-Referer", strstr(h, "HTTP-Referer") != NULL);
        TEST("has X-Title", strstr(h, "X-Title: Hermes-C") != NULL);
        free(h);
    }

    /* NULL key → still has Referer and Title */
    char *h2 = ops->build_headers(p, NULL);
    TEST("headers with NULL key", h2 != NULL);
    if (h2) {
        TEST("no key still has Referer", strstr(h2, "HTTP-Referer") != NULL);
        TEST("no key still has X-Title", strstr(h2, "X-Title") != NULL);
        free(h2);
    }

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Standard OpenAI-compatible response */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-or\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"Hello from OpenRouter!\""
          "},"
          "\"finish_reason\":\"stop\""
        "}],"
        "\"usage\":{"
          "\"prompt_tokens\":10,"
          "\"completion_tokens\":5"
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("response parsed", resp != NULL);
    if (resp) {
        TEST("content correct", strcmp(resp->content, "Hello from OpenRouter!") == 0);
        TEST("input_tokens", resp->input_tokens == 10);
        TEST("output_tokens", resp->output_tokens == 5);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_reasoning(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("reasoning provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Response with reasoning_content */
    const char *json =
        "{"
        "\"choices\":[{"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"The answer is 42.\","
            "\"reasoning_content\":\"Let me think step by step...\""
          "}"
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("reasoning parsed", resp != NULL);
    if (resp) {
        TEST("reasoning present", resp->reasoning != NULL);
        if (resp->reasoning)
            TEST("reasoning content", strstr(resp->reasoning, "step by step") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("tool call provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"choices\":[{"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":null,"
            "\"tool_calls\":[{"
              "\"id\":\"call_xyz\","
              "\"function\":{"
                "\"name\":\"search\","
                "\"arguments\":\"{\\\"q\\\":\\\"test\\\"}\""
              "}"
            "}]"
          "}"
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("tool call parsed", resp != NULL);
    if (resp) {
        TEST("tool call count", resp->tool_calls_count == 1);
        TEST("tool call id", strcmp(resp->tool_calls[0].id, "call_xyz") == 0);
        TEST("tool call name", strcmp(resp->tool_calls[0].name, "search") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"error\":{"
          "\"message\":\"Insufficient credits\""
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("error response", resp != NULL);
    if (resp) {
        TEST("OpenRouter prefix", strstr(resp->content, "OpenRouter API error") != NULL);
        TEST("error msg", strstr(resp->content, "Insufficient credits") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    provider_response_t *r1 = ops->parse_response(p, "{bad}");
    TEST("malformed not null", r1 != NULL);
    if (r1) {
        TEST("JSON parse error prefix", strstr(r1->content, "JSON parse error") != NULL);
        ops->free_response(r1);
    }

    provider_response_t *r2 = ops->parse_response(p, "");
    TEST("empty parsed", r2 != NULL);
    if (r2) ops->free_response(r2);

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("null chunk", r1 != NULL);
    if (r1) { TEST("null empty", strcmp(r1->content, "") == 0); ops->free_response(r1); }

    provider_response_t *r2 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("done marker", r2 != NULL);
    if (r2) { TEST("done empty", strcmp(r2->content, "") == 0); ops->free_response(r2); }

    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"},\"index\":0}]}");
    TEST("content delta", r3 != NULL);
    if (r3) {
        TEST("delta correct", strcmp(r3->content, "Hello") == 0);
        ops->free_response(r3);
    }

    provider_response_t *r4 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\",\"index\":0}]}");
    TEST("finish_reason", r4 != NULL);
    if (r4) {
        TEST("finish_reason stop", strcmp(r4->finish_reason, "stop") == 0);
        ops->free_response(r4);
    }

    provider_response_t *r5 = ops->parse_stream_chunk(p, "raw passthrough");
    TEST("non-prefixed", r5 != NULL);
    if (r5) {
        TEST("passthrough content", r5->content && r5->content[0] == '\0');
        ops->free_response(r5);
    }

    provider_free(p);
}

int main(void)
{
    provider_register(PROVIDER_OPENROUTER, &PROVIDER_OPS_OPENROUTER);
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_with_reasoning();
    test_parse_response_with_tool_calls();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
