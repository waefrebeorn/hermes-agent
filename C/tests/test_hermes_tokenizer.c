/*
 * test_hermes_tokenizer.c — Tokenizer test suite (N01).
 *
 * Tests: token family detection, chars-per-token ratio, token count,
 *        cost rates, message counting, context window sizes.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include \
 *       tests/test_hermes_tokenizer.c src/hermes_tokenizer.c \
 *       -o /tmp/hermes_test_hermes_tokenizer -lm
 *
 * Run:
 *   /tmp/hermes_test_hermes_tokenizer
 */

#include "hermes_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_NE(name, a, b) TEST(name, (a) != (b))
#define TEST_NULL(name, p) TEST(name, (p) == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, (p) != NULL)
#define TEST_FLOAT_EQ(name, a, b) TEST(name, fabsf((a) - (b)) < 0.001f)
#define TEST_DBL_EQ(name, a, b) TEST(name, fabs((a) - (b)) < 0.001)

/* ================================================================
 *  1. token_family_from_model
 * ================================================================ */
static void test_family_from_model(void) {
    printf("\n--- token_family_from_model ---\n");

    /* OpenAI */
    TEST_EQ("gpt-4o -> GPT4",
        hermes_token_family_from_model("gpt-4o"), TOKEN_FAMILY_GPT4);
    TEST_EQ("gpt-4-turbo -> GPT4",
        hermes_token_family_from_model("gpt-4-turbo"), TOKEN_FAMILY_GPT4);
    TEST_EQ("gpt-4 -> GPT4",
        hermes_token_family_from_model("gpt-4"), TOKEN_FAMILY_GPT4);
    TEST_EQ("gpt4 -> GPT4",
        hermes_token_family_from_model("gpt4"), TOKEN_FAMILY_GPT4);
    TEST_EQ("o1-preview -> GPT4",
        hermes_token_family_from_model("o1-preview"), TOKEN_FAMILY_GPT4);
    TEST_EQ("o3-mini -> GPT4",
        hermes_token_family_from_model("o3-mini"), TOKEN_FAMILY_GPT4);
    TEST_EQ("gpt-3.5-turbo -> GPT35",
        hermes_token_family_from_model("gpt-3.5-turbo"), TOKEN_FAMILY_GPT35);
    TEST_EQ("gpt35 -> GPT35",
        hermes_token_family_from_model("gpt35"), TOKEN_FAMILY_GPT35);

    /* Anthropic */
    TEST_EQ("claude-sonnet-4 -> CLAUDE",
        hermes_token_family_from_model("claude-sonnet-4"), TOKEN_FAMILY_CLAUDE);
    TEST_EQ("claude-3-haiku -> CLAUDE",
        hermes_token_family_from_model("claude-3-haiku"), TOKEN_FAMILY_CLAUDE);
    TEST_EQ("claude -> CLAUDE",
        hermes_token_family_from_model("claude"), TOKEN_FAMILY_CLAUDE);

    /* DeepSeek */
    TEST_EQ("deepseek-chat -> DEEPSEEK",
        hermes_token_family_from_model("deepseek-chat"), TOKEN_FAMILY_DEEPSEEK);
    TEST_EQ("deepseek-coder -> DEEPSEEK",
        hermes_token_family_from_model("deepseek-coder"), TOKEN_FAMILY_DEEPSEEK);

    /* Gemini */
    TEST_EQ("gemini-1.5-pro -> GEMINI",
        hermes_token_family_from_model("gemini-1.5-pro"), TOKEN_FAMILY_GEMINI);
    TEST_EQ("gemini-2.0-flash -> GEMINI",
        hermes_token_family_from_model("gemini-2.0-flash"), TOKEN_FAMILY_GEMINI);

    /* Llama */
    TEST_EQ("llama-3.1-8b -> LLAMA",
        hermes_token_family_from_model("llama-3.1-8b"), TOKEN_FAMILY_LLAMA);
    TEST_EQ("codellama -> LLAMA",
        hermes_token_family_from_model("codellama"), TOKEN_FAMILY_LLAMA);
    TEST_EQ("llm -> LLAMA",
        hermes_token_family_from_model("llm"), TOKEN_FAMILY_LLAMA);

    /* Mistral */
    TEST_EQ("mistral-large -> MISTRAL",
        hermes_token_family_from_model("mistral-large"), TOKEN_FAMILY_MISTRAL);
    TEST_EQ("mixtral-8x7b -> MISTRAL",
        hermes_token_family_from_model("mixtral-8x7b"), TOKEN_FAMILY_MISTRAL);
    TEST_EQ("ministral-8b -> MISTRAL",
        hermes_token_family_from_model("ministral-8b"), TOKEN_FAMILY_MISTRAL);

    /* Command */
    TEST_EQ("command-r -> COMMAND",
        hermes_token_family_from_model("command-r"), TOKEN_FAMILY_COMMAND);
    TEST_EQ("command-r+ -> COMMAND",
        hermes_token_family_from_model("command-r+"), TOKEN_FAMILY_COMMAND);

    /* Unknown */
    TEST_EQ("unknown model -> UNKNOWN",
        hermes_token_family_from_model("unknown-xyz-123"), TOKEN_FAMILY_UNKNOWN);
    TEST_EQ("NULL -> UNKNOWN",
        hermes_token_family_from_model(NULL), TOKEN_FAMILY_UNKNOWN);
    TEST_EQ("empty string -> UNKNOWN",
        hermes_token_family_from_model(""), TOKEN_FAMILY_UNKNOWN);
}

/* ================================================================
 *  2. chars_per_token
 * ================================================================ */
static void test_chars_per_token(void) {
    printf("\n--- chars_per_token ---\n");

    TEST_FLOAT_EQ("GPT4 = 4.0",
        hermes_token_chars_per_token(TOKEN_FAMILY_GPT4), 4.0f);
    TEST_FLOAT_EQ("GPT35 = 4.0",
        hermes_token_chars_per_token(TOKEN_FAMILY_GPT35), 4.0f);
    TEST_FLOAT_EQ("CLAUDE = 3.5",
        hermes_token_chars_per_token(TOKEN_FAMILY_CLAUDE), 3.5f);
    TEST_FLOAT_EQ("DEEPSEEK = 3.8",
        hermes_token_chars_per_token(TOKEN_FAMILY_DEEPSEEK), 3.8f);
    TEST_FLOAT_EQ("GEMINI = 3.7",
        hermes_token_chars_per_token(TOKEN_FAMILY_GEMINI), 3.7f);
    TEST_FLOAT_EQ("LLAMA = 4.2",
        hermes_token_chars_per_token(TOKEN_FAMILY_LLAMA), 4.2f);
    TEST_FLOAT_EQ("MISTRAL = 4.0",
        hermes_token_chars_per_token(TOKEN_FAMILY_MISTRAL), 4.0f);
    TEST_FLOAT_EQ("COMMAND = 4.0",
        hermes_token_chars_per_token(TOKEN_FAMILY_COMMAND), 4.0f);
    TEST_FLOAT_EQ("UNKNOWN = 4.0 (default)",
        hermes_token_chars_per_token(TOKEN_FAMILY_UNKNOWN), 4.0f);
}

/* ================================================================
 *  3. token_count
 * ================================================================ */
static void test_token_count(void) {
    printf("\n--- token_count ---\n");

    /* NULL / empty */
    TEST_EQ("NULL -> 0",
        hermes_token_count(NULL, TOKEN_FAMILY_GPT4), (size_t)0);
    TEST_EQ("empty -> 0",
        hermes_token_count("", TOKEN_FAMILY_GPT4), (size_t)0);

    /* Short text with known ratios */
    /* "hello world" = 11 chars / 4.0 = 2.75, ceil = 3 */
    TEST_EQ("hello world GPT4 = 3 tokens",
        hermes_token_count("hello world", TOKEN_FAMILY_GPT4), (size_t)3);

    /* "hello" = 5 chars / 4.0 = 1.25, ceil = 2 */
    TEST_EQ("hello GPT4 = 2 tokens",
        hermes_token_count("hello", TOKEN_FAMILY_GPT4), (size_t)2);

    /* Claude (3.5 cpt): "abcdefgh" = 8 / 3.5 = 2.29, ceil = 3 */
    TEST_EQ("8 chars Claude = 3 tokens",
        hermes_token_count("abcdefgh", TOKEN_FAMILY_CLAUDE), (size_t)3);

    /* Llama (4.2 cpt): "hello world" = 11 / 4.2 = 2.62, ceil = 3 */
    TEST_EQ("hello world Llama = 3 tokens",
        hermes_token_count("hello world", TOKEN_FAMILY_LLAMA), (size_t)3);

    /* Long text: "a" * 1000 / 4.0 = 250 */
    {
        char long_text[1001];
        memset(long_text, 'a', 1000);
        long_text[1000] = '\0';
        TEST_EQ("1000 chars GPT4 = 250 tokens",
            hermes_token_count(long_text, TOKEN_FAMILY_GPT4), (size_t)250);
    }

    /* Single char → ceil(1/4.0) = 1 */
    TEST_EQ("single char GPT4 = 1 token",
        hermes_token_count("x", TOKEN_FAMILY_GPT4), (size_t)1);
}

/* ================================================================
 *  4. token_count_for_model
 * ================================================================ */
static void test_count_for_model(void) {
    printf("\n--- token_count_for_model ---\n");

    /* Auto-detects family from model name */
    TEST_EQ("gpt-4o + text = 3 tokens",
        hermes_token_count_for_model("hello world", "gpt-4o"), (size_t)3);

    /* Claude: 11 / 3.5 = 3.14, ceil = 4 */
    TEST_EQ("claude + text = 4 tokens",
        hermes_token_count_for_model("hello world", "claude-sonnet-4"), (size_t)4);

    /* Unknown model → default 4.0 cpt */
    TEST_EQ("unknown model = 3 tokens (default 4.0)",
        hermes_token_count_for_model("hello world", "some-unknown-model"), (size_t)3);
}

/* ================================================================
 *  5. cost_rates
 * ================================================================ */
static void test_cost_rates(void) {
    printf("\n--- cost_rates ---\n");

    token_cost_rates_t r;

    r = hermes_token_cost_rates(TOKEN_FAMILY_GPT4);
    TEST_DBL_EQ("GPT4 input=$10", r.input_per_1m, 10.0);
    TEST_DBL_EQ("GPT4 output=$30", r.output_per_1m, 30.0);

    r = hermes_token_cost_rates(TOKEN_FAMILY_GPT35);
    TEST_DBL_EQ("GPT35 input=$0.50", r.input_per_1m, 0.5);
    TEST_DBL_EQ("GPT35 output=$1.50", r.output_per_1m, 1.5);

    r = hermes_token_cost_rates(TOKEN_FAMILY_CLAUDE);
    TEST_DBL_EQ("CLAUDE input=$3.0", r.input_per_1m, 3.0);
    TEST_DBL_EQ("CLAUDE output=$15.0", r.output_per_1m, 15.0);

    r = hermes_token_cost_rates(TOKEN_FAMILY_DEEPSEEK);
    TEST_DBL_EQ("DEEPSEEK input=$0.27", r.input_per_1m, 0.27);
    TEST_DBL_EQ("DEEPSEEK output=$1.10", r.output_per_1m, 1.10);

    r = hermes_token_cost_rates(TOKEN_FAMILY_GEMINI);
    TEST_DBL_EQ("GEMINI input=$0.35", r.input_per_1m, 0.35);
    TEST_DBL_EQ("GEMINI output=$1.05", r.output_per_1m, 1.05);

    r = hermes_token_cost_rates(TOKEN_FAMILY_LLAMA);
    TEST_DBL_EQ("LLAMA input=$0.25", r.input_per_1m, 0.25);
    TEST_DBL_EQ("LLAMA output=$1.0", r.output_per_1m, 1.0);

    r = hermes_token_cost_rates(TOKEN_FAMILY_MISTRAL);
    TEST_DBL_EQ("MISTRAL input=$0.25", r.input_per_1m, 0.25);
    TEST_DBL_EQ("MISTRAL output=$0.75", r.output_per_1m, 0.75);

    r = hermes_token_cost_rates(TOKEN_FAMILY_COMMAND);
    TEST_DBL_EQ("COMMAND unknown = 0,0", r.input_per_1m, 0.0);
    TEST_DBL_EQ("COMMAND unknown = 0,0", r.output_per_1m, 0.0);

    r = hermes_token_cost_rates(TOKEN_FAMILY_UNKNOWN);
    TEST_DBL_EQ("UNKNOWN = 0,0", r.input_per_1m, 0.0);
    TEST_DBL_EQ("UNKNOWN = 0,0", r.output_per_1m, 0.0);
}

/* ================================================================
 *  6. token_count_messages
 * ================================================================ */
static void test_count_messages(void) {
    printf("\n--- token_count_messages ---\n");

    const char *single[] = {"hello world"};

    /* single: 3 tokens + 4 overhead = 7 */
    size_t n = hermes_token_count_messages(single, 1, TOKEN_FAMILY_GPT4);
    TEST_EQ("single message GPT4 = 7 (3+4)",
        n, (size_t)7);

    /* Multi-message */
    const char *multi[] = {"hi", "hello world", "test"};
    /* hi: 1 token + 4 = 5; hello world: 3 + 4 = 7; test: 1 + 4 = 5 */
    /* Total: 5 + 7 + 5 = 17 */
    n = hermes_token_count_messages(multi, 3, TOKEN_FAMILY_GPT4);
    TEST_EQ("3 messages GPT4 = 17",
        n, (size_t)17);

    /* NULL / empty */
    TEST_EQ("NULL messages -> 0",
        hermes_token_count_messages(NULL, 0, TOKEN_FAMILY_GPT4), (size_t)0);
    TEST_EQ("0 count -> 0",
        hermes_token_count_messages(single, 0, TOKEN_FAMILY_GPT4), (size_t)0);
}

/* ================================================================
 *  7. token_context_size
 * ================================================================ */
static void test_context_size(void) {
    printf("\n--- token_context_size ---\n");

    TEST_EQ("gpt-4o = 128k",
        hermes_token_context_size("gpt-4o"), (size_t)128000);
    TEST_EQ("gpt-4-turbo = 128k",
        hermes_token_context_size("gpt-4-turbo"), (size_t)128000);
    TEST_EQ("o3-mini = 200k",
        hermes_token_context_size("o3-mini"), (size_t)200000);
    TEST_EQ("claude-sonnet-4 = 200k",
        hermes_token_context_size("claude-sonnet-4"), (size_t)200000);
    TEST_EQ("deepseek-chat = 64k",
        hermes_token_context_size("deepseek-chat"), (size_t)65536);
    TEST_EQ("gemini-1.5-pro = 1M",
        hermes_token_context_size("gemini-1.5-pro"), (size_t)1048576);
    TEST_EQ("llama-3.1-8b = 128k",
        hermes_token_context_size("llama-3.1-8b"), (size_t)131072);
    TEST_EQ("mistral-large = 128k",
        hermes_token_context_size("mistral-large"), (size_t)131072);

    /* Unknown / edge */
    TEST_EQ("unknown model -> 0",
        hermes_token_context_size("nonexistent-model-9000"), (size_t)0);
    TEST_EQ("NULL -> 0",
        hermes_token_context_size(NULL), (size_t)0);
    TEST_EQ("empty -> 0",
        hermes_token_context_size(""), (size_t)0);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Hermes Tokenizer Test Suite ===\n");

    test_family_from_model();
    test_chars_per_token();
    test_token_count();
    test_count_for_model();
    test_cost_rates();
    test_count_messages();
    test_context_size();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
