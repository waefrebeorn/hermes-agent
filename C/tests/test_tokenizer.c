#include "hermes_tokenizer.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Tokenizer Tests ===\n");

    /* Test model family detection */
    TEST("gpt-4o → GPT4", hermes_token_family_from_model("gpt-4o") == TOKEN_FAMILY_GPT4);
    TEST("gpt-4-turbo → GPT4", hermes_token_family_from_model("gpt-4-turbo") == TOKEN_FAMILY_GPT4);
    TEST("claude-sonnet-4 → CLAUDE", hermes_token_family_from_model("claude-sonnet-4") == TOKEN_FAMILY_CLAUDE);
    TEST("deepseek-chat → DEEPSEEK", hermes_token_family_from_model("deepseek-chat") == TOKEN_FAMILY_DEEPSEEK);
    TEST("gemini-1.5-pro → GEMINI", hermes_token_family_from_model("gemini-1.5-pro") == TOKEN_FAMILY_GEMINI);
    TEST("llama-3.1-70b → LLAMA", hermes_token_family_from_model("llama-3.1-70b") == TOKEN_FAMILY_LLAMA);
    TEST("mistral-large → MISTRAL", hermes_token_family_from_model("mistral-large") == TOKEN_FAMILY_MISTRAL);
    TEST("command-r → COMMAND", hermes_token_family_from_model("command-r") == TOKEN_FAMILY_COMMAND);
    TEST("unknown → UNKNOWN", hermes_token_family_from_model("foo-bar-baz") == TOKEN_FAMILY_UNKNOWN);
    TEST("NULL → UNKNOWN", hermes_token_family_from_model(NULL) == TOKEN_FAMILY_UNKNOWN);
    TEST("empty → UNKNOWN", hermes_token_family_from_model("") == TOKEN_FAMILY_UNKNOWN);

    /* Test chars per token ratio */
    TEST("GPT4 chars/token ≈ 4.0", hermes_token_chars_per_token(TOKEN_FAMILY_GPT4) == 4.0f);
    TEST("CLAUDE chars/token ≈ 3.5", hermes_token_chars_per_token(TOKEN_FAMILY_CLAUDE) == 3.5f);
    TEST("UNKNOWN chars/token ≈ 4.0", hermes_token_chars_per_token(TOKEN_FAMILY_UNKNOWN) == 4.0f);

    /* Test token counting */
    TEST("empty → 0", hermes_token_count("", TOKEN_FAMILY_UNKNOWN) == 0);
    TEST("NULL → 0", hermes_token_count(NULL, TOKEN_FAMILY_UNKNOWN) == 0);
    /* "hello" = 5 chars / 4.0 ≈ 2 tokens */
    TEST("short text ≈ 2 tokens", hermes_token_count("hello", TOKEN_FAMILY_UNKNOWN) == 2);
    /* "a" = 1 char / 4.0 → ceil(0.25) = 1 */
    TEST("single char → 1", hermes_token_count("a", TOKEN_FAMILY_UNKNOWN) == 1);
    /* 20 chars / 4.0 = 5 tokens */
    TEST("20 chars → 5 tokens", hermes_token_count("12345678901234567890", TOKEN_FAMILY_UNKNOWN) == 5);
    /* 21 chars / 4.0 = ceil(5.25) = 6 */
    TEST("21 chars → 6 tokens", hermes_token_count("123456789012345678901", TOKEN_FAMILY_UNKNOWN) == 6);
    /* Claude ratio: 21 / 3.5 = 6 */
    TEST("21 chars claude → 6 tokens", hermes_token_count("123456789012345678901", TOKEN_FAMILY_CLAUDE) == 6);
    /* DeepSeek ratio: 21 / 3.8 = ceil(5.53) = 6 */
    TEST("21 chars deepseek → 6 tokens", hermes_token_count("123456789012345678901", TOKEN_FAMILY_DEEPSEEK) == 6);

    /* Test model-aware convenience wrapper */
    TEST("model-aware gpt-4o", hermes_token_count_for_model("hello world", "gpt-4o") > 0);
    TEST("model-aware claude", hermes_token_count_for_model("hello world", "claude-sonnet-4") > 0);

    /* Test context window sizes */
    TEST("gpt-4o ctx = 128K", hermes_token_context_size("gpt-4o") == 128000);
    TEST("claude-sonnet-4 ctx = 200K", hermes_token_context_size("claude-sonnet-4") == 200000);
    TEST("deepseek-chat ctx = 64K", hermes_token_context_size("deepseek-chat") == 65536);
    TEST("gemini-1.5-pro ctx = 1M", hermes_token_context_size("gemini-1.5-pro") == 1048576);
    TEST("unknown model ctx = 0", hermes_token_context_size("foobar-v3") == 0);
    TEST("NULL model ctx = 0", hermes_token_context_size(NULL) == 0);

    /* Test cost rates */
    TEST("GPT4 input cost > 0", hermes_token_cost_rates(TOKEN_FAMILY_GPT4).input_per_1m > 0);
    TEST("CLAUDE output cost > 0", hermes_token_cost_rates(TOKEN_FAMILY_CLAUDE).output_per_1m > 0);
    TEST("UNKNOWN cost = 0", hermes_token_cost_rates(TOKEN_FAMILY_UNKNOWN).input_per_1m == 0.0);

    /* Test message counting */
    const char *msgs[] = {"hello", "world", "test message here"};
    size_t msg_tok = hermes_token_count_messages(msgs, 3, TOKEN_FAMILY_UNKNOWN);
    TEST("3 messages count > 0", msg_tok > 0);
    TEST("NULL msgs → 0", hermes_token_count_messages(NULL, 0, TOKEN_FAMILY_UNKNOWN) == 0);
    TEST("0 count → 0", hermes_token_count_messages(msgs, 0, TOKEN_FAMILY_UNKNOWN) == 0);

    /* Test family from model — partial match */
    TEST("o3-mini → GPT4 (o-series)", hermes_token_family_from_model("o3-mini") == TOKEN_FAMILY_GPT4);
    TEST("deepseek-v3 → DEEPSEEK", hermes_token_family_from_model("deepseek-v3") == TOKEN_FAMILY_DEEPSEEK);
    TEST("codellama → LLAMA", hermes_token_family_from_model("codellama-34b") == TOKEN_FAMILY_LLAMA);

    printf("\n=== Tokenizer: %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
