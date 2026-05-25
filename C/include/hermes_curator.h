/**
 * @file hermes_curator.h
 * @brief B05: Curator — background skill maintenance orchestrator.
 *
 * Manages skill lifecycle: tracks state (last_run_at, run_count, paused),
 * provides status for the /curator CLI command.
 */
#ifndef HERMES_CURATOR_H
#define HERMES_CURATOR_H

#include "hermes.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum summary length */
#define CURATOR_SUMMARY_MAX 4096

/** Curator state persisted to .curator_state JSON file */
typedef struct {
    bool    enabled;           /* Is curator enabled in config */
    bool    paused;            /* User-paused state */
    int     run_count;         /* Total runs since state reset */
    time_t  last_run_at;       /* Unix timestamp of last run */
    double  last_run_duration; /* Seconds the last run took */
    char    last_run_summary[CURATOR_SUMMARY_MAX]; /* Text summary */
} curator_state_t;

/**
 * Load curator state from the state file.
 * Returns false if no state file exists (first run).
 */
bool curator_load_state(curator_state_t *state);

/**
 * Save curator state to the state file.
 */
void curator_save_state(const curator_state_t *state);

/**
 * Initialize curator state with defaults.
 */
void curator_init_state(curator_state_t *state);

/**
 * Update state after a run completes.
 * Sets last_run_at, increments run_count, stores summary.
 */
void curator_record_run(curator_state_t *state, double duration_secs,
                         const char *summary);

/**
 * Format human-readable duration string from seconds.
 * Fills buf with "Xm Ys", "Xh Ym", "Xd Xh", or "Xs".
 */
void curator_format_duration(double seconds, char *buf, size_t sz);

/**
 * Format a time_t as a relative time string ("5m ago", "2h ago", etc.).
 */
void curator_format_reltime(time_t t, char *buf, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CURATOR_H */
