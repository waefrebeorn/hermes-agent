/*
 * test_tokenizer.c — Tests for model-aware token counting.
 */

#include "hermes_tokenizer.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ================================================================
 *  Model family detection
 * ================================================================ */

static void test_family_detection(void) {
    TEST("gpt-4 → TOKEN_FAMILY_GPT4");
    ASSERT(hermes_token_family_from_model("gpt-4") == TOKEN_FAMILY_GPT4, "wrong");
    PASS();

    TEST("gpt-4o → TOKEN_FAMILY_GPT4");
    ASSERT(hermes_token_family_from_model("gpt-4o") == TOKEN_FAMILY_GPT4, "wrong");
    PASS();

    TEST("gpt-4-turbo → TOKEN_FAMILY_GPT4");
    ASSERT(hermes_token_family_from_model("gpt-4-turbo") == TOKEN_FAMILY_GPT4, "wrong");
    PASS();

    TEST("gpt-3.5-turbo → TOKEN_FAMILY_GPT35");
    ASSERT(hermes_token_family_from_model("gpt-3.5-turbo") == TOKEN_FAMILY_GPT35, "wrong");
    PASS();

    TEST("claude-3-opus → TOKEN_FAMILY_CLAUDE");
    ASSERT(hermes_token_family_from_model("claude-3-opus") == TOKEN_FAMILY_CLAUDE, "wrong");
    PASS();

    TEST("claude-4-sonnet → TOKEN_FAMILY_CLAUDE");
    ASSERT(hermes_token_family_from_model("claude-4-sonnet") == TOKEN_FAMILY_CLAUDE, "wrong");
    PASS();

    TEST("deepseek-chat → TOKEN_FAMILY_DEEPSEEK");
    ASSERT(hermes_token_family_from_model("deepseek-chat") == TOKEN_FAMILY_DEEPSEEK, "wrong");
    PASS();

    TEST("gemini-pro → TOKEN_FAMILY_GEMINI");
    ASSERT(hermes_token_family_from_model("gemini-pro") == TOKEN_FAMILY_GEMINI, "wrong");
    PASS();

    TEST("llama-3-70b → TOKEN_FAMILY_LLAMA");
    ASSERT(hermes_token_family_from_model("llama-3-70b") == TOKEN_FAMILY_LLAMA, "wrong");
    PASS();

    TEST("mistral-large → TOKEN_FAMILY_MISTRAL");
    ASSERT(hermes_token_family_from_model("mistral-large") == TOKEN_FAMILY_MISTRAL, "wrong");
    PASS();

    TEST("command-r → TOKEN_FAMILY_COMMAND");
    ASSERT(hermes_token_family_from_model("command-r") == TOKEN_FAMILY_COMMAND, "wrong");
    PASS();

    TEST("unknown-model → TOKEN_FAMILY_UNKNOWN");
    ASSERT(hermes_token_family_from_model("unknown-model") == TOKEN_FAMILY_UNKNOWN, "wrong");
    PASS();

    TEST("NULL → TOKEN_FAMILY_UNKNOWN");
    ASSERT(hermes_token_family_from_model(NULL) == TOKEN_FAMILY_UNKNOWN, "wrong");
    PASS();

    TEST("empty → TOKEN_FAMILY_UNKNOWN");
    ASSERT(hermes_token_family_from_model("") == TOKEN_FAMILY_UNKNOWN, "wrong");
    PASS();
}

/* ================================================================
 *  Token counting
 * ================================================================ */

static void test_token_count(void) {
    TEST("empty text → 0 tokens");
    ASSERT(hermes_token_count("", TOKEN_FAMILY_GPT4) == 0, "wrong");
    PASS();

    TEST("NULL text → 0");
    ASSERT(hermes_token_count(NULL, TOKEN_FAMILY_GPT4) == 0, "wrong");
    PASS();

    TEST("short text ~4 chars → ~1 token");
    /* "test" = 4 chars, GPT4 = 4.0 cpt → 1 token */
    ASSERT(hermes_token_count("test", TOKEN_FAMILY_GPT4) == 1, "wrong");
    PASS();

    TEST("8 chars → 2 tokens for GPT4");
    ASSERT(hermes_token_count("12345678", TOKEN_FAMILY_GPT4) == 2, "wrong");
    PASS();

    TEST("9 chars → 3 tokens (ceil)");
    ASSERT(hermes_token_count("123456789", TOKEN_FAMILY_GPT4) == 3, "wrong");
    PASS();

    TEST("claude cpt (3.5) different from GPT4 (4.0)");
    /* 14 chars / 3.5 = 4.0 → 4 tokens vs 14/4.0 = 3.5 → 4 tokens
     * Need more chars to differentiate: 350 chars */
    char buf[351];
    memset(buf, 'x', 350);
    buf[350] = '\0';
    size_t gpt_count = hermes_token_count(buf, TOKEN_FAMILY_GPT4);
    size_t claude_count = hermes_token_count(buf, TOKEN_FAMILY_CLAUDE);
    ASSERT(gpt_count != claude_count, "claude and gpt4 should give different counts");
    ASSERT(gpt_count < claude_count, "GPT4 (4.0 cpt) < Claude (3.5 cpt)");
    PASS();
}

static void test_count_for_model(void) {
    TEST("count_for_model with gpt-4");
    size_t n = hermes_token_count_for_model("Hello world, this is a test.", "gpt-4");
    ASSERT(n > 0, "should be >0");
    PASS();

    TEST("count_for_model with claude");
    n = hermes_token_count_for_model("Hello world, this is a test.", "claude-3-opus");
    ASSERT(n > 0, "should be >0");
    PASS();
}

/* ================================================================
 *  Context window sizes
 * ================================================================ */

static void test_context_size(void) {
    TEST("gpt-4o context window known");
    size_t ctx = hermes_token_context_size("gpt-4o");
    ASSERT(ctx > 0, "gpt-4o should have known context");
    PASS();

    TEST("gpt-4o exact context window");
    ASSERT(ctx == 128000, "gpt-4o context should be 128K");

    TEST("claude-3 context window");
    ctx = hermes_token_context_size("claude-3-opus");
    ASSERT(ctx > 0, "claude should have known context");
    PASS();

    TEST("unknown model → 0");
    ctx = hermes_token_context_size("unknown-model-xyz");
    ASSERT(ctx == 0, "unknown should return 0");
    PASS();

    TEST("NULL → 0");
    ctx = hermes_token_context_size(NULL);
    ASSERT(ctx == 0, "NULL should return 0");
    PASS();
}

/* ================================================================
 *  Cost rates
 * ================================================================ */

static void test_cost_rates(void) {
    TEST("GPT4 cost rates > 0");
    token_cost_rates_t r = hermes_token_cost_rates(TOKEN_FAMILY_GPT4);
    ASSERT(r.input_per_1m > 0 && r.output_per_1m > 0, "GPT4 should have costs");
    PASS();

    TEST("UNKNOWN cost rates == 0");
    r = hermes_token_cost_rates(TOKEN_FAMILY_UNKNOWN);
    ASSERT(r.input_per_1m == 0 && r.output_per_1m == 0, "unknown should be 0 cost");
    PASS();
}

/* ================================================================
 *  Message array counting
 * ================================================================ */

static void test_message_count(void) {
    TEST("single message");
    const char *msgs[] = {"Hello"};
    size_t n = hermes_token_count_messages(msgs, 1, TOKEN_FAMILY_GPT4);
    ASSERT(n > 0, "should have tokens");
    PASS();

    TEST("two messages");
    const char *msgs2[] = {"Hello", "World"};
    n = hermes_token_count_messages(msgs2, 2, TOKEN_FAMILY_GPT4);
    ASSERT(n > 0, "should have tokens");
    PASS();

    TEST("empty messages array");
    n = hermes_token_count_messages(NULL, 0, TOKEN_FAMILY_GPT4);
    ASSERT(n == 0, "empty should be 0");
    PASS();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Tokenizer Tests ===\n\n");

    test_family_detection();
    test_token_count();
    test_count_for_model();
    test_context_size();
    test_cost_rates();
    test_message_count();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
