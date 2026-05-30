/*
 * test_error_classifier.c — Tests for error classification.
 */

#include "error_classifier.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while(0)

/* ─── 401 Auth ─────────────────────────────────────── */

static int test_auth_401(void) {
    classified_error_t r;
    error_classify(401, "{\"error\":{\"message\":\"Invalid API key\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_AUTH, "401 should be auth");
    ASSERT(!r.retryable, "401 not retryable");
    ASSERT(r.should_rotate_credential, "401 should rotate credential");
    ASSERT(r.should_fallback, "401 should fallback");
    return 1;
}

/* ─── 403 Auth ─────────────────────────────────────── */

static int test_auth_403(void) {
    classified_error_t r;
    error_classify(403, "{\"error\":{\"message\":\"Forbidden\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_AUTH, "403 should be auth");
    ASSERT(!r.retryable, "403 not retryable");
    return 1;
}

/* ─── 403 Billing (key limit exceeded) ─────────────── */

static int test_billing_403(void) {
    classified_error_t r;
    error_classify(403, "{\"error\":{\"message\":\"Key limit exceeded\"}}",
                   "openrouter", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_BILLING, "403+key_limit should be billing");
    return 1;
}

/* ─── 402 Billing ──────────────────────────────────── */

static int test_billing_402(void) {
    classified_error_t r;
    error_classify(402, "{\"error\":{\"message\":\"Billing hard limit\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_BILLING, "402 billing should be billing");
    ASSERT(!r.retryable, "402 billing not retryable");
    return 1;
}

/* ─── 402 Rate limit (transient usage limit) ───────── */

static int test_ratelimit_402_transient(void) {
    classified_error_t r;
    error_classify(402, "{\"error\":{\"message\":\"Usage limit exceeded, try again in 5 minutes\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_RATE_LIMIT, "402+transient should be rate_limit");
    ASSERT(r.retryable, "transient 402 retryable");
    return 1;
}

/* ─── 429 Rate limit ──────────────────────────────── */

static int test_ratelimit_429(void) {
    classified_error_t r;
    error_classify(429, "{\"error\":{\"message\":\"Rate limit exceeded\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_RATE_LIMIT, "429 should be rate_limit");
    ASSERT(r.retryable, "429 retryable");
    ASSERT(r.should_rotate_credential, "429 should rotate");
    return 1;
}

/* ─── 413 Payload too large ───────────────────────── */

static int test_payload_too_large(void) {
    classified_error_t r;
    error_classify(413, "{\"error\":{\"message\":\"Request too large\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_PAYLOAD_TOO_LARGE, "413 should be payload_too_large");
    ASSERT(r.should_compress, "413 should compress");
    return 1;
}

/* ─── 404 Model not found ─────────────────────────── */

static int test_model_not_found(void) {
    classified_error_t r;
    error_classify(404, "{\"error\":{\"message\":\"Model not found\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_MODEL_NOT_FOUND, "404+model not found");
    ASSERT(!r.retryable, "model not found not retryable");
    ASSERT(r.should_fallback, "model not found should fallback");
    return 1;
}

/* ─── 404 Provider policy blocked ─────────────────── */

static int test_provider_policy_blocked(void) {
    classified_error_t r;
    error_classify(404, "{\"error\":{\"message\":\"No endpoints available matching your guardrail restrictions\"}}",
                   "openrouter", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_PROVIDER_POLICY_BLOCKED,
           "404+policy should be provider_policy_blocked");
    ASSERT(!r.should_fallback, "policy blocked should NOT fallback");
    return 1;
}

/* ─── 500 Server error ────────────────────────────── */

static int test_server_error(void) {
    classified_error_t r;
    error_classify(500, "{\"error\":{\"message\":\"Internal server error\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_SERVER_ERROR, "500 should be server_error");
    ASSERT(r.retryable, "500 retryable");
    return 1;
}

/* ─── 503 Overloaded ──────────────────────────────── */

static int test_overloaded(void) {
    classified_error_t r;
    error_classify(503, "{\"error\":{\"message\":\"Service unavailable\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_OVERLOADED, "503 should be overloaded");
    ASSERT(r.retryable, "503 retryable");
    return 1;
}

/* ─── 400 Context overflow ────────────────────────── */

static int test_context_overflow(void) {
    classified_error_t r;
    error_classify(400, "{\"error\":{\"message\":\"Context length exceeded, reduce your prompt\"}}",
                   "anthropic", "claude-3", 150000, 200000, &r);
    ASSERT(r.reason == FAILOVER_CONTEXT_OVERFLOW, "400+context should be context_overflow");
    ASSERT(r.should_compress, "context overflow should compress");
    return 1;
}

/* ─── 400 Image too large ─────────────────────────── */

static int test_image_too_large(void) {
    classified_error_t r;
    error_classify(400, "{\"error\":{\"message\":\"image exceeds 5 MB maximum\"}}",
                   "anthropic", "claude-3", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_IMAGE_TOO_LARGE, "400+image should be image_too_large");
    return 1;
}

/* ─── 400 Multimodal tool content unsupported ─────── */

static int test_multimodal_unsupported(void) {
    classified_error_t r;
    error_classify(400, "{\"error\":{\"message\":\"Param Incorrect\",\"param\":\"text is not set\"}}",
                   "xiaomi", "mimo", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_MULTIMODAL_TOOL_UNSUPPORTED,
           "400+text is not set should be multimodal_unsupported");
    return 1;
}

/* ─── 400 Anthropic thinking signature ────────────── */

static int test_thinking_signature(void) {
    classified_error_t r;
    error_classify(400, "{\"error\":{\"message\":\"thinking block has invalid signature\"}}",
                   "anthropic", "claude-3", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_THINKING_SIGNATURE,
           "400+signature+thinking should be thinking_signature");
    return 1;
}

/* ─── 429 Anthropic long context tier ─────────────── */

static int test_long_context_tier(void) {
    classified_error_t r;
    error_classify(429, "{\"error\":{\"message\":\"extra usage long context tier activated\"}}",
                   "anthropic", "claude-3", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_LONG_CONTEXT_TIER,
           "429+extra usage+long context should be long_context_tier");
    ASSERT(r.should_compress, "long context tier should compress");
    return 1;
}

/* ─── llama.cpp grammar rejection ─────────────────── */

static int test_llama_grammar(void) {
    classified_error_t r;
    error_classify(400, "{\"error\":{\"message\":\"Error parsing grammar for tool calls\"}}",
                   "llamacpp", "llama-3", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_LLAMA_CPP_GRAMMAR,
           "400+error parsing grammar should be llama_cpp_grammar");
    return 1;
}

/* ─── Error code classification ───────────────────── */

static int test_error_code_ratelimit(void) {
    classified_error_t r;
    error_classify(-1, "{\"error\":{\"code\":\"rate_limit_exceeded\",\"message\":\"Too fast\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_RATE_LIMIT, "error code rate_limit_exceeded");
    return 1;
}

static int test_error_code_context(void) {
    classified_error_t r;
    error_classify(-1, "{\"error\":{\"code\":\"context_length_exceeded\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_CONTEXT_OVERFLOW, "error code context_length_exceeded");
    ASSERT(r.should_compress, "context overflow should compress");
    return 1;
}

/* ─── Message pattern (no status) ─────────────────── */

static int test_message_billing(void) {
    classified_error_t r;
    error_classify(-1, "{\"message\":\"Insufficient credits. Please top up.\"}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_BILLING, "message billing pattern");
    return 1;
}

static int test_message_auth(void) {
    classified_error_t r;
    error_classify(-1, "{\"error\":{\"message\":\"Invalid API key provided\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_AUTH, "message auth pattern");
    ASSERT(!r.retryable, "auth not retryable");
    return 1;
}

/* ─── Unknown fallback ────────────────────────────── */

static int test_unknown(void) {
    classified_error_t r;
    error_classify(-1, "{\"error\":{\"message\":\"Something weird happened\"}}",
                   "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_UNKNOWN, "unknown pattern should be unknown");
    ASSERT(r.retryable, "unknown retryable");
    return 1;
}

/* ─── Format helper ───────────────────────────────── */

static int test_format(void) {
    classified_error_t r;
    error_classify(429, "{\"error\":{\"message\":\"Rate limited\"}}",
                   "test-pro", "test-model", 0, 0, &r);
    char buf[1024];
    int n = error_format(&r, buf, sizeof(buf));
    ASSERT(n > 0, "format should produce output");
    ASSERT(strstr(buf, "reason=rate_limit") != NULL, "format should contain reason");
    ASSERT(strstr(buf, "provider=test-pro") != NULL, "format should contain provider");
    return 1;
}

/* ─── No error body (NULL) ────────────────────────── */

static int test_null_body(void) {
    classified_error_t r;
    error_classify(429, NULL, "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_RATE_LIMIT, "null body 429 should be rate_limit");
    return 1;
}

/* ─── Empty error body ────────────────────────────── */

static int test_empty_body(void) {
    classified_error_t r;
    error_classify(500, "", "openai", "gpt-4", 0, 0, &r);
    ASSERT(r.reason == FAILOVER_SERVER_ERROR, "empty body 500 should be server_error");
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Runner
 * ════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("=== Error Classifier Tests ===\n\n");

    TEST(auth_401);
    TEST(auth_403);
    TEST(billing_403);
    TEST(billing_402);
    TEST(ratelimit_402_transient);
    TEST(ratelimit_429);
    TEST(payload_too_large);
    TEST(model_not_found);
    TEST(provider_policy_blocked);
    TEST(server_error);
    TEST(overloaded);
    TEST(context_overflow);
    TEST(image_too_large);
    TEST(multimodal_unsupported);
    TEST(thinking_signature);
    TEST(long_context_tier);
    TEST(llama_grammar);
    TEST(error_code_ratelimit);
    TEST(error_code_context);
    TEST(message_billing);
    TEST(message_auth);
    TEST(unknown);
    TEST(format);
    TEST(null_body);
    TEST(empty_body);

    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n",
           tests_passed, tests_failed, tests_run - tests_passed - tests_failed);
    printf("==============================================\n");

    return tests_failed > 0 ? 1 : 0;
}
