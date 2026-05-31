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

static void test_url_building_edge_cases(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds url edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    /* Trailing slash */
    char *u1 = ops->build_url(p, "https://api.deepseek.com/");
    TEST("ds trailing slash url", u1 != NULL);
    if (u1) {
        TEST("ds no double slash", strstr(u1, "//chat") == NULL);
        free(u1);
    }

    /* Custom proxy with path */
    char *u2 = ops->build_url(p, "https://proxy.example.com/company/v1");
    TEST("ds proxy url", u2 != NULL);
    if (u2) {
        TEST("ds proxy has /chat/completions", strstr(u2, "/chat/completions") != NULL);
        TEST("ds proxy preserved", strstr(u2, "proxy.example.com") != NULL);
        free(u2);
    }

    /* Empty base */
    char *u3 = ops->build_url(p, "");
    TEST("ds empty base", u3 != NULL);
    free(u3);

    provider_free(p);
}

static void test_headers_edge_cases(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);

    /* Empty API key */
    provider_t *p = provider_create("deepseek", "deepseek-chat", "", "");
    TEST("ds empty key provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            char *h = ops->build_headers(p, "");
            TEST("ds headers with empty key", h != NULL);
            free(h);
        }
        provider_free(p);
    }

    /* Negative cache TTL (edge case) */
    p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds neg ttl provider", p != NULL);
    if (p) {
        p->config.deepseek_cache_ttl = -1;
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            char *h = ops->build_headers(p, "sk-ds-test");
            TEST("ds headers with neg ttl", h != NULL);
            free(h);
        }
        provider_free(p);
    }
}

static void test_parse_response_with_tool_calls(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds tool call provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Single tool call (OpenAI-compatible) */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-dstc\","
        "\"choices\":[{\"index\":0,\"message\":{"
          "\"role\":\"assistant\",\"content\":null,"
          "\"tool_calls\":[{\"id\":\"call_ds1\",\"type\":\"function\","
            "\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Beijing\\\"}\"}}]"
        "},\"finish_reason\":\"tool_calls\"}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("ds tool call parsed", resp != NULL);
    if (resp) {
        TEST("ds tool count 1", resp->tool_calls_count == 1);
        TEST("ds tool id", strcmp(resp->tool_calls[0].id, "call_ds1") == 0);
        TEST("ds tool name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("ds tool args has Beijing", strstr(resp->tool_calls[0].arguments, "Beijing") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_reasoning_and_tools(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-reasoner", "sk-ds-test", "");
    TEST("ds reasoning+tool provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* DeepSeek response with both reasoning and tool call */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-dsrt\","
        "\"choices\":[{\"index\":0,\"message\":{"
          "\"role\":\"assistant\",\"content\":null,"
          "\"reasoning_content\":\"I need to check weather first...\","
          "\"tool_calls\":[{\"id\":\"call_rt1\",\"type\":\"function\","
            "\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Shanghai\\\"}\"}}]"
        "},\"finish_reason\":\"tool_calls\"}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("ds reasoning+tool parsed", resp != NULL);
    if (resp) {
        TEST("ds reasoning present", resp->reasoning != NULL);
        TEST("ds tool count 1", resp->tool_calls_count == 1);
        TEST("ds tool name correct", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_stream_chunk_edge_cases(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds stream edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Chunk with finish_reason=length */
    provider_response_t *r1 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}");
    TEST("ds stream length finish", r1 != NULL);
    if (r1) {
        TEST("ds length finish reason", strcmp(r1->finish_reason, "length") == 0);
        ops->free_response(r1);
    }

    /* Chunk with finish_reason=content_filter */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"content_filter\"}]}");
    TEST("ds stream content_filter finish", r2 != NULL);
    if (r2) {
        TEST("ds content_filter finish reason", strcmp(r2->finish_reason, "content_filter") == 0);
        ops->free_response(r2);
    }

    /* Chunk with role delta (first chunk) */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\"},\"finish_reason\":null}]}");
    TEST("ds stream role delta", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    /* Chunk with both content and reasoning */
    provider_response_t *r4 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"answer\",\"reasoning_content\":\"think\"},\"finish_reason\":null}]}");
    TEST("ds stream content+reasoning", r4 != NULL);
    if (r4) {
        TEST("ds stream reasoning text", r4->reasoning && strcmp(r4->reasoning, "think") == 0);
        ops->free_response(r4);
    }

    /* Whitespace chunk */
    provider_response_t *r5 = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\" \"},\"finish_reason\":null}]}");
    TEST("ds stream whitespace content", r5 != NULL);
    if (r5) { ops->free_response(r5); }

    /* Empty string chunk */
    provider_response_t *r6 = ops->parse_stream_chunk(p, "");
    TEST("ds stream empty string", r6 != NULL);
    if (r6) { ops->free_response(r6); }

    provider_free(p);
}

static void test_fim_response_edge_cases(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds fim edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_fim_response) { provider_free(p); return; }

    /* FIM response with empty text */
    provider_response_t *r1 = ops->parse_fim_response(p,
        "{\"id\":\"fim-e\",\"choices\":[{\"index\":0,\"text\":\"\"}],\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0}}");
    TEST("ds fim empty text", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* FIM response with no choices */
    provider_response_t *r2 = ops->parse_fim_response(p,
        "{\"id\":\"fim-nc\",\"choices\":[]}");
    TEST("ds fim no choices", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* Empty JSON */
    provider_response_t *r3 = ops->parse_fim_response(p, "{}");
    TEST("ds fim empty json", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    /* Invalid JSON */
    provider_response_t *r4 = ops->parse_fim_response(p, "broken json");
    TEST("ds fim broken json", r4 != NULL);
    if (r4) { ops->free_response(r4); }

    provider_free(p);
}

static void test_fim_body_edge_cases(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds fim body edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_fim_body) { provider_free(p); return; }

    /* Empty prompt, non-empty suffix */
    char *b1 = ops->build_fim_body(p, "", "    pass", 50);
    TEST("ds fim empty prompt", b1 != NULL);
    if (b1) { free(b1); }

    /* Empty suffix, non-empty prompt */
    char *b2 = ops->build_fim_body(p, "def foo():", "", 50);
    TEST("ds fim empty suffix", b2 != NULL);
    if (b2) { free(b2); }

    /* Very long prompt (2000+ chars) */
    char long_prompt[2048];
    memset(long_prompt, 'x', sizeof(long_prompt) - 1);
    long_prompt[sizeof(long_prompt) - 1] = '\0';
    char *b3 = ops->build_fim_body(p, long_prompt, "return", 100);
    TEST("ds fim long prompt", b3 != NULL);
    if (b3) { free(b3); }

    /* Zero max_tokens */
    char *b4 = ops->build_fim_body(p, "test", "test", 0);
    TEST("ds fim zero max_tokens", b4 != NULL);
    if (b4) { free(b4); }

    provider_free(p);
}

static void test_parse_response_empty_choices(void)
{
    provider_register(PROVIDER_DEEPSEEK, &PROVIDER_OPS_DEEPSEEK);
    provider_t *p = provider_create("deepseek", "deepseek-chat", "sk-ds-test", "");
    TEST("ds empty choice provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Response with empty choices array */
    provider_response_t *r1 = ops->parse_response(p,
        "{\"id\":\"empty_choice\",\"choices\":[],\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0}}");
    TEST("ds empty choices", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Response with no choices key */
    provider_response_t *r2 = ops->parse_response(p,
        "{\"id\":\"no_choice\",\"object\":\"chat.completion\"}");
    TEST("ds no choices key", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* Response with null choices */
    provider_response_t *r3 = ops->parse_response(p,
        "{\"id\":\"null_choice\",\"choices\":null}");
    TEST("ds null choices", r3 != NULL);
    if (r3) { ops->free_response(r3); }

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
    test_url_building_edge_cases();
    test_headers_edge_cases();
    test_parse_response_with_tool_calls();
    test_parse_response_with_reasoning_and_tools();
    test_parse_stream_chunk_edge_cases();
    test_fim_response_edge_cases();
    test_fim_body_edge_cases();
    test_parse_response_empty_choices();

    printf("provider_deepseek: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
