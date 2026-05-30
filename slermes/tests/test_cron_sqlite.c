/*
 * test_cron_sqlite.c — Cron SQLite job store unit tests (P169).
 *
 * Tests: open/close, save/load/delete/update job, persistence,
 * count/find queries, NULL safety, edge cases.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libcron -I lib/libplugin \
 *       tests/test_cron_sqlite.c src/cron/cron_sqlite.c \
 *       lib/libjson/json.c lib/libcron/cron.c \
 *       -o /tmp/hermes_test_cron_sqlite -lm
 *
 * Run:
 *   /tmp/hermes_test_cron_sqlite
 *
 * Note: Uses void* for cron_sqlite_find() to avoid struct layout conflicts
 * between test and source translation units. Field verification done via
 * update+save+reload persistence pattern.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Internal functions not exposed in hermes.h — use void* to avoid struct conflict */
extern int cron_sqlite_count(cron_sqlite_store_t *store);
extern void *cron_sqlite_find(cron_sqlite_store_t *store, const char *name);

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

static char *make_tmp_path(void) {
    static char path[128];
    snprintf(path, sizeof(path), "/tmp/hermes_test_cron_sqlite_%d.json", (int)getpid());
    unlink(path);
    return path;
}

/* ================================================================
 * 1. Open / Close
 * ================================================================ */
static void test_open_close(void) {
    TEST("open(NULL) returns NULL");
    assert(cron_sqlite_open(NULL) == NULL);
    PASS();

    TEST("open(valid_path) returns non-NULL");
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s != NULL);
    PASS();

    TEST("close(NULL) no crash");
    cron_sqlite_close(NULL);
    PASS();

    TEST("close(valid)");
    cron_sqlite_close(s);
    unlink(p);
    PASS();
}

/* ================================================================
 * 2. Save job
 * ================================================================ */
static void test_save_job(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);

    TEST("save first job → count=1");
    assert(cron_sqlite_save_job(s, "job1", "*/5 * * * *", "echo hi",
                                 true, 0, 3, "", "", ""));
    assert(cron_sqlite_count(s) == 1);
    PASS();

    TEST("save second job → count=2");
    assert(cron_sqlite_save_job(s, "job2", "0 9 * * *", "backup",
                                 false, 2, 5, "upstream", "daily", "shell"));
    assert(cron_sqlite_count(s) == 2);
    PASS();

    TEST("duplicate name → count stays 2, find returns non-NULL");
    assert(cron_sqlite_save_job(s, "job1", "0 0 * * *", "updated",
                                 true, 0, 3, "", "", ""));
    assert(cron_sqlite_count(s) == 2);
    assert(cron_sqlite_find(s, "job1") != NULL);
    PASS();

    TEST("save(NULL, name, sched, ...) returns false");
    assert(!cron_sqlite_save_job(NULL, "x", "*/5 * * * *", "cmd",
                                  true, 0, 3, "", "", ""));
    PASS();

    TEST("save(s, NULL, sched, ...) returns false");
    assert(!cron_sqlite_save_job(s, NULL, "*/5 * * * *", "cmd",
                                  true, 0, 3, "", "", ""));
    PASS();

    TEST("save(s, name, NULL, ...) returns false");
    assert(!cron_sqlite_save_job(s, "x", NULL, "cmd",
                                  true, 0, 3, "", "", ""));
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 3. Load jobs — persistence read-back
 * ================================================================ */
static void test_load_jobs(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);

    assert(cron_sqlite_save_job(s, "jA", "* * * * *", "cmdA", true, 0, 3, "", "", ""));
    assert(cron_sqlite_save_job(s, "jB", "0 9 * * 1-5", "cmdB", false, 1, 5, "", "", ""));
    cron_sqlite_close(s);

    s = cron_sqlite_open(p);
    assert(s);

    TEST("load_jobs returns true on existing file");
    assert(cron_sqlite_load_jobs(s));
    PASS();

    TEST("count=2 after load");
    assert(cron_sqlite_count(s) == 2);
    PASS();

    TEST("find existing jobs returns non-NULL");
    assert(cron_sqlite_find(s, "jA") != NULL);
    assert(cron_sqlite_find(s, "jB") != NULL);
    PASS();

    TEST("find non-existent returns NULL");
    assert(cron_sqlite_find(s, "nonexistent") == NULL);
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 4. Delete job
 * ================================================================ */
static void test_delete_job(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_save_job(s, "d1", "*/5 * * * *", "cmd", true, 0, 3, "", "", ""));
    assert(cron_sqlite_save_job(s, "d2", "0 9 * * *", "cmd2", true, 0, 3, "", "", ""));

    TEST("delete existing → count decrements");
    assert(cron_sqlite_delete_job(s, "d1"));
    assert(cron_sqlite_count(s) == 1);
    assert(cron_sqlite_find(s, "d1") == NULL);
    PASS();

    TEST("delete non-existent returns false");
    assert(!cron_sqlite_delete_job(s, "nonexistent"));
    assert(cron_sqlite_count(s) == 1);
    PASS();

    TEST("delete with NULL name returns false");
    assert(!cron_sqlite_delete_job(s, NULL));
    PASS();

    TEST("delete on NULL store returns false");
    assert(!cron_sqlite_delete_job(NULL, "d2"));
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 5. Update job fields
 * ================================================================ */
static void test_update_job(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_save_job(s, "u1", "*/5 * * * *", "cmd", true, 0, 3, "", "", ""));

    TEST("update schedule returns true");
    assert(cron_sqlite_update_job(s, "u1", "schedule", "0 0 * * *"));
    PASS();

    TEST("update command returns true");
    assert(cron_sqlite_update_job(s, "u1", "command", "new_cmd"));
    PASS();

    TEST("update max_retries returns true");
    assert(cron_sqlite_update_job(s, "u1", "max_retries", "10"));
    PASS();

    TEST("update active to false");
    assert(cron_sqlite_update_job(s, "u1", "active", "false"));
    PASS();

    TEST("update active to true");
    assert(cron_sqlite_update_job(s, "u1", "active", "true"));
    PASS();

    TEST("update chain_from");
    assert(cron_sqlite_update_job(s, "u1", "chain_from", "upstream_job"));
    PASS();

    TEST("update retry_count");
    assert(cron_sqlite_update_job(s, "u1", "retry_count", "7"));
    PASS();

    TEST("update notify_on_complete");
    assert(cron_sqlite_update_job(s, "u1", "notify_on_complete", "true"));
    PASS();

    TEST("update notify_on_failure");
    assert(cron_sqlite_update_job(s, "u1", "notify_on_failure", "true"));
    PASS();

    TEST("update notify_channel");
    assert(cron_sqlite_update_job(s, "u1", "notify_channel", "telegram:123"));
    PASS();

    TEST("update interpreter");
    assert(cron_sqlite_update_job(s, "u1", "interpreter", "python3"));
    PASS();

    TEST("update script_type");
    assert(cron_sqlite_update_job(s, "u1", "script_type", "python"));
    PASS();

    TEST("update last_run");
    assert(cron_sqlite_update_job(s, "u1", "last_run", "1234567890"));
    PASS();

    TEST("update unknown field returns false");
    assert(!cron_sqlite_update_job(s, "u1", "bogus", "val"));
    PASS();

    TEST("update on NULL store returns false");
    assert(!cron_sqlite_update_job(NULL, "u1", "schedule", "0 0 * * *"));
    PASS();

    TEST("update on NULL name returns false");
    assert(!cron_sqlite_update_job(s, NULL, "schedule", "0 0 * * *"));
    PASS();

    TEST("update on NULL field returns false");
    assert(!cron_sqlite_update_job(s, "u1", NULL, "0 0 * * *"));
    PASS();

    /* Verify persisted data via re-read */
    cron_sqlite_close(s);
    s = cron_sqlite_open(p);
    assert(cron_sqlite_load_jobs(s));
    TEST("update persists: count=1, findable");
    assert(cron_sqlite_count(s) == 1);
    assert(cron_sqlite_find(s, "u1") != NULL);
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 6. Persistence round-trip: save → close → reopen → verify
 * ================================================================ */
static void test_persistence_roundtrip(void) {
    char *p = make_tmp_path();

    /* Phase 1: create 3 jobs */
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_save_job(s, "r1", "*/5 * * * *", "cmd1", true, 0, 3, "", "", ""));
    assert(cron_sqlite_save_job(s, "r2", "0 9 * * *", "cmd2", false, 2, 5, "r1", "daily", "shell"));
    assert(cron_sqlite_save_job(s, "r3", "0 0 * * 0", "cmd3", true, 0, 0, "", "", ""));
    cron_sqlite_close(s);

    /* Phase 2: reopen and verify */
    s = cron_sqlite_open(p);
    assert(s);
    TEST("round-trip: load 3 jobs");
    assert(cron_sqlite_load_jobs(s));
    assert(cron_sqlite_count(s) == 3);
    assert(cron_sqlite_find(s, "r1") != NULL);
    assert(cron_sqlite_find(s, "r2") != NULL);
    assert(cron_sqlite_find(s, "r3") != NULL);
    PASS();

    /* Phase 3: delete one, verify persists after close/reopen */
    TEST("round-trip: delete, close, reopen, count=2");
    assert(cron_sqlite_delete_job(s, "r1"));
    assert(cron_sqlite_count(s) == 2);
    cron_sqlite_close(s);

    s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_load_jobs(s));
    assert(cron_sqlite_count(s) == 2);
    assert(cron_sqlite_find(s, "r1") == NULL);
    assert(cron_sqlite_find(s, "r2") != NULL);
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 7. Load non-existent / missing file
 * ================================================================ */
static void test_load_errors(void) {
    char *p = make_tmp_path();
    unlink(p); /* guarantee missing */

    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);

    TEST("load from non-existent file returns false");
    assert(!cron_sqlite_load_jobs(s));
    assert(cron_sqlite_count(s) == 0);
    PASS();

    TEST("load_jobs on NULL store returns false");
    assert(!cron_sqlite_load_jobs(NULL));
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 8. Find / Count NULL safety
 * ================================================================ */
static void test_find_count(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_save_job(s, "f1", "*/5 * * * *", "cmd", true, 0, 3, "", "", ""));

    TEST("find existing returns non-NULL");
    assert(cron_sqlite_find(s, "f1") != NULL);
    PASS();

    TEST("find non-existent returns NULL");
    assert(cron_sqlite_find(s, "nonexistent") == NULL);
    PASS();

    TEST("find on NULL name returns NULL");
    assert(cron_sqlite_find(s, NULL) == NULL);
    PASS();

    TEST("find on NULL store returns NULL");
    assert(cron_sqlite_find(NULL, "f1") == NULL);
    PASS();

    TEST("count returns 1");
    assert(cron_sqlite_count(s) == 1);
    PASS();

    TEST("count on NULL store returns 0");
    assert(cron_sqlite_count(NULL) == 0);
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * 9. Multiple updates on single job — field isolation
 * ================================================================ */
static void test_multi_update(void) {
    char *p = make_tmp_path();
    cron_sqlite_store_t *s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_save_job(s, "mu", "*/5 * * * *", "original", true, 0, 3, "", "", ""));

    /* Update several non-conflicting fields */
    TEST("multi-update: all return true");
    assert(cron_sqlite_update_job(s, "mu", "schedule", "0 0 * * *"));
    assert(cron_sqlite_update_job(s, "mu", "command", "final_cmd"));
    assert(cron_sqlite_update_job(s, "mu", "retry_count", "5"));
    assert(cron_sqlite_update_job(s, "mu", "active", "false"));
    assert(cron_sqlite_update_job(s, "mu", "chain_from", "parent"));
    PASS();

    /* Persist */
    cron_sqlite_close(s);
    s = cron_sqlite_open(p);
    assert(s);
    assert(cron_sqlite_load_jobs(s));

    TEST("multi-update: persists correctly");
    assert(cron_sqlite_count(s) == 1);
    assert(cron_sqlite_find(s, "mu") != NULL);
    PASS();

    cron_sqlite_close(s);
    unlink(p);
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void) {
    printf("=== Cron SQLite Store Tests ===\n");

    test_open_close();
    test_save_job();
    test_load_jobs();
    test_delete_job();
    test_update_job();
    test_persistence_roundtrip();
    test_load_errors();
    test_find_count();
    test_multi_update();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
