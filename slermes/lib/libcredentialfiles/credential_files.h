#ifndef HERMES_CREDENTIAL_FILES_H
#define HERMES_CREDENTIAL_FILES_H

/*
 * credential_files.h — File passthrough registry for remote terminal backends.
 * Port of Python tools/credential_files.py.
 *
 * Remote backends (Docker, Modal, SSH) create sandboxes with no host files.
 * This module ensures that credential files, skill directories, and host-side
 * cache directories are mounted or synced into those sandboxes.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ── */

#define CREDFILES_MAX_PATH      4096
#define CREDFILES_MAX_PAIRS     256   /* max registered credential file pairs */
#define CREDFILES_MAX_ENTRIES   128   /* max config-based entries */
#define CREDFILES_MAX_CACHE_DIRS 16   /* max cache directory mounts */

/* Cache subdirectory names matching Python's _CACHE_DIRS */
#define CREDFILES_CACHE_DOCUMENTS     "cache/documents"
#define CREDFILES_CACHE_IMAGES        "cache/images"
#define CREDFILES_CACHE_AUDIO         "cache/audio"
#define CREDFILES_CACHE_SCREENSHOTS   "cache/screenshots"

/* ── Types ── */

/** A host→container path pair */
typedef struct {
    char host_path[CREDFILES_MAX_PATH];
    char container_path[CREDFILES_MAX_PATH];
} credfiles_path_pair_t;

/** Registered credential files list */
typedef struct {
    credfiles_path_pair_t pairs[CREDFILES_MAX_PAIRS];
    int count;
} credfiles_registry_t;

/** Result for functions that enumerate path pairs */
typedef struct {
    credfiles_path_pair_t entries[CREDFILES_MAX_PAIRS];
    int count;
} credfiles_path_list_t;

/* ── Registry API ── */

/** Get the per-session credential file registry singleton */
credfiles_registry_t *credfiles_get_registry(void);

/**
 * Register a credential file for mounting into remote sandboxes.
 * @param relative_path  Path relative to HERMES_HOME (e.g. "google_token.json")
 * @param container_base Container base path (default: "/root/.hermes")
 * @return true if file exists and was registered
 */
bool credfiles_register_file(const char *relative_path,
                              const char *container_base);

/**
 * Register multiple credential files from a list of entries.
 * Each entry is either a path string or a "key=value" format.
 * @param entries      Array of entry strings
 * @param entry_count  Number of entries
 * @param container_base Container base path
 * @param missing      Output buffer for paths that were not found (optional)
 * @param missing_size Size of missing output buffer
 * @return Number of missing files written to missing buffer
 */
int credfiles_register_files(const char *entries[], int entry_count,
                              const char *container_base,
                              char missing[][CREDFILES_MAX_PATH],
                              int missing_size);

/** Reset the skill-scoped registry (e.g. on session reset) */
void credfiles_clear(void);

/* ── Mount Queries ── */

/**
 * Return all credential files that should be mounted into remote sandboxes.
 * Combines skill-registered files with available config-based entries.
 */
credfiles_path_list_t credfiles_get_mounts(void);

/**
 * Load credential file paths from config.yaml's terminal.credential_files.
 * Returns entries that exist on disk.
 */
credfiles_path_list_t credfiles_load_config_mounts(void);

/* ── Skills Directory Mounts ── */

/**
 * Return mount info for the local skills directory.
 * Returns empty list if skills dir doesn't exist.
 * NOTE: For simplicity, this returns the raw skills dir path.
 * Symlink-safe copy is not implemented at this level.
 */
credfiles_path_list_t credfiles_get_skills_mount(const char *container_base);

/* ── Cache Directory Mounts ── */

/**
 * Return mount entries for each cache directory that exists on disk.
 * Cache dirs: cache/documents, cache/images, cache/audio, cache/screenshots.
 */
credfiles_path_list_t credfiles_get_cache_mounts(const char *container_base);

/* ── Utility ── */

/**
 * Resolve HERMES_HOME path.
 * Returns the value of HERMES_HOME env var, or ~/.hermes as fallback.
 */
const char *credfiles_get_hermes_home(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CREDENTIAL_FILES_H */
