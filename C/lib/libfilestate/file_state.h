/*
 * file_state.h — Cross-agent file state coordination.
 *
 * Port of tools/file_state.py. Prevents mangled edits when concurrent
 * subagents touch the same file. Thread-safe via pthreads.
 */

#ifndef HERMES_FILE_STATE_H
#define HERMES_FILE_STATE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum entries tracked */
#define FS_MAX_PATHS_PER_AGENT  256
#define FS_MAX_AGENTS           32
#define FS_MAX_GLOBAL_WRITERS   512

/* ─── Read stamp ─────────────────────────────────────────── */

typedef struct {
    double mtime;
    double read_ts;
    bool   partial;
} fs_read_stamp_t;

/* ─── Public API ─────────────────────────────────────────── */

/** Initialize the file state registry. Must be called once at startup. */
void fs_init(void);

/** fs_record_read(task_id, path, partial) — Record that an agent read a file.
 *  If mtime is 0, uses stat() to get current mtime. */
void fs_record_read(const char *task_id, const char *path, bool partial, double mtime);

/** fs_note_write(task_id, path, mtime) — Record a successful write.
 *  Updates global last-writer map AND this agent's own read stamp.
 *  If mtime is 0, uses stat() to get current mtime. */
void fs_note_write(const char *task_id, const char *path, double mtime);

/** fs_check_stale(task_id, path) — Return warning if write would be stale.
 *  Returns NULL if safe, malloc'd warning string if stale.
 *  Caller must free(). */
char *fs_check_stale(const char *task_id, const char *path);

/** fs_lock_path(path) / fs_unlock_path(path) — Per-path mutex.
 *  Same process, same filesystem — threads on the same path serialize. */
void fs_lock_path(const char *path);
void fs_unlock_path(const char *path);

/** fs_writes_since(exclude_task_id, since_ts, path_list, path_count, out) —
 *  Find paths written after since_ts by other agents. */
typedef struct {
    char writer_task_id[64];
    char paths[FS_MAX_GLOBAL_WRITERS][256];
    int  path_count;
} fs_writes_result_t;
bool fs_writes_since(const char *exclude_task_id, double since_ts,
                     const char **path_list, int path_count,
                     fs_writes_result_t *out);

/** fs_known_reads(task_id, out, out_count) — List resolved paths read. */
void fs_known_reads(const char *task_id, char out[][256], int *out_count);

/** fs_clear() — Reset all state. Intended for tests. */
void fs_clear(void);

/** fs_is_disabled() — True if HERMES_DISABLE_FILE_STATE_GUARD=1. */
bool fs_is_disabled(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FILE_STATE_H */
