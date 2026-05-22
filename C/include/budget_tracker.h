/*
 * budget_tracker.h — Per-session budget tracking for Hermes C (P84).
 *
 * Tracks token usage and cost per session with configurable limits.
 * Issues warnings when approaching limits and can signal the agent
 * loop to shut down gracefully.
 */

#ifndef BUDGET_TRACKER_H
#define BUDGET_TRACKER_H

#include "hermes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Budget Tracker
 * ================================================================ */

typedef struct budget_tracker_t {
    /* ---- Limits (0 = unlimited) ---- */
    long long max_input_tokens;    /* session-wide input token limit */
    long long max_output_tokens;   /* session-wide output token limit */
    double    max_cost_usd;        /* session-wide USD cost limit */
    int       max_turns;           /* session-wide turn limit */

    /* ---- Running totals ---- */
    long long total_input_tokens;
    long long total_output_tokens;
    double    total_cost_usd;
    int       turn_count;

    /* ---- Alert state ---- */
    double    warn_at_pct;         /* fraction at which to warn (default 0.8) */
    bool      warned_input;
    bool      warned_output;
    bool      warned_cost;
    bool      warned_turns;
    bool      reported_input;      /* warning already emitted for this dimension */
    bool      reported_output;
    bool      reported_cost;
    bool      reported_turns;

    /* ---- Last-turn breakdown ---- */
    long long last_input_tokens;
    long long last_output_tokens;
    double    last_cost_usd;
    char      last_model[128];     /* model used for this turn */
} budget_tracker_t;

/* ================================================================
 *  API
 * ================================================================ */

/* Create a budget tracker with defaults (no limits, warn at 80%). */
void budget_tracker_init(budget_tracker_t *bt);

/* Set limits. Pass 0 for unlimited on any dimension. */
void budget_tracker_set_limits(budget_tracker_t *bt,
                                long long max_input,
                                long long max_output,
                                double max_cost,
                                int max_turns);

/* Report a turn's token/cost usage. Call after each LLM response.
 * - input_tokens: tokens in the request (prompt)
 * - output_tokens: tokens in the response (completion)
 * - cost_usd: estimated USD cost for this turn (0.0 if unknown)
 * - model: model name used (for breakdown tracking)
 * Updates running totals and checks for warnings. */
void budget_tracker_report_turn(budget_tracker_t *bt,
                                 long long input_tokens,
                                 long long output_tokens,
                                 double cost_usd,
                                 const char *model);

/* Check if any limit has been exceeded (session should stop).
 * Returns true if ANY limit is exceeded (not just warned). */
bool budget_tracker_is_exceeded(const budget_tracker_t *bt);

/* Check if any warning should be emitted (limit approaching).
 * Returns the first warning message (static buffer), or NULL if no warning.
 * Clears the warning flag so it's only emitted once per limit. */
const char *budget_tracker_get_warning(budget_tracker_t *bt);

/* Get remaining budget. Returns remaining tokens/cost.
 * Negative = over budget. */
long long budget_tracker_remaining_input(const budget_tracker_t *bt);
long long budget_tracker_remaining_output(const budget_tracker_t *bt);
double    budget_tracker_remaining_cost(const budget_tracker_t *bt);
int       budget_tracker_remaining_turns(const budget_tracker_t *bt);

/* Compute estimated USD cost from token counts.
 * Uses a simple per-model rate lookup. Returns 0.0 if model unknown.
 * This is a rough estimate; actual cost may differ. */
double budget_tracker_estimate_cost(const char *model,
                                     long long input_tokens,
                                     long long output_tokens);

/* Get stats as JSON string (malloc'd). Caller must free(). */
char *budget_tracker_stats_json(const budget_tracker_t *bt);

/* Reset all totals and warnings (start a new session). */
void budget_tracker_reset(budget_tracker_t *bt);

#ifdef __cplusplus
}
#endif

#endif /* BUDGET_TRACKER_H */
