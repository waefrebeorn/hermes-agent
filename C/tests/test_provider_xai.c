/* xAI (Grok) provider tests — URL, headers, parse, stream, model retirement.
 *
 * xAI is OpenAI-compatible for chat completions. Additional features:
 * - Default base URL: https://api.x.ai/v1
 * - Model retirement detection (xai_is_model_retired)
 * - Encrypted content in responses
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
    provider_register(PROVIDER_XAI, &PROVIDER_OPS_XAI);

    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "https://api.x.ai/v1");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Default xAI base → append /chat/completions */
    char *url = ops->build_url(p, "https://api.x.ai/v1");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url contains chat/completions", strstr(url, "chat/completions") != NULL);
        TEST("url starts with xai", strstr(url, "api.x.ai") != NULL);
        free(url);
    }

    /* NULL base → default xAI URL */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    if (url2) {
        TEST("default url uses x.ai", strstr(url2, "x.ai") != NULL);
        TEST("default url has chat/completions", strstr(url2, "chat/completions") != NULL);
        free(url2);
    }

    /* Empty base → default */
    char *url3 = ops->build_url(p, "");
    TEST("url with empty base", url3 != NULL);
    free(url3);

    /* Already has /chat/completions → pass through */
    char *url4 = ops->build_url(p, "https://api.x.ai/v1/chat/completions");
    TEST("url with full path", url4 != NULL);
    if (url4) {
        TEST("full path preserved", strcmp(url4, "https://api.x.ai/v1/chat/completions") == 0);
        free(url4);
    }

    /* Trailing slash handled */
    char *url5 = ops->build_url(p, "https://api.x.ai/v1/");
    TEST("trailing slash url", url5 != NULL);
    free(url5);

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test-key", "https://api.x.ai/v1");
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
        free(h);
    }

    /* NULL api_key → headers without Authorization */
    char *h2 = ops->build_headers(p, NULL);
    TEST("headers with NULL key", h2 != NULL);
    free(h2);

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "https://api.x.ai/v1");
    TEST("response parse provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Standard xAI chat completion response (OpenAI-compatible) */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-123\","
        "\"object\":\"chat.completion\","
        "\"created\":1234567890,"
        "\"model\":\"grok-4.3\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"Hello! I am Grok.\""
          "},"
          "\"finish_reason\":\"stop\""
        "}],"
        "\"usage\":{"
          "\"prompt_tokens\":10,"
          "\"completion_tokens\":8,"
          "\"total_tokens\":18"
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("response parsed", resp != NULL);
    if (resp) {
        TEST("content extracted", resp->content != NULL);
        TEST("content correct", strcmp(resp->content, "Hello! I am Grok.") == 0);
        TEST("input_tokens", resp->input_tokens == 10);
        TEST("output_tokens", resp->output_tokens == 8);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "");
    TEST("tool call provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"id\":\"chatcmpl-tc\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":null,"
            "\"tool_calls\":[{"
              "\"id\":\"call_abc123\","
              "\"type\":\"function\","
              "\"function\":{"
                "\"name\":\"get_weather\","
                "\"arguments\":\"{\\\"location\\\":\\\"NYC\\\"}\""
              "}"
            "}]"
          "},"
          "\"finish_reason\":\"tool_calls\""
        "}],"
        "\"usage\":{"
          "\"prompt_tokens\":50,"
          "\"completion_tokens\":20"
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("tool call response parsed", resp != NULL);
    if (resp) {
        TEST("tool_calls_count", resp->tool_calls_count == 1);
        TEST("tool call id", strcmp(resp->tool_calls[0].id, "call_abc123") == 0);
        TEST("tool call name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("tool call args contains NYC", strstr(resp->tool_calls[0].arguments, "NYC") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "");
    TEST("error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* xAI error response */
    const char *json =
        "{"
        "\"error\":{"
          "\"message\":\"Incorrect API key provided.\","
          "\"type\":\"invalid_request_error\","
          "\"code\":\"invalid_api_key\""
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("error response parsed", resp != NULL);
    if (resp) {
        TEST("error message has xAI prefix", strstr(resp->content, "xAI API error") != NULL);
        TEST("error details present", strstr(resp->content, "Incorrect API key") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "");
    TEST("malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Malformed JSON */
    const char *bad_json = "{this is not valid json}";
    provider_response_t *resp = ops->parse_response(p, bad_json);
    TEST("malformed response not null", resp != NULL);
    if (resp) {
        TEST("error message contains JSON parse error",
             strstr(resp->content, "JSON parse error") != NULL);
        ops->free_response(resp);
    }

    /* Empty string */
    const char *empty = "";
    provider_response_t *resp2 = ops->parse_response(p, empty);
    TEST("empty response parsed", resp2 != NULL);
    if (resp2) ops->free_response(resp2);

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("xai", "grok-4.3", "sk-test", "");
    TEST("stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Null chunk */
    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("null chunk parsed", r1 != NULL);
    if (r1) { TEST("null chunk empty", strcmp(r1->content, "") == 0); ops->free_response(r1); }

    /* [DONE] marker */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("done marker parsed", r2 != NULL);
    if (r2) { TEST("done marker empty", strcmp(r2->content, "") == 0); ops->free_response(r2); }

    /* Normal content delta */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"},\"index\":0}]}");
    TEST("content delta parsed", r3 != NULL);
    if (r3) {
        TEST("delta content correct", strcmp(r3->content, "Hello") == 0);
        ops->free_response(r3);
    }

    /* Finish reason */
    provider_response_t *r4 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\",\"index\":0}]}");
    TEST("finish_reason chunk parsed", r4 != NULL);
    if (r4) {
        TEST("finish_reason is stop", strcmp(r4->finish_reason, "stop") == 0);
        ops->free_response(r4);
    }

    /* Non-prefixed content (passthrough) */
    provider_response_t *r6 = ops->parse_stream_chunk(p, "raw data");
    TEST("non-prefixed passthrough", r6 != NULL);
    if (r6) {
        TEST("passthrough content", strcmp(r6->content, "raw data") == 0);
        ops->free_response(r6);
    }

    /* Encrypted content in stream (xAI-specific) */
    provider_response_t *r7 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\"encrypted response\",\"encrypted_content\":[{\"type\":\"text\",\"text\":\"secret\"}]},\"index\":0}]}");
    TEST("encrypted content stream", r7 != NULL);
    if (r7) {
        TEST("encrypted_content set", r7->encrypted_content != NULL);
        ops->free_response(r7);
    }

    provider_free(p);
}

static void test_model_retirement(void)
{
    /* xai_is_model_retired is declared extern in provider_xai.c but not in a header.
     * Test the retirement logic through the provider ops or direct call.
     * The function is non-static, so it's accessible via extern declaration. */

    /* Test known retired models */
    {
        char replacement[64];
        char reasoning[64];
        bool retired = xai_is_model_retired("grok-3", replacement, sizeof(replacement),
                                             reasoning, sizeof(reasoning));
        TEST("grok-3 is retired", retired == true);
        if (retired) {
            TEST("grok-3 replacement is grok-4.3", strcmp(replacement, "grok-4.3") == 0);
        }
    }

    {
        char replacement[64];
        bool retired = xai_is_model_retired("x-ai/grok-3", replacement, sizeof(replacement), NULL, 0);
        TEST("x-ai/grok-3 prefix stripped and retired", retired == true);
        if (retired) {
            TEST("x-ai/grok-3 replacement", strcmp(replacement, "grok-4.3") == 0);
        }
    }

    {
        char replacement[64];
        char reasoning[64];
        bool retired = xai_is_model_retired("grok-4-fast-non-reasoning", replacement,
                                             sizeof(replacement), reasoning, sizeof(reasoning));
        TEST("grok-4-fast-non-reasoning retired", retired == true);
        if (retired) {
            TEST("replacement set", replacement[0] != '\0');
            TEST("reasoning_effort=none", strcmp(reasoning, "none") == 0);
        }
    }

    /* Current model is not retired */
    {
        bool retired = xai_is_model_retired("grok-4.3", NULL, 0, NULL, 0);
        TEST("grok-4.3 not retired", retired == false);
    }

    /* Unknown model is not retired */
    {
        bool retired = xai_is_model_retired("fake-model", NULL, 0, NULL, 0);
        TEST("unknown model not retired", retired == false);
    }

    /* NULL input */
    {
        bool retired = xai_is_model_retired(NULL, NULL, 0, NULL, 0);
        TEST("NULL model not retired", retired == false);
    }

    /* Empty input */
    {
        bool retired = xai_is_model_retired("", NULL, 0, NULL, 0);
        TEST("empty model not retired", retired == false);
    }

    /* xai prefix */
    {
        char replacement[64];
        bool retired = xai_is_model_retired("xai/grok-3", replacement, sizeof(replacement), NULL, 0);
        TEST("xai/grok-3 prefix stripped", retired == true);
    }
}

int main(void)
{
    /* Register xAI provider */
    provider_register(PROVIDER_XAI, &PROVIDER_OPS_XAI);
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_with_tool_calls();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk();
    test_model_retirement();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
