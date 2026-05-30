#ifndef HERMES_SLASH_CONFIRM_H
#define HERMES_SLASH_CONFIRM_H

/*
 * slash_confirm.h — Generic slash-command confirmation primitive (gateway-side).
 * Port of Python tools/slash_confirm.py.
 *
 * Slash commands that have a non-destructive but expensive side effect
 * (e.g. /reload-mcp) route through this module for user confirmation.
 *
 * State is stored module-level so platform adapters can resolve callbacks
 * without needing a backreference to the gateway instance.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ── */

#define SLASHCONFIRM_MAX_SESSIONS  64
#define SLASHCONFIRM_MAX_KEY       128
#define SLASHCONFIRM_MAX_CONFIRM_ID 64
#define SLASHCONFIRM_MAX_COMMAND   64

/** Default timeout — pending confirm older than this is stale (300s = 5 min) */
#define SLASHCONFIRM_DEFAULT_TIMEOUT_SECONDS 300.0

/* ── Types ── */

/** Handler function type. Called with choice ("once", "always", "cancel").
 * Returns an allocated result string (caller frees) or NULL. */
typedef char *(*slashconfirm_handler_t)(const char *choice);

/** A pending confirmation entry */
typedef struct {
    char session_key[SLASHCONFIRM_MAX_KEY];
    char confirm_id[SLASHCONFIRM_MAX_CONFIRM_ID];
    char command[SLASHCONFIRM_MAX_COMMAND];
    slashconfirm_handler_t handler;
    double created_at;   /* time(NULL) seconds */
} slashconfirm_entry_t;

/* ── API ── */

/**
 * Register a pending slash-command confirmation.
 * Overwrites any prior pending confirm for the same session_key.
 */
void slashconfirm_register(const char *session_key,
                           const char *confirm_id,
                           const char *command,
                           slashconfirm_handler_t handler);

/**
 * Get a copy of the pending confirm for a session, or NULL if none.
 * Caller must free the returned pointer with slashconfirm_free_entry().
 */
slashconfirm_entry_t *slashconfirm_get_pending(const char *session_key);

/** Free a pending entry returned by slashconfirm_get_pending() */
void slashconfirm_free_entry(slashconfirm_entry_t *entry);

/** Drop the pending confirm for session_key without running it. */
void slashconfirm_clear(const char *session_key);

/**
 * Drop the pending confirm if older than timeout seconds.
 * Returns true if an entry was dropped.
 */
bool slashconfirm_clear_if_stale(const char *session_key,
                                  double timeout_seconds);

/**
 * Resolve a pending confirm.
 * choice must be "once", "always", or "cancel".
 * Returns the handler's output string (caller must free), or NULL if the
 * confirm was stale, already resolved, or confirm_id doesn't match.
 */
char *slashconfirm_resolve(const char *session_key,
                           const char *confirm_id,
                           const char *choice,
                           double timeout_seconds);

/** Remove all pending confirmations (e.g. on shutdown). */
void slashconfirm_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SLASH_CONFIRM_H */
