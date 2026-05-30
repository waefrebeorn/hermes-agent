/*
 * file_state.c — Cross-agent file state coordination implementation.
 *
 * Thread-safe registry tracking per-agent read stamps and global writers.
 * Prevents concurrent subagents from overwriting each other's changes.
 */

#include "file_state.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ─── Internal state ─────────────────────────────────────── */

/* Per-agent read stamps: reads[agent_idx][path_idx] */
static struct {
    char            task_id[64];
    int             count;
    char            paths[FS_MAX_PATHS_PER_AGENT][256];
    fs_read_stamp_t stamps[FS_MAX_PATHS_PER_AGENT];
} g_reads[FS_MAX_AGENTS];
static int g_agent_count = 0;

/* Global last-writer map: by path index */
static struct {
    int     count;
    char    paths[FS_MAX_GLOBAL_WRITERS][256];
    char    task_ids[FS_MAX_GLOBAL_WRITERS][64];
    double  timestamps[FS_MAX_GLOBAL_WRITERS];
} g_writers;

/* Per-path locks */
static struct {
    int             count;
    char            paths[FS_MAX_GLOBAL_WRITERS][256];
    pthread_mutex_t mutexes[FS_MAX_GLOBAL_WRITERS];
} g_path_locks;

static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_meta_lock  = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;

/* ─── Helpers ────────────────────────────────────────────── */

static int find_agent(const char *task_id) {
    for (int i = 0; i < g_agent_count; i++)
        if (strcmp(g_reads[i].task_id, task_id) == 0)
            return i;
    return -1;
}

static int add_agent(const char *task_id) {
    if (g_agent_count >= FS_MAX_AGENTS) return -1;
    int idx = g_agent_count++;
    strncpy(g_reads[idx].task_id, task_id, sizeof(g_reads[idx].task_id) - 1);
    g_reads[idx].task_id[sizeof(g_reads[idx].task_id) - 1] = '\0';
    g_reads[idx].count = 0;
    return idx;
}

static int find_agent_path(int agent_idx, const char *path) {
    for (int i = 0; i < g_reads[agent_idx].count; i++)
        if (strcmp(g_reads[agent_idx].paths[i], path) == 0)
            return i;
    return -1;
}

static int find_writer(const char *path) {
    for (int i = 0; i < g_writers.count; i++)
        if (strcmp(g_writers.paths[i], path) == 0)
            return i;
    return -1;
}

static int find_path_lock(const char *path) {
    for (int i = 0; i < g_path_locks.count; i++)
        if (strcmp(g_path_locks.paths[i], path) == 0)
            return i;
    return -1;
}

static bool get_mtime(const char *path, double *out) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    *out = (double)st.st_mtime;
    return true;
}

static double now_ts(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* When an agent path array fills up, the NEXT add overwrites index 0
   (oldest). No shift needed — linear scan means overwrite is fine.
   This function exists for the principle of bounded growth but is a no-op
   when count naturally stays <= MAX. */
static void cap_agent_paths(int agent_idx) {
    (void)agent_idx;
}

/* Same no-op logic as cap_agent_paths — writers use ring-buffer overwrite. */
static void cap_writers(void) {
}

/* ─── Initialization ─────────────────────────────────────── */

void fs_init(void) {
    if (g_initialized) return;
    g_initialized = true;
    g_agent_count = 0;
    g_writers.count = 0;
    g_path_locks.count = 0;
}

/* ─── Public API ─────────────────────────────────────────── */

void fs_record_read(const char *task_id, const char *path, bool partial, double mtime) {
    if (fs_is_disabled() || !task_id || !path || !*path) return;

    if (mtime == 0.0) {
        if (!get_mtime(path, &mtime)) return;
    }
    double ts = now_ts();

    pthread_mutex_lock(&g_state_lock);

    int aidx = find_agent(task_id);
    if (aidx < 0) aidx = add_agent(task_id);
    if (aidx < 0) { pthread_mutex_unlock(&g_state_lock); return; }

    int pidx = find_agent_path(aidx, path);
    if (pidx < 0) {
        pidx = g_reads[aidx].count;
        if (pidx >= FS_MAX_PATHS_PER_AGENT) {
            /* Replace oldest */
            pidx = 0;
        }
        strncpy(g_reads[aidx].paths[pidx], path, sizeof(g_reads[aidx].paths[pidx]) - 1);
        g_reads[aidx].paths[pidx][sizeof(g_reads[aidx].paths[pidx]) - 1] = '\0';
        if (pidx == g_reads[aidx].count && g_reads[aidx].count < FS_MAX_PATHS_PER_AGENT)
            g_reads[aidx].count++;
    }

    g_reads[aidx].stamps[pidx].mtime   = mtime;
    g_reads[aidx].stamps[pidx].read_ts = ts;
    g_reads[aidx].stamps[pidx].partial = partial;

    cap_agent_paths(aidx);
    pthread_mutex_unlock(&g_state_lock);
}

void fs_note_write(const char *task_id, const char *path, double mtime) {
    if (fs_is_disabled() || !task_id || !path || !*path) return;

    if (mtime == 0.0) {
        if (!get_mtime(path, &mtime)) return;
    }
    double ts = now_ts();

    pthread_mutex_lock(&g_state_lock);

    /* Update global last-writer */
    int widx = find_writer(path);
    if (widx < 0) {
        widx = g_writers.count;
        if (widx >= FS_MAX_GLOBAL_WRITERS) widx = 0; /* overwrite oldest */
        strncpy(g_writers.paths[widx], path, sizeof(g_writers.paths[widx]) - 1);
        g_writers.paths[widx][sizeof(g_writers.paths[widx]) - 1] = '\0';
        if (widx == g_writers.count && g_writers.count < FS_MAX_GLOBAL_WRITERS)
            g_writers.count++;
    }
    strncpy(g_writers.task_ids[widx], task_id, sizeof(g_writers.task_ids[widx]) - 1);
    g_writers.task_ids[widx][sizeof(g_writers.task_ids[widx]) - 1] = '\0';
    g_writers.timestamps[widx] = ts;

    /* Writer's own read stamp is now up-to-date */
    int aidx = find_agent(task_id);
    if (aidx < 0) aidx = add_agent(task_id);
    if (aidx >= 0) {
        int pidx = find_agent_path(aidx, path);
        if (pidx < 0) {
            pidx = g_reads[aidx].count;
            if (pidx >= FS_MAX_PATHS_PER_AGENT) pidx = 0;
            strncpy(g_reads[aidx].paths[pidx], path, sizeof(g_reads[aidx].paths[pidx]) - 1);
            g_reads[aidx].paths[pidx][sizeof(g_reads[aidx].paths[pidx]) - 1] = '\0';
            if (pidx == g_reads[aidx].count && g_reads[aidx].count < FS_MAX_PATHS_PER_AGENT)
                g_reads[aidx].count++;
        }
        g_reads[aidx].stamps[pidx].mtime   = mtime;
        g_reads[aidx].stamps[pidx].read_ts = ts;
        g_reads[aidx].stamps[pidx].partial = false;
    }

    cap_writers();
    if (aidx >= 0) cap_agent_paths(aidx);
    pthread_mutex_unlock(&g_state_lock);
}

char *fs_check_stale(const char *task_id, const char *path) {
    if (fs_is_disabled() || !task_id || !path || !*path) return NULL;

    pthread_mutex_lock(&g_state_lock);

    int aidx = find_agent(task_id);
    int pidx = (aidx >= 0) ? find_agent_path(aidx, path) : -1;
    int widx = find_writer(path);

    /* Case: never read AND no writer record — net-new file */
    if (pidx < 0 && widx < 0) {
        pthread_mutex_unlock(&g_state_lock);
        return NULL;
    }

    pthread_mutex_unlock(&g_state_lock);

    /* Get current mtime */
    double current_mtime;
    if (!get_mtime(path, &current_mtime)) {
        /* File doesn't exist — write will create it */
        return NULL;
    }

    pthread_mutex_lock(&g_state_lock);

    /* Re-read after stat (state may have changed) */
    aidx = find_agent(task_id);
    pidx = (aidx >= 0) ? find_agent_path(aidx, path) : -1;
    widx = find_writer(path);

    char *result = NULL;

    /* Case 1: sibling subagent wrote after our last read */
    if (widx >= 0 && pidx >= 0) {
        const char *writer_tid = g_writers.task_ids[widx];
        double writer_ts = g_writers.timestamps[widx];
        if (strcmp(writer_tid, task_id) != 0) {
            double read_ts = g_reads[aidx].stamps[pidx].read_ts;
            if (writer_ts > read_ts) {
                char buf[512];
                snprintf(buf, sizeof(buf),
                    "%s was modified by sibling subagent %s — after "
                    "this agent's last read. Re-read before writing.",
                    path, writer_tid);
                result = strdup(buf);
                pthread_mutex_unlock(&g_state_lock);
                return result;
            }
        }
    }

    /* Case 1b: never read but writer exists */
    if (widx >= 0 && pidx < 0) {
        const char *writer_tid = g_writers.task_ids[widx];
        if (strcmp(writer_tid, task_id) != 0) {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "%s was modified by sibling subagent %s but this agent "
                "never read it. Read before writing to avoid overwriting.",
                path, writer_tid);
            result = strdup(buf);
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
    }

    /* Case 2: external/unknown modification (mtime drifted) */
    if (pidx >= 0) {
        double read_mtime = g_reads[aidx].stamps[pidx].mtime;
        bool partial = g_reads[aidx].stamps[pidx].partial;
        if (current_mtime != read_mtime) {
            result = strdup(
                "File was modified on disk since you last read it. "
                "Re-read before writing.");
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
        if (partial) {
            result = strdup(
                "File was last read with offset/limit pagination "
                "(partial view). Re-read the whole file before overwriting.");
            pthread_mutex_unlock(&g_state_lock);
            return result;
        }
    }

    /* Case 3: never read */
    if (pidx < 0) {
        result = strdup(
            "File was not read by this agent. "
            "Read the file first to write an informed edit.");
        pthread_mutex_unlock(&g_state_lock);
        return result;
    }

    pthread_mutex_unlock(&g_state_lock);
    return NULL;  /* safe */
}

void fs_lock_path(const char *path) {
    if (!path || !*path) return;

    pthread_mutex_lock(&g_meta_lock);

    int idx = find_path_lock(path);
    if (idx < 0) {
        idx = g_path_locks.count;
        if (idx >= FS_MAX_GLOBAL_WRITERS) idx = 0; /* should not happen */
        strncpy(g_path_locks.paths[idx], path, sizeof(g_path_locks.paths[idx]) - 1);
        g_path_locks.paths[idx][sizeof(g_path_locks.paths[idx]) - 1] = '\0';
        pthread_mutex_init(&g_path_locks.mutexes[idx], NULL);
        if (idx == g_path_locks.count && g_path_locks.count < FS_MAX_GLOBAL_WRITERS)
            g_path_locks.count++;
    }

    pthread_mutex_t *lock = &g_path_locks.mutexes[idx];
    pthread_mutex_unlock(&g_meta_lock);

    pthread_mutex_lock(lock);
}

void fs_unlock_path(const char *path) {
    if (!path || !*path) return;

    pthread_mutex_lock(&g_meta_lock);
    int idx = find_path_lock(path);
    if (idx >= 0) {
        pthread_mutex_unlock(&g_path_locks.mutexes[idx]);
    }
    pthread_mutex_unlock(&g_meta_lock);
}

bool fs_writes_since(const char *exclude_task_id, double since_ts,
                     const char **path_list, int path_count,
                     fs_writes_result_t *out) {
    if (fs_is_disabled() || !out) return false;
    memset(out, 0, sizeof(*out));
    out->path_count = 0;

    pthread_mutex_lock(&g_state_lock);

    for (int w = 0; w < g_writers.count; w++) {
        if (strcmp(g_writers.task_ids[w], exclude_task_id) == 0)
            continue;
        if (g_writers.timestamps[w] < since_ts)
            continue;

        /* Check if path is in the caller's path list */
        bool found = false;
        for (int p = 0; p < path_count; p++) {
            if (path_list[p] && strcmp(g_writers.paths[w], path_list[p]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) continue;

        /* Add to result */
        if (out->path_count >= FS_MAX_GLOBAL_WRITERS) break;
        strncpy(out->writer_task_id, g_writers.task_ids[w],
                sizeof(out->writer_task_id) - 1);
        strncpy(out->paths[out->path_count], g_writers.paths[w],
                sizeof(out->paths[out->path_count]) - 1);
        out->path_count++;
    }

    pthread_mutex_unlock(&g_state_lock);
    return true;
}

void fs_known_reads(const char *task_id, char out[][256], int *out_count) {
    if (fs_is_disabled() || !out || !out_count) return;
    *out_count = 0;

    pthread_mutex_lock(&g_state_lock);

    int aidx = find_agent(task_id);
    if (aidx >= 0) {
        for (int i = 0; i < g_reads[aidx].count; i++) {
            strncpy(out[i], g_reads[aidx].paths[i], 255);
            out[i][255] = '\0';
            (*out_count)++;
        }
    }

    pthread_mutex_unlock(&g_state_lock);
}

void fs_clear(void) {
    pthread_mutex_lock(&g_state_lock);
    g_agent_count = 0;
    g_writers.count = 0;
    pthread_mutex_unlock(&g_state_lock);

    pthread_mutex_lock(&g_meta_lock);
    for (int i = 0; i < g_path_locks.count; i++)
        pthread_mutex_destroy(&g_path_locks.mutexes[i]);
    g_path_locks.count = 0;
    pthread_mutex_unlock(&g_meta_lock);
}

bool fs_is_disabled(void) {
    const char *val = getenv("HERMES_DISABLE_FILE_STATE_GUARD");
    return (val && strcmp(val, "1") == 0);
}
