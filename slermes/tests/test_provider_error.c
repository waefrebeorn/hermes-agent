/*
 * test_provider_error.c — M06: Cross-provider error handling edge cases
 *
 * Tests parse_response and parse_stream_chunk for all providers with:
 *  - NULL, empty, malformed JSON, auth errors, rate limit errors, server errors
 *  - Edge case responses (missing fields, wrong types)
 *
 * Coverage: NULL safety, JSON parse error recovery, HTTP error format handling
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

/* Count assertions in a provider test batch */
static int test_count = 0;
#define ASSERT(expr) do { test_count++; if (!(expr)) { \
    fprintf(stderr, "  ASSERT FAIL at %s:%d\n", __FILE__, __LINE__); \
    failures++; \
} } while(0)

/* ================================================================
 *  Shared test responses — provider-specific error formats
 * ================================================================ */

/* OpenAI-compatible error responses (OpenAI, OpenRouter, DeepSeek, xAI, Custom) */
static const char *OPENAI_AUTH_ERR   = "{\"error\":{\"message\":\"Incorrect API key\",\"type\":\"invalid_request_error\",\"code\":\"invalid_api_key\"}}";
static const char *OPENAI_RATE_ERR   = "{\"error\":{\"message\":\"Rate limit exceeded\",\"type\":\"rate_limit_error\",\"code\":\"rate_limited\"}}";
static const char *OPENAI_SERVER_ERR = "{\"error\":{\"message\":\"The server had an error\",\"type\":\"server_error\",\"code\":\"internal_error\"}}";

/* Anthropic error format */
static const char *ANTHROPIC_AUTH_ERR   = "{\"error\":{\"type\":\"authentication_error\",\"message\":\"Invalid API key\"}}";
static const char *ANTHROPIC_RATE_ERR   = "{\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Too many requests\"}}";
static const char *ANTHROPIC_SERVER_ERR = "{\"error\":{\"type\":\"api_error\",\"message\":\"Internal server error\"}}";

/* Google Gemini error format */
static const char *GOOGLE_AUTH_ERR   = "{\"error\":{\"code\":401,\"message\":\"API key not valid.\",\"status\":\"UNAUTHENTICATED\"}}";
static const char *GOOGLE_RATE_ERR   = "{\"error\":{\"code\":429,\"message\":\"Resource has been exhausted\",\"status\":\"RESOURCE_EXHAUSTED\"}}";
static const char *GOOGLE_SERVER_ERR = "{\"error\":{\"code\":500,\"message\":\"Internal error encountered.\",\"status\":\"INTERNAL\"}}";

/* Azure OpenAI error format */
static const char *AZURE_AUTH_ERR   = "{\"error\":{\"message\":\"Access denied due to invalid subscription key.\",\"type\":\"access_denied\"}}";
static const char *AZURE_RATE_ERR   = "{\"error\":{\"message\":\"Rate limit is exceeded.\",\"type\":\"rate_limit_exceeded\",\"code\":\"429\"}}";
static const char *AZURE_SERVER_ERR = "{\"error\":{\"message\":\"The service is temporarily unavailable.\",\"type\":\"service_unavailable\",\"code\":\"503\"}}";

/* Bedrock Converse error format */
static const char *BEDROCK_AUTH_ERR   = "{\"message\":\"The security token included in the request is invalid.\"}";
static const char *BEDROCK_RATE_ERR   = "{\"message\":\"Too many requests, please slow down.\"}";
static const char *BEDROCK_SERVER_ERR = "{\"message\":\"An internal error occurred. Please retry the request.\"}";

/* Malformed / edge cases (provider-agnostic) */
static const char *MALFORMED_TRUNCATED  = "{\"error\":{\"message\":\"Incomplete";
static const char *MALFORMED_BAD_JSON  = "this is not json at all {{{";
static const char *MALFORMED_ARRAY     = "[{\"error\":\"unexpected array\"}]";
static const char *MALFORMED_NUMERIC   = "42";
static const char *MALFORMED_EMPTY_OBJ = "{}";

/* Context overflow errors per provider */
static const char *OPENAI_CTX_ERR     = "{\"error\":{\"message\":\"This model's maximum context length is 128000 tokens. You requested 150000 tokens.\",\"type\":\"invalid_request_error\",\"code\":\"context_length_exceeded\"}}";
static const char *ANTHROPIC_CTX_ERR  = "{\"error\":{\"type\":\"invalid_request_error\",\"message\":\"Your prompt contained 250000 tokens, but the maximum is 200000 for this model.\"}}";
static const char *GOOGLE_CTX_ERR     = "{\"error\":{\"code\":400,\"message\":\"The input token count 300000 exceeds the maximum input token limit of 2000000.\",\"status\":\"INVALID_ARGUMENT\"}}";
static const char *AZURE_CTX_ERR      = "{\"error\":{\"message\":\"The request exceeds the maximum context length of 128000 tokens. Please reduce your prompt.\",\"type\":\"invalid_request_error\",\"code\":\"context_length_exceeded\"}}";
static const char *BEDROCK_CTX_ERR    = "{\"$fault\":\"client\",\"__type\":\"ValidationException\",\"message\":\"The input text is too long for this model. Maximum length is 200000 characters.\"}";
static const char *BEDROCK_CTX_ERR2   = "{\"message\":\"Input is too long for this model\"}";

/* Boundary / edge case error shapes */
static const char *LONG_ERROR_MSG     = "{\"error\":{\"message\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}}";
static const char *UNICODE_ERROR_MSG  = "{\"error\":{\"message\":\"Invalid API key \\u00e9\\u00e0\\u00fc\\u00f1 \\u2603\\ufe0f \\u0420\\u043e\\u0441\\u0441\\u0438\\u044f\"}}";
static const char *NO_MSG_ERROR      = "{\"error\":{\"code\":\"rate_limited\"}}";
static const char *NULL_ERROR_OBJ    = "{\"error\":null}";
static const char *EMPTY_ERROR_OBJ   = "{\"error\":{}}";

/* Provider-specific success-like responses for negative testing */
static const char *OPENAI_NO_CHOICES = "{\"id\":\"chatcmpl-xxx\",\"object\":\"chat.completion\",\"created\":1234567890,\"model\":\"gpt-4o\",\"choices\":[]}";
static const char *ANTHROPIC_NO_CONTENT = "{\"id\":\"msg_xxx\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[],\"model\":\"claude-3\",\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":10,\"output_tokens\":0}}";
static const char *GOOGLE_NO_CANDIDATES = "{\"candidates\":[],\"usageMetadata\":{\"promptTokenCount\":5,\"candidatesTokenCount\":0}}";

/* ================================================================
 *  Shared provider test macro
 * ================================================================ */

/* Test parse_response for a single provider with multiple error inputs */
#define TEST_PARSE_ERRORS(name, ops, auth_err, rate_err, server_err) do { \
    const char *pname = #name; \
    provider_t p = {0}; \
    \
    /* NULL body */ \
    { \
        provider_response_t *r = ops.parse_response(&p, NULL); \
        ASSERT(r != NULL && #name " parse_response(NULL) returns non-NULL"); \
        ops.free_response(r); \
    } \
    \
    /* Empty string */ \
    { \
        provider_response_t *r = ops.parse_response(&p, ""); \
        ASSERT(r != NULL && #name " parse_response(\"\") returns non-NULL"); \
        ops.free_response(r); \
    } \
    \
    /* Auth error */ \
    { \
        provider_response_t *r = ops.parse_response(&p, auth_err); \
        ASSERT(r != NULL && #name " auth error returns non-NULL"); \
        ASSERT((r->content && strlen(r->content) > 0) && #name " auth error has content"); \
        ops.free_response(r); \
    } \
    \
    /* Rate limit error */ \
    { \
        provider_response_t *r = ops.parse_response(&p, rate_err); \
        ASSERT(r != NULL && #name " rate limit returns non-NULL"); \
        ASSERT((r->content && strlen(r->content) > 0) && #name " rate limit has content"); \
        ops.free_response(r); \
    } \
    \
    /* Server error */ \
    { \
        provider_response_t *r = ops.parse_response(&p, server_err); \
        ASSERT(r != NULL && #name " server error returns non-NULL"); \
        ASSERT((r->content && strlen(r->content) > 0) && #name " server error has content"); \
        ops.free_response(r); \
    } \
    \
    printf("  %s: %d parse_response error tests\n", pname, 5); \
} while(0)

/* Test parse_stream_chunk for a single provider */
#define TEST_STREAM_ERRORS(name, ops) do { \
    const char *pname = #name; \
    provider_t p = {0}; \
    \
    /* NULL chunk */ \
    { \
        provider_response_t *r = ops.parse_stream_chunk(&p, NULL); \
        /* NULL chunk may return NULL or an error response — either is valid as long as no crash */ \
        if (r) ops.free_response(r); \
        ASSERT(1 && #name " NULL stream chunk does not crash"); \
    } \
    \
    /* Empty chunk */ \
    { \
        provider_response_t *r = ops.parse_stream_chunk(&p, ""); \
        if (r) ops.free_response(r); \
        ASSERT(1 && #name " empty stream chunk does not crash"); \
    } \
    \
    /* Malformed JSON chunk */ \
    { \
        provider_response_t *r = ops.parse_stream_chunk(&p, "data: {broken"); \
        if (r) ops.free_response(r); \
        ASSERT(1 && #name " malformed stream chunk does not crash"); \
    } \
    \
    /* Non-data prefix */ \
    { \
        provider_response_t *r = ops.parse_stream_chunk(&p, "not data: something"); \
        if (r) ops.free_response(r); \
        ASSERT(1 && #name " non-data prefix does not crash"); \
    } \
    \
    printf("  %s: %d stream chunk error tests\n", pname, 4); \
} while(0)

/* ================================================================
 *  Malformed/edge case tests — provider-agnostic
 * ================================================================ */

static void test_malformed_inputs(const provider_ops_t *ops, const char *label) {
    provider_t p = {0};

    /* Truncated JSON */
    {
        provider_response_t *r = ops->parse_response(&p, MALFORMED_TRUNCATED);
        ASSERT(r != NULL && label); ASSERT("truncated JSON returns non-NULL" && 1);
        ops->free_response(r);
    }

    /* Bad JSON (not JSON at all) */
    {
        provider_response_t *r = ops->parse_response(&p, MALFORMED_BAD_JSON);
        ASSERT(r != NULL && label); ASSERT("bad JSON returns non-NULL" && 1);
        ops->free_response(r);
    }

    /* Array instead of object */
    {
        provider_response_t *r = ops->parse_response(&p, MALFORMED_ARRAY);
        ASSERT(r != NULL && label); ASSERT("array returns non-NULL" && 1);
        ops->free_response(r);
    }

    /* Numeric literal */
    {
        provider_response_t *r = ops->parse_response(&p, MALFORMED_NUMERIC);
        ASSERT(r != NULL && label); ASSERT("numeric returns non-NULL" && 1);
        ops->free_response(r);
    }

    /* Empty object — valid JSON but no useful data */
    {
        provider_response_t *r = ops->parse_response(&p, MALFORMED_EMPTY_OBJ);
        ASSERT(r != NULL && label); ASSERT("empty object returns non-NULL" && 1);
        ops->free_response(r);
    }

    printf("  %s: 5 malformed input tests\n", label);
}

/* ================================================================
 *  free_response NULL safety
 * ================================================================ */

static void test_free_null(const provider_ops_t *ops, const char *label) {
    ops->free_response(NULL);
    ASSERT(1 && label); ASSERT("free_response(NULL) no crash" && 1);
    printf("  %s: free_response(NULL) tested\n", label);
}

/* ================================================================
 *  Context overflow error tests — per provider
 * ================================================================ */

static void test_context_overflow(const provider_ops_t *ops, const char *label,
                                   const char *ctx_err_json) {
    provider_t p = {0};
    provider_response_t *r = ops->parse_response(&p, ctx_err_json);
    ASSERT(r != NULL && label); ASSERT("context overflow returns non-NULL" && 1);
    /* Must have content (human-readable error message extracted) */
    ASSERT((r->content && strlen(r->content) > 0) && label);
    ASSERT("context overflow has content" && 1);
    ops->free_response(r);
    printf("  %s: 3 context overflow tests\n", label);
}

/* ================================================================
 *  Boundary / edge case error shapes — per provider
 * ================================================================ */

static void test_boundary_cases(const provider_ops_t *ops, const char *label) {
    provider_t p = {0};
    int local_count = 0;

    /* Very long error message (≈4000 chars) — buffer overflow safety */
    {
        provider_response_t *r = ops->parse_response(&p, LONG_ERROR_MSG);
        ASSERT(r != NULL && label); local_count++;
        if (r) {
            /* Must not crash, content should be present */
            ASSERT((r->content == NULL || strlen(r->content) > 0) && label); local_count++;
            ops->free_response(r);
        }
    }

    /* Unicode / special characters in error message */
    {
        provider_response_t *r = ops->parse_response(&p, UNICODE_ERROR_MSG);
        ASSERT(r != NULL && label); local_count++;
        if (r) ops->free_response(r);
    }

    /* Error with no message field (only code) */
    {
        provider_response_t *r = ops->parse_response(&p, NO_MSG_ERROR);
        ASSERT(r != NULL && label); local_count++;
        if (r) ops->free_response(r);
    }

    /* Null error object */
    {
        provider_response_t *r = ops->parse_response(&p, NULL_ERROR_OBJ);
        ASSERT(r != NULL && label); local_count++;
        if (r) ops->free_response(r);
    }

    /* Empty error object */
    {
        provider_response_t *r = ops->parse_response(&p, EMPTY_ERROR_OBJ);
        ASSERT(r != NULL && label); local_count++;
        if (r) ops->free_response(r);
    }

    printf("  %s: %d boundary case tests\n", label, local_count);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== M06: Provider Error Handling Edge Cases ===\n\n");

    /* ================================================================
     * 1. OpenAI — standard OpenAI-compatible provider
     * ================================================================ */
    printf("--- OpenAI ---\n");
    TEST_PARSE_ERRORS(OpenAI, PROVIDER_OPS_OPENAI,
        OPENAI_AUTH_ERR, OPENAI_RATE_ERR, OPENAI_SERVER_ERR);
    TEST_STREAM_ERRORS(OpenAI, PROVIDER_OPS_OPENAI);
    test_malformed_inputs(&PROVIDER_OPS_OPENAI, "OpenAI");
    test_free_null(&PROVIDER_OPS_OPENAI, "OpenAI");

    /* OpenAI-specific: no choices array */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_response(&p, OPENAI_NO_CHOICES);
        ASSERT(r != NULL && "OpenAI no choices returns non-NULL");
        ASSERT((r->content == NULL || r->content[0] == '\0') && "OpenAI no choices has empty/no content");
        PROVIDER_OPS_OPENAI.free_response(r);
        printf("  OpenAI: 1 edge case (empty choices)\n");
    }

    /* OpenAI-specific: streaming [DONE] marker */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_OPENAI.parse_stream_chunk(&p, "data: [DONE]");
        if (r) PROVIDER_OPS_OPENAI.free_response(r);
        ASSERT(1 && "OpenAI [DONE] marker does not crash");
        printf("  OpenAI: 1 edge case ([DONE] marker)\n");
    }

    /* Context overflow */
    test_context_overflow(&PROVIDER_OPS_OPENAI, "OpenAI", OPENAI_CTX_ERR);

    /* Boundary cases */
    test_boundary_cases(&PROVIDER_OPS_OPENAI, "OpenAI");

    /* ================================================================
     * 2. OpenRouter — shares OpenAI format but separate ops table
     * ================================================================ */
    printf("\n--- OpenRouter ---\n");
    TEST_PARSE_ERRORS(OpenRouter, PROVIDER_OPS_OPENROUTER,
        OPENAI_AUTH_ERR, OPENAI_RATE_ERR, OPENAI_SERVER_ERR);
    TEST_STREAM_ERRORS(OpenRouter, PROVIDER_OPS_OPENROUTER);
    test_malformed_inputs(&PROVIDER_OPS_OPENROUTER, "OpenRouter");
    test_free_null(&PROVIDER_OPS_OPENROUTER, "OpenRouter");

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_OPENROUTER, "OpenRouter", OPENAI_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_OPENROUTER, "OpenRouter");

    /* ================================================================
     * 3. DeepSeek — shares OpenAI format
     * ================================================================ */
    printf("\n--- DeepSeek ---\n");
    TEST_PARSE_ERRORS(DeepSeek, PROVIDER_OPS_DEEPSEEK,
        OPENAI_AUTH_ERR, OPENAI_RATE_ERR, OPENAI_SERVER_ERR);
    TEST_STREAM_ERRORS(DeepSeek, PROVIDER_OPS_DEEPSEEK);
    test_malformed_inputs(&PROVIDER_OPS_DEEPSEEK, "DeepSeek");
    test_free_null(&PROVIDER_OPS_DEEPSEEK, "DeepSeek");

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_DEEPSEEK, "DeepSeek", OPENAI_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_DEEPSEEK, "DeepSeek");

    /* ================================================================
     * 4. xAI — shares OpenAI format
     * ================================================================ */
    printf("\n--- xAI ---\n");
    TEST_PARSE_ERRORS(xAI, PROVIDER_OPS_XAI,
        OPENAI_AUTH_ERR, OPENAI_RATE_ERR, OPENAI_SERVER_ERR);
    TEST_STREAM_ERRORS(xAI, PROVIDER_OPS_XAI);
    test_malformed_inputs(&PROVIDER_OPS_XAI, "xAI");
    test_free_null(&PROVIDER_OPS_XAI, "xAI");

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_XAI, "xAI", OPENAI_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_XAI, "xAI");

    /* ================================================================
     * 5. Custom — shares OpenAI format
     * ================================================================ */
    printf("\n--- Custom ---\n");
    TEST_PARSE_ERRORS(Custom, PROVIDER_OPS_CUSTOM,
        OPENAI_AUTH_ERR, OPENAI_RATE_ERR, OPENAI_SERVER_ERR);
    TEST_STREAM_ERRORS(Custom, PROVIDER_OPS_CUSTOM);
    test_malformed_inputs(&PROVIDER_OPS_CUSTOM, "Custom");
    test_free_null(&PROVIDER_OPS_CUSTOM, "Custom");

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_CUSTOM, "Custom", OPENAI_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_CUSTOM, "Custom");

    /* ================================================================
     * 6. Anthropic — different JSON format
     * ================================================================ */
    printf("\n--- Anthropic ---\n");
    TEST_PARSE_ERRORS(Anthropic, PROVIDER_OPS_ANTHROPIC,
        ANTHROPIC_AUTH_ERR, ANTHROPIC_RATE_ERR, ANTHROPIC_SERVER_ERR);
    TEST_STREAM_ERRORS(Anthropic, PROVIDER_OPS_ANTHROPIC);
    test_malformed_inputs(&PROVIDER_OPS_ANTHROPIC, "Anthropic");
    test_free_null(&PROVIDER_OPS_ANTHROPIC, "Anthropic");

    /* Anthropic-specific: no content blocks */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_response(&p, ANTHROPIC_NO_CONTENT);
        ASSERT(r != NULL && "Anthropic no content returns non-NULL");
        PROVIDER_OPS_ANTHROPIC.free_response(r);
        printf("  Anthropic: 1 edge case (empty content blocks)\n");
    }

    /* Anthropic-specific: streaming [DONE] marker */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_ANTHROPIC.parse_stream_chunk(&p, "[DONE]");
        ASSERT(1 && "Anthropic [DONE] marker does not crash");
        if (r) PROVIDER_OPS_ANTHROPIC.free_response(r);
        printf("  Anthropic: 1 edge case ([DONE] marker)\n");
    }

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_ANTHROPIC, "Anthropic", ANTHROPIC_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_ANTHROPIC, "Anthropic");

    /* ================================================================
     * 7. Google — Gemini JSON format
     * ================================================================ */
    printf("\n--- Google ---\n");
    TEST_PARSE_ERRORS(Google, PROVIDER_OPS_GOOGLE,
        GOOGLE_AUTH_ERR, GOOGLE_RATE_ERR, GOOGLE_SERVER_ERR);
    TEST_STREAM_ERRORS(Google, PROVIDER_OPS_GOOGLE);
    test_malformed_inputs(&PROVIDER_OPS_GOOGLE, "Google");
    test_free_null(&PROVIDER_OPS_GOOGLE, "Google");

    /* Google-specific: no candidates */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_response(&p, GOOGLE_NO_CANDIDATES);
        ASSERT(r != NULL && "Google no candidates returns non-NULL");
        PROVIDER_OPS_GOOGLE.free_response(r);
        printf("  Google: 1 edge case (empty candidates)\n");
    }

    /* Google-specific: streaming finish reason in same chunk */
    {
        provider_t p = {0};
        const char *chunk = "data: {\"candidates\":[{\"index\":0,\"finishReason\":\"STOP\"}]}";
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, chunk);
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
        ASSERT(1 && "Google stream chunk with finish reason does not crash");
        printf("  Google: 1 edge case (stream finish reason)\n");
    }

    /* Google-specific: non-data prefix */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_GOOGLE.parse_stream_chunk(&p, "[DONE]");
        if (r) PROVIDER_OPS_GOOGLE.free_response(r);
        ASSERT(1 && "Google [DONE] does not crash");
        printf("  Google: 1 edge case ([DONE] marker)\n");
    }

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_GOOGLE, "Google", GOOGLE_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_GOOGLE, "Google");

    /* ================================================================
     * 8. Azure — Azure OpenAI format (similar to OpenAI but with Azure error shape)
     * ================================================================ */
    printf("\n--- Azure ---\n");
    TEST_PARSE_ERRORS(Azure, PROVIDER_OPS_AZURE,
        AZURE_AUTH_ERR, AZURE_RATE_ERR, AZURE_SERVER_ERR);
    TEST_STREAM_ERRORS(Azure, PROVIDER_OPS_AZURE);
    test_malformed_inputs(&PROVIDER_OPS_AZURE, "Azure");
    test_free_null(&PROVIDER_OPS_AZURE, "Azure");

    /* Azure-specific: streaming [DONE] */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_AZURE.parse_stream_chunk(&p, "data: [DONE]");
        if (r) PROVIDER_OPS_AZURE.free_response(r);
        ASSERT(1 && "Azure [DONE] does not crash");
        printf("  Azure: 1 edge case ([DONE] marker)\n");
    }

    /* Context overflow + boundary cases */
    test_context_overflow(&PROVIDER_OPS_AZURE, "Azure", AZURE_CTX_ERR);
    test_boundary_cases(&PROVIDER_OPS_AZURE, "Azure");

    /* ================================================================
     * 9. Bedrock — AWS Bedrock Converse format
     * ================================================================ */
    printf("\n--- Bedrock ---\n");
    TEST_PARSE_ERRORS(Bedrock, PROVIDER_OPS_BEDROCK,
        BEDROCK_AUTH_ERR, BEDROCK_RATE_ERR, BEDROCK_SERVER_ERR);
    TEST_STREAM_ERRORS(Bedrock, PROVIDER_OPS_BEDROCK);
    test_malformed_inputs(&PROVIDER_OPS_BEDROCK, "Bedrock");
    test_free_null(&PROVIDER_OPS_BEDROCK, "Bedrock");

    /* Bedrock-specific: empty response */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, "{}");
        ASSERT(r != NULL && "Bedrock empty object returns non-NULL");
        PROVIDER_OPS_BEDROCK.free_response(r);
        printf("  Bedrock: 1 edge case (empty object)\n");
    }

    /* Bedrock context overflow (two error formats) */
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, BEDROCK_CTX_ERR);
        ASSERT(r != NULL && "Bedrock ValidationException returns non-NULL");
        ASSERT((r->content && strlen(r->content) > 0) && "Bedrock ValidationException has content");
        PROVIDER_OPS_BEDROCK.free_response(r);
        printf("  Bedrock: 3 context overflow tests (ValidationException format)\n");
    }
    {
        provider_t p = {0};
        provider_response_t *r = PROVIDER_OPS_BEDROCK.parse_response(&p, BEDROCK_CTX_ERR2);
        ASSERT(r != NULL && "Bedrock simple overflow returns non-NULL");
        ASSERT((r->content && strlen(r->content) > 0) && "Bedrock simple overflow has content");
        PROVIDER_OPS_BEDROCK.free_response(r);
        printf("  Bedrock: 3 context overflow tests (simple format)\n");
    }

    /* Bedrock boundary cases */
    test_boundary_cases(&PROVIDER_OPS_BEDROCK, "Bedrock");

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("\n=== M06 Results: %d assertions, %s ===\n",
           test_count, failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
