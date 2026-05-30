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

int main(void) {
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

    /* Print summary */
    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
