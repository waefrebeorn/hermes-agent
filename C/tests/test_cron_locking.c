/*
 * test_cron_locking.c — Tests for cron job locking (P171).
 *
 * Tests: acquire/release/locked cycle, stale lock removal, NULL safety,
 * set_dir isolation, shutdown flag.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libcron \
 *       tests/test_cron_locking.c src/cron/cron_locking.c \
 *       -o /tmp/t_cron_locking
 *
 * Run:
 *   /tmp/t_cron_locking
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ─── Helpers ────────────────────────────────────── */

static void setup_temp_dir(char *buf, size_t sz, int suffix) {
    snprintf(buf, sz, "/tmp/cron_lock_test_%d_%d", getpid(), suffix);
    mkdir(buf, 0755);
}

static void cleanup_temp_dir(const char *dir) {
    /* Remove any .lock files */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    system(cmd);
}

/* ─── Tests ──────────────────────────────────────── */

static void test_acquire_release_cycle(void) {
    printf("\n--- Acquire/Release Cycle ---\n");
    char tmpdir[4096];
    setup_temp_dir(tmpdir, sizeof(tmpdir), 1);

    cron_lock_set_dir(tmpdir);

    /* Acquire */
    bool ok = cron_lock_acquire("test_job");
    TEST("acquire returns true", ok);

    /* Should be locked now */
    TEST("is_locked returns true", cron_lock_is_locked("test_job"));

    /* Release */
    cron_lock_release("test_job");
    TEST("is_locked returns false after release", !cron_lock_is_locked("test_job"));

    /* Can re-acquire after release */
    ok = cron_lock_acquire("test_job");
    TEST("re-acquire returns true", ok);
    cron_lock_release("test_job");

    cleanup_temp_dir(tmpdir);
}

static void test_double_acquire_fails(void) {
    printf("\n--- Double Acquire ---\n");
    char tmpdir[4096];
    setup_temp_dir(tmpdir, sizeof(tmpdir), 0);
    cron_lock_set_dir(tmpdir);

    /* First acquire should succeed */
    bool ok = cron_lock_acquire("double_lock");
    TEST("first acquire succeeds", ok);

    /* Second acquire while still held should fail (same process) */
    ok = cron_lock_acquire("double_lock");
    /* Note: same PID, but lock file already exists. The current process
     * is alive, so kill(old_pid, 0) returns 0, making acquire return false */
    TEST("second acquire fails while held", !ok);

    cron_lock_release("double_lock");
    cleanup_temp_dir(tmpdir);
}

static void test_stale_lock_removal(void) {
    printf("\n--- Stale Lock Removal ---\n");
    char tmpdir[4096];
    setup_temp_dir(tmpdir, sizeof(tmpdir), 0);
    cron_lock_set_dir(tmpdir);

    /* Write a stale lock file with a non-existent PID */
    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/stale.lock", tmpdir);

    /* Use PID 999999 which is very unlikely to exist */
    FILE *f = fopen(lock_path, "w");
    TEST("create stale lock file", f != NULL);
    if (f) {
        fprintf(f, "999999\n");
        fclose(f);
    }

    /* Acquire should succeed (stale lock is removed) */
    bool ok = cron_lock_acquire("stale");
    TEST("acquire succeeds after stale lock", ok);

    /* Lock file should now contain our PID */
    cron_lock_release("stale");
    cleanup_temp_dir(tmpdir);
}

static void test_is_locked_with_nonexistent(void) {
    printf("\n--- is_locked with Nonexistent ---\n");
    char tmpdir[4096];
    setup_temp_dir(tmpdir, sizeof(tmpdir), 0);
    cron_lock_set_dir(tmpdir);

    /* Non-existent lock */
    bool locked = cron_lock_is_locked("nonexistent_job");
    TEST("is_locked returns false for nonexistent", !locked);

    cleanup_temp_dir(tmpdir);
}

static void test_null_safety(void) {
    printf("\n--- NULL Safety ---\n");

    bool ok = cron_lock_acquire(NULL);
    TEST("acquire NULL returns false", !ok);

    /* is_locked(NULL) should be safe and return false */
    bool locked = cron_lock_is_locked(NULL);
    TEST("is_locked NULL returns false", !locked);

    /* release(NULL) should be safe (no crash) */
    cron_lock_release(NULL);
    TEST("release NULL no crash", true);
}

static void test_shutdown_flag(void) {
    printf("\n--- Shutdown Flag ---\n");

    /* Initially false */
    bool requested = cron_shutdown_requested();
    TEST("shutdown not requested initially", !requested);
}

static void test_set_dir_isolation(void) {
    printf("\n--- Set Dir Isolation ---\n");
    char tmpdir1[4096], tmpdir2[4096];
    setup_temp_dir(tmpdir1, sizeof(tmpdir1), 5);
    setup_temp_dir(tmpdir2, sizeof(tmpdir2), 6);

    /* Locks in dir1 */
    cron_lock_set_dir(tmpdir1);
    bool ok = cron_lock_acquire("isolated_job");
    TEST("acquire in dir1 succeeds", ok);

    /* Should be locked in dir1 */
    TEST("is_locked in dir1", cron_lock_is_locked("isolated_job"));

    /* Switch to dir2 — locks should not be visible */
    cron_lock_set_dir(tmpdir2);
    TEST("is_locked in dir2 returns false (different dir)", !cron_lock_is_locked("isolated_job"));

    /* Can acquire in dir2 independently */
    ok = cron_lock_acquire("isolated_job");
    TEST("acquire in dir2 succeeds (independent)", ok);
    cron_lock_release("isolated_job");

    /* Cleanup dir1's lock */
    cron_lock_set_dir(tmpdir1);
    cron_lock_release("isolated_job");

    cleanup_temp_dir(tmpdir1);
    cleanup_temp_dir(tmpdir2);
}

static void test_release_all_locks(void) {
    printf("\n--- Release All Locks ---\n");
    char tmpdir[4096];
    setup_temp_dir(tmpdir, sizeof(tmpdir), 0);
    cron_lock_set_dir(tmpdir);

    /* Acquire multiple locks */
    bool ok1 = cron_lock_acquire("lock_a");
    TEST("acquire lock_a", ok1);

    bool ok_b = cron_lock_acquire("lock_b");
    TEST("acquire lock_b", ok_b);

    /* Verify both are locked */
    TEST("lock_a is locked", cron_lock_is_locked("lock_a"));
    TEST("lock_b is locked", cron_lock_is_locked("lock_b"));

    /* Release all */
    cron_release_all_locks();

    /* Both should be released */
    TEST("lock_a released", !cron_lock_is_locked("lock_a"));
    TEST("lock_b released", !cron_lock_is_locked("lock_b"));

    cleanup_temp_dir(tmpdir);
}

int main(void) {
    printf("=== Cron Locking Tests ===\n");

    test_acquire_release_cycle();
    test_double_acquire_fails();
    test_stale_lock_removal();
    test_is_locked_with_nonexistent();
    test_null_safety();
    test_shutdown_flag();
    test_set_dir_isolation();
    test_release_all_locks();

    printf("\n---\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
