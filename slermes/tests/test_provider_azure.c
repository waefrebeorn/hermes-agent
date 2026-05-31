/* Azure OpenAI provider tests — URL format, api-key header, parse, stream.
 *
 * Azure differs from OpenAI:
 * - URL: {base}/openai/deployments/{deploy}/chat/completions?api-version={ver}
 * - Auth: api-key header (not Bearer)
 * - Deployment name = config azure_deployment_id or model name
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
    provider_register(PROVIDER_AZURE, &PROVIDER_OPS_AZURE);

    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "https://my-resource.openai.azure.com");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Standard Azure URL with deployment = model name */
    char *url = ops->build_url(p, "https://my-resource.openai.azure.com");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url starts with resource", strstr(url, "my-resource.openai.azure.com") != NULL);
        TEST("url has deployments path", strstr(url, "/openai/deployments/") != NULL);
        TEST("url has deployment name", strstr(url, "gpt-4o") != NULL);
        TEST("url has chat/completions", strstr(url, "chat/completions") != NULL);
        TEST("url has api-version", strstr(url, "api-version=") != NULL);
        free(url);
    }

    /* NULL base → default Azure URL */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    if (url2) {
        TEST("default url uses azure.com", strstr(url2, "azure.com") != NULL);
        free(url2);
    }

    provider_free(p);
}

static void test_url_with_deployment_id(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "https://my-resource.openai.azure.com");
    TEST("deploy provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Set deployment ID via config (simulate by modifying model) */
    /* Default: deployment = model name (gpt-4o) */
    char *url = ops->build_url(p, "https://my-resource.openai.azure.com");
    TEST("deploy url contains model", url != NULL);
    if (url) {
        TEST("model used as deployment", strstr(url, "gpt-4o") != NULL);
        free(url);
    }

    provider_free(p);
}

static void test_url_trailing_slash(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "https://my-resource.openai.azure.com/");
    TEST("trailing slash provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Trailing slash should be stripped */
    char *url = ops->build_url(p, "https://my-resource.openai.azure.com/");
    TEST("trailing slash url built", url != NULL);
    if (url) {
        TEST("no double slash", strstr(url, "azure.com//openai") == NULL);
        free(url);
    }

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test-key", "https://my-resource.openai.azure.com");
    TEST("header provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_headers) { provider_free(p); return; }

    char *h = ops->build_headers(p, "sk-test-key");
    TEST("headers built", h != NULL);
    if (h) {
        TEST("has api-key (not Bearer)", strstr(h, "api-key:") != NULL);
        TEST("has test key", strstr(h, "sk-test-key") != NULL);
        TEST("has Content-Type", strstr(h, "application/json") != NULL);
        TEST("no Bearer prefix", strstr(h, "Bearer") == NULL);
        free(h);
    }

    /* NULL api_key → headers without api-key */
    char *h2 = ops->build_headers(p, NULL);
    TEST("headers with NULL key", h2 != NULL);
    free(h2);

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "https://my-resource.openai.azure.com");
    TEST("response parse provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Standard Azure/OpenAI chat completion response */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-123\","
        "\"object\":\"chat.completion\","
        "\"created\":1234567890,"
        "\"model\":\"gpt-4o\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"Hello from Azure!\""
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
        TEST("content correct", strcmp(resp->content, "Hello from Azure!") == 0);
        TEST("input_tokens", resp->input_tokens == 10);
        TEST("output_tokens", resp->output_tokens == 8);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "");
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
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "");
    TEST("error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Azure API error response */
    const char *json =
        "{"
        "\"error\":{"
          "\"message\":\"Access denied due to invalid subscription key.\","
          "\"type\":\"access_denied\","
          "\"code\":\"401\""
        "}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("error response parsed", resp != NULL);
    if (resp) {
        TEST("error message has Azure prefix", strstr(resp->content, "Azure API error") != NULL);
        TEST("error details present", strstr(resp->content, "Access denied") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "");
    TEST("malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Malformed JSON */
    const char *bad_json = "{not valid}";
    provider_response_t *resp = ops->parse_response(p, bad_json);
    TEST("malformed response not null", resp != NULL);
    if (resp) {
        TEST("error message contains JSON parse",
             strstr(resp->content, "JSON parse error") != NULL);
        ops->free_response(resp);
    }

    /* Empty string */
    provider_response_t *resp2 = ops->parse_response(p, "");
    TEST("empty response parsed", resp2 != NULL);
    if (resp2) ops->free_response(resp2);

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("azure", "gpt-4o", "sk-test", "");
    TEST("stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Null chunk */
    provider_response_t *r1 = ops->parse_stream_chunk(p, NULL);
    TEST("null chunk", r1 != NULL);
    if (r1) { TEST("null chunk empty", strcmp(r1->content, "") == 0); ops->free_response(r1); }

    /* [DONE] marker */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("done marker", r2 != NULL);
    if (r2) { TEST("done empty", strcmp(r2->content, "") == 0); ops->free_response(r2); }

    /* Content delta */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"},\"index\":0}]}");
    TEST("content delta", r3 != NULL);
    if (r3) {
        TEST("delta correct", strcmp(r3->content, "Hi") == 0);
        ops->free_response(r3);
    }

    /* Finish reason */
    provider_response_t *r4 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\",\"index\":0}]}");
    TEST("finish_reason chunk", r4 != NULL);
    if (r4) {
        TEST("finish_reason stop", strcmp(r4->finish_reason, "stop") == 0);
        ops->free_response(r4);
    }

    /* Non-prefixed passthrough */
    provider_response_t *r5 = ops->parse_stream_chunk(p, "raw data");
    TEST("non-prefixed passthrough", r5 != NULL);
    if (r5) {
        TEST("passthrough content", r5->content && r5->content[0] == '\0');
        ops->free_response(r5);
    }

    provider_free(p);
}

int main(void)
{
    /* Register Azure provider */
    provider_register(PROVIDER_AZURE, &PROVIDER_OPS_AZURE);
    test_url_building();
    test_url_with_deployment_id();
    test_url_trailing_slash();
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
