#ifndef HERMES_CREDENTIAL_H
#define HERMES_CREDENTIAL_H

/*
 * credential.h — File passthrough registry for remote terminal backends.
 *
 * Port of Python tools/credential_files.py.
 *
 * Remote backends (Docker, Modal, SSH) create sandboxes with no host files.
 * This module manages a session-scoped registry of credential files, skill
 * directories, and cache directories that should be mounted into those
 * sandboxes so the agent can access them.
 *
 * Security: paths are validated against HERMES_HOME to prevent traversal
 * attacks via malicious skill declarations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ─── Limits ─────────────────────────────────────────────────────────── */

/** Maximum number of registered credential files per session */
#define CREDENTIAL_MAX_FILES 128

/** Maximum path length for internal buffers */
#define CREDENTIAL_MAX_PATH 4096

/** Maximum number of cache directory types */
#define CREDENTIAL_CACHE_DIRS 4

/** Default container mount base */
#define CREDENTIAL_DEFAULT_BASE "/root/.hermes"

/* ─── Mount entry ────────────────────────────────────────────────────── */

/** A single mount entry mapping a host path to a container path */
typedef struct {
    char host_path[CREDENTIAL_MAX_PATH];
    char container_path[CREDENTIAL_MAX_PATH];
} credential_mount_t;

/* ─── Registration (session-scoped) ─────────────────────────────────── */

/**
 * Register a single credential file for mounting into remote sandboxes.
 *
 * @param relative_path  Path relative to HERMES_HOME (e.g. "google_token.json")
 * @param container_base Base path in the container (default: "/root/.hermes")
 * @return true if the file exists and was registered, false otherwise.
 */
bool credential_register_file(const char *relative_path,
                              const char *container_base);

/**
 * Register multiple credential files from an array of path strings.
 *
 * @param paths          Array of relative path strings.
 * @param count          Number of entries in paths.
 * @param container_base Base path in the container.
 * @param missing_out    [out] Optional array to receive paths that were NOT found.
 * @param missing_cap    Capacity of missing_out.
 * @return Number of files that were NOT found (missing count).
 */
int credential_register_files(const char **paths, int count,
                              const char *container_base,
                              char **missing_out, int missing_cap);

/* ─── Query ──────────────────────────────────────────────────────────── */

/**
 * Return all registered credential file mounts.
 *
 * @param mounts [out] Array to receive mount entries.
 * @param cap     Capacity of the mounts array.
 * @return Number of mount entries written.
 */
int credential_get_mounts(credential_mount_t *mounts, int cap);

/* ─── Skills directory ───────────────────────────────────────────────── */

/**
 * Return mount entries for the local skills directory.
 *
 * If the skills directory contains symlinks, creates a sanitized copy in
 * a temp directory and returns that path instead.
 *
 * @param container_base Base path in the container (default: "/root/.hermes")
 * @param mounts         [out] Array to receive mount entries (at most 1).
 * @param cap            Capacity of the mounts array.
 * @return Number of mount entries written (0 or 1).
 */
int credential_get_skills_mount(const char *container_base,
                                credential_mount_t *mounts, int cap);

/**
 * List individual skill files for backends that upload files individually.
 *
 * @param container_base Base path in the container.
 * @param mounts         [out] Array to receive file entries.
 * @param cap            Capacity of the mounts array.
 * @return Number of entries written.
 */
int credential_iter_skills_files(const char *container_base,
                                 credential_mount_t *mounts, int cap);

/* ─── Cache directories ─────────────────────────────────────────────── */

/**
 * Return mount entries for cache directories that exist on disk.
 *
 * Maps: cache/documents, cache/images, cache/audio, cache/screenshots.
 *
 * @param container_base Base path in the container.
 * @param mounts         [out] Array to receive mount entries.
 * @param cap            Capacity of the mounts array.
 * @return Number of entries written (0 to CREDENTIAL_CACHE_DIRS).
 */
int credential_get_cache_mounts(const char *container_base,
                                credential_mount_t *mounts, int cap);

/**
 * Translate a host cache path to its mounted path inside a sandbox.
 *
 * Returns a malloc'd string with the translated path, or NULL if the
 * path is not under any auto-mounted cache directory. Caller must free().
 *
 * @param host_path      Absolute path on the host.
 * @param container_base Base path in the container.
 * @return Malloc'd translated path, or NULL if not a cache path.
 */
char *credential_to_visible_cache_path(const char *host_path,
                                       const char *container_base);
/**
 * List individual cache files for backends that upload files individually.
 *
 * @param container_base Base path in the container.
 * @param mounts         [out] Array to receive file entries.
 * @param cap            Capacity of the mounts array.
 * @return Number of entries written.
 */
int credential_iter_cache_files(const char *container_base,
                                credential_mount_t *mounts, int cap);

/* ─── Lifecycle ──────────────────────────────────────────────────────── */

/** Reset the session-scoped registry (e.g. on session reset). */
void credential_clear(void);

/** Free resources (temp dirs, etc.). Call once at shutdown. */
void credential_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CREDENTIAL_H */
