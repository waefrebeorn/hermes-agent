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
 * @param out_user_id  Buffer for user_id string (CAMOFOX_USER_ID_MAX).
 * @param out_session_key  Buffer for session_key string (CAMOFOX_SESSION_KEY_MAX).
 * @return true on success, false on allocation failure.
 */
bool camofox_gen_identity(const char *hermes_home,
                           const char *task_id,
                           char out_user_id[CAMOFOX_USER_ID_MAX],
                           char out_session_key[CAMOFOX_SESSION_KEY_MAX]);

/**
 * Persist browser session state (CDP URL + identity) to disk.
 * Writes a JSON file at <state_dir>/sessions/<task_id>.json.
 *
 * @param hermes_home  Path to HERMES_HOME.
 * @param task_id      Unique task/session identifier.
 * @param cdp_url      The CDP WebSocket URL (e.g. ws://127.0.0.1:9222).
 * @return true on success, false on I/O error.
 */
bool camofox_save_session(const char *hermes_home,
                           const char *task_id,
                           const char *cdp_url);

/**
 * Load a previously saved browser session from disk.
 *
 * @param hermes_home  Path to HERMES_HOME.
 * @param task_id      Task/session identifier to look up.
 * @param out_cdp_url  Buffer for CDP URL (CAMOFOX_PATH_MAX size).
 * @return true if session found and loaded, false if not found or error.
 */
bool camofox_load_session(const char *hermes_home,
                           const char *task_id,
                           char out_cdp_url[CAMOFOX_PATH_MAX]);

/**
 * Remove a saved browser session file from disk.
 *
 * @param hermes_home  Path to HERMES_HOME.
 * @param task_id      Task/session identifier to remove.
 * @return true on success or if file didn't exist, false on I/O error.
 */
bool camofox_delete_session(const char *hermes_home,
                             const char *task_id);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CAMOFOX_STATE_H */
