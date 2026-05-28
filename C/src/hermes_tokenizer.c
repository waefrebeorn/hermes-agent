/*
 * hermes_tokenizer.c — Token counting for Hermes C.
 *
 * N01: Heuristic token counting for session tracking, cost
 * estimation, and budget enforcement. No BPE tables needed.
 */

#include "hermes_tokenizer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 *  Model family detection
 * ================================================================ */

token_family_t hermes_token_family_from_model(const char *model) {
    if (!model || model[0] == '\0')
        return TOKEN_FAMILY_UNKNOWN;

    /* Normalize: lowercase + trim prefix */
    /* We check against known prefixes */
    const char *p = model;

    /* OpenAI models */
    if (strstr(p, "gpt-4") || strstr(p, "gpt4") ||
        strstr(p, "o1") || strstr(p, "o3")) {
        /* Check for older gpt-4 vs gpt-4o */
        if (strstr(p, "gpt-4o"))
            return TOKEN_FAMILY_GPT4;
        if (strstr(p, "gpt-4-turbo"))
            return TOKEN_FAMILY_GPT4;
        return TOKEN_FAMILY_GPT4;
    }
    if (strstr(p, "gpt-3.5") || strstr(p, "gpt35"))
        return TOKEN_FAMILY_GPT35;

    /* Anthropic */
    if (strstr(p, "claude"))
        return TOKEN_FAMILY_CLAUDE;

    /* DeepSeek */
    if (strstr(p, "deepseek"))
        return TOKEN_FAMILY_DEEPSEEK;

    /* Google Gemini */
    if (strstr(p, "gemini"))
        return TOKEN_FAMILY_GEMINI;

    /* Meta Llama */
    if (strstr(p, "llama") || strstr(p, "llm") || strstr(p, "codellama"))
        return TOKEN_FAMILY_LLAMA;

    /* Mistral */
    if (strstr(p, "mistral") || strstr(p, "mixtral") || strstr(p, "ministral"))
        return TOKEN_FAMILY_MISTRAL;

    /* Cohere Command */
    if (strstr(p, "command"))
        return TOKEN_FAMILY_COMMAND;

    return TOKEN_FAMILY_UNKNOWN;
}

/* ================================================================
 *  Chars-per-token ratio
 * ================================================================ */

float hermes_token_chars_per_token(token_family_t family) {
    switch (family) {
        case TOKEN_FAMILY_GPT4:     return TOKEN_CHARS_PER_TOKEN_GPT4;
        case TOKEN_FAMILY_GPT35:    return TOKEN_CHARS_PER_TOKEN_GPT35;
        case TOKEN_FAMILY_CLAUDE:   return TOKEN_CHARS_PER_TOKEN_CLAUDE;
        case TOKEN_FAMILY_DEEPSEEK: return TOKEN_CHARS_PER_TOKEN_DEEPSEEK;
        case TOKEN_FAMILY_GEMINI:   return TOKEN_CHARS_PER_TOKEN_GEMINI;
        case TOKEN_FAMILY_LLAMA:    return TOKEN_CHARS_PER_TOKEN_LLAMA;
        case TOKEN_FAMILY_MISTRAL:  return TOKEN_CHARS_PER_TOKEN_MISTRAL;
        case TOKEN_FAMILY_COMMAND:  return TOKEN_CHARS_PER_TOKEN_COMMAND;
        default:                    return TOKEN_CHARS_PER_TOKEN_DEFAULT;
    }
}

/* ================================================================
 *  Core token counting
 * ================================================================ */

size_t hermes_token_count(const char *text, token_family_t family) {
    if (!text || text[0] == '\0')
        return 0;

    size_t len = strlen(text);

    /* Ceiling division using scaled integer arithmetic to avoid float precision issues.
     * e.g. 1000/4.0 with ceil: 250 not 251, which (float)(1000/4.0 + 0.999) gets wrong. */
    static const int cpt_x10_table[] = {
        40, /* UNKNOWN */
        40, /* GPT4 */
        40, /* GPT35 */
        35, /* CLAUDE */
        38, /* DEEPSEEK */
        37, /* GEMINI */
        42, /* LLAMA */
        40, /* MISTRAL */
        40, /* COMMAND */
    };
    int cpt10 = (family >= 0 && family < (int)(sizeof(cpt_x10_table)/sizeof(cpt_x10_table[0])))
                ? cpt_x10_table[family] : 40;
    size_t tokens = (len * 10 + (size_t)cpt10 - 1) / (size_t)cpt10;
    if (tokens == 0) tokens = 1;
    return tokens;
}

size_t hermes_token_count_for_model(const char *text, const char *model) {
    token_family_t family = hermes_token_family_from_model(model);
    return hermes_token_count(text, family);
}

/* ================================================================
 *  Cost estimation
 * ================================================================ */

token_cost_rates_t hermes_token_cost_rates(token_family_t family) {
    token_cost_rates_t rates = {0, 0};
    switch (family) {
        case TOKEN_FAMILY_GPT4:
            rates.input_per_1m = 10.0;   /* gpt-4o: $10/M input */
            rates.output_per_1m = 30.0;  /* gpt-4o: $30/M output */
            break;
        case TOKEN_FAMILY_GPT35:
            rates.input_per_1m = 0.5;    /* gpt-3.5-turbo: $0.50/M */
            rates.output_per_1m = 1.5;
            break;
        case TOKEN_FAMILY_CLAUDE:
            rates.input_per_1m = 3.0;    /* claude-sonnet-4: $3/M */
            rates.output_per_1m = 15.0;
            break;
        case TOKEN_FAMILY_DEEPSEEK:
            rates.input_per_1m = 0.27;   /* deepseek-chat: $0.27/M */
            rates.output_per_1m = 1.10;
            break;
        case TOKEN_FAMILY_GEMINI:
            rates.input_per_1m = 0.35;   /* gemini-1.5-pro: $0.35/M */
            rates.output_per_1m = 1.05;
            break;
        case TOKEN_FAMILY_LLAMA:
            rates.input_per_1m = 0.25;   /* meta-llama: $0.25/M (Together) */
            rates.output_per_1m = 1.0;
            break;
        case TOKEN_FAMILY_MISTRAL:
            rates.input_per_1m = 0.25;   /* mistral-large: $2-4/M, but varies */
            rates.output_per_1m = 0.75;
            break;
        default:
            break; /* unknown — returns 0 */
    }
    return rates;
}

/* ================================================================
 *  Conversation counting
 * ================================================================ */

size_t hermes_token_count_messages(const char *const *messages,
                                    size_t count,
                                    token_family_t family) {
    if (!messages || count == 0) return 0;

    size_t total = 0;
    /* Per-message overhead: role markers + formatting */
    const size_t MSG_OVERHEAD_TOKENS = 4;

    for (size_t i = 0; i < count; i++) {
        if (messages[i]) {
            total += hermes_token_count(messages[i], family);
            total += MSG_OVERHEAD_TOKENS;
        }
    }
    return total;
}

/* ================================================================
 *  Context window sizes
 * ================================================================ */

/* Known context windows from various model families */
typedef struct {
    const char *pattern;
    size_t context;
} context_window_t;

static const context_window_t KNOWN_CONTEXTS[] = {
    /* OpenAI */
    {"gpt-4o",           128000},
    {"gpt-4-turbo",      128000},
    {"o1-preview",       128000},
    {"o1-mini",          128000},
    {"gpt-3.5-turbo",     16385},
    {"o3-mini",          200000},

    /* Anthropic */
    {"claude-sonnet-4",  200000},
    {"claude-3.5-sonnet",200000},
    {"claude-3-haiku",   200000},
    {"claude-3-opus",    200000},

    /* DeepSeek */
    {"deepseek-chat",    65536},
    {"deepseek-coder",   65536},
    {"deepseek-v3",      65536},

    /* Google */
    {"gemini-1.5-pro",   1048576}, /* 1M tokens */
    {"gemini-1.5-flash", 1048576},
    {"gemini-2.0",      1048576},
    {"gemini-2.5",      1048576},

    /* Meta */
    {"llama-3.1-405b",   131072},
    {"llama-3.1-70b",    131072},
    {"llama-3.1-8b",     131072},

    /* Mistral */
    {"mistral-large",    131072},
    {"ministral-8b",     131072},
};

#define KNOWN_CONTEXTS_COUNT \
    (sizeof(KNOWN_CONTEXTS) / sizeof(KNOWN_CONTEXTS[0]))

size_t hermes_token_context_size(const char *model) {
    if (!model || model[0] == '\0') return 0;

    for (size_t i = 0; i < KNOWN_CONTEXTS_COUNT; i++) {
        if (strstr(model, KNOWN_CONTEXTS[i].pattern))
            return KNOWN_CONTEXTS[i].context;
    }
    return 0; /* unknown */
}
