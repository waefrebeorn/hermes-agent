#ifndef HERMES_FILE_SAFETY_H
#define HERMES_FILE_SAFETY_H

/*
 * file_safety.h — File path safety checks for Hermes C.
 * Mirrors Python agent/file_safety.py: denied write paths,
 * safe write root, read-block for credential files.
 *
 * Security sector (P1): prevents prompt-injected file writes
 * from overwriting SSH keys, shell config, .env, etc.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Check if a path is blocked by the write denylist.
 *
 * Checks against exact denied paths (SSH keys, .env, shell configs,
 * /etc/sudoers, /etc/passwd) and denied directory prefixes (.ssh/,
 * .aws/, .gnupg/, /etc/sudoers.d/, etc.), plus Hermes control files
 * (auth.json, config.yaml, webhook_subscriptions.json, mcp-tokens/).
 *
 * If HERMES_WRITE_SAFE_ROOT is set, all paths outside that root are
 * also denied.
 *
 * @param path  Absolute or relative path to check (will be resolved).
 * @return true if the path must not be written.
 */
bool file_is_write_denied(const char *path);

/**
 * Return an error message string when a read targets a denied Hermes path.
 *
 * Blocks reads of internal cache files (skills/.hub/), credential stores
 * (auth.json, .env, .anthropic_oauth.json, webhook_subscriptions.json),
 * and mcp-tokens/.
 *
 * @param path  Absolute or relative path to check.
 * @return Newly allocated error string if blocked, NULL if allowed.
 *         Caller must free().
 */
char *file_get_read_block_error(const char *path);

/**
 * Set the hermes home and root paths for testing (avoids env dep).
 */
void file_safety_set_test_paths(const char *hermes_home, const char *hermes_root);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FILE_SAFETY_H */
