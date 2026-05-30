#ifndef HERMES_ONBOARDING_H
#define HERMES_ONBOARDING_H

/*
 * hermes_onboarding.h — Contextual first-touch onboarding hints.
 * Port of Python agent/onboarding.py.
 *
 * Shows one-time hints the first time the user hits a behavior fork.
 * Each hint is shown once per install (tracked in HERMES_HOME/onboarding.json)
 * and then never again.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flag names (stable — used as JSON keys) */
#define ONBOARDING_BUSY_INPUT_FLAG      "busy_input_prompt"
#define ONBOARDING_TOOL_PROGRESS_FLAG   "tool_progress_prompt"
#define ONBOARDING_OPENCLAW_RESIDUE_FLAG "openclaw_residue_cleanup"

/* === Hint text generators === */

/* Busy-input hint for gateway (markdown). mode: "queue", "steer", or "interrupt" */
const char *onboarding_busy_input_hint_gateway(const char *mode);

/* Busy-input hint for CLI (plain text). mode: "queue", "steer", or "interrupt" */
const char *onboarding_busy_input_hint_cli(const char *mode);

/* Tool-progress hint for gateway (markdown) */
const char *onboarding_tool_progress_hint_gateway(void);

/* Tool-progress hint for CLI (plain text) */
const char *onboarding_tool_progress_hint_cli(void);

/* OpenClaw residue hint for CLI (banner text) */
const char *onboarding_openclaw_residue_hint_cli(void);

/* === State management === */

/*
 * Check if a flag has been seen.
 * onboarding_path: path to HERMES_HOME/onboarding.json (or NULL to auto-detect).
 * Returns true if the flag is marked as seen.
 */
bool onboarding_is_seen(const char *onboarding_path, const char *flag);

/*
 * Mark a flag as seen.
 * onboarding_path: path to HERMES_HOME/onboarding.json (or NULL to auto-detect).
 * Performs an atomic file write (write to temp, rename).
 * Returns true on success.
 */
bool onboarding_mark_seen(const char *onboarding_path, const char *flag);

/*
 * Auto-detect the onboarding state file path.
 * Returns a malloc'd string: $HERMES_HOME/onboarding.json
 * Caller must free().
 */
char *onboarding_default_path(void);

/*
 * Check if ~/.openclaw/ directory exists (heritage OpenClaw install).
 */
bool onboarding_detect_openclaw_residue(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ONBOARDING_H */
