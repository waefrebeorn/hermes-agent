/* DeepSeek provider tests — verify response parsing, URL, headers, FIM.
 *
 * DeepSeek is OpenAI-compatible but adds: reasoning_content streaming,
 * FIM (Fill-in-the-Middle) endpoint, x-ds-cache-ttl headers.
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
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);

    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    char *url = ops->build_url(p, "https://api.deepseek.com/v1");
    TEST("ds url built", url != NULL);
    if (url) {
        TEST("ds url has /chat/completions", strstr(url, "/chat/completions") != NULL);
        free(url);
    }

    /* NULL base URL */
    char *url2 = ops->build_url(p, NULL);
    TEST("ds url NULL base", url2 != NULL);
    if (url2) free(url2);

    /* URL already has /chat/completions */
    char *url3 = ops->build_url(p, "https://api.deepseek.com/v1/chat/completions");
    TEST("ds url already has chat", url3 != NULL);
    if (url3) {
        TEST("used as-is", strcmp(url3, "https://api.deepseek.com/v1/chat/completions") == 0);
        free(url3);
    }

    provider_free(p);
}

static void test_headers_with_cache_ttl(void)
{
    /* Test with default cache TTL */
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-key", "");
    TEST("ds header provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_headers) { provider_free(p); return; }

    /* Default TTL (300) should include x-ds-cache-ttl */
    char *h = ops->build_headers(p, "sk-ds-key");
    TEST("ds headers built", h != NULL);
    if (h) {
        TEST("ds has Bearer", strstr(h, "Bearer") != NULL);
        TEST("ds has key", strstr(h, "sk-ds-key") != NULL);
        TEST("ds has x-ds-cache-ttl", strstr(h, "x-ds-cache-ttl") != NULL);
        free(h);
    }

    /* NULL api_key */
    char *h2 = ops->build_headers(p, NULL);
    TEST("ds headers NULL key", h2 != NULL);
    if (h2) {
        TEST("no Bearer when NULL key", strstr(h2, "Bearer") == NULL);
        TEST("still has cache ttl", strstr(h2, "x-ds-cache-ttl") != NULL);
        free(h2);
    }

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{"
        "\"id\":\"chatcmpl-ds123\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"Hello from DeepSeek!\""
          "},"
          "\"finish_reason\":\"stop\""
        "}],"
        "\"usage\":{\"prompt_tokens\":20,\"completion_tokens\":15}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("ds response parsed", resp != NULL);
    if (resp) {
        TEST("ds content", resp->content != NULL && strcmp(resp->content, "Hello from DeepSeek!") == 0);
        TEST("ds input_tokens", resp->input_tokens == 20);
        TEST("ds output_tokens", resp->output_tokens == 15);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_reasoning(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-reasoner", "sk-ds-test", "");
    TEST("ds reasoning provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* DeepSeek R1-style response with reasoning_content */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-r1\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"The answer.\","
            "\"reasoning_content\":\"Step-by-step reasoning...\""
          "}"
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("ds reasoning parsed", resp != NULL);
    if (resp) {
        TEST("ds reasoning content", resp->reasoning != NULL);
        if (resp->reasoning) TEST("ds reasoning text", strcmp(resp->reasoning, "Step-by-step reasoning...") == 0);
        TEST("ds content still present", resp->content != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    const char *json =
        "{\"error\":{\"message\":\"Insufficient quota\",\"type\":\"insufficient_quota\",\"code\":\"quota_exceeded\"}}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("ds error parsed", resp != NULL);
    if (resp) {
        TEST("ds error msg", strstr(resp->content, "Insufficient quota") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    provider_response_t *r1 = ops->parse_response(p, "{}");
    TEST("ds empty json", r1 != NULL); if (r1) ops->free_response(r1);

    provider_response_t *r2 = ops->parse_response(p, "not json");
    TEST("ds invalid json", r2 != NULL); if (r2) ops->free_response(r2);

    provider_response_t *r3 = ops->parse_response(p, NULL);
    TEST("ds NULL body", r3 != NULL); if (r3) ops->free_response(r3);

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Standard content delta */
    provider_response_t *r1 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}");
    TEST("ds stream content", r1 != NULL);
    if (r1) { TEST("ds stream text", r1->content && strcmp(r1->content, "Hello") == 0); ops->free_response(r1); }

    /* Reasoning content delta */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"\",\"reasoning_content\":\"Let me think\"},\"finish_reason\":null}]}");
    TEST("ds stream reasoning", r2 != NULL);
    if (r2) {
        TEST("ds stream reasoning text", r2->reasoning && strcmp(r2->reasoning, "Let me think") == 0);
        ops->free_response(r2);
    }

    /* [DONE] */
    provider_response_t *r3 = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("ds stream DONE", r3 != NULL);
    if (r3) { TEST("ds stream DONE empty", r3->content && strlen(r3->content) == 0); ops->free_response(r3); }

    /* NULL */
    provider_response_t *r4 = ops->parse_stream_chunk(p, NULL);
    TEST("ds stream NULL", r4 != NULL); if (r4) ops->free_response(r4);

    provider_free(p);
}

static void test_fim_url(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds FIM provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_fim_url) { provider_free(p); return; }

    /* Standard base URL → /beta/completions */
    char *url = ops->build_fim_url(p, "https://api.deepseek.com/v1");
    TEST("ds fim url built", url != NULL);
    if (url) {
        TEST("ds fim has /beta/completions", strstr(url, "/beta/completions") != NULL);
        TEST("ds fim not chat", strstr(url, "/chat/completions") == NULL);
        free(url);
    }

    /* NULL base URL */
    char *url2 = ops->build_fim_url(p, NULL);
    TEST("ds fim NULL base", url2 != NULL);
    if (url2) free(url2);

    provider_free(p);
}

static void test_fim_body(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds fim body provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_fim_body) { provider_free(p); return; }

    char *body = ops->build_fim_body(p, "def hello():", "    return 42", 128);
    TEST("ds fim body built", body != NULL);
    if (body) {
        TEST("ds fim has prompt", strstr(body, "def hello()") != NULL);
        TEST("ds fim has suffix", strstr(body, "return 42") != NULL);
        TEST("ds fim has max_tokens", strstr(body, "128") != NULL);
        TEST("ds fim has model", strstr(body, "deepseek-chat") != NULL);
        TEST("ds fim has stream=false", strstr(body, "false") != NULL);
        free(body);
    }

    /* NULL prompt and suffix */
    char *body2 = ops->build_fim_body(p, NULL, NULL, 0);
    TEST("ds fim NULL params", body2 != NULL);
    if (body2) free(body2);

    provider_free(p);
}

static void test_parse_fim_response_basic(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds fim resp provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_fim_response) { provider_free(p); return; }

    /* FIM response uses choices[0].text (not message.content!) */
    const char *json =
        "{\"id\":\"fim-123\",\"choices\":[{\"index\":0,\"text\":\"def hello():\\n    pass\"}],"
        "\"usage\":{\"prompt_tokens\":10,\"completion_tokens\":5}}";

    provider_response_t *resp = ops->parse_fim_response(p, json);
    TEST("ds fim resp parsed", resp != NULL);
    if (resp) {
        TEST("ds fim text", resp->content != NULL);
        if (resp->content) TEST("ds fim completion", strstr(resp->content, "pass") != NULL);
        ops->free_response(resp);
    }

    /* Error response */
    provider_response_t *r2 = ops->parse_fim_response(p,
        "{\"error\":{\"message\":\"FIM not supported for this model\"}}");
    TEST("ds fim error", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* NULL */
    provider_response_t *r3 = ops->parse_fim_response(p, NULL);
    TEST("ds fim NULL", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    provider_free(p);
}

static void test_free_response_null(void)
{
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds free provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->free_response) { provider_free(p); return; }

    ops->free_response(NULL);
    TEST("ds free_response(NULL) safe", 1);

    provider_free(p);
}

int main(void)
{
    test_url_building();
    test_headers_with_cache_ttl();
    test_parse_response_basic();
    test_parse_response_with_reasoning();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk();
    test_fim_url();
    test_fim_body();
    test_parse_fim_response_basic();
    test_free_response_null();

    printf("provider_deepseek: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
