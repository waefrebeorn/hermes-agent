/**
 * @defgroup hermes_tokenizer Tokenizer
 * @brief Model-aware token counting.
 *
 * 
Heuristic token counter supporting 9 model families
with configurable chars-per-token ratio, context window
sizes, and cost rate estimation.
 *
 * @{
 */
#ifndef HERMES_TOKENIZER_H
#define HERMES_TOKENIZER_H

/*
 * hermes_tokenizer.h — Token counting for Hermes C.
 *
 * N01: Provides model-aware token counting for accurate session
 * token tracking, cost estimation, and budget enforcement.
 *
 * Python equivalent: tiktoken via tiktoken.encoding_for_model()
 * + tiktoken.get_encoding("cl100k_base") for word/char estimates.
 *
 * Since full tiktoken BPE tables are complex to embed, uses
 * heuristic estimates calibrated per model family:
 *   - OpenAI (gpt-4, gpt-3.5): ~4 chars/token
 *   - Anthropic (claude): ~3.5 chars/token
 *   - Other: ~4 chars/token (best guess)
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Token counting
 * ================================================================ */

/* Known model families for token counting calibration */
typedef enum {
    TOKEN_FAMILY_UNKNOWN = 0,   /* fallback: 4 chars/token */
    TOKEN_FAMILY_GPT4,          /* gpt-4, gpt-4o, gpt-4-turbo */
    TOKEN_FAMILY_GPT35,         /* gpt-3.5-turbo */
    TOKEN_FAMILY_CLAUDE,        /* claude-3, claude-3.5, claude-4 */
    TOKEN_FAMILY_DEEPSEEK,      /* deepseek, deepseek-chat */
    TOKEN_FAMILY_GEMINI,        /* gemini-pro, gemini-ultra */
    TOKEN_FAMILY_LLAMA,         /* llama, llama3, etc. */
    TOKEN_FAMILY_MISTRAL,       /* mistral, mixtral */
    TOKEN_FAMILY_COMMAND,       /* command-r, command-r+ */
} token_family_t;

/* Detect token family from model name string */
token_family_t hermes_token_family_from_model(const char *model);

/* Characters per token ratio for a given family */
#define TOKEN_CHARS_PER_TOKEN_GPT4       4.0f
#define TOKEN_CHARS_PER_TOKEN_GPT35      4.0f
#define TOKEN_CHARS_PER_TOKEN_CLAUDE     3.5f
#define TOKEN_CHARS_PER_TOKEN_DEEPSEEK   3.8f
#define TOKEN_CHARS_PER_TOKEN_GEMINI     3.7f
#define TOKEN_CHARS_PER_TOKEN_LLAMA      4.2f
#define TOKEN_CHARS_PER_TOKEN_MISTRAL    4.0f
#define TOKEN_CHARS_PER_TOKEN_COMMAND    4.0f
#define TOKEN_CHARS_PER_TOKEN_DEFAULT    4.0f

/* Get chars-per-token ratio for a family */
float hermes_token_chars_per_token(token_family_t family);

/* Count tokens in text for a given model family.
 * Uses heuristic: ceil(len(text) / chars_per_token)
 * Returns 0 for NULL/empty input. */
size_t hermes_token_count(const char *text, token_family_t family);

/* Count tokens in text, auto-detecting family from model name.
 * Convenience wrapper. */
size_t hermes_token_count_for_model(const char *text, const char *model);

/* Estimate cost for a given number of tokens.
 * Uses approximate $/1M token rates for each family.
 * Returns cost in USD * 1e6 (microdollars) for integer precision.
 * 0 = unknown cost. */
typedef struct {
    double input_per_1m;    /* $ per 1M input tokens */
    double output_per_1m;   /* $ per 1M output tokens */
} token_cost_rates_t;

token_cost_rates_t hermes_token_cost_rates(token_family_t family);

/* Count total tokens in a message array (conversation).
 * Includes per-message overhead (~4 tokens for role markers). */
size_t hermes_token_count_messages(const char *const *messages,
                                    size_t count,
                                    token_family_t family);

/* Get token limit for a model (max context window). 0 = unknown. */
size_t hermes_token_context_size(const char *model);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_tokenizer group */
#endif /* HERMES_TOKENIZER_H */
