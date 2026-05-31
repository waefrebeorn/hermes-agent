/* OpenAI provider tests — verify response parsing, URL building, headers.
 *
 * Uses a provider instance to call ops through the function pointer table.
 * No network access — tests are purely structural/parsing.
 */

#include "hermes.h"
#include "hermes_json.h"
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
    provider_register(PROVIDER_OPENAI, &PROVIDER_OPS_OPENAI);

    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "https://api.openai.com/v1");
    TEST("provider created", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    TEST("ops not null", ops != NULL);
    if (!ops || !ops->build_url) { provider_free(p); return; }

    char *url = ops->build_url(p, "https://api.openai.com/v1");
    TEST("url built", url != NULL);
    if (url) {
        TEST("url contains chat/completions", strstr(url, "chat/completions") != NULL);
        TEST("url starts with base", strstr(url, "api.openai.com") != NULL);
        free(url);
    }

    /* Test with NULL base_url */
    char *url2 = ops->build_url(p, NULL);
    TEST("url with NULL base", url2 != NULL);
    free(url2);

    provider_free(p);
}

static void test_headers(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test-key", "https://api.openai.com/v1");
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

    /* Test with NULL api_key */
    char *h2 = ops->build_headers(p, NULL);
    TEST("headers with NULL key", h2 != NULL);
    free(h2);

    provider_free(p);
}

static void test_parse_response_basic(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "https://api.openai.com/v1");
    TEST("response parse provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Standard OpenAI chat completion response */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-123\","
        "\"object\":\"chat.completion\","
        "\"created\":1234567890,"
        "\"model\":\"gpt-4\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"Hello! How can I help you today?\""
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
        TEST("content correct", strcmp(resp->content, "Hello! How can I help you today?") == 0);
        TEST("input_tokens", resp->input_tokens == 10);
        TEST("output_tokens", resp->output_tokens == 8);
        TEST("finish_reason matches", strcmp(resp->finish_reason, "") == 0 || strcmp(resp->finish_reason, "stop") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_tool_calls(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
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
        TEST("input_tokens parsed", resp->input_tokens == 50);
        TEST("output_tokens parsed", resp->output_tokens == 20);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_with_reasoning(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("reasoning provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* DeepSeek-style response with reasoning_content */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-reason\","
        "\"choices\":[{"
          "\"index\":0,"
          "\"message\":{"
            "\"role\":\"assistant\","
            "\"content\":\"The answer is 42.\","
            "\"reasoning_content\":\"Let me think about this step by step...\""
          "},"
          "\"finish_reason\":\"stop\""
        "}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("reasoning response parsed", resp != NULL);
    if (resp) {
        TEST("content present", resp->content != NULL);
        TEST("reasoning present", resp->reasoning != NULL);
        if (resp->reasoning)
            TEST("reasoning content", strstr(resp->reasoning, "step by step") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_error(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("error provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* API error response */
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
        TEST("error message", strstr(resp->content, "Incorrect API key") != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_malformed(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("malformed provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty JSON */
    provider_response_t *r1 = ops->parse_response(p, "{}");
    TEST("empty json parsed", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Invalid JSON */
    provider_response_t *r2 = ops->parse_response(p, "not json at all");
    TEST("invalid json parsed", r2 != NULL);
    if (r2) {
        TEST("error message in content", r2->content != NULL);
        ops->free_response(r2);
    }

    /* NULL */
    provider_response_t *r3 = ops->parse_response(p, NULL);
    TEST("NULL body", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    /* Empty string */
    provider_response_t *r4 = ops->parse_response(p, "");
    TEST("empty string", r4 != NULL);
    if (r4) { ops->free_response(r4); }

    provider_free(p);
}

static void test_parse_stream_chunk(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("stream provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Standard OpenAI streaming chunk with content delta (SSE format) */
    const char *chunk1 =
        "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\","
        "\"choices\":[{\"index\":0,\"delta\":{\"content\":\"Hello\"},\"finish_reason\":null}]}";

    provider_response_t *r1 = ops->parse_stream_chunk(p, chunk1);
    TEST("stream chunk parsed", r1 != NULL);
    if (r1) {
        TEST("chunk content extracted", r1->content != NULL);
        if (r1->content) TEST("chunk content is Hello", strcmp(r1->content, "Hello") == 0);
        ops->free_response(r1);
    }

    /* [DONE] sentinel */
    provider_response_t *r_done = ops->parse_stream_chunk(p, "data: [DONE]");
    TEST("DONE chunk parsed", r_done != NULL);
    if (r_done) {
        TEST("DONE content empty", r_done->content != NULL && strlen(r_done->content) == 0);
        ops->free_response(r_done);
    }

    /* Chunk with finish_reason */
    const char *chunk2 =
        "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\","
        "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}";

    provider_response_t *r2 = ops->parse_stream_chunk(p, chunk2);
    TEST("finish chunk parsed", r2 != NULL);
    if (r2) {
        /* Content should be empty string for finish chunk with no delta content */
        TEST("finish reason stop", strcmp(r2->finish_reason, "stop") == 0);
        ops->free_response(r2);
    }

    /* Tool call streaming chunk (SSE format) */
    const char *chunk3 =
        "data: {\"id\":\"chatcmpl-123\",\"object\":\"chat.completion.chunk\","
        "\"choices\":[{\"index\":0,\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\","
        "\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"loc\"}}]},\"finish_reason\":null}]}";

    provider_response_t *r3 = ops->parse_stream_chunk(p, chunk3);
    TEST("tool call chunk parsed", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    /* NULL chunk */
    provider_response_t *r4 = ops->parse_stream_chunk(p, NULL);
    TEST("NULL chunk handled", r4 != NULL);
    if (r4) { ops->free_response(r4); }

    /* Empty chunk */
    provider_response_t *r5 = ops->parse_stream_chunk(p, "");
    TEST("empty chunk handled", r5 != NULL);
    if (r5) { ops->free_response(r5); }

    provider_free(p);
}

static void test_free_response_null(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("free provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->free_response) { provider_free(p); return; }

    /* Should not crash */
    ops->free_response(NULL);
    TEST("free_response(NULL) safe", 1);

    provider_free(p);
}

static void test_url_building_edge_cases(void)
{
    /* Trailing slash variants */
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "https://api.openai.com/v1/");
    TEST("trailing slash provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p, "https://api.openai.com/v1/");
            TEST("url with trailing slash base", url != NULL);
            if (url) {
                TEST("trailing slash url has chat/completions", strstr(url, "chat/completions") != NULL);
                /* No double slash */
                TEST("no double slash", strstr(url, "//chat") == NULL && strstr(url, "v1//") == NULL);
                free(url);
            }
        }
        provider_free(p);
    }

    /* Custom base URL with path */
    p = provider_create("openai", "gpt-4", "sk-test", "https://my-proxy.example.com/company/v1");
    TEST("custom path provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p, "https://my-proxy.example.com/company/v1");
            TEST("custom path url", url != NULL);
            if (url) {
                TEST("custom path preserved", strstr(url, "my-proxy.example.com") != NULL);
                TEST("company path preserved", strstr(url, "/company/v1") != NULL);
                free(url);
            }
        }
        provider_free(p);
    }

    /* Empty base URL */
    p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("empty base provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p, "");
            TEST("empty base url", url != NULL);
            free(url);
        }
        provider_free(p);
    }
}

static void test_headers_edge_cases(void)
{
    /* Empty API key */
    provider_t *p = provider_create("openai", "gpt-4", "", "https://api.openai.com/v1");
    TEST("empty key provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            char *h = ops->build_headers(p, "");
            TEST("headers with empty key", h != NULL);
            free(h);
        }
        provider_free(p);
    }

    /* Organization ID header */
    p = provider_create("openai", "gpt-4", "sk-real-key", "https://api.openai.com/v1");
    TEST("org provider", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            char *h = ops->build_headers(p, "sk-real-key");
            TEST("headers with org key", h != NULL);
            if (h) {
                TEST("has Authorization", strstr(h, "Authorization") != NULL);
                TEST("has sk- prefix in Bearer", strstr(h, "sk-real-key") != NULL);
                free(h);
            }
        }
        provider_free(p);
    }
}

static void test_parse_response_multiple_tool_calls(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("multi tool provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Response with 3 tool calls */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-multi\","
        "\"choices\":[{\"index\":0,\"message\":{"
          "\"role\":\"assistant\",\"content\":null,"
          "\"tool_calls\":["
            "{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"city\\\":\\\"Paris\\\"}\"}},"
            "{\"id\":\"call_2\",\"type\":\"function\",\"function\":{\"name\":\"search_web\",\"arguments\":\"{\\\"q\\\":\\\"latest news\\\"}\"}},"
            "{\"id\":\"call_3\",\"type\":\"function\",\"function\":{\"name\":\"send_email\",\"arguments\":\"{\\\"to\\\":\\\"a@b.com\\\",\\\"subject\\\":\\\"hello\\\"}\"}}"
          "]"
        "},\"finish_reason\":\"tool_calls\"}],"
        "\"usage\":{\"prompt_tokens\":100,\"completion_tokens\":50}"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("multi tool parsed", resp != NULL);
    if (resp) {
        TEST("3 tool calls", resp->tool_calls_count == 3);
        TEST("first call id", strcmp(resp->tool_calls[0].id, "call_1") == 0);
        TEST("first call name", strcmp(resp->tool_calls[0].name, "get_weather") == 0);
        TEST("second call name", strcmp(resp->tool_calls[1].name, "search_web") == 0);
        TEST("third call name", strcmp(resp->tool_calls[2].name, "send_email") == 0);
        TEST("third call args has to field", strstr(resp->tool_calls[2].arguments, "a@b.com") != NULL);
        TEST("finish reason is tool_calls", strcmp(resp->finish_reason, "tool_calls") == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_tool_call_no_args(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("noargs tool provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Tool call with no arguments */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-noargs\","
        "\"choices\":[{\"index\":0,\"message\":{"
          "\"role\":\"assistant\",\"content\":null,"
          "\"tool_calls\":[{\"id\":\"call_noargs\",\"type\":\"function\","
            "\"function\":{\"name\":\"list_files\",\"arguments\":\"{}\"}}]"
        "},\"finish_reason\":\"tool_calls\"}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("noargs tool parsed", resp != NULL);
    if (resp) {
        TEST("one tool call", resp->tool_calls_count == 1);
        TEST("name is list_files", strcmp(resp->tool_calls[0].name, "list_files") == 0);
        /* Empty object for args */
        TEST("args is {} or empty", resp->tool_calls[0].arguments != NULL);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_response_null_content(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("null content provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Content is null, no tool_calls */
    const char *json =
        "{"
        "\"id\":\"chatcmpl-nullc\","
        "\"choices\":[{\"index\":0,\"message\":{"
          "\"role\":\"assistant\",\"content\":null"
        "},\"finish_reason\":\"stop\"}]"
        "}";

    provider_response_t *resp = ops->parse_response(p, json);
    TEST("null content parsed", resp != NULL);
    if (resp) {
        TEST("content is NULL or empty", resp->content == NULL || strlen(resp->content) == 0);
        ops->free_response(resp);
    }

    provider_free(p);
}

static void test_parse_stream_chunk_edge_cases(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("stream edge provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Chunk with only whitespace */
    provider_response_t *r1 = ops->parse_stream_chunk(p, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\" \"},\"finish_reason\":null}]}");
    TEST("whitespace chunk parsed", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Chunk with role delta (first chunk) */
    provider_response_t *r2 = ops->parse_stream_chunk(p, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\"},\"finish_reason\":null}]}");
    TEST("role delta chunk parsed", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* Streaming chunk with reasoning content */
    provider_response_t *r3 = ops->parse_stream_chunk(p, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"thinking\",\"reasoning_content\":\"Let me calculate\"},\"finish_reason\":null}]}");
    TEST("reasoning stream chunk parsed", r3 != NULL);
    if (r3) {
        TEST("reasoning in stream chunk", r3->reasoning != NULL);
        ops->free_response(r3);
    }

    /* Chunk with finish_reason=length */
    provider_response_t *r4 = ops->parse_stream_chunk(p, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"length\"}]}");
    TEST("length finish chunk parsed", r4 != NULL);
    if (r4) {
        TEST("finish reason is length", strcmp(r4->finish_reason, "length") == 0);
        ops->free_response(r4);
    }

    /* Chunk with finish_reason=content_filter */
    provider_response_t *r5 = ops->parse_stream_chunk(p, "data: {\"object\":\"chat.completion.chunk\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"content_filter\"}]}");
    TEST("content_filter finish chunk parsed", r5 != NULL);
    if (r5) {
        TEST("finish reason is content_filter", strcmp(r5->finish_reason, "content_filter") == 0);
        ops->free_response(r5);
    }

    /* Non-data line (comment/event) */
    provider_response_t *r6 = ops->parse_stream_chunk(p, "event: done\ndata: [DONE]");
    TEST("event + data line parsed", r6 != NULL);
    if (r6) { ops->free_response(r6); }

    provider_free(p);
}

static void test_parse_response_empty(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("empty response provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Response with empty choices array */
    provider_response_t *r1 = ops->parse_response(p, "{\"id\":\"empty\",\"object\":\"chat.completion\",\"choices\":[],\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0}}");
    TEST("empty choices parsed", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* Response with no choices key */
    provider_response_t *r2 = ops->parse_response(p, "{\"id\":\"nochoices\",\"object\":\"chat.completion\"}");
    TEST("no choices key parsed", r2 != NULL);
    if (r2) { ops->free_response(r2); }

    /* Response with choices null */
    provider_response_t *r3 = ops->parse_response(p, "{\"id\":\"nullchoices\",\"choices\":null}");
    TEST("null choices parsed", r3 != NULL);
    if (r3) { ops->free_response(r3); }

    provider_free(p);
}

static void test_build_request_body_basic(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("body provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_request_body) { provider_free(p); return; }

    /* Build a simple message list (array of pointers) */
    message_t m0, m1;
    memset(&m0, 0, sizeof(m0));
    memset(&m1, 0, sizeof(m1));
    m0.role = MSG_SYSTEM;
    m0.content = strdup("You are a helpful assistant.");
    m1.role = MSG_USER;
    m1.content = strdup("Hello!");
    const message_t *msgs[] = {&m0, &m1};

    char *body = ops->build_request_body(p, msgs, 2, NULL, false);
    TEST("body built", body != NULL);
    if (body) {
        TEST("body has model name", strstr(body, "gpt-4") != NULL);
        TEST("body has system message", strstr(body, "helpful assistant") != NULL);
        TEST("body has user message", strstr(body, "Hello!") != NULL);
        TEST("body is JSON", body[0] == '{');
        free(body);
    }

    free(m0.content);
    free(m1.content);

    provider_free(p);
}

static void test_build_request_body_tools(void)
{
    provider_t *p = provider_create("openai", "gpt-4", "sk-test", "");
    TEST("body tools provider", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->build_request_body) { provider_free(p); return; }

    message_t m0;
    memset(&m0, 0, sizeof(m0));
    m0.role = MSG_USER;
    m0.content = strdup("What's the weather?");

    const message_t *msgs[] = {&m0};

    /* Simple tool schema */
    json_node_t *tools = json_parse(
        "[{\"type\":\"function\",\"function\":{\"name\":\"get_weather\","
        "\"description\":\"Get weather\",\"parameters\":{\"type\":\"object\","
        "\"properties\":{\"location\":{\"type\":\"string\"}},\"required\":[\"location\"]}}}]",
        NULL
    );

    char *body = ops->build_request_body(p, msgs, 1, tools, false);
    TEST("body with tools built", body != NULL);
    if (body) {
        TEST("body has tools key", strstr(body, "tools") != NULL);
        TEST("body has get_weather", strstr(body, "get_weather") != NULL);
        free(body);
    }

    json_free(tools);
    free(m0.content);
    provider_free(p);
}

int main(void)
{
    test_url_building();
    test_headers();
    test_parse_response_basic();
    test_parse_response_with_tool_calls();
    test_parse_response_with_reasoning();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_stream_chunk();
    test_free_response_null();
    test_url_building_edge_cases();
    test_headers_edge_cases();
    test_parse_response_multiple_tool_calls();
    test_parse_response_tool_call_no_args();
    test_parse_response_null_content();
    test_parse_stream_chunk_edge_cases();
    test_parse_response_empty();
    test_build_request_body_basic();
    test_build_request_body_tools();

    printf("provider_openai: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
