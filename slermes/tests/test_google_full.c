/*
 * test_google_full.c — Google provider comprehensive tests
 *
 * Covers: URL/headers building, generation config, safety settings,
 * system instruction, response_format/json_mode, tool conversion,
 * response parsing, streaming, error handling, null safety.
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
static const message_t *msgs[] = {&user_msg};

int main(void) {
    printf("=== Google provider comprehensive tests ===\n\n");

    /* ──────────── URL building ──────────── */

    /* Default URL */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, NULL);
        TEST("default URL contains generative language API",
             url && strstr(url, "generativelanguage.googleapis.com/v1beta/models/"));
        TEST("default URL ends with :generateContent",
             url && strstr(url, ":generateContent"));
        TEST("default model is gemini-2.0-flash",
             url && strstr(url, "gemini-2.0-flash"));
        free(url);
    }

    /* Custom model */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gemini-2.5-pro");
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, NULL);
        TEST("custom model in URL",
             url && strstr(url, "gemini-2.5-pro"));
        free(url);
    }

    /* Custom base URL */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gpt-4o"); /* model doesn't matter for URL */
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, "https://custom.google.com/v1beta");
        TEST("custom base URL prefix",
             url && strncmp(url, "https://custom.google.com/v1beta/models/gpt-4o:generateContent", 64) == 0);
        free(url);
    }

    /* URL with :generateContent already present */
    {
        provider_t p = {0};
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p,
            "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent");
        TEST("URL with :generateContent used as-is",
             url && strstr(url, ":generateContent") &&
             !strstr(url, "models/models")); /* no double model path */
        free(url);
    }

    /* URL with trailing slash */
    {
        provider_t p = {0};
        snprintf(p.model, sizeof(p.model), "gemini-test");
        char *url = PROVIDER_OPS_GOOGLE.build_url(&p, "https://custom.google.com/v1beta/");
        TEST("trailing slash URL strips slash",
             url && strstr(url, "/models/gemini-test:generateContent"));
        TEST("trailing slash URL no double slash",
             url && !strstr(url, "//models"));
        free(url);
    }

    /* ──────────── Headers ──────────── */

    /* Headers with API key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_GOOGLE.build_headers(&p, "AIzaSyTest123");
        TEST("headers contain x-goog-api-key", hdr && strstr(hdr, "x-goog-api-key: AIzaSyTest123"));
        TEST("headers contain Content-Type", hdr && strstr(hdr, "Content-Type: application/json"));
        free(hdr);
    }

    /* Headers without API key */
    {
        provider_t p = {0};
        char *hdr = PROVIDER_OPS_GOOGLE.build_headers(&p, NULL);
        TEST("headers without key have no Authorization",
             hdr && !strstr(hdr, "Authorization"));
        TEST("headers without key still have Content-Type",
             hdr && strstr(hdr, "Content-Type: application/json"));
        free(hdr);
    }

    /* ──────────── Request body — generation config ──────────── */

    /* maxOutputTokens in body */
    {
        provider_t p = {0};
        p.config.max_tokens = 8192;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("maxOutputTokens in body", body && strstr(body, "\"maxOutputTokens\":8192"));
        free(body);
    }

    /* temperature in generationConfig */
    {
        provider_t p = {0};
        p.config.temperature = 0.7f;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature in body", body && strstr(body, "\"temperature\":0.699"));
        free(body);
    }

    /* temperature=0.0 included */
    {
        provider_t p = {0};
        p.config.temperature = 0.0f;
        p.config.max_tokens = 100;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("temperature=0.0 included", body && strstr(body, "\"temperature\":0"));
        free(body);
    }

    /* top_p in generationConfig */
    {
        provider_t p = {0};
        p.config.top_p = 0.9f;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("top_p in body", body && strstr(body, "\"topP\":0.899"));
        free(body);
    }

    /* stopSequences */
    {
        provider_t p = {0};
        p.config.stop_count = 2;
        snprintf(p.config.stop_sequences[0], sizeof(p.config.stop_sequences[0]), "\\n");
        snprintf(p.config.stop_sequences[1], sizeof(p.config.stop_sequences[1]), "###");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("stopSequences in body", body && strstr(body, "\"stopSequences\""));
        free(body);
    }

    /* ──────────── B30: top_k + candidate_count ──────────── */

    /* top_k in generationConfig */
    {
        provider_t p = {0};
        p.config.top_k = 40;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("topK in generationConfig", body && strstr(body, "\"topK\":40"));
        free(body);
    }

    /* candidate_count in generationConfig */
    {
        provider_t p = {0};
        p.config.candidate_count = 2;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("candidateCount in generationConfig", body && strstr(body, "\"candidateCount\":2"));
        free(body);
    }

    /* ──────────── B29: safety_settings ──────────── */

    /* safety_settings JSON array parsed and injected */
    {
        provider_t p = {0};
        snprintf(p.config.safety_settings, sizeof(p.config.safety_settings),
                 "[{\"category\":\"HARM_CATEGORY_HATE_SPEECH\",\"threshold\":\"BLOCK_ONLY_HIGH\"}]");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("safetySettings in root", body && strstr(body, "\"safetySettings\""));
        TEST("safetySettings category", body && strstr(body, "HARM_CATEGORY_HATE_SPEECH"));
        TEST("safetySettings threshold", body && strstr(body, "BLOCK_ONLY_HIGH"));
        free(body);
    }

    /* Empty safety_settings omitted */
    {
        provider_t p = {0};
        p.config.safety_settings[0] = '\0';
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("empty safety_settings omitted", body && !strstr(body, "\"safetySettings\""));
        free(body);
    }

    /* Non-JSON safety_settings silently dropped */
    {
        provider_t p = {0};
        snprintf(p.config.safety_settings, sizeof(p.config.safety_settings), "not-json");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("non-JSON safety_settings dropped", body && !strstr(body, "not-json"));
        free(body);
    }

    /* ──────────── System instruction ──────────── */

    /* System message becomes systemInstruction */
    {
        provider_t p = {0};
        message_t sys_msg = {.role = MSG_SYSTEM, .content = (char*)"You are helpful."};
        const message_t *with_sys[] = {&sys_msg, &user_msg};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, with_sys, 2, NULL, false);
        TEST("systemInstruction present", body && strstr(body, "\"systemInstruction\""));
        TEST("systemInstruction has parts array", body && strstr(body, "\"parts\""));
        TEST("systemInstruction text content",
             body && strstr(body, "\"text\":\"You are helpful.\""));
        TEST("system prompt not in contents",
             body && strstr(body, "\"role\":\"user\"") && !strstr(body, "\"role\":\"system\""));
        free(body);
    }

    /* Multiple system messages concatenated */
    {
        provider_t p = {0};
        message_t sys1 = {.role = MSG_SYSTEM, .content = (char*)"Part1."};
        message_t sys2 = {.role = MSG_SYSTEM, .content = (char*)"Part2."};
        const message_t *with_sys[] = {&sys1, &sys2, &user_msg};
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, with_sys, 3, NULL, false);
        TEST("concatenated system messages in instruction",
             body && strstr(body, "Part1.Part2."));
        free(body);
    }

    /* ──────────── Tools ──────────── */

    /* Tools converted from OpenAI to Google format */
    {
        provider_t p = {0};
        json_t *tools = json_new_array();
        json_t *tool = json_new_object();
        json_t *func = json_new_object();
        json_object_set(func, "name", json_new_string("search"));
        json_object_set(func, "description", json_new_string("Search tool"));
        json_t *params = json_new_object();
        json_object_set(params, "type", json_new_string("object"));
        json_t *props = json_new_object();
        json_t *arg = json_new_object();
        json_object_set(arg, "type", json_new_string("string"));
        json_object_set(props, "q", arg);
        json_object_set(params, "properties", props);
        json_object_set(func, "parameters", params);
        json_object_set(tool, "type", json_new_string("function"));
        json_object_set(tool, "function", func);
        json_array_append(tools, tool);

        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, tools, false);
        TEST("tools with functionDeclarations",
             body && strstr(body, "\"functionDeclarations\""));
        TEST("tool name preserved", body && strstr(body, "\"search\""));
        TEST("no type:function wrapper in body",
             body && !strstr(body, "\"type\":\"function\""));
        TEST("parameters preserved", body && strstr(body, "\"parameters\""));
        json_free(tools);
        free(body);
    }

    /* ──────────── response_format + json_mode ──────────── */

    /* response_format in body */
    {
        provider_t p = {0};
        snprintf(p.config.response_format, sizeof(p.config.response_format),
                 "{\"type\":\"json_object\"}");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("response_format in body", body && strstr(body, "\"response_format\""));
        free(body);
    }

    /* json_mode sets json_object */
    {
        provider_t p = {0};
        p.config.json_mode = true;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("json_mode response_format has json_object",
             body && strstr(body, "\"type\":\"json_object\""));
        free(body);
    }

    /* ──────────── tool_choice + parallel_tool_calls ──────────── */

    /* tool_choice in body */
    {
        provider_t p = {0};
        snprintf(p.config.tool_choice, sizeof(p.config.tool_choice), "auto");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("tool_choice in body", body && strstr(body, "\"tool_choice\":\"auto\""));
        free(body);
    }

    /* parallel_tool_calls disabled */
    {
        provider_t p = {0};
        p.config.parallel_tool_calls = false;
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("parallel_tool_calls=false in body",
             body && strstr(body, "\"parallel_tool_calls\":false"));
        free(body);
    }

    /* ──────────── extra_body merging ──────────── */

    /* extra_body JSON merged into root */
    {
        provider_t p = {0};
        snprintf(p.config.extra_body, sizeof(p.config.extra_body),
                 "{\"customField\":\"val\",\"numField\":42}");
        char *body = PROVIDER_OPS_GOOGLE.build_request_body(&p, msgs, 1, NULL, false);
        TEST("extra_body customField merged",
             body && strstr(body, "\"customField\":\"val\""));
        TEST("extra_body numField merged",
             body && strstr(body, "\"numField\":42"));
        free(body);
    }

    /* ──────────── Response parsing ──────────── */

    /* Text-only response */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"candidates\":[{\"content\":{\"role\":\"model\",\"parts\":["
            "{\"text\":\"Hello! How can I help?\"}]},"
            "\"finishReason\":\"STOP\"}],"
            "\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":8}}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, resp_json);
        TEST("text response parsed", r != NULL);
        TEST("text response content",
             r && r->content && strcmp(r->content, "Hello! How can I help?") == 0);
        TEST("text response input_tokens", r && r->input_tokens == 10);
        TEST("text response output_tokens", r && r->output_tokens == 8);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Response with functionCall */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"candidates\":[{\"content\":{\"role\":\"model\",\"parts\":["
            "{\"text\":\"Let me search\"},"
            "{\"functionCall\":{\"name\":\"web_search\",\"args\":{\"query\":\"weather\"}}}]},"
            "\"finishReason\":\"STOP\"}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, resp_json);
        TEST("functionCall response parsed", r != NULL);
        TEST("functionCall has text content",
             r && r->content && strcmp(r->content, "Let me search") == 0);
        TEST("functionCall has tool calls", r && r->tool_calls_count == 1);
        TEST("functionCall name", r && strcmp(r->tool_calls[0].name, "web_search") == 0);
        TEST("functionCall args has query",
             r && strstr(r->tool_calls[0].arguments, "\"query\""));
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* No text, only functionCall */
    {
        provider_t p = {0};
        const char *resp_json =
            "{\"candidates\":[{\"content\":{\"role\":\"model\",\"parts\":["
            "{\"functionCall\":{\"name\":\"get_weather\",\"args\":{\"location\":\"NYC\"}}}]},"
            "\"finishReason\":\"STOP\"}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, resp_json);
        TEST("functionCall-only response parsed", r != NULL);
        TEST("functionCall-only empty content",
             r && r->content && strcmp(r->content, "") == 0);
        TEST("functionCall-only has tool calls", r && r->tool_calls_count == 1);
        TEST("functionCall-only name", r && strcmp(r->tool_calls[0].name, "get_weather") == 0);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Empty candidates array */
    {
        provider_t p = {0};
        const char *resp_json = "{\"candidates\":[]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, resp_json);
        TEST("empty candidates parsed", r != NULL);
        TEST("empty candidates content NULL or empty",
             r && (!r->content || r->content[0] == '\0'));
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Error response */
    {
        provider_t p = {0};
        const char *err_json = "{\"error\":{\"message\":\"API key not valid\"}}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, err_json);
        TEST("error response parsed", r != NULL);
        TEST("error message includes API error",
             r && r->content && strstr(r->content, "Google API error"));
        TEST("error message includes text",
             r && r->content && strstr(r->content, "API key not valid"));
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* ──────────── Streaming ──────────── */

    /* Stream chunk with text */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}\n";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("stream text chunk parsed", r != NULL);
        TEST("stream text content", r && r->content && strcmp(r->content, "Hello") == 0);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Stream chunk with finishReason */
    {
        provider_t p = {0};
        const char *chunk =
            "data: {\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\"}]},"
            "\"finishReason\":\"STOP\"}]}\n";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        TEST("stream finishReason chunk parsed", r != NULL);
        TEST("stream finishReason stop", r && strcmp(r->finish_reason, "stop") == 0);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Stream [DONE] marker */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, "data: [DONE]\n");
        TEST("stream [DONE] marker parsed", r != NULL);
        TEST("stream [DONE] empty content", r && r->content && strcmp(r->content, "") == 0);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Stream non-data prefix */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, ":\n\n");
        TEST("stream non-data prefix returns content", r != NULL);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* ──────────── Null safety ──────────── */

    /* free_response with NULL */
    {
        PROVIDER_OPS_GOOGLE.free_response(NULL);
        TEST("free_response(NULL) no crash", 1);
    }

    /* Parse NULL response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, NULL);
        TEST("parse_response(NULL) returns non-NULL", r != NULL);
        TEST("parse_response NULL has error text",
             r && r->content && strstr(r->content, "NULL input"));
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Parse empty response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, "");
        TEST("parse_response(empty) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Parse invalid JSON */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, "{invalid}");
        TEST("parse_response(invalid JSON) returns non-NULL", r != NULL);
        TEST("parse_response invalid has error text",
             r && r->content && strstr(r->content, "JSON parse error"));
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    /* Stream NULL chunk */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, NULL);
        TEST("parse_stream_chunk(NULL) returns non-NULL", r != NULL);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}
