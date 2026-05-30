#ifndef HERMES_ENV_PASSTHROUGH_H
#define HERMES_ENV_PASSTHROUGH_H

/*
 * env_passthrough.h — Environment variable passthrough registry.
 *
 * Port of Python tools/env_passthrough.py.
 *
 * Manages a session-scoped allowlist of environment variables that should
 * pass through to sandboxed execution environments (execute_code, terminal).
 * By default, Hermes provider credentials are blocked to prevent credential
 * exfiltration (GHSA-rhgp-j443-p4rf).
 *
 * Two sources feed the allowlist:
 *   1. Skill declarations — registered via env_passthrough_register()
 *   2. (Future) User config — terminal.env_passthrough in config.yaml
 *
 * Thread-safe: all operations protected by a mutex.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Maximum number of registered passthrough variables */
#define ENV_PASSTHROUGH_MAX_VARS 64

/** Maximum length of an environment variable name */
#define ENV_PASSTHROUGH_MAX_NAME 128

/**
 * Check whether an env var name is a Hermes-managed provider credential
 * that must never be passed through to sandboxed execution environments.
 * @param name  Environment variable name (case-sensitive).
 * @return true if the name is in the blocklist.
 */
bool env_passthrough_is_blocked(const char *name);

/**
 * Register an environment variable as allowed in sandboxed environments.
 * Returns false if the variable is blocked (Hermes provider credential).
 * @param name  Environment variable name.
 * @return true if registered, false if blocked or at capacity.
 */
bool env_passthrough_register(const char *name);

/**
 * Register multiple environment variables at once.
 * Blocked variables are silently skipped.
 * @param names   Array of variable name strings.
 * @param count   Number of names in the array.
 * @return Number of variables successfully registered (non-blocked).
 */
int env_passthrough_register_batch(const char **names, int count);

/**
 * Check whether an environment variable is allowed to pass through.
 * Returns true if the variable was registered via env_passthrough_register().
 * @param name  Environment variable name.
 * @return true if allowed.
 */
bool env_passthrough_is_allowed(const char *name);

/**
 * Get the list of all registered passthrough variables.
 * Caller must free with env_passthrough_free_list().
 * @param out        [out] Pointer to receive allocated array.
 * @param out_count  [out] Number of entries in the array.
 */
void env_passthrough_get_all(char ***out, int *out_count);

/**
 * Reset the session-scoped allowlist.
 */
void env_passthrough_clear(void);

/**
 * Free a list returned by env_passthrough_get_all().
 * @param list   Array of strings.
 * @param count  Number of entries.
 */
void env_passthrough_free_list(char **list, int count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ENV_PASSTHROUGH_H */
