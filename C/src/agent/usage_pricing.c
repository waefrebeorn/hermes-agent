/*
 * usage_pricing.c -- Token cost estimation for Hermes C.
 *
 * Per-model and per-family pricing. Python equivalent: agent/usage_pricing.py
 *
 * MIT License -- WuBu Hermes Project
 */

#include "usage_pricing.h"
#include "hermes_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

/* ================================================================
 *  Per-model pricing entries
 * ================================================================ */

typedef struct {
    const char *provider;
    const char *model_name;    /* substring match (NULL = catch-all for provider) */
    double  input_per_1m;      /* $ per 1M input tokens */
    double  output_per_1m;     /* $ per 1M output tokens */
    double  cache_read_per_1m; /* $ per 1M cache read tokens */
    double  cache_write_per_1m;
} pricing_entry_t;

/* Official docs snapshot pricing for common models.
 * Cache rates: Anthropic = 10% input for read, 125% input for write.
 * OpenAI = 50% input for read. DeepSeek = 10% input for read. */
static const pricing_entry_t PRICING_TABLE[] = {
    /* -- Anthropic Claude 4.x Opus -- */
    {"anthropic","claude-opus-4",          5.00,  25.00,  0.50,   6.25},
    /* -- Anthropic Claude 4.x Sonnet -- */
    {"anthropic","claude-sonnet-4",        3.00,  15.00,  0.30,   3.75},
    /* -- Anthropic Claude 4.x Haiku -- */
    {"anthropic","claude-haiku-4",         1.00,   5.00,  0.10,   1.25},
    /* -- Anthropic Claude 3.5 -- */
    {"anthropic","claude-3.5",             2.00,  10.00,  0.20,   2.50},
    {"anthropic","claude-3",               2.00,  10.00,  0.20,   2.50},
    /* -- Anthropic catch-all -- */
    {"anthropic",NULL,                     3.00,  15.00,  0.30,   3.75},

    /* -- OpenAI GPT-4o -- */
    {"openai","gpt-4o",                   10.00,  30.00,  5.00,   0.0},
    {"openai","gpt-4o-mini",              0.15,   0.60,  0.075,  0.0},
    {"openai","gpt-4-turbo",             10.00,  30.00,  5.00,   0.0},
    {"openai","gpt-4",                   30.00,  60.00, 15.00,   0.0},
    {"openai","gpt-3.5-turbo",            0.50,   1.50,  0.25,   0.0},
    {"openai","o1",                      15.00,  60.00,  7.50,   0.0},
    {"openai","o3",                      10.00,  40.00,  5.00,   0.0},
    /* -- OpenAI catch-all -- */
    {"openai",NULL,                       10.00,  30.00,  5.00,   0.0},

    /* -- DeepSeek -- */
    {"deepseek","deepseek-chat",           0.27,   1.10,  0.027,  0.0},
    {"deepseek","deepseek-reasoner",       0.55,   2.19,  0.055,  0.0},
    {"deepseek","deepseek-r1",             0.55,   2.19,  0.055,  0.0},
    /* -- DeepSeek catch-all -- */
    {"deepseek",NULL,                      0.27,   1.10,  0.027,  0.0},

    /* -- xAI Grok -- */
    {"xai","grok-3",                      5.00,  15.00,  2.50,   0.0},
    {"xai","grok-2",                      2.00,  10.00,  1.00,   0.0},
    {"xai",NULL,                          5.00,  15.00,  2.50,   0.0},

    /* -- Google Gemini -- */
    {"google","gemini-2.0",               1.25,   5.00,  0.0,    0.0},
    {"google","gemini-1.5-pro",           1.25,   5.00,  0.0,    0.0},
    {"google","gemini-1.5-flash",         0.075,  0.30,  0.0,    0.0},
    {"google","gemini-2.0-flash",         0.10,   0.40,  0.0,    0.0},
    {"google",NULL,                       1.25,   5.00,  0.0,    0.0},

    /* -- Azure OpenAI -- */
    {"azure","gpt-4o",                   10.00,  30.00,  5.00,   0.0},
    {"azure",NULL,                       10.00,  30.00,  5.00,   0.0},

    /* -- Amazon Bedrock -- */
    {"bedrock","claude",                   3.00,  15.00,  0.30,   3.75},
    {"bedrock",NULL,                       3.00,  15.00,  0.30,   3.75},
};

static const int PRICING_TABLE_COUNT = sizeof(PRICING_TABLE) / sizeof(PRICING_TABLE[0]);

/* ================================================================
 *  Provider + model extraction
 * ================================================================ */

/* Extract provider from "provider/model" format, or empty string if none. */
static void extract_provider(const char *model, char *provider, size_t psize) {
    provider[0] = '\0';
    if (!model) return;
    const char *slash = strchr(model, '/');
    if (slash) {
        size_t len = (size_t)(slash - model);
        if (len >= psize) len = psize - 1;
        memcpy(provider, model, len);
        provider[len] = '\0';
    }
}

/* Get the model name part (after "/" if any, otherwise the whole string). */
static const char *model_name_part(const char *model) {
    if (!model) return "";
    const char *slash = strchr(model, '/');
    return slash ? slash + 1 : model;
}

/* ================================================================
 *  Lookup
 * ================================================================ */

/* Find the best pricing entry for a provider+model combination.
 * First tries exact model match, then provider catch-all, then returns family from tokenizer. */
static bool lookup_pricing(const char *provider, const char *model,
                           double *input, double *output,
                           double *cache_read, double *cache_write,
                           char *source, size_t source_size) {
    /* Try per-model exact/substring match first */
    int provider_catchall = -1;
    for (int i = 0; i < PRICING_TABLE_COUNT; i++) {
        if (strcasecmp(PRICING_TABLE[i].provider, provider) != 0)
            continue;
        if (PRICING_TABLE[i].model_name == NULL) {
            if (provider_catchall < 0)
                provider_catchall = i;
            continue;
        }
        /* Substring match */
        if (strstr(model, PRICING_TABLE[i].model_name)) {
            *input = PRICING_TABLE[i].input_per_1m;
            *output = PRICING_TABLE[i].output_per_1m;
            *cache_read = PRICING_TABLE[i].cache_read_per_1m;
            *cache_write = PRICING_TABLE[i].cache_write_per_1m;
            snprintf(source, source_size, "docs_snapshot");
            return true;
        }
    }

    /* Provider catch-all */
    if (provider_catchall >= 0) {
        const pricing_entry_t *p = &PRICING_TABLE[provider_catchall];
        *input = p->input_per_1m;
        *output = p->output_per_1m;
        *cache_read = p->cache_read_per_1m;
        *cache_write = p->cache_write_per_1m;
        snprintf(source, source_size, "docs_snapshot");
        return true;
    }

    /* Fall back to family-level rates from tokenizer */
    token_family_t family = hermes_token_family_from_model(model);
    token_cost_rates_t rates = hermes_token_cost_rates(family);
    if (rates.input_per_1m > 0 || rates.output_per_1m > 0) {
        *input = rates.input_per_1m;
        *output = rates.output_per_1m;
        *cache_read = rates.input_per_1m * 0.5;   /* 50% of input for cache read */
        *cache_write = 0.0;                         /* unknown */
        snprintf(source, source_size, "family_rates");
        return true;
    }

    return false; /* unknown pricing */
}

/* ================================================================
 *  Public API
 * ================================================================ */

usage_cost_t usage_pricing_estimate(const char *model, const usage_counts_t *usage) {
    usage_cost_t cost;
    memset(&cost, 0, sizeof(cost));

    if (!model) model = "unknown";
    if (!usage) {
        cost.status = COST_STATUS_UNKNOWN;
        cost.has_pricing = false;
        return cost;
    }

    char provider[PRICING_PROVIDER_MAX] = "";
    extract_provider(model, provider, sizeof(provider));
    const char *mname = model_name_part(model);

    double input_rate = 0, output_rate = 0, cache_read_rate = 0, cache_write_rate = 0;
    char source[PRICING_SOURCE_MAX] = "none";

    bool found = lookup_pricing(provider, mname,
                                &input_rate, &output_rate,
                                &cache_read_rate, &cache_write_rate,
                                source, sizeof(source));

    if (!found) {
        cost.status = COST_STATUS_UNKNOWN;
        cost.has_pricing = false;
        snprintf(cost.label, sizeof(cost.label), "%s: unknown pricing", model);
        return cost;
    }

    cost.has_pricing = true;
    cost.status = COST_STATUS_ESTIMATED;
    snprintf(cost.source, sizeof(cost.source), "%s", source);

    /* Compute costs: tokens / 1M * rate */
    cost.input_cost = (double)usage->input_tokens / 1000000.0 * input_rate;
    cost.output_cost = (double)usage->output_tokens / 1000000.0 * output_rate;
    cost.cache_read_cost = (double)usage->cache_read_tokens / 1000000.0 * cache_read_rate;
    cost.cache_write_cost = (double)usage->cache_write_tokens / 1000000.0 * cache_write_rate;
    cost.total_cost = cost.input_cost + cost.output_cost
                    + cost.cache_read_cost + cost.cache_write_cost;

    snprintf(cost.label, sizeof(cost.label),
             "$%.2f/$%.2f per 1M tokens (in/out)",
             input_rate, output_rate);

    if (cache_read_rate > 0 || cache_write_rate > 0) {
        size_t len = strlen(cost.label);
        snprintf(cost.label + len, sizeof(cost.label) - len,
                 "; cache $%.2f/$%.2f", cache_read_rate, cache_write_rate);
    }

    return cost;
}

bool usage_pricing_known(const char *model) {
    if (!model) return false;
    char provider[PRICING_PROVIDER_MAX] = "";
    extract_provider(model, provider, sizeof(provider));
    const char *mname = model_name_part(model);

    double input_rate, output_rate, cache_read_rate, cache_write_rate;
    char source[PRICING_SOURCE_MAX];

    return lookup_pricing(provider, mname,
                          &input_rate, &output_rate,
                          &cache_read_rate, &cache_write_rate,
                          source, sizeof(source));
}

const char *usage_pricing_format_cost(double usd) {
    static char buf[32];
    if (usd < 0.0001) {
        snprintf(buf, sizeof(buf), "$0.00");
    } else if (usd < 0.01) {
        snprintf(buf, sizeof(buf), "$%.4f", usd);
    } else if (usd < 1.0) {
        snprintf(buf, sizeof(buf), "$%.2f", usd);
    } else if (usd < 100.0) {
        snprintf(buf, sizeof(buf), "$%.2f", usd);
    } else {
        snprintf(buf, sizeof(buf), "$%.0f", usd);
    }
    return buf;
}

const char *usage_pricing_format_duration(double seconds) {
    static char buf[24];
    if (seconds < 0) seconds = 0;

    if (seconds < 60.0) {
        snprintf(buf, sizeof(buf), "%ds", (int)seconds);
    } else if (seconds < 3600.0) {
        snprintf(buf, sizeof(buf), "%dm", (int)(seconds / 60.0));
    } else if (seconds < 86400.0) {
        snprintf(buf, sizeof(buf), "%.1fh", seconds / 3600.0);
    } else {
        snprintf(buf, sizeof(buf), "%.1fd", seconds / 86400.0);
    }
    return buf;
}
