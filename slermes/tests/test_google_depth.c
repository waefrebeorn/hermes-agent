/*
 * test_google_depth.c — B30-B34: Google provider depth tests
 *
 * Tests: build_url, build_headers, build_request_body (top_k, candidate_count,
 * system_instruction, generationConfig, safety_settings, tools, streaming),
 * parse_response (text, function_call, error, blocked).
 */
#include "hermes.h"
#include "hermes_json.h"
#include "provider.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

static const message_t user_msg = {.role = MSG_USER, .content = (char*)"hello"};
static const message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"Be helpful"};
static const message_t *msgs1[] = {&user_msg};
static const message_t *msgs_with_sys[] = {&sys_msg, &user_msg};

static int test_original_suite(void) {
    printf("=== B30-B34: Google provider depth ===\n\n");

    /* ── build_url edge cases ────────────────────────────────────── */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gemini-2.0-flash");
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, NULL);
        TEST("build_url has model", url && strstr(url, "gemini-2.0-flash") != NULL);
        TEST("build_url has generateContent", url && strstr(url, ":generateContent") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        /* Empty model uses default */
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, NULL);
        TEST("build_url empty model uses gemini-2.0-flash", url && strstr(url, "gemini-2.0-flash") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gemini-pro");
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, "https://custom.endpoint.com/v1");
        TEST("build_url custom base", url && strstr(url, "custom.endpoint.com") != NULL);
        TEST("build_url custom base keeps model", url && strstr(url, "gemini-pro") != NULL);
        free(url);
    }
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gemini-pro");
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, "https://custom.endpoint.com/v1/");
        TEST("build_url trailing slash stripped", url && strstr(url, "models/gemini-pro") != NULL);
        TEST("build_url no double slash", url && strstr(url, "//models") == NULL);
        free(url);
    }
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, "https://api.example.com/models/gemini-pro:generateContent");
        TEST("build_url existing generateContent preserved", url && strcmp(url, "https://api.example.com/models/gemini-pro:generateContent") == 0);
        free(url);
    }

    /* ── build_headers ──────────────────────────────────────────── */
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_GOOGLE.build_headers(&p, "AIzaSyTest123");
        TEST("headers x-goog-api-key present", h && strstr(h, "x-goog-api-key: AIzaSyTest123") != NULL);
        TEST("headers Content-Type", h && strstr(h, "Content-Type: application/json") != NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_GOOGLE.build_headers(&p, "");
        TEST("headers empty key no x-goog-api-key", h && strstr(h, "x-goog-api-key") == NULL);
        free(h);
    }
    {
        provider_t p = {0};
        char *h = PROVIDER_OPS_GOOGLE.build_headers(&p, NULL);
        TEST("headers NULL key valid", h != NULL);
        free(h);
    }

    /* B30: top_k + candidate_count in gen_config */
    {
        provider_t p = {0};
        p.config.top_k = 40;
        p.config.candidate_count = 2;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("topK present", body && strstr(body, "\"topK\":40"));
        TEST("candidateCount present", body && strstr(body, "\"candidateCount\":2"));
        free(body);
    }

    /* B30: defaults (0) omit both fields */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("topK omitted when 0", body && !strstr(body, "\"topK\""));
        TEST("candidateCount omitted when 0", body && !strstr(body, "\"candidateCount\""));
        free(body);
    }

    /* B31: system_instruction from MSG_SYSTEM messages */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs_with_sys, 2, NULL, false);
        TEST("systemInstruction present", body && strstr(body, "\"systemInstruction\""));
        TEST("systemInstruction has text", body && strstr(body, "Be helpful"));
        TEST("contents has user role", body && strstr(body, "\"role\":\"user\""));
        free(body);
    }

    /* B31: no system_instruction when no MSG_SYSTEM */
    {
        provider_t p = {0};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("no systemInstruction when absent", body && !strstr(body, "\"systemInstruction\""));
        free(body);
    }

    /* B32: generationConfig with temperature, top_p, top_k, stop sequences */
    {
        provider_t p = {0};
        p.config.temperature = 0.8f;
        p.config.top_p = 0.9f;
        p.config.top_k = 20;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "END");
        p.config.stop_count = 1;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("genConfig temperature in body", body && strstr(body, "\"temperature\":0.8"));
        TEST("genConfig topP in body", body && strstr(body, "\"topP\":0.899") != NULL);
        TEST("genConfig topK in body", body && strstr(body, "\"topK\":20"));
        TEST("genConfig stopSequences in body", body && strstr(body, "\"stopSequences\""));
        TEST("stop sequence END in body", body && strstr(body, "\"END\""));
        free(body);
    }

    /* B32: generationConfig defaults (maxOutputTokens) */
    {
        provider_t p = {0};
        p.config.max_tokens = 0; /* should use 4096 default */
        p.config.temperature = -1.0f; /* should omit */
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("maxOutputTokens default 4096", body && strstr(body, "\"maxOutputTokens\":4096"));
        TEST("no temperature when negative", body && strstr(body, "\"temperature\"") == NULL);
        free(body);
    }

    /* B33: contents array with multiple messages */
    {
        provider_t p = {0};
        message_t assist = {.role = MSG_ASSISTANT, .content = (char*)"I can help"};
        const message_t *msgs_multi[] = {&user_msg, &assist, &user_msg};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs_multi, 3, NULL, false);
        TEST("contents has 3 entries", body && strstr(body, "\"contents\""));
        TEST("user role present", body && strstr(body, "\"role\":\"user\""));
        TEST("model role present", body && strstr(body, "\"role\":\"model\""));
        TEST("text hello present", body && strstr(body, "hello"));
        TEST("text \"I can help\" present", body && strstr(body, "I can help"));
        free(body);
    }

    /* B33: streaming flag adds streamGenerateContent endpoint */
    {
        provider_t p = {0};
        /* build_request_body doesn't change the URL, the streaming URL
         * is chosen by the caller based on stream=true.
         * The body itself should not change much for streaming. */
        char *body_stream = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, true);
        char *body_no_stream = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs1, 1, NULL, false);
        TEST("streaming body is valid", body_stream != NULL);
        TEST("non-streaming body is valid", body_no_stream != NULL);
        free(body_stream);
        free(body_no_stream);
    }

    /* B34: parse_response — normal text */
    {
        const char *resp = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello from Gemini\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"promptTokenCount\":5,\"candidatesTokenCount\":3}}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, resp);
        TEST("parse text content", r && r->content && strcmp(r->content, "Hello from Gemini") == 0);
        TEST("parse input tokens", r && r->input_tokens == 5);
        TEST("parse output tokens", r && r->output_tokens == 3);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Function call response */
        const char *resp = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"I'll search\"},{\"functionCall\":{\"name\":\"web_search\",\"args\":{\"q\":\"test\"}}}],\"role\":\"model\"},\"finishReason\":\"STOP\"}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, resp);
        TEST("parse fc text", r && r->content && strcmp(r->content, "I'll search") == 0);
        TEST("parse fc count 1", r && r->tool_calls_count == 1);
        TEST("parse fc name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("parse fc args", r && strstr(r->tool_calls[0].arguments, "test") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Error response */
        const char *resp = "{\"error\":{\"code\":400,\"message\":\"API key not valid\",\"status\":\"INVALID_ARGUMENT\"}}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, resp);
        TEST("parse error", r && r->content && strstr(r->content, "API key not valid") != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Blocked response (finishReason = SAFETY) */
        const char *resp = "{\"candidates\":[{\"finishReason\":\"SAFETY\",\"safetyRatings\":[{\"category\":\"HARM_CATEGORY_DANGEROUS\",\"probability\":\"HIGH\"}]}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, resp);
        TEST("parse blocked response", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Multiple text parts */
        const char *resp = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Part A\"},{\"text\":\"Part B\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, resp);
        TEST("parse multi-part", r && r->content && strcmp(r->content, "Part APart B") == 0);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Null body */
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, NULL);
        TEST("parse null body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }
    {
        /* Empty body */
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(NULL, "");
        TEST("parse empty body", r != NULL);
        if (r) { free(r->content); free(r->reasoning); free(r); }
    }

    /* ── google_is_native_base_url ──────────────────────────── */
    printf("\n--- google_is_native_base_url ---\n");
    {
        TEST("native_base_url NULL", !google_is_native_base_url(NULL));
    }
    {
        TEST("native_base_url empty", !google_is_native_base_url(""));
    }
    {
        TEST("native_base_url non-google", !google_is_native_base_url("https://api.openai.com/v1"));
    }
    {
        TEST("native_base_url google default",
             google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta"));
    }
    {
        TEST("native_base_url google with trailing slash",
             google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta/"));
    }
    {
        TEST("native_base_url google mixed case",
             google_is_native_base_url("HTTPS://GENERATIVELANGUAGE.GOOGLEAPIS.COM/V1BETA"));
    }
    {
        TEST("native_base_url openai-compat endpoint",
             !google_is_native_base_url("https://generativelanguage.googleapis.com/v1beta/openai"));
    }

    /* ── google_coerce_content_to_text ────────────────────────── */
    printf("\n--- google_coerce_content_to_text ---\n");
    {
        char *r = google_coerce_content_to_text(NULL);
        TEST("coerce NULL", r && strcmp(r, "") == 0);
        free(r);
    }
    {
        /* String content */
        json_t *s = json_string("hello world");
        char *r = google_coerce_content_to_text(s);
        TEST("coerce string", r && strcmp(r, "hello world") == 0);
        free(r);
        json_free(s);
    }
    {
        /* Array with string parts */
        json_t *arr = json_array();
        json_append(arr, json_string("part one"));
        json_append(arr, json_string("part two"));
        char *r = google_coerce_content_to_text(arr);
        TEST("coerce array of strings", r && strcmp(r, "part one\npart two") == 0);
        free(r);
        json_free(arr);
    }
    {
        /* Array with object parts (type="text") */
        json_t *obj1 = json_object();
        json_set(obj1, "type", json_string("text"));
        json_set(obj1, "text", json_string("text one"));
        json_t *obj2 = json_object();
        json_set(obj2, "type", json_string("text"));
        json_set(obj2, "text", json_string("text two"));
        json_t *arr = json_array();
        json_append(arr, obj1);
        json_append(arr, obj2);
        char *r = google_coerce_content_to_text(arr);
        TEST("coerce array of objects", r && strcmp(r, "text one\ntext two") == 0);
        free(r);
        json_free(arr);
    }
    {
        /* Mixed array: string + object */
        json_t *obj = json_object();
        json_set(obj, "type", json_string("text"));
        json_set(obj, "text", json_string("obj text"));
        json_t *arr = json_array();
        json_append(arr, json_string("str part"));
        json_append(arr, obj);
        char *r = google_coerce_content_to_text(arr);
        TEST("coerce mixed array", r && strcmp(r, "str part\nobj text") == 0);
        free(r);
        json_free(arr);
    }
    {
        /* Empty array */
        json_t *arr = json_array();
        char *r = google_coerce_content_to_text(arr);
        TEST("coerce empty array", r && strcmp(r, "") == 0);
        free(r);
        json_free(arr);
    }
    {
        /* Empty string */
        json_t *s = json_string("");
        char *r = google_coerce_content_to_text(s);
        TEST("coerce empty string", r && strcmp(r, "") == 0);
        free(r);
        json_free(s);
    }
    {
        /* Null value (json_null) */
        json_t *n = json_null();
        char *r = google_coerce_content_to_text(n);
        TEST("coerce null json", r && strcmp(r, "") == 0);
        free(r);
        json_free(n);
    }

    /* ── google_tool_call_extra_signature ─────────────────────── */
    printf("\n--- google_tool_call_extra_signature ---\n");
    {
        TEST("extra_sig NULL", google_tool_call_extra_signature(NULL) == NULL);
    }
    {
        /* No extra_content */
        json_t *tc = json_object();
        json_set(tc, "function", json_object());
        json_set(json_obj_get(tc, "function"), "name", json_string("test_fn"));
        json_set(json_obj_get(tc, "function"), "arguments", json_string("{}"));
        TEST("extra_sig no extra_content", google_tool_call_extra_signature(tc) == NULL);
        json_free(tc);
    }
    {
        /* extra_content.google.thought_signature */
        json_t *tc = json_object();
        json_t *extra = json_object();
        json_t *google = json_object();
        json_set(google, "thought_signature", json_string("abc123"));
        json_set(extra, "google", google);
        json_set(tc, "extra_content", extra);
        char *sig = google_tool_call_extra_signature(tc);
        TEST("extra_sig google.thought_signature", sig && strcmp(sig, "abc123") == 0);
        free(sig);
        json_free(tc);
    }
    {
        /* extra_content.google.thoughtSignature (camelCase variant) */
        json_t *tc = json_object();
        json_t *extra = json_object();
        json_t *google = json_object();
        json_set(google, "thoughtSignature", json_string("sig456"));
        json_set(extra, "google", google);
        json_set(tc, "extra_content", extra);
        char *sig = google_tool_call_extra_signature(tc);
        TEST("extra_sig camelCase", sig && strcmp(sig, "sig456") == 0);
        free(sig);
        json_free(tc);
    }
    {
        /* extra_content.thought_signature (string shortcut) */
        json_t *tc = json_object();
        json_t *extra = json_object();
        json_set(extra, "thought_signature", json_string("direct_sig"));
        json_set(tc, "extra_content", extra);
        char *sig = google_tool_call_extra_signature(tc);
        TEST("extra_sig direct thought_signature", sig && strcmp(sig, "direct_sig") == 0);
        free(sig);
        json_free(tc);
    }
    {
        /* Empty string in thought_signature should return NULL */
        json_t *tc = json_object();
        json_t *extra = json_object();
        json_set(extra, "thought_signature", json_string(""));
        json_set(tc, "extra_content", extra);
        TEST("extra_sig empty string", google_tool_call_extra_signature(tc) == NULL);
        json_free(tc);
    }

    /* ── google_translate_tool_call ───────────────────────────── */
    printf("\n--- google_translate_tool_call ---\n");
    {
        json_t *r = google_translate_tool_call(NULL);
        json_t *fc = json_obj_get(r, "functionCall");
        TEST("translate NULL returns object with functionCall", fc != NULL);
        json_free(r);
    }
    {
        /* Basic tool call */
        json_t *tc = json_object();
        json_t *fn = json_object();
        json_set(fn, "name", json_string("get_weather"));
        json_set(fn, "arguments", json_string("{\"city\": \"London\"}"));
        json_set(tc, "function", fn);
        json_set(tc, "id", json_string("call_123"));
        json_set(tc, "type", json_string("function"));

        json_t *r = google_translate_tool_call(tc);
        json_t *fc = json_obj_get(r, "functionCall");
        TEST("translate has functionCall", fc != NULL);
        if (fc) {
            TEST("translate name", strcmp(json_get_str(fc, "name", ""), "get_weather") == 0);
            json_t *args = json_obj_get(fc, "args");
            TEST("translate args exists", args != NULL);
            if (args) {
                TEST("translate args.city", strcmp(json_get_str(args, "city", ""), "London") == 0);
            }
        }
        json_free(r);
        json_free(tc);
    }
    {
        /* Tool call with thought signature */
        json_t *tc = json_object();
        json_t *fn = json_object();
        json_set(fn, "name", json_string("search"));
        json_set(fn, "arguments", json_string("{}"));
        json_set(tc, "function", fn);
        json_t *extra = json_object();
        json_set(extra, "thought_signature", json_string("sig789"));
        json_set(tc, "extra_content", extra);

        json_t *r = google_translate_tool_call(tc);
        TEST("translate with thoughtSignature",
             strcmp(json_get_str(r, "thoughtSignature", ""), "sig789") == 0);
        json_free(r);
        json_free(tc);
    }
    {
        /* Empty arguments */
        json_t *tc = json_object();
        json_t *fn = json_object();
        json_set(fn, "name", json_string("no_args"));
        json_set(fn, "arguments", json_string(""));
        json_set(tc, "function", fn);

        json_t *r = google_translate_tool_call(tc);
        json_t *fc = json_obj_get(r, "functionCall");
        TEST("translate empty args", fc != NULL);
        if (fc) {
            json_t *args = json_obj_get(fc, "args");
            TEST("translate empty args is object", args != NULL && args->type == JSON_OBJECT);
        }
        json_free(r);
        json_free(tc);
    }

    /* Print summary */
    printf("\n=== Original tests: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures;
}

/* ── google_translate_tool_result ───────────────────────── */
static int test_translate_tool_result(void) {
    int f = 0;
    json_t *r, *fr, *resp;

    printf("\n--- google_translate_tool_result ---\n");

    /* 1. NULL message */
    r = google_translate_tool_result(NULL, NULL);
    fr = json_obj_get(r, "functionResponse");
    TEST("tr NULL has functionResponse", fr != NULL);
    if (fr) {
        TEST("tr NULL name = tool", strcmp(json_get_str(fr, "name", ""), "tool") == 0);
        resp = json_obj_get(fr, "response");
        if (resp) {
            TEST("tr NULL response.output empty", strcmp(json_get_str(resp, "output", ""), "") == 0);
        } else { f++; fprintf(stderr, "FAIL: tr NULL response missing\n"); }
    } else { f++; fprintf(stderr, "FAIL: tr NULL functionResponse missing\n"); }
    json_free(r);

    /* 2. Basic tool result with name + content */
    {
        json_t *msg = json_object();
        json_set(msg, "name", json_string("get_weather"));
        json_set(msg, "tool_call_id", json_string("call_001"));
        json_set(msg, "content", json_string("{\"temp\": 72, \"condition\": \"sunny\"}"));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        TEST("tr basic has functionResponse", fr != NULL);
        if (fr) {
            TEST("tr basic name = get_weather", strcmp(json_get_str(fr, "name", ""), "get_weather") == 0);
            resp = json_obj_get(fr, "response");
            if (resp) {
                TEST("tr basic response.temp = 72", json_get_num(resp, "temp", 0) == 72);
                TEST("tr basic response.condition = sunny", strcmp(json_get_str(resp, "condition", ""), "sunny") == 0);
            } else { f++; fprintf(stderr, "FAIL: tr basic response missing\n"); }
        } else { f++; fprintf(stderr, "FAIL: tr basic functionResponse missing\n"); }
        json_free(r);
        json_free(msg);
    }

    /* 3. No name → tool_call_id fallback */
    {
        json_t *msg = json_object();
        json_set(msg, "tool_call_id", json_string("call_002"));
        json_set(msg, "content", json_string("result text"));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        TEST("tr no name uses tool_call_id", fr && strcmp(json_get_str(fr, "name", ""), "call_002") == 0);
        json_free(r);
        json_free(msg);
    }

    /* 4. No name, no tool_call_id → "tool" fallback */
    {
        json_t *msg = json_object();
        json_set(msg, "content", json_string("result"));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        TEST("tr all fallback = tool", fr && strcmp(json_get_str(fr, "name", ""), "tool") == 0);
        json_free(r);
        json_free(msg);
    }

    /* 5. tool_name_by_call_id mapping */
    {
        json_t *msg = json_object();
        json_set(msg, "tool_call_id", json_string("call_003"));
        json_set(msg, "content", json_string("data"));

        json_t *name_map = json_object();
        json_set(name_map, "call_003", json_string("web_search_from_map"));

        r = google_translate_tool_result(msg, name_map);
        fr = json_obj_get(r, "functionResponse");
        TEST("tr mapping uses mapped name", fr && strcmp(json_get_str(fr, "name", ""), "web_search_from_map") == 0);
        json_free(r);
        json_free(msg);
        json_free(name_map);
    }

    /* 6. Plain text content → wrapped as {"output": "..."} */
    {
        json_t *msg = json_object();
        json_set(msg, "name", json_string("some_tool"));
        json_set(msg, "content", json_string("This is plain text result"));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        resp = json_obj_get(fr, "response");
        TEST("tr plain text wrapped in output", resp && strcmp(json_get_str(resp, "output", ""), "This is plain text result") == 0);
        json_free(r);
        json_free(msg);
    }

    /* 7. JSON array content → wrapped (not dict) */
    {
        json_t *msg = json_object();
        json_set(msg, "name", json_string("list_tool"));
        json_set(msg, "content", json_string("[\"a\", \"b\", \"c\"]"));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        resp = json_obj_get(fr, "response");
        TEST("tr array wrapped in output", resp && strcmp(json_get_str(resp, "output", ""), "[\"a\", \"b\", \"c\"]") == 0);
        json_free(r);
        json_free(msg);
    }

    /* 8. Empty content */
    {
        json_t *msg = json_object();
        json_set(msg, "name", json_string("empty_tool"));
        json_set(msg, "content", json_string(""));

        r = google_translate_tool_result(msg, NULL);
        fr = json_obj_get(r, "functionResponse");
        resp = json_obj_get(fr, "response");
        TEST("tr empty content", resp && strcmp(json_get_str(resp, "output", ""), "") == 0);
        json_free(r);
        json_free(msg);
    }

    return f;
}

/* ── google_translate_tool_choice_to_gemini ────────────── */
static int test_translate_tool_choice(void) {
    int f = 0;
    json_t *r, *cfg;

    printf("\n--- google_translate_tool_choice_to_gemini ---\n");

    /* 1. NULL */
    r = google_translate_tool_choice_to_gemini(NULL);
    TEST("tc NULL returns NULL", r == NULL);

    /* 2. "auto" */
    r = google_translate_tool_choice_to_gemini(json_string("auto"));
    TEST("tc auto returns non-NULL", r != NULL);
    if (r) {
        cfg = json_obj_get(r, "functionCallingConfig");
        TEST("tc auto mode AUTO", cfg && strcmp(json_get_str(cfg, "mode", ""), "AUTO") == 0);
    }
    json_free(r);

    /* 3. "required" */
    r = google_translate_tool_choice_to_gemini(json_string("required"));
    TEST("tc required non-NULL", r != NULL);
    if (r) {
        cfg = json_obj_get(r, "functionCallingConfig");
        TEST("tc required mode ANY", cfg && strcmp(json_get_str(cfg, "mode", ""), "ANY") == 0);
    }
    json_free(r);

    /* 4. "none" */
    r = google_translate_tool_choice_to_gemini(json_string("none"));
    TEST("tc none non-NULL", r != NULL);
    if (r) {
        cfg = json_obj_get(r, "functionCallingConfig");
        TEST("tc none mode NONE", cfg && strcmp(json_get_str(cfg, "mode", ""), "NONE") == 0);
    }
    json_free(r);

    /* 5. Unknown string */
    r = google_translate_tool_choice_to_gemini(json_string("unknown"));
    TEST("tc unknown returns NULL", r == NULL);
    json_free(r);

    /* 6. Dict: {"function": {"name": "my_tool"}} */
    {
        json_t *fn = json_object();
        json_set(fn, "name", json_string("my_tool"));
        json_t *tc = json_object();
        json_set(tc, "function", fn);
        r = google_translate_tool_choice_to_gemini(tc);
        TEST("tc dict non-NULL", r != NULL);
        if (r) {
            cfg = json_obj_get(r, "functionCallingConfig");
            TEST("tc dict mode ANY", cfg && strcmp(json_get_str(cfg, "mode", ""), "ANY") == 0);
            json_t *names = cfg ? json_obj_get(cfg, "allowedFunctionNames") : NULL;
            TEST("tc dict has allowedFunctionNames", names != NULL && names->type == JSON_ARRAY);
            if (names && names->c.count > 0) {
                json_t *n0 = names->c.items[0];
                TEST("tc dict name = my_tool", n0 && n0->type == JSON_STRING && strcmp(n0->str_val, "my_tool") == 0);
            }
        }
        json_free(r);
        json_free(tc);
    }

    /* 7. Dict with missing function.name */
    {
        json_t *fn = json_object();
        json_set(fn, "name", json_string(""));
        json_t *tc = json_object();
        json_set(tc, "function", fn);
        r = google_translate_tool_choice_to_gemini(tc);
        TEST("tc empty name returns NULL", r == NULL);
        json_free(r);
        json_free(tc);
    }

    return f;
}

/* ── google_normalize_thinking_config ──────────────────── */
static int test_normalize_thinking_config(void) {
    int f = 0;
    json_t *r;

    printf("\n--- google_normalize_thinking_config ---\n");

    /* 1. NULL */
    r = google_normalize_thinking_config(NULL);
    TEST("nt NULL returns NULL", r == NULL);

    /* 2. Empty object */
    r = google_normalize_thinking_config(json_object());
    TEST("nt empty returns NULL", r == NULL);
    json_free(r);

    /* 3. thinkingBudget */
    {
        json_t *cfg = json_object();
        json_set(cfg, "thinkingBudget", json_number(20000));
        r = google_normalize_thinking_config(cfg);
        TEST("nt budget non-NULL", r != NULL);
        if (r) {
            TEST("nt budget = 20000", json_get_num(r, "thinkingBudget", 0) == 20000);
        }
        json_free(r);
        json_free(cfg);
    }

    /* 4. thinking_budget (snake_case alias) */
    {
        json_t *cfg = json_object();
        json_set(cfg, "thinking_budget", json_number(15000));
        r = google_normalize_thinking_config(cfg);
        TEST("nt snake budget non-NULL", r != NULL);
        if (r) {
            TEST("nt snake budget = 15000", json_get_num(r, "thinkingBudget", 0) == 15000);
        }
        json_free(r);
        json_free(cfg);
    }

    /* 5. includeThoughts */
    {
        json_t *cfg = json_object();
        json_set(cfg, "includeThoughts", json_bool(true));
        r = google_normalize_thinking_config(cfg);
        TEST("nt include non-NULL", r != NULL);
        if (r) {
            TEST("nt include = true", json_get_bool(r, "includeThoughts", false) == true);
        }
        json_free(r);
        json_free(cfg);
    }

    /* 6. thinking_level with strip+lower */
    {
        json_t *cfg = json_object();
        json_set(cfg, "thinking_level", json_string("  DEEP  "));
        r = google_normalize_thinking_config(cfg);
        TEST("nt level non-NULL", r != NULL);
        if (r) {
            TEST("nt level = deep", strcmp(json_get_str(r, "thinkingLevel", ""), "deep") == 0);
        }
        json_free(r);
        json_free(cfg);
    }

    /* 7. All fields together */
    {
        json_t *cfg = json_object();
        json_set(cfg, "thinkingBudget", json_number(50000));
        json_set(cfg, "includeThoughts", json_bool(false));
        json_set(cfg, "thinkingLevel", json_string("MODERATE"));
        r = google_normalize_thinking_config(cfg);
        TEST("nt all non-NULL", r != NULL);
        if (r) {
            TEST("nt all budget = 50000", json_get_num(r, "thinkingBudget", 0) == 50000);
            TEST("nt all include = false", json_get_bool(r, "includeThoughts", true) == false);
            TEST("nt all level = moderate", strcmp(json_get_str(r, "thinkingLevel", ""), "moderate") == 0);
        }
        json_free(r);
        json_free(cfg);
    }

    /* 8. No valid fields → NULL */
    {
        json_t *cfg = json_object();
        json_set(cfg, "unknown", json_string("value"));
        r = google_normalize_thinking_config(cfg);
        TEST("nt unknown returns NULL", r == NULL);
        json_free(r);
        json_free(cfg);
    }

    /* 9. Non-object (string) */
    r = google_normalize_thinking_config(json_string("test"));
    TEST("nt non-object returns NULL", r == NULL);
    json_free(r);

    return f;
}

/* ── google_translate_tools_to_gemini ──────────────────── */
static int test_translate_tools_to_gemini(void) {
    int f = 0;
    json_t *r;

    printf("\n--- google_translate_tools_to_gemini ---\n");

    /* 1. NULL tools */
    r = google_translate_tools_to_gemini(NULL);
    TEST("tt NULL returns empty array", r && r->type == JSON_ARRAY && r->c.count == 0);
    json_free(r);

    /* 2. Empty tools array */
    r = google_translate_tools_to_gemini(json_array());
    TEST("tt empty returns empty array", r && r->type == JSON_ARRAY && r->c.count == 0);
    json_free(r);

    /* 3. Single tool with name only */
    {
        json_t *tools = json_array();
        json_t *fn = json_object();
        json_set(fn, "name", json_string("get_weather"));
        json_t *tool = json_object();
        json_set(tool, "type", json_string("function"));
        json_set(tool, "function", fn);
        json_append(tools, tool);

        r = google_translate_tools_to_gemini(tools);
        TEST("tt single tool has result", r && r->type == JSON_ARRAY && r->c.count == 1);
        if (r && r->c.count > 0) {
            json_t *wrapper = r->c.items[0];
            json_t *decls = json_obj_get(wrapper, "functionDeclarations");
            TEST("tt has functionDeclarations", decls && decls->type == JSON_ARRAY);
            if (decls && decls->c.count > 0) {
                json_t *decl = decls->c.items[0];
                TEST("tt decl name = get_weather",
                     strcmp(json_get_str(decl, "name", ""), "get_weather") == 0);
                TEST("tt decl no description",
                     json_obj_get(decl, "description") == NULL);
            }
        }
        json_free(r);
        json_free(tools);
    }

    /* 4. Tool with description + parameters */
    {
        json_t *tools = json_array();
        json_t *params = json_object();
        json_set(params, "type", json_string("object"));
        json_t *props = json_object();
        json_t *loc = json_object();
        json_set(loc, "type", json_string("string"));
        json_set(props, "location", loc);
        json_set(params, "properties", props);
        json_t *fn = json_object();
        json_set(fn, "name", json_string("search"));
        json_set(fn, "description", json_string("Search the web"));
        json_set(fn, "parameters", params);
        json_t *tool = json_object();
        json_set(tool, "type", json_string("function"));
        json_set(tool, "function", fn);
        json_append(tools, tool);

        r = google_translate_tools_to_gemini(tools);
        TEST("tt full tool has result", r && r->c.count > 0);
        if (r && r->c.count > 0) {
            json_t *wrapper = r->c.items[0];
            json_t *decls = json_obj_get(wrapper, "functionDeclarations");
            if (decls && decls->c.count > 0) {
                json_t *decl = decls->c.items[0];
                TEST("tt decl name = search",
                     strcmp(json_get_str(decl, "name", ""), "search") == 0);
                TEST("tt decl has description",
                     strcmp(json_get_str(decl, "description", ""), "Search the web") == 0);
                json_t *p = json_obj_get(decl, "parameters");
                TEST("tt decl has parameters", p != NULL);
                if (p) {
                    TEST("tt decl params.type = object",
                         strcmp(json_get_str(p, "type", ""), "object") == 0);
                }
            }
        }
        json_free(r);
        json_free(tools);
    }

    /* 5. Multiple tools */
    {
        json_t *tools = json_array();
        json_t *fn1 = json_object();
        json_set(fn1, "name", json_string("tool_a"));
        json_t *t1 = json_object();
        json_set(t1, "type", json_string("function"));
        json_set(t1, "function", fn1);
        json_append(tools, t1);
        json_t *fn2 = json_object();
        json_set(fn2, "name", json_string("tool_b"));
        json_t *t2 = json_object();
        json_set(t2, "type", json_string("function"));
        json_set(t2, "function", fn2);
        json_append(tools, t2);

        r = google_translate_tools_to_gemini(tools);
        TEST("tt multi tool has result", r && r->c.count > 0);
        if (r && r->c.count > 0) {
            json_t *decls = json_obj_get(r->c.items[0], "functionDeclarations");
            TEST("tt multi has 2 declarations", decls && decls->c.count == 2);
        }
        json_free(r);
        json_free(tools);
    }

    /* 6. Tool with no name → skipped */
    {
        json_t *tools = json_array();
        json_t *fn = json_object();
        json_set(fn, "name", json_string(""));  /* empty name */
        json_t *tool = json_object();
        json_set(tool, "function", fn);
        json_append(tools, tool);

        r = google_translate_tools_to_gemini(tools);
        TEST("tt empty name skipped", r && r->type == JSON_ARRAY && r->c.count == 0);
        json_free(r);
        json_free(tools);
    }

    /* 7. Not an array */
    {
        json_t *not_array = json_object();
        json_set(not_array, "type", json_string("function"));
        r = google_translate_tools_to_gemini(not_array);
        TEST("tt non-array returns empty", r && r->type == JSON_ARRAY && r->c.count == 0);
        json_free(r);
        json_free(not_array);
    }

    return f;
}

/* ── google_extract_multimodal_parts ────────────────────── */
static int test_extract_multimodal_parts(void) {
    int f = 0;

    printf("\n--- google_extract_multimodal_parts ---\n");

    /* 1. NULL content */
    json_t *r = google_extract_multimodal_parts(NULL);
    TEST("emp NULL returns empty array", r && r->type == JSON_ARRAY && r->c.count == 0);
    json_free(r);

    /* 2. String content */
    r = google_extract_multimodal_parts(json_string("hello"));
    TEST("emp string has 1 part", r && r->c.count == 1);
    if (r && r->c.count > 0) {
        TEST("emp string text=hello", strcmp(json_get_str(r->c.items[0], "text", ""), "hello") == 0);
    }
    json_free(r);

    /* 3. Array of strings */
    {
        json_t *arr = json_array();
        json_append(arr, json_string("part one"));
        json_append(arr, json_string("part two"));
        r = google_extract_multimodal_parts(arr);
        TEST("emp array has 2 parts", r && r->c.count == 2);
        json_free(r);
        json_free(arr);
    }

    /* 4. Array with text objects */
    {
        json_t *arr = json_array();
        json_t *t1 = json_object();
        json_set(t1, "type", json_string("text"));
        json_set(t1, "text", json_string("obj text"));
        json_append(arr, t1);
        r = google_extract_multimodal_parts(arr);
        TEST("emp text obj has 1 part", r && r->c.count == 1);
        if (r && r->c.count > 0) {
            TEST("emp text obj text=obj text", strcmp(json_get_str(r->c.items[0], "text", ""), "obj text") == 0);
        }
        json_free(r);
        json_free(arr);
    }

    /* 5. Empty string array item (should produce empty text part) */
    {
        json_t *arr = json_array();
        json_append(arr, json_string(""));
        r = google_extract_multimodal_parts(arr);
        TEST("emp empty string has 1 part", r && r->c.count == 1);
        json_free(r);
        json_free(arr);
    }

    return f;
}

/* ── google_tool_call_extra_from_part ──────────────────── */
static int test_tool_call_extra_from_part(void) {
    int f = 0;

    printf("\n--- google_tool_call_extra_from_part ---\n");

    /* 1. NULL */
    json_t *r = google_tool_call_extra_from_part(NULL);
    TEST("tce NULL returns NULL", r == NULL);

    /* 2. No thoughtSignature */
    {
        json_t *part = json_object();
        json_set(part, "text", json_string("hello"));
        r = google_tool_call_extra_from_part(part);
        TEST("tce no sig returns NULL", r == NULL);
        json_free(r);
        json_free(part);
    }

    /* 3. With thoughtSignature */
    {
        json_t *part = json_object();
        json_set(part, "thoughtSignature", json_string("sig123"));
        r = google_tool_call_extra_from_part(part);
        TEST("tce has sig", r != NULL);
        if (r) {
            json_t *google = json_obj_get(r, "google");
            TEST("tce has google", google != NULL);
            if (google) {
                TEST("tce thought_signature = sig123",
                     strcmp(json_get_str(google, "thought_signature", ""), "sig123") == 0);
            }
        }
        json_free(r);
        json_free(part);
    }

    /* 4. Empty thoughtSignature */
    {
        json_t *part = json_object();
        json_set(part, "thoughtSignature", json_string(""));
        r = google_tool_call_extra_from_part(part);
        TEST("tce empty sig returns NULL", r == NULL);
        json_free(r);
        json_free(part);
    }

    return f;
}

/* ── google_build_gemini_contents ──────────────────────── */
static int test_build_gemini_contents(void) {
    int f = 0;

    printf("\n--- google_build_gemini_contents ---\n");

    /* 1. NULL messages */
    json_t *r = google_build_gemini_contents(NULL);
    json_t *contents = json_obj_get(r, "contents");
    TEST("bgc NULL has contents", contents != NULL);
    TEST("bgc NULL contents empty", contents && contents->c.count == 0);
    json_free(r);

    /* 2. Empty array */
    r = google_build_gemini_contents(json_array());
    contents = json_obj_get(r, "contents");
    TEST("bgc empty has contents", contents != NULL);
    TEST("bgc empty contents empty", contents && contents->c.count == 0);
    json_free(r);

    /* 3. Single user message */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("user"));
        json_set(msg, "content", json_string("hello world"));
        json_append(msgs, msg);
        r = google_build_gemini_contents(msgs);
        contents = json_obj_get(r, "contents");
        TEST("bgc user has 1 content", contents && contents->c.count == 1);
        if (contents && contents->c.count > 0) {
            json_t *c = contents->c.items[0];
            TEST("bgc user role = user", strcmp(json_get_str(c, "role", ""), "user") == 0);
            json_t *parts = json_obj_get(c, "parts");
            TEST("bgc user has parts", parts && parts->type == JSON_ARRAY);
            if (parts && parts->c.count > 0) {
                TEST("bgc user text = hello world",
                     strcmp(json_get_str(parts->c.items[0], "text", ""), "hello world") == 0);
            }
        }
        TEST("bgc user no systemInstruction", json_obj_get(r, "systemInstruction") == NULL);
        json_free(r);
        json_free(msgs);
    }

    /* 4. System message */
    {
        json_t *msgs = json_array();
        json_t *msg = json_object();
        json_set(msg, "role", json_string("system"));
        json_set(msg, "content", json_string("Be helpful"));
        json_append(msgs, msg);
        json_t *umsg = json_object();
        json_set(umsg, "role", json_string("user"));
        json_set(umsg, "content", json_string("hi"));
        json_append(msgs, umsg);
        r = google_build_gemini_contents(msgs);
        contents = json_obj_get(r, "contents");
        json_t *si = json_obj_get(r, "systemInstruction");
        TEST("bgc system has systemInstruction", si != NULL);
        if (si) {
            json_t *si_parts = json_obj_get(si, "parts");
            TEST("bgc si has parts", si_parts && si_parts->c.count > 0);
            if (si_parts && si_parts->c.count > 0) {
                TEST("bgc si text = Be helpful",
                     strcmp(json_get_str(si_parts->c.items[0], "text", ""), "Be helpful") == 0);
            }
        }
        TEST("bgc system has 1 content (system excluded)", contents && contents->c.count == 1);
        json_free(r);
        json_free(msgs);
    }

    /* 5. Assistant with tool_calls */
    {
        json_t *msgs = json_array();
        json_t *fn = json_object();
        json_set(fn, "name", json_string("search_web"));
        json_set(fn, "arguments", json_string("{}"));
        json_t *tc = json_object();
        json_set(tc, "id", json_string("call_001"));
        json_set(tc, "type", json_string("function"));
        json_set(tc, "function", fn);
        json_t *tcs = json_array();
        json_append(tcs, tc);
        json_t *msg = json_object();
        json_set(msg, "role", json_string("assistant"));
        json_set(msg, "content", json_string("Let me search"));
        json_set(msg, "tool_calls", tcs);
        json_append(msgs, msg);

        r = google_build_gemini_contents(msgs);
        contents = json_obj_get(r, "contents");
        TEST("bgc tool_calls has 1 content", contents && contents->c.count == 1);
        if (contents && contents->c.count > 0) {
            json_t *c = contents->c.items[0];
            TEST("bgc tool_calls role = model", strcmp(json_get_str(c, "role", ""), "model") == 0);
            json_t *parts = json_obj_get(c, "parts");
            TEST("bgc tool_calls has parts", parts != NULL);
            if (parts) {
                /* Should have text part + functionCall part = 2 parts */
                bool found_text = false, found_fc = false;
                for (size_t i = 0; i < parts->c.count; i++) {
                    json_t *p = parts->c.items[i];
                    if (json_get_str(p, "text", NULL)) found_text = true;
                    if (json_obj_get(p, "functionCall")) found_fc = true;
                }
                TEST("bgc tool_calls has text part", found_text);
                TEST("bgc tool_calls has functionCall", found_fc);
            }
        }
        json_free(r);
        json_free(msgs);
    }

    /* 6. Tool result */
    {
        json_t *msgs = json_array();
        json_t *tr = json_object();
        json_set(tr, "role", json_string("tool"));
        json_set(tr, "tool_call_id", json_string("call_001"));
        json_set(tr, "name", json_string("search_web"));
        json_set(tr, "content", json_string("{\"result\": \"data\"}"));
        json_append(msgs, tr);
        r = google_build_gemini_contents(msgs);
        contents = json_obj_get(r, "contents");
        TEST("bgc tool_result has 1 content", contents && contents->c.count == 1);
        if (contents && contents->c.count > 0) {
            json_t *c = contents->c.items[0];
            TEST("bgc tool_result role = user", strcmp(json_get_str(c, "role", ""), "user") == 0);
            json_t *parts = json_obj_get(c, "parts");
            TEST("bgc tool_result has parts", parts && parts->c.count > 0);
            if (parts && parts->c.count > 0) {
                json_t *fr = json_obj_get(parts->c.items[0], "functionResponse");
                TEST("bgc tool_result has functionResponse", fr != NULL);
                if (fr) {
                    TEST("bgc tool_result name = search_web",
                         strcmp(json_get_str(fr, "name", ""), "search_web") == 0);
                }
            }
        }
        json_free(r);
        json_free(msgs);
    }

    return f;
}

int main(void) {
    int total_fail = 0;
    total_fail += test_original_suite();
    total_fail += test_translate_tool_result();
    total_fail += test_translate_tools_to_gemini();
    total_fail += test_translate_tool_choice();
    total_fail += test_normalize_thinking_config();
    total_fail += test_extract_multimodal_parts();
    total_fail += test_tool_call_extra_from_part();
    total_fail += test_build_gemini_contents();
    printf("\n=== Overall: %s ===\n", total_fail ? "SOME FAILED" : "ALL PASSED");
    return total_fail ? 1 : 0;
}
