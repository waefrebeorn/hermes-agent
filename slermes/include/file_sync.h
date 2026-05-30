/*
 * file_sync.h — Environment file sync for remote execution backends
 *
 * Port of Python tools/environments/file_sync.py (402 LOC).
 * Collects credential, skills, and cache files for upload to
 * remote execution environments (Modal, SSH, Daytona).
 *
 * Usage:
 *   file_sync_list_t *files = file_sync_collect("/root/.hermes");
 *   for (int i = 0; i < files->count; i++)
 *       upload(files->entries[i].host_path, files->entries[i].remote_path);
 *   file_sync_list_free(files);
 */

#ifndef FILE_SYNC_H
#define FILE_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types ──────────────────────────────────────────────────────── */

typedef struct {
    char host_path[1024];   /* Absolute path on host */
    char remote_path[1024]; /* Absolute path in container */
} file_sync_entry_t;

typedef struct {
    file_sync_entry_t *entries;
    int count;
    int capacity;
} file_sync_list_t;

/* Callback types for remote operations */
typedef bool (*file_sync_upload_fn)(const char *host_path, const char *remote_path, void *ctx);
typedef bool (*file_sync_bulk_upload_fn)(file_sync_list_t *files, void *ctx);
typedef bool (*file_sync_delete_fn)(const char *remote_path, void *ctx);

/* ── Core API ───────────────────────────────────────────────────── */

/* Enumerate all files to sync: credentials + skills + cache.
 * container_base is the remote base path (e.g. /root/.hermes).
 * Returns NULL on allocation failure. Caller must file_sync_list_free(). */
file_sync_list_t *file_sync_collect(const char *container_base);

/* Build parent directories command for remote shell execution.
 * Returns malloc'd string with "mkdir -p <dir1> <dir2> ...". */
char *file_sync_mkdir_cmd(file_sync_list_t *files);

/* Build deletion command for stale remote files.
 * files is the CURRENT state; existing_remote is a space-separated list.
 * Returns malloc'd "rm -f <stale1> <stale2> ...". */
char *file_sync_rm_stale_cmd(file_sync_list_t *files, file_sync_list_t *existing);

/* Sync all files via upload callback.
 * Calls mkdir_cmd (via mkdir_fn) then uploads each file.
 * Returns true on success, false on any failure. */
bool file_sync_upload_all(file_sync_list_t *files,
                           file_sync_upload_fn upload, void *upload_ctx);

/* Free a file_sync_list_t (including entries). */
void file_sync_list_free(file_sync_list_t *list);

/* Set HERMES_HOME path (default: ~/.hermes). Must be called before collect. */
void file_sync_set_home(const char *home);

#ifdef __cplusplus
}
#endif

#endif /* FILE_SYNC_H */
