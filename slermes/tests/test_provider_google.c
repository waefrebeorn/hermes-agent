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

/* === URL edge cases === */
static void test_url_edge_cases(void)
{
    provider_register(PROVIDER_GOOGLE, &PROVIDER_OPS_GOOGLE);

    /* URL already containing :streamGenerateContent */
    provider_t *p1 = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("url edge p1", p1 != NULL);
    if (p1) {
        const provider_ops_t *ops = provider_ops(p1);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p1, "https://custom.example.com/v1/models/gemini-pro:streamGenerateContent");
            TEST("url stream endpoint", url != NULL);
            if (url) {
                TEST("url used as-is with stream", strcmp(url, "https://custom.example.com/v1/models/gemini-pro:streamGenerateContent") == 0);
                free(url);
            }
        }
        provider_free(p1);
    }

    /* Proxy URL with path */
    provider_t *p2 = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("url edge p2", p2 != NULL);
    if (p2) {
        const provider_ops_t *ops = provider_ops(p2);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p2, "https://proxy.example.com/gemini/v1beta");
            TEST("url proxy", url != NULL);
            if (url) {
                TEST("url has proxy base", strstr(url, "proxy.example.com") != NULL);
                TEST("url has :generateContent", strstr(url, ":generateContent") != NULL);
                free(url);
            }
        }
        provider_free(p2);
    }

    /* First ops: vtable is built by provider_create, so this is fine */
    /* Provider created without base URL and with empty model name */
    provider_t *p3 = provider_create("google", "", "AIza-test", "");
    TEST("url edge p3", p3 != NULL);
    if (p3) {
        const provider_ops_t *ops = provider_ops(p3);
        if (ops && ops->build_url) {
            char *url = ops->build_url(p3, NULL);
            TEST("url null base empty model", url != NULL);
            if (url) {
                TEST("url default model gemini-2.0-flash", strstr(url, "gemini-2.0-flash") != NULL);
                free(url);
            }
        }
        provider_free(p3);
    }
}

/* === Header edge cases === */
static void test_header_edge_cases(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "", "");
    TEST("header edge p", p != NULL);
    if (p) {
        const provider_ops_t *ops = provider_ops(p);
        if (ops && ops->build_headers) {
            /* Empty API key */
            char *h1 = ops->build_headers(p, "");
            TEST("header empty key", h1 != NULL);
            if (h1) {
                TEST("no x-goog-api-key with empty key", strstr(h1, "x-goog-api-key") == NULL);
                free(h1);
            }

            /* NULL API key */
            char *h2 = ops->build_headers(p, NULL);
            TEST("header NULL key", h2 != NULL);
            if (h2) {
                TEST("no x-goog-api-key with NULL", strstr(h2, "x-goog-api-key") == NULL);
                free(h2);
            }

            /* Long API key */
            char *long_key = "AIzaSyDxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
            char *h3 = ops->build_headers(p, long_key);
            TEST("header long key", h3 != NULL);
            if (h3) {
                TEST("has x-goog-api-key with long key", strstr(h3, "x-goog-api-key") != NULL);
                TEST("has long key value", strstr(h3, long_key) != NULL);
                free(h3);
            }
        }
        provider_free(p);
    }
}

/* === Finish reason mapping (response parsing) === */
static void test_parse_response_finish_reasons(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("finish reason p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    struct { const char *reason; const char *expected; } cases[] = {
        {"STOP", "stop"},
        {"MAX_TOKENS", "length"},
        {"SAFETY", "content_filter"},
        {"RECITATION", "content_filter"},
        {"OTHER", "stop"},
        {"BLOCKLIST", "content_filter"},
        {"PROHIBITED_CONTENT", "content_filter"},
        {"SPAM", "content_filter"},
        {"IMAGE_SAFETY", "content_filter"},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < ncases; i++) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"hello\"}]},"
            "\"finishReason\":\"%s\"}]}", cases[i].reason);
        provider_response_t *r = ops->parse_response(p, json);
        TEST("finish reason parsed", r != NULL);
        if (r) {
            TEST("finish reason matches", strcmp(r->finish_reason, cases[i].expected) == 0);
            ops->free_response(r);
        }
    }

    provider_free(p);
}

/* === Content blocked by safety (no content text) === */
static void test_parse_response_content_blocked(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("blocked p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* SAFETY finish with no content parts */
    const char *json =
        "{\"candidates\":[{\"finishReason\":\"SAFETY\","
        "\"safetyRatings\":[{\"category\":\"HARM_CATEGORY_HARASSMENT\",\"probability\":\"HIGH\"}]}]}";
    provider_response_t *r = ops->parse_response(p, json);
    TEST("blocked parsed", r != NULL);
    if (r) {
        TEST("blocked content message", r->content != NULL);
        if (r->content)
            TEST("has blocked msg", strstr(r->content, "blocked") != NULL);
        TEST("finish reason content_filter", strcmp(r->finish_reason, "content_filter") == 0);
        ops->free_response(r);
    }

    /* BLOCKLIST finish with no content */
    const char *json2 =
        "{\"candidates\":[{\"finishReason\":\"BLOCKLIST\"}]}";
    provider_response_t *r2 = ops->parse_response(p, json2);
    TEST("blocklist parsed", r2 != NULL);
    if (r2) {
        TEST("blocklist blocked msg", r2->content && strstr(r2->content, "blocked") != NULL);
        TEST("blocklist finish reason", strcmp(r2->finish_reason, "content_filter") == 0);
        ops->free_response(r2);
    }

    /* PROHIBITED_CONTENT finish with no content */
    const char *json3 =
        "{\"candidates\":[{\"finishReason\":\"PROHIBITED_CONTENT\"}]}";
    provider_response_t *r3 = ops->parse_response(p, json3);
    TEST("prohibited parsed", r3 != NULL);
    if (r3) {
        TEST("prohibited blocked msg", r3->content && strstr(r3->content, "blocked") != NULL);
        TEST("prohibited finish reason", strcmp(r3->finish_reason, "content_filter") == 0);
        ops->free_response(r3);
    }

    provider_free(p);
}

/* === Response parsing: empty/no candidates === */
static void test_parse_response_no_candidates(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("no candidates p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_response) { provider_free(p); return; }

    /* Empty candidates array */
    const char *json1 = "{\"candidates\":[]}";
    provider_response_t *r1 = ops->parse_response(p, json1);
    TEST("empty candidates", r1 != NULL);
    if (r1) {
        TEST("empty candidates safe", 1);
        ops->free_response(r1);
    }

    /* No candidates key */
    const char *json2 = "{\"usageMetadata\":{\"promptTokenCount\":5,\"candidatesTokenCount\":3}}";
    provider_response_t *r2 = ops->parse_response(p, json2);
    TEST("no candidates key", r2 != NULL);
    if (r2) {
        TEST("no candidates usage tokens", r2->input_tokens == 5 && r2->output_tokens == 3);
        ops->free_response(r2);
    }

    /* Candidates with no content key */
    const char *json3 = "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";
    provider_response_t *r3 = ops->parse_response(p, json3);
    TEST("candidate no content", r3 != NULL);
    if (r3) {
        TEST("candidate no content safe", 1);
        ops->free_response(r3);
    }

    /* Candidates with empty parts (content remains NULL) */
    const char *json4 = "{\"candidates\":[{\"content\":{\"parts\":[]},\"finishReason\":\"STOP\"}]}";
    provider_response_t *r4 = ops->parse_response(p, json4);
    TEST("empty parts", r4 != NULL);
    if (r4) {
        TEST("empty parts null content", r4->content == NULL);
        ops->free_response(r4);
    }

    provider_free(p);
}

/* === google_is_native_base_url tests === */
static void test_is_native_base_url(void)
{
    /* Standard Google URL */
    TEST("native standard", google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta") == true);
    TEST("native with slash", google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta/") == true);

    /* OpenAI-compat endpoint (ends with /openai) */
    TEST("not native openai compat", google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta/openai") == false);

    /* Custom URLs */
    TEST("not native custom", google_is_native_base_url("https://api.example.com/v1") == false);
    TEST("not native empty domain", google_is_native_base_url("https://other.com") == false);

    /* NULL/empty */
    TEST("not native NULL", google_is_native_base_url(NULL) == false);
    TEST("not native empty", google_is_native_base_url("") == false);
}

/* === google_coerce_content_to_text tests === */
static void test_google_coerce_content_to_text(void)
{
    /* NULL input */
    char *r0 = google_coerce_content_to_text(NULL);
    TEST("coerce NULL", r0 != NULL && r0[0] == '\0');
    free(r0);

    /* String input */
    json_t *str = json_string("hello world");
    char *r1 = google_coerce_content_to_text(str);
    TEST("coerce string", r1 != NULL && strcmp(r1, "hello world") == 0);
    free(r1);
    json_free(str);

    /* Empty string */
    json_t *estr = json_string("");
    char *r2 = google_coerce_content_to_text(estr);
    TEST("coerce empty string", r2 != NULL && r2[0] == '\0');
    free(r2);
    json_free(estr);

    /* Array of strings */
    json_t *arr = json_array();
    json_append(arr, json_string("part1"));
    json_append(arr, json_string("part2"));
    json_append(arr, json_string("part3"));
    char *r3 = google_coerce_content_to_text(arr);
    TEST("coerce array strings", r3 != NULL);
    if (r3) {
        TEST("coerce array has part1", strstr(r3, "part1") != NULL);
        TEST("coerce array has part2", strstr(r3, "part2") != NULL);
        TEST("coerce array has part3", strstr(r3, "part3") != NULL);
        TEST("coerce array newline separated", strstr(r3, "part1\npart2") != NULL);
    }
    free(r3);
    json_free(arr);

    /* Array with object having type=text */
    json_t *obj = json_object();
    json_set(obj, "type", json_string("text"));
    json_set(obj, "text", json_string("object text"));
    json_t *arr2 = json_array();
    json_append(arr2, json_copy(obj));
    char *r4 = google_coerce_content_to_text(arr2);
    TEST("coerce text object", r4 != NULL && strcmp(r4, "object text") == 0);
    free(r4);
    json_free(arr2);
    json_free(obj);

    /* Empty array */
    json_t *empty_arr = json_array();
    char *r5 = google_coerce_content_to_text(empty_arr);
    TEST("coerce empty array", r5 != NULL && r5[0] == '\0');
    free(r5);
    json_free(empty_arr);
}

/* === Streaming finish reason depth === */
static void test_parse_stream_finish_reason_depth(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("stream finish p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* SAFETY finish reason in stream */
    provider_response_t *r1 = ops->parse_stream_chunk(p,
        "{\"candidates\":[{\"finishReason\":\"SAFETY\"}]}");
    TEST("stream SAFETY", r1 != NULL);
    if (r1) {
        TEST("stream SAFETY finish", strcmp(r1->finish_reason, "content_filter") == 0);
        ops->free_response(r1);
    }

    /* MAX_TOKENS finish reason in stream */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "{\"candidates\":[{\"finishReason\":\"MAX_TOKENS\"}]}");
    TEST("stream MAX_TOKENS", r2 != NULL);
    if (r2) {
        TEST("stream MAX_TOKENS finish", strcmp(r2->finish_reason, "length") == 0);
        ops->free_response(r2);
    }

    /* SPAM finish reason in stream */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "{\"candidates\":[{\"finishReason\":\"SPAM\"}]}");
    TEST("stream SPAM", r3 != NULL);
    if (r3) {
        TEST("stream SPAM finish", strcmp(r3->finish_reason, "content_filter") == 0);
        ops->free_response(r3);
    }

    provider_free(p);
}

/* === Streaming empty candidates === */
static void test_parse_stream_empty_candidates(void)
{
    provider_t *p = provider_create("google", "gemini-2.0-flash", "AIza-test", "");
    TEST("stream empty p", p != NULL);
    if (!p) return;

    const provider_ops_t *ops = provider_ops(p);
    if (!ops || !ops->parse_stream_chunk) { provider_free(p); return; }

    /* Empty candidates array */
    provider_response_t *r1 = ops->parse_stream_chunk(p,
        "{\"candidates\":[]}");
    TEST("stream empty candidates", r1 != NULL);
    if (r1) { ops->free_response(r1); }

    /* No candidates key (usage only) */
    provider_response_t *r2 = ops->parse_stream_chunk(p,
        "{\"usageMetadata\":{\"promptTokenCount\":5,\"candidatesTokenCount\":3}}");
    TEST("stream usage only", r2 != NULL);
    if (r2) {
        TEST("stream usage input", r2->input_tokens == 5);
        TEST("stream usage output", r2->output_tokens == 3);
        ops->free_response(r2);
    }

    /* Candidates with no content key */
    provider_response_t *r3 = ops->parse_stream_chunk(p,
        "{\"candidates\":[{\"finishReason\":\"STOP\"}]}");
    TEST("stream candidate no content", r3 != NULL);
    if (r3) {
        TEST("stream finish stop", strcmp(r3->finish_reason, "stop") == 0);
        ops->free_response(r3);
    }

    provider_free(p);
}

int main(void)
{
    test_url_building();
    test_url_edge_cases();
    test_headers();
    test_header_edge_cases();
    test_parse_response_basic();
    test_parse_response_multiple_parts();
    test_parse_response_function_call();
    test_parse_response_error();
    test_parse_response_malformed();
    test_parse_response_finish_reasons();
    test_parse_response_content_blocked();
    test_parse_response_no_candidates();
    test_is_native_base_url();
    test_google_coerce_content_to_text();
    test_parse_stream_chunk_text();
    test_parse_stream_finish_reason();
    test_parse_stream_finish_reason_depth();
    test_parse_stream_empty_candidates();
    test_parse_stream_usage();
    test_parse_stream_edge_cases();
    test_free_response_null();

    printf("provider_google: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
