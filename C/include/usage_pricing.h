#ifndef USAGE_PRICING_H
#define USAGE_PRICING_H

/*
 * usage_pricing.h — Token cost estimation for Hermes C.
 *
 * Per-model and per-family pricing for cost estimation.
 * Python equivalent: agent/usage_pricing.py
 *
 * MIT License — WuBu Slermes Project
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRICING_MODEL_NAME_MAX 64
#define PRICING_PROVIDER_MAX   32
#define PRICING_SOURCE_MAX     32

/* Cost status labels matching Python's CostStatus */
typedef enum {
    COST_STATUS_ACTUAL   = 0,  /* fetched from provider API */
    COST_STATUS_ESTIMATED,      /* from docs snapshot / family rates */
    COST_STATUS_INCLUDED,       /* included in subscription */
    COST_STATUS_UNKNOWN,        /* no pricing data */
} cost_status_t;

/* Cost estimate result */
typedef struct {
    double     input_cost;       /* USD */
    double     output_cost;      /* USD */
    double     cache_read_cost;  /* USD */
    double     cache_write_cost; /* USD */
    double     total_cost;       /* USD */
    cost_status_t status;
    char       source[PRICING_SOURCE_MAX];  /* e.g. "family_rates", "docs_snapshot" */
    char       label[64];        /* e.g. "gpt-4o: $10/$30 per 1M tokens" */
    bool       has_pricing;      /* true if model is known */
} usage_cost_t;

/* Token usage breakdown */
typedef struct {
    long long input_tokens;
    long long output_tokens;
    long long cache_read_tokens;
    long long cache_write_tokens;
    long long reasoning_tokens;
    int       request_count;
} usage_counts_t;

/* === Core API === */

/* Estimate cost for given token usage and model.
 * Uses per-model rates when available, falls back to family-level rates.
 * Model name can include provider prefix (e.g. "anthropic/claude-sonnet-4-6"). */
usage_cost_t usage_pricing_estimate(const char *model, const usage_counts_t *usage);

/* Check if a model has known pricing.
 * Returns true if per-model or per-family rates exist. */
bool usage_pricing_known(const char *model);

/* Format a floating-point USD cost as compact human string.
 * e.g. $0.00, $0.12, $1.50, $12.34, $123.45
 * Returns a pointer to a static buffer (NOT thread-safe). */
const char *usage_pricing_format_cost(double usd);

/* Format a duration in seconds as compact human string.
 * e.g. "0s", "30s", "2m", "1h", "3d"
 * Returns a pointer to a static buffer (NOT thread-safe). */
const char *usage_pricing_format_duration(double seconds);

#ifdef __cplusplus
}
#endif

#endif /* USAGE_PRICING_H */
