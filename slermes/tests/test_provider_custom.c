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

/* === URL edge cases === */
static void test_url_edge_cases(void)
{
    provider_register(PROVIDER_CUSTOM, &PROVIDER_OPS_CUSTOM);

    /* Double-slash fix */
    provider_t *p1 = provider_create("custom", "my-model", "sk-test", "http://localhost:8080/v1/");
    TEST("url edge p1", p1 != NULL);
    if (p1) {
        const provider_ops_t *ops = provider_ops(p1);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p1, "http://localhost:8080/v1/");
            TEST("url trailing", url != NULL);
            if (url) {
                TEST("no double slash", strstr(url, "//chat") == NULL);
                free(url);
            }
        }
        provider_free(p1);
    }

    /* Proxy path */
    provider_t *p2 = provider_create("custom", "my-model", "sk-test", "https://proxy.example.com/custom/v1");
    TEST("url edge p2", p2 != NULL);
    if (p2) {
        const provider_ops_t *ops = provider_ops(p2);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p2, "https://proxy.example.com/custom/v1");
            TEST("url proxy", url != NULL);
            if (url) {
                TEST("proxy preserved", strstr(url, "proxy.example.com") != NULL);
                free(url);
            }
        }
        provider_free(p2);
    }

    /* Empty base → default */
    provider_t *p3 = provider_create("custom", "my-model", "sk-test", "");
    TEST("url edge p3", p3 != NULL);
    if (p3) {
        const provider_ops_t *ops = provider_ops(p3);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p3, "");
            TEST("url empty base", url != NULL);
            if (url) {
                TEST("default localhost", strstr(url, "localhost:8080") != NULL);
                free(url);
            }
        }
        provider_free(p3);
    }
}

/* === Header edge cases === */
static void test_header_edge_cases(void)
{
    provider_t *p = provider_create("custom", "my-model", "", "");
    TEST("header edge p", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            /* Empty key */
            char *h1 = ops->build_headers(p, "");
            TEST("header empty key", h1 != NULL);
            if (h1) {
                TEST("no Bearer empty key", strstr(h1, "Bearer") == NULL);
                free(h1);
            }

            /* NULL key */
            char *h2 = ops->build_headers(p, NULL);
            TEST("header NULL key", h2 != NULL);
            if (h2) {
                TEST("no Bearer NULL key", strstr(h2, "Bearer") == NULL);
                free(h2);
            }

            /* Long key */
            char *long_key = "sk-custom-key-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
            char *h3 = ops->build_headers(p, long_key);
            TEST("header long key", h3 != NULL);
            if (h3) {
                TEST("has Bearer", strstr(h3, "Bearer") != NULL);
                TEST("has long key", strstr(h3, long_key) != NULL);
                free(h3);
            }
        }
        provider_free(p);
    }
}

/* === Response edge cases === */
static void test_parse_response_edge_cases(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
    TEST("edge parse p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty choices */
    provider_response_t *r1 = ops->parse_response(p,
        "{\"choices\":[],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}");
    TEST("empty choices", r1 != NULL);
    if (r1) {
        TEST("empty choices tokens", r1->input_tokens == 5 && r1->output_tokens == 3);
        ops->free_response(r1);
    }

    /* No choices */
    provider_response_t *r2 = ops->parse_response(p,
        "{\"usage\":{\"prompt_tokens\":7,\"completion_tokens\":2}}");
    TEST("no choices", r2 != NULL);
    if (r2) {
        TEST("no choices tokens", r2->input_tokens == 7 && r2->output_tokens == 2);
        ops->free_response(r2);
    }

    /* Null content */
    provider_response_t *r3 = ops->parse_response(p,
        "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":null},\"finish_reason\":\"stop\"}]}");
    TEST("null content", r3 != NULL);
    if (r3) {
        TEST("null content empty", r3->content && strlen(r3->content) == 0);
        ops->free_response(r3);
    }

    /* Finish reason = length */
    provider_response_t *r4 = ops->parse_response(p,
        "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"partial\"},\"finish_reason\":\"length\"}]}");
    TEST("length finish", r4 != NULL);
    if (r4) {
        TEST("length content", r4->content && strcmp(r4->content, "partial") == 0);
        TEST("length finish_reason", strcmp(r4->finish_reason, "length") == 0);
        ops->free_response(r4);
    }

    /* No usage */
    provider_response_t *r5 = ops->parse_response(p,
        "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"no usage\"},\"finish_reason\":\"stop\"}]}");
    TEST("no usage", r5 != NULL);
    if (r5) {
        TEST("no usage zero tokens", r5->input_tokens == 0 && r5->output_tokens == 0);
        ops->free_response(r5);
    }

    provider_free(p);
}

/* === Streaming edge depth === */
static void test_parse_stream_edge_depth(void)
{
    provider_t *p = provider_create("custom", "my-model", "sk-test", "");
    TEST("stream depth p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Empty delta */
    provider_response_t *r1 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{},\"index\":0}]}");
    TEST("empty delta", r1 != NULL);
    if (r1) {
        TEST("empty delta empty", r1->content && strlen(r1->content) == 0);
        ops->free_response(r1);
    }

    /* Empty chunk */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "");
    TEST("empty chunk", r2 != NULL);
    if (r2) {
        TEST("empty chunk safe", strcmp(r2->content, "") == 0);
        ops->free_response(r2);
    }

    /* Length finish */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"length\",\"index\":0}]}");
    TEST("stream length", r3 != NULL);
    if (r3) {
        TEST("stream length reason", strcmp(r3->finish_reason, "length") == 0);
        ops->free_response(r3);
    }

    /* Whitespace */
    provider_response_t *r4 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\" \"},\"index\":0}]}");
    TEST("whitespace", r4 != NULL);
    if (r4) {
        TEST("whitespace preserved", r4->content && strcmp(r4->content, " ") == 0);
        ops->free_response(r4);
    }

    provider_free(p);
}

int main(void)
{
    provider_register(PROVIDER_CUSTOM, &PROVIDER_OPS_CUSTOM);
    test_url_building();
    test_url_edge_cases();
    test_headers();
    test_header_edge_cases();
    test_parse_response_basic();
    test_parse_response_with_tool_calls();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_response_edge_cases();
    test_parse_stream_chunk();
    test_parse_stream_edge_depth();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
