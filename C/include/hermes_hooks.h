/**
 * @file hermes_hooks.h
 * @brief P186: Hook registry — event-driven callback dispatch.
 *
 * Lightweight publish/subscribe for lifecycle events.
 * Shell hooks and plugin callbacks register with the same API.
 * Events are string names matching Python's VALID_HOOKS.
 *
 * Usage:
 *   // Register
 *   void my_cb(const char *event, const char *payload, void *userdata);
 *   hook_register("pre_tool_call", my_cb, NULL);
 *
 *   // Invoke all callbacks for an event
 *   hook_invoke("pre_tool_call", "{\"tool_name\":\"terminal\",...}");
 *
 *   // Cleanup
 *   hook_unregister("pre_tool_call", my_cb);
 */
#ifndef HERMES_HOOKS_H
#define HERMES_HOOKS_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types ──────────────────────────────────────────────────────── */

/** Maximum hook events tracked. */
#define HOOK_MAX_EVENTS   32

/** Maximum callbacks per event. */
#define HOOK_MAX_CBS      16

/** Maximum event name length. */
#define HOOK_EVENT_NAME_MAX 64

/**
 * Callback signature. Receives the event name and a JSON payload.
 * Returns a malloc'd JSON response string, or NULL for no response.
 * The caller frees the returned string.
 */
typedef char *(*hook_callback_t)(const char *event, const char *payload, void *userdata);

/* ── API ────────────────────────────────────────────────────────── */

/**
 * Register a callback for an event.
 * The callback will be called on every hook_invoke for this event.
 * Returns true on success, false if max callbacks reached.
 */
bool hook_register(const char *event, hook_callback_t cb, void *userdata);

/**
 * Unregister a callback for an event.
 * Both the function pointer and userdata must match.
 * Returns true if found and removed.
 */
bool hook_unregister(const char *event, hook_callback_t cb, void *userdata);

/**
 * Invoke all registered callbacks for an event.
 * Each callback receives the JSON payload string.
 * Results are COLLECTED from callbacks that return non-NULL.
 * Returns a malloc'd JSON array of results, or NULL if no results.
 * Caller must free.
 */
char *hook_invoke(const char *event, const char *payload_json);

/**
 * Return true if any callbacks are registered for event.
 */
bool hook_has_callbacks(const char *event);

/**
 * Return the number of registered events.
 */
int hook_event_count(void);

/**
 * Clear all registrations. Useful for testing or shutdown.
 */
void hook_reset_all(void);

/**
 * Convert a shell-hook callback result into a standardized
 * block/allow decision from stdout JSON.
 *
 * Accepts:
 *   {"decision":"block","reason":"..."}   (Claude-Code style)
 *   {"action":"block","message":"..."}    (Hermes canonical)
 *   {"context":"..."}                     (pre_llm_call style)
 *   Empty or non-matching → allow.
 */
typedef enum {
    HOOK_DECISION_ALLOW,
    HOOK_DECISION_BLOCK,
    HOOK_DECISION_CONTEXT,
} hook_decision_t;

typedef struct {
    hook_decision_t decision;
    char            message[512];
} hook_result_t;

/**
 * Parse a callback result string into a structured decision.
 * Returns the parsed decision; message is populated if block.
 */
hook_result_t hook_parse_result(const char *stdout_json);

#ifdef __cplusplus
}
#endif

/* ── Shell hooks integration ────────────────────────────────── */

/**
 * Parse shell hooks config from a JSON object (the "hooks:" config block).
 * Each key is an event name, value is an array of hook specs.
 * Returns number of parsed specs.
 */
int shell_hooks_parse_json(const json_t *hooks_json);

/**
 * Register all parsed shell hooks on the hook registry.
 * Must be called after shell_hooks_parse_json() and before any hook_invoke().
 * Returns number of registered hooks.
 */
int shell_hooks_register_all(void);

/**
 * Shut down shell hooks and clean up registrations.
 */
void shell_hooks_shutdown(void);

/**
 * Return count of configured shell hooks.
 */
int shell_hooks_count(void);

#endif /* HERMES_HOOKS_H */
