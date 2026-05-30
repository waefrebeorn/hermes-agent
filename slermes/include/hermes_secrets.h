/*
 * hermes_secrets.h — Secrets Manager (L01: Bitwarden Secrets Manager)
 *
 * Provides lazy-install of `bws` CLI and runtime resolution of
 * ${BSM:secret-name} references in config values.
 */
#ifndef HERMES_SECRETS_H
#define HERMES_SECRETS_H

#include <stdbool.h>
#include "hermes.h"

/* Max length of a resolved secret value */
#define HERMES_SECRET_MAX_VAL 4096

/* Max length of a BSM secret reference (including ${BSM:...} syntax) */
#define HERMES_SECRET_REF_MAX 128

/* ----------------------------------------------------------------
 *  Initialization
 * ---------------------------------------------------------------- */

/*
 * Initialize the secrets subsystem.
 * - Checks for bws CLI; lazy-installs if enabled and missing.
 * - Returns false if enabled but init fails (token missing, install fails).
 * Does nothing if !cfg->secrets.enabled.
 */
bool hermes_secrets_init(const hermes_config_t *cfg);

/* ----------------------------------------------------------------
 *  Secret resolution
 * ---------------------------------------------------------------- */

/*
 * Resolve a single ${BSM:secret-name} reference to its actual value.
 * Returns strdup'd value on success, NULL if not found or error.
 * Caller must free() the result.
 */
char *hermes_secrets_resolve(const char *ref);

/*
 * Post-process a config value string, resolving all ${BSM:...}
 * references in-place. Returns strdup'd resolved string, or NULL
 * on unresolvable reference (if strict=true).
 * Caller must free() the result.
 */
char *hermes_secrets_resolve_string(const char *input, bool strict);

/*
 * Clean up the secrets subsystem (free cached project/secret data).
 */
void hermes_secrets_cleanup(void);

#endif /* HERMES_SECRETS_H */
