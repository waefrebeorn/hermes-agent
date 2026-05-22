/*
 * budget_tracker.c — Per-session budget tracking (P84).
 *
 * Tracks token/cost usage with configurable limits and alerts.
 */

#include "budget_tracker.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Per-model pricing (approximate USD per 1M tokens)
 *  Source: public API pricing as of May 2026.
 * ================================================================ */

typedef struct {
    const char *model_prefix;  /* matched by prefix */
    double input_per_1m;       /* USD per 1M input tokens */
    double output_per_1m;      /* USD per 1M output tokens */
} model_pricing_t;

static const model_pricing_t PRICING[] = {
    /* OpenAI — longest prefixes first to avoid false matches */
    {"gpt-4.1",        2.00,  8.00},
    {"gpt-4o-mini",    0.15,  0.60},
    {"gpt-4o",         2.50, 10.00},
    {"gpt-4-turbo",   10.00, 30.00},
    {"gpt-4",         30.00, 60.00},
    {"gpt-3.5",        0.50,  1.50},
    {"o1",            15.00, 60.00},
    {"o3-mini",        1.10,  4.40},
    /* Anthropic */
    {"claude-3.5-sonnet", 3.00, 15.00},
    {"claude-3.5-haiku", 0.80,  4.00},
    {"claude-3-opus",   15.00, 75.00},
    {"claude-4",        3.00, 15.00},
    /* Google */
    {"gemini-2.0-pro",  2.00,  8.00},
    {"gemini-2.0-flash", 0.10, 0.40},
    {"gemini-1.5-pro",  3.50, 10.50},
    {"gemini-1.5-flash", 0.075, 0.30},
    /* DeepSeek */
    {"deepseek-chat",   0.27,  1.10},
    {"deepseek-reasoner", 0.55, 2.19},
    /* Default fallback */
    {NULL,              1.00,  4.00}, /* catch-all */
};

/* ================================================================
 *  Helpers
 * ================================================================ */

static const model_pricing_t *find_pricing(const char *model) {
    if (!model) return &PRICING[sizeof(PRICING)/sizeof(PRICING[0]) - 1]; /* default */
    for (int i = 0; PRICING[i].model_prefix; i++) {
        if (strncasecmp(model, PRICING[i].model_prefix, strlen(PRICING[i].model_prefix)) == 0)
            return &PRICING[i];
    }
    return &PRICING[sizeof(PRICING)/sizeof(PRICING[0]) - 1]; /* default */
}

/* ================================================================
 *  Public API
 * ================================================================ */

void budget_tracker_init(budget_tracker_t *bt) {
    memset(bt, 0, sizeof(*bt));
    bt->warn_at_pct = 0.8;
    /* All limits default to 0 = unlimited */
}

void budget_tracker_set_limits(budget_tracker_t *bt,
                                long long max_input,
                                long long max_output,
                                double max_cost,
                                int max_turns) {
    if (!bt) return;
    bt->max_input_tokens = max_input;
    bt->max_output_tokens = max_output;
    bt->max_cost_usd = max_cost;
    bt->max_turns = max_turns;
}

void budget_tracker_report_turn(budget_tracker_t *bt,
                                 long long input_tokens,
                                 long long output_tokens,
                                 double cost_usd,
                                 const char *model) {
    if (!bt) return;

    bt->total_input_tokens += input_tokens;
    bt->total_output_tokens += output_tokens;
    bt->total_cost_usd += cost_usd;
    bt->turn_count++;

    bt->last_input_tokens = input_tokens;
    bt->last_output_tokens = output_tokens;
    bt->last_cost_usd = cost_usd;
    if (model)
        snprintf(bt->last_model, sizeof(bt->last_model), "%s", model);

    /* Check warnings */
    if (bt->max_input_tokens > 0 &&
        !bt->warned_input &&
        bt->total_input_tokens >= (long long)(bt->max_input_tokens * bt->warn_at_pct)) {
        bt->warned_input = true;
    }
    if (bt->max_output_tokens > 0 &&
        !bt->warned_output &&
        bt->total_output_tokens >= (long long)(bt->max_output_tokens * bt->warn_at_pct)) {
        bt->warned_output = true;
    }
    if (bt->max_cost_usd > 0.0 &&
        !bt->warned_cost &&
        bt->total_cost_usd >= bt->max_cost_usd * bt->warn_at_pct) {
        bt->warned_cost = true;
    }
    if (bt->max_turns > 0 &&
        !bt->warned_turns &&
        bt->turn_count >= (int)(bt->max_turns * bt->warn_at_pct)) {
        bt->warned_turns = true;
    }
}

bool budget_tracker_is_exceeded(const budget_tracker_t *bt) {
    if (!bt) return false;
    if (bt->max_input_tokens > 0 && bt->total_input_tokens >= bt->max_input_tokens)
        return true;
    if (bt->max_output_tokens > 0 && bt->total_output_tokens >= bt->max_output_tokens)
        return true;
    if (bt->max_cost_usd > 0.0 && bt->total_cost_usd >= bt->max_cost_usd)
        return true;
    if (bt->max_turns > 0 && bt->turn_count >= bt->max_turns)
        return true;
    return false;
}

const char *budget_tracker_get_warning(budget_tracker_t *bt) {
    if (!bt) return NULL;

    /* Static buffer for warning messages */
    static char warn_buf[256];

    if (bt->warned_input) {
        bt->warned_input = false; /* Clear so we only emit once */
        long long remaining = budget_tracker_remaining_input(bt);
        snprintf(warn_buf, sizeof(warn_buf),
                 "Input token budget at %lld%% — %lld remaining",
                 (long long)((double)bt->total_input_tokens / bt->max_input_tokens * 100),
                 remaining > 0 ? remaining : 0);
        return warn_buf;
    }
    if (bt->warned_output) {
        bt->warned_output = false;
        long long remaining = budget_tracker_remaining_output(bt);
        snprintf(warn_buf, sizeof(warn_buf),
                 "Output token budget at %lld%% — %lld remaining",
                 (long long)((double)bt->total_output_tokens / bt->max_output_tokens * 100),
                 remaining > 0 ? remaining : 0);
        return warn_buf;
    }
    if (bt->warned_cost) {
        bt->warned_cost = false;
        double remaining = budget_tracker_remaining_cost(bt);
        snprintf(warn_buf, sizeof(warn_buf),
                 "Cost budget at %lld%% — $%.4f remaining",
                 (long long)(bt->total_cost_usd / bt->max_cost_usd * 100),
                 remaining > 0.0 ? remaining : 0.0);
        return warn_buf;
    }
    if (bt->warned_turns) {
        bt->warned_turns = false;
        int remaining = budget_tracker_remaining_turns(bt);
        snprintf(warn_buf, sizeof(warn_buf),
                 "Turn budget at %d%% — %d remaining",
                 (int)((double)bt->turn_count / bt->max_turns * 100),
                 remaining > 0 ? remaining : 0);
        return warn_buf;
    }

    return NULL;
}

long long budget_tracker_remaining_input(const budget_tracker_t *bt) {
    if (!bt || bt->max_input_tokens <= 0) return -1;
    return bt->max_input_tokens - bt->total_input_tokens;
}

long long budget_tracker_remaining_output(const budget_tracker_t *bt) {
    if (!bt || bt->max_output_tokens <= 0) return -1;
    return bt->max_output_tokens - bt->total_output_tokens;
}

double budget_tracker_remaining_cost(const budget_tracker_t *bt) {
    if (!bt || bt->max_cost_usd <= 0.0) return -1.0;
    return bt->max_cost_usd - bt->total_cost_usd;
}

int budget_tracker_remaining_turns(const budget_tracker_t *bt) {
    if (!bt || bt->max_turns <= 0) return -1;
    return bt->max_turns - bt->turn_count;
}

double budget_tracker_estimate_cost(const char *model,
                                     long long input_tokens,
                                     long long output_tokens) {
    const model_pricing_t *p = find_pricing(model);
    double input_cost = (double)input_tokens / 1000000.0 * p->input_per_1m;
    double output_cost = (double)output_tokens / 1000000.0 * p->output_per_1m;
    return input_cost + output_cost;
}

char *budget_tracker_stats_json(const budget_tracker_t *bt) {
    if (!bt) return NULL;

    json_node_t *root = json_object();

    json_set(root, "turn_count", json_number(bt->turn_count));
    json_set(root, "total_input_tokens", json_number((double)bt->total_input_tokens));
    json_set(root, "total_output_tokens", json_number((double)bt->total_output_tokens));
    json_set(root, "total_cost_usd", json_number(bt->total_cost_usd));

    json_set(root, "last_input_tokens", json_number((double)bt->last_input_tokens));
    json_set(root, "last_output_tokens", json_number((double)bt->last_output_tokens));
    json_set(root, "last_cost_usd", json_number(bt->last_cost_usd));
    if (bt->last_model[0])
        json_set(root, "last_model", json_string(bt->last_model));

    /* Limits */
    json_node_t *limits = json_object();
    if (bt->max_input_tokens > 0)
        json_set(limits, "max_input_tokens", json_number((double)bt->max_input_tokens));
    if (bt->max_output_tokens > 0)
        json_set(limits, "max_output_tokens", json_number((double)bt->max_output_tokens));
    if (bt->max_cost_usd > 0.0)
        json_set(limits, "max_cost_usd", json_number(bt->max_cost_usd));
    if (bt->max_turns > 0)
        json_set(limits, "max_turns", json_number(bt->max_turns));
    json_set(root, "limits", limits);

    /* Warnings */
    json_node_t *warn = json_object();
    json_set(warn, "warn_at_pct", json_number(bt->warn_at_pct));
    json_set(warn, "input_warned", json_bool(bt->warned_input));
    json_set(warn, "output_warned", json_bool(bt->warned_output));
    json_set(warn, "cost_warned", json_bool(bt->warned_cost));
    json_set(warn, "turns_warned", json_bool(bt->warned_turns));
    json_set(root, "warnings", warn);

    json_set(root, "exceeded", json_bool(budget_tracker_is_exceeded(bt)));

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}

void budget_tracker_reset(budget_tracker_t *bt) {
    if (!bt) return;
    long long max_in = bt->max_input_tokens;
    long long max_out = bt->max_output_tokens;
    double max_cost = bt->max_cost_usd;
    int max_turns = bt->max_turns;
    double warn_at = bt->warn_at_pct;

    memset(bt, 0, sizeof(*bt));

    bt->max_input_tokens = max_in;
    bt->max_output_tokens = max_out;
    bt->max_cost_usd = max_cost;
    bt->max_turns = max_turns;
    bt->warn_at_pct = warn_at;
}
