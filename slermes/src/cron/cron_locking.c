/*
 * cron_locking.c — P171: Job locking for Hermes C cron.
 *
 * Prevents duplicate execution with PID files.
 * Supports graceful shutdown.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* ================================================================
 *  Lock File State
 * ================================================================ */

#define LOCK_DIR ".slermes/cron/locks"

static char g_lock_dir[4096] = {0};
static bool g_shutdown_requested = false;

/* SIGTERM/SIGINT handler for graceful shutdown */
static void shutdown_handler(int sig) {
    (void)sig;
    g_shutdown_requested = true;
}

/* Get lock directory path */
static const char *get_lock_dir(void) {
    if (g_lock_dir[0]) return g_lock_dir;

    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(g_lock_dir, sizeof(g_lock_dir), "%s/%s", home, LOCK_DIR);
    mkdir(g_lock_dir, 0755);
    return g_lock_dir;
}

/* Set custom lock directory */
void cron_lock_set_dir(const char *dir) {
    if (dir) snprintf(g_lock_dir, sizeof(g_lock_dir), "%s", dir);
}

/* ================================================================
 *  Lock API
 * ================================================================ */

/* Acquire a lock by creating a PID file.
 * Returns true if lock was acquired (or already held by this PID). */
bool cron_lock_acquire(const char *lock_name) {
    if (!lock_name) return false;

    const char *lock_dir = get_lock_dir();
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/%s.lock", lock_dir, lock_name);

    /* Check existing lock */
    FILE *f = fopen(lock_path, "r");
    if (f) {
        pid_t old_pid = 0;
        if (fscanf(f, "%d", &old_pid) == 1) {
            fclose(f);

            /* Check if the process is still alive */
            if (kill(old_pid, 0) == 0) {
                /* Process still running — lock is held */
                return false;
            }

            /* Stale lock — remove it */
            unlink(lock_path);
        } else {
            fclose(f);
        }
    } else if (errno != ENOENT) {
        return false; /* Can't check */
    }

    /* Write our PID */
    f = fopen(lock_path, "w");
    if (!f) return false;

    fprintf(f, "%d\n", getpid());
    fclose(f);

    /* Register shutdown handler on first lock */
    static bool handler_registered = false;
    if (!handler_registered) {
        signal(SIGTERM, shutdown_handler);
        handler_registered = true;
    }

    return true;
}

/* Release a lock by removing the PID file */
void cron_lock_release(const char *lock_name) {
    if (!lock_name) return;

    const char *lock_dir = get_lock_dir();
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/%s.lock", lock_dir, lock_name);

    /* Only release if we own the lock */
    FILE *f = fopen(lock_path, "r");
    if (f) {
        pid_t our_pid = getpid();
        pid_t file_pid = 0;
        if (fscanf(f, "%d", &file_pid) == 1 && file_pid == our_pid) {
            fclose(f);
            unlink(lock_path);
        } else {
            fclose(f);
        }
    }
}

/* Check if a lock is currently held */
bool cron_lock_is_locked(const char *lock_name) {
    if (!lock_name) return false;

    const char *lock_dir = get_lock_dir();
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/%s.lock", lock_dir, lock_name);

    FILE *f = fopen(lock_path, "r");
    if (!f) return false;

    pid_t pid = 0;
    bool locked = false;
    if (fscanf(f, "%d", &pid) == 1) {
        locked = (kill(pid, 0) == 0);
    }
    fclose(f);

    if (!locked) {
        /* Stale lock — clean up */
        unlink(lock_path);
    }

    return locked;
}

/* ================================================================
 *  Shutdown support
 * ================================================================ */

/* Check if shutdown has been requested */
bool cron_shutdown_requested(void) {
    return g_shutdown_requested;
}

/* Release all locks owned by this process */
void cron_release_all_locks(void) {
    const char *lock_dir = get_lock_dir();

    DIR *d = opendir(lock_dir);
    if (!d) return;

    pid_t our_pid = getpid();
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG) continue;

        /* Verify it ends with .lock */
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".lock") != 0)
            continue;

        char lock_path[4096];
        snprintf(lock_path, sizeof(lock_path), "%s/%s", lock_dir, entry->d_name);

        FILE *f = fopen(lock_path, "r");
        if (f) {
            pid_t file_pid = 0;
            if (fscanf(f, "%d", &file_pid) == 1 && file_pid == our_pid) {
                fclose(f);
                unlink(lock_path);
            } else {
                fclose(f);
            }
        }
    }
    closedir(d);
}
