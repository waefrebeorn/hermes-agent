#ifndef HERMES_CAMOFOX_STATE_H
#define HERMES_CAMOFOX_STATE_H

/*
 * camofox_state.h — Hermes-managed Camofox browser state helpers.
 *
 * Port of Python tools/browser_camofox_state.py.
 *
 * Provides profile-scoped identity and state directory paths for Camofox
 * persistent browser profiles. Uses UUID5 (SHA-1) name-based digests for
 * deterministic identity generation.
 *
 * The user identity is profile-scoped (same HERMES_HOME = same userId).
 * The session key is scoped to the logical browser task so newly created
 * tabs within the same profile reuse the same identity contract.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Subdirectory name for Camofox state */
#define CAMOFOX_STATE_DIR_NAME "browser_auth"
#define CAMOFOX_STATE_SUBDIR   "camofox"

/** Buffer sizes for identity output strings */
#define CAMOFOX_USER_ID_MAX    32   /* "hermes_" + 10 hex chars + null */
#define CAMOFOX_SESSION_KEY_MAX 64  /* "task_" + 16 hex chars + null */
#define CAMOFOX_PATH_MAX       1024

/**
 * Build the Camofox state directory path.
 * Returns out_path (for convenience) or NULL if hermes_home is NULL.
 * Result: <hermes_home>/browser_auth/camofox
 */
const char *camofox_state_dir(const char *hermes_home, char out_path[CAMOFOX_PATH_MAX]);

/**
 * Generate a stable Camofox user identity for the given Hermes home.
 *
 * The user_id is deterministic: UUID5(camofox-user:<scope_root>), first 10 hex chars.
 * The session_key is deterministic: UUID5(camofox-session:<scope_root>:<task_id>), first 16 hex chars.
 *
 * @param hermes_home  Path to HERMES_HOME (for scope root).
 * @param task_id      Optional task/session ID. NULL treated as "default".
 * @param out_user_id  Buffer for user_id string (C resolve (): filled with hex digest.
 * @param out_session_key  Buffer for session_key string (CAMOFOX_SESSION_KEY_MAX).
 * @return true on success, false on allocation failure.
 */
bool camofox_gen_identity(const char *hermes_home,
                           const char *task_id,
                           char out_user_id[CAMOFOX_USER_ID_MAX],
                           char out_session_key[CAMOFOX_SESSION_KEY_MAX]);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CAMOFOX_STATE_H */
