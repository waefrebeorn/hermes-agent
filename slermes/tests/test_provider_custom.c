/* Custom provider tests — OpenAI-compatible passthrough.
 *
 * Default base: http://localhost:8080/v1 (user-configurable)
 * Standard Bearer auth. OpenAI-compatible response format.
 * Error prefix: "Custom provider API error:"
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
    provider_register(PROVIDER_CUSTOM, &PROVIDER_OPS_CUSTOM);

    provider_t *p = provider_create("custom", "my-model", "sk-test", "http://localhost:8080/v1");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    char *url = ops->build_url(p, "http://localhost:8080/v1");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url has chat/completions", strstr(url, "chat/completions") != NULL);
        TEST("url uses localhost", strstr(url, "localhost:8080") != NULL);
        free(url);
    }

    /* NULL base → default localhost */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    if (url2) {
        TEST("default is localhost", strstr(url2, "localhost:8080") != NULL);
        free(url2);
    }

    /* Full URL pass through */
    char *url3 = ops->build_url(p, "http://localhost:8080/v1/chat/completions");
    TEST("full path preserved", url3 != NULL);
    if (url3) {
        TEST("exact match", strcmp(url3, "http://localhost:8080/v1/chat/completions") == 0);
        free(url3);
    }

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test-key", "http://localhost:8080/v1");
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

    char *h2 = ops->build_headers(p, NULL);
    TEST("NULL key headers", h2 != NULL);
    free(h2);

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
    TEST("response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"choices\":[{"
          "\"message\":{"
            "\"content\":\"Hello from custom!\""
          "},"
          "\"finish_reason\":\"stop\""
        "}],"
        "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("response parsed", resp != NULL);
    if (resp) {
        TEST("content correct", strcmp(resp->content, "Hello from custom!") == 0);
        TEST("input_tokens", resp->input_tokens == 5);
        TEST("output_tokens", resp->output_tokens == 3);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"choices\":[{"
          "\"message\":{"
            "\"content\":null,"
            "\"tool_calls\":[{"
              "\"id\":\"call_custom\","
              "\"function\":{"
                "\"name\":\"custom_tool\","
                "\"arguments\":\"{}\""
              "}"
            "}]"
          "}"
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("tool call parsed", resp != NULL);
    if (resp) {
        TEST("tool call count", resp->tool_calls_count == 1);
        TEST("tool call name", strcmp(resp->tool_calls[0].name, "custom_tool") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json = "{\"error\":{\"message\":\"Not found\"}}";
    provider_response_t *resp = ops->parse_response(p, json);
    TEST("error parsed", resp != NULL);
    if (resp) {
        TEST("Custom prefix", strstr(resp->content, "Custom provider API error") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
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
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
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
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"},\"index\":0}]}");
    TEST("content delta", r3 != NULL);
    if (r3) {
        TEST("delta correct", strcmp(r3->content, "Hi") == 0);
        ops->free_response(r3);
    }

    provider_response_t *r5 = ops->parse_stream_chunk(p, "raw");
    TEST("non-prefixed", r5 != NULL);
    if (r5) {
        TEST("passthrough", r5->content && r5->content[0] == '\0');
        ops->free_response(r5);
    }

    provider_free(p);
}

int main(void)
{
    provider_register(PROVIDER_CUSTOM, &PROVIDER_OPS_CUSTOM);
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_with_tool_calls();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
