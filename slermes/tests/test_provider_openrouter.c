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

/* === URL edge cases (double-slash fix, proxy, trailing slash) === */
static void test_url_edge_cases(void)
{
    provider_register(PROVIDER_OPENROUTER, &PROVIDER_OPS_OPENROUTER);

    /* Double-slash fix */
    provider_t *p1 = provider_create("openrouter", "openai/gpt-4o", "sk-test", "https://openrouter.ai/api/v1/");
    TEST("url edge p1", p1 != NULL);
    if (p1) {
        const provider_ops_t *ops = provider_ops(p1);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p1, "https://openrouter.ai/api/v1/");
            TEST("url trailing", url != NULL);
            if (url) {
                TEST("no double slash", strstr(url, "//chat") == NULL);
                TEST("has chat/completions", strstr(url, "/chat/completions") != NULL);
                free(url);
            }
        }
        provider_free(p1);
    }

    /* Proxy path */
    provider_t *p2 = provider_create("openrouter", "openai/gpt-4o", "sk-test", "https://proxy.example.com/or/v1");
    TEST("url edge p2", p2 != NULL);
    if (p2) {
        const provider_ops_t *ops = provider_ops(p2);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p2, "https://proxy.example.com/or/v1");
            TEST("url proxy", url != NULL);
            if (url) {
                TEST("proxy base preserverd", strstr(url, "proxy.example.com") != NULL);
                free(url);
            }
        }
        provider_free(p2);
    }

    /* Empty base → default */
    provider_t *p3 = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("url edge p3", p3 != NULL);
    if (p3) {
        const provider_ops_t *ops = provider_ops(p3);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p3, "");
            TEST("url empty base", url != NULL);
            if (url) {
                TEST("default openrouter.ai", strstr(url, "openrouter.ai") != NULL);
                free(url);
            }
        }
        provider_free(p3);
    }
}

/* === Header edge cases === */
static void test_header_edge_cases(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "", "");
    TEST("header edge p", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            /* Empty key */
            char *h1 = ops->build_headers(p, "");
            TEST("header empty key", h1 != NULL);
            if (h1) {
                TEST("no Bearer with empty key", strstr(h1, "Bearer") == NULL);
                TEST("still has Referer", strstr(h1, "HTTP-Referer") != NULL);
                free(h1);
            }

            /* NULL key */
            char *h2 = ops->build_headers(p, NULL);
            TEST("header NULL key", h2 != NULL);
            if (h2) {
                TEST("no Bearer with NULL", strstr(h2, "Bearer") == NULL);
                TEST("still has X-Title", strstr(h2, "X-Title") != NULL);
                free(h2);
            }

            /* Long key */
            char *long_key = "sk-or-test-key-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
            char *h3 = ops->build_headers(p, long_key);
            TEST("header long key", h3 != NULL);
            if (h3) {
                TEST("Bearer present", strstr(h3, "Bearer") != NULL);
                TEST("long key value", strstr(h3, long_key) != NULL);
                free(h3);
            }
        }
        provider_free(p);
    }
}

/* === Response edge cases === */
static void test_parse_response_edge_cases(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("edge parse p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty choices array */
    provider_response_t *r1 = ops->parse_response(p,
        "{\"choices\":[],\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":3}}");
    TEST("empty choices", r1 != NULL);
    if (r1) {
        TEST("empty choices tokens", r1->input_tokens == 5 && r1->output_tokens == 3);
        ops->free_response(r1);
    }

    /* No choices key */
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

    /* No usage metadata */
    provider_response_t *r5 = ops->parse_response(p,
        "{\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"no usage\"},\"finish_reason\":\"stop\"}]}");
    TEST("no usage", r5 != NULL);
    if (r5) {
        TEST("no usage zero tokens", r5->input_tokens == 0 && r5->output_tokens == 0);
        ops->free_response(r5);
    }

    provider_free(p);
}

/* === Reasoning content in streaming (OpenRouter-specific) === */
static void test_parse_stream_reasoning(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
    TEST("stream reason p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* P22: reasoning_content in stream delta */
    provider_response_t *r = ops->parse_stream_chunk(p,
        "data: {\"choices\":[{\"delta\":{\"content\":\"answer\",\"reasoning_content\":\"thinking...\"},\"index\":0}]}");
    TEST("stream reasoning", r != NULL);
    if (r) {
        TEST("reasoning content", r->content && strcmp(r->content, "answer") == 0);
        ops->free_response(r);
    }

    provider_free(p);
}

/* === Streaming edge depth === */
static void test_parse_stream_edge_depth(void)
{
    provider_t *p = provider_create("openrouter", "openai/gpt-4o", "sk-test", "");
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
    provider_register(PROVIDER_OPENROUTER, &PROVIDER_OPS_OPENROUTER);
    test_url_building();
    test_url_edge_cases();
    test_headers();
    test_header_edge_cases();
    test_parse_response_basic();
    test_parse_response_with_reasoning();
    test_parse_response_with_tool_calls();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_response_edge_cases();
    test_parse_stream_chunk();
    test_parse_stream_reasoning();
    test_parse_stream_edge_depth();

    fprintf(stderr, "Results: %d passed, %d failed\n", passed, tests - passed);
    fprintf(stdout, "Results: %d passed, %d failed\n", passed, tests - passed);
    return passed == tests ? 0 : 1;
}
