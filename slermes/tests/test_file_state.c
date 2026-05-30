/*
 * test_file_state.c — Tests for cross-agent file state coordination.
 */

#include "file_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while(0)

/* ─── Setup/teardown ─────────────────────────────────────── */

static void setup(void) {
    fs_init();
    fs_clear();
}

/* Create a temp file and return its mtime */
static double create_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "test content\n");
    fclose(f);
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (double)st.st_mtime;
}

/* ─── Tests ──────────────────────────────────────────────── */

static int test_record_read_basic(void) {
    setup();
    create_file("/tmp/fstest_read.txt");
    fs_record_read("agent_1", "/tmp/fstest_read.txt", false, 0.0);

    char paths[256][256];
    int count = 0;
    fs_known_reads("agent_1", paths, &count);
    ASSERT(count == 1, "Should have 1 read entry");
    ASSERT(strstr(paths[0], "/tmp/fstest_read.txt") != NULL, "Path should match");

    remove("/tmp/fstest_read.txt");
    return 1;
}

static int test_note_write_update_self(void) {
    setup();
    create_file("/tmp/fstest_write.txt");
    fs_note_write("agent_1", "/tmp/fstest_write.txt", 0.0);

    /* Write should also update agent's own read stamp */
    char *warn = fs_check_stale("agent_1", "/tmp/fstest_write.txt");
    ASSERT(warn == NULL, "Own write should not be stale");
    free(warn);

    remove("/tmp/fstest_write.txt");
    return 1;
}

static int test_check_stale_no_read(void) {
    setup();
    /* Never read this file, file doesn't exist */
    char *warn = fs_check_stale("agent_1", "/tmp/fstest_unknown.txt");
    ASSERT(warn == NULL, "Unknown file should not be stale");
    free(warn);
    return 1;
}

static int test_sibling_write_stale(void) {
    setup();
    /* Create the file so mtime check works */
    create_file("/tmp/fstest_sibling.txt");

    /* Agent 1 reads file with known mtime */
    struct stat st1;
    stat("/tmp/fstest_sibling.txt", &st1);
    fs_record_read("agent_1", "/tmp/fstest_sibling.txt", false, (double)st1.st_mtime);

    /* Agent 2 writes — immediately after, with same mtime */
    /* We pass a LATER timestamp to simulate the sibling having read it */
    fs_note_write("agent_2", "/tmp/fstest_sibling.txt", (double)st1.st_mtime + 1.0);

    /* Agent 1 tries to write — should warn */
    char *warn = fs_check_stale("agent_1", "/tmp/fstest_sibling.txt");
    ASSERT(warn != NULL, "Sibling write should be stale");
    ASSERT(strstr(warn, "sibling") != NULL, "Warning should mention sibling");
    free(warn);

    remove("/tmp/fstest_sibling.txt");
    return 1;
}

static int test_partial_read_warning(void) {
    setup();
    create_file("/tmp/fstest_partial.txt");

    /* Read partial */
    struct stat st;
    stat("/tmp/fstest_partial.txt", &st);
    fs_record_read("agent_1", "/tmp/fstest_partial.txt", true, (double)st.st_mtime);

    /* Write by self clears partial flag */
    fs_note_write("agent_1", "/tmp/fstest_partial.txt", (double)st.st_mtime);

    /* After write, partial is cleared */
    char *warn = fs_check_stale("agent_1", "/tmp/fstest_partial.txt");
    ASSERT(warn == NULL, "After write, partial cleared -> not stale");
    free(warn);

    remove("/tmp/fstest_partial.txt");
    return 1;
}

static int test_writes_since(void) {
    setup();
    create_file("/tmp/fstest_shared.txt");
    fs_note_write("agent_2", "/tmp/fstest_shared.txt", 3000.0);

    const char *paths[] = {"/tmp/fstest_shared.txt"};
    fs_writes_result_t result;
    bool ok = fs_writes_since("agent_1", 2000.0, paths, 1, &result);
    ASSERT(ok == true, "writes_since should succeed");
    ASSERT(result.path_count == 1, "Should find 1 write");
    ASSERT(strcmp(result.writer_task_id, "agent_2") == 0, "Writer should be agent_2");

    remove("/tmp/fstest_shared.txt");
    return 1;
}

static int test_writes_since_exclude_self(void) {
    setup();
    create_file("/tmp/fstest_own.txt");
    fs_note_write("agent_1", "/tmp/fstest_own.txt", 4000.0);

    const char *paths[] = {"/tmp/fstest_own.txt"};
    fs_writes_result_t result;
    bool ok = fs_writes_since("agent_1", 3000.0, paths, 1, &result);
    ASSERT(ok == true, "writes_since should succeed");
    ASSERT(result.path_count == 0, "Own writes should be excluded");

    remove("/tmp/fstest_own.txt");
    return 1;
}

static int test_clear(void) {
    setup();
    create_file("/tmp/fstest_clear.txt");
    fs_record_read("agent_1", "/tmp/fstest_clear.txt", false, 0.0);

    char paths[256][256];
    int count = 0;
    fs_known_reads("agent_1", paths, &count);
    ASSERT(count == 1, "Should have 1 read before clear");

    fs_clear();
    fs_known_reads("agent_1", paths, &count);
    ASSERT(count == 0, "Should have 0 reads after clear");

    remove("/tmp/fstest_clear.txt");
    return 1;
}

static int test_lock_path(void) {
    setup();
    fs_lock_path("/tmp/fstest_locked.txt");
    /* Should not deadlock on self */
    fs_unlock_path("/tmp/fstest_locked.txt");
    return 1;
}

static int test_disabled(void) {
    setup();
    ASSERT(fs_is_disabled() == false, "Should not be disabled by default");
    return 1;
}

static int test_disabled_with_env(void) {
    setup();
    setenv("HERMES_DISABLE_FILE_STATE_GUARD", "1", 1);
    ASSERT(fs_is_disabled() == true, "Should be disabled with env var");
    unsetenv("HERMES_DISABLE_FILE_STATE_GUARD");
    return 1;
}

static int test_known_reads_empty(void) {
    setup();
    char paths[256][256];
    int count = 99;
    fs_known_reads("nobody", paths, &count);
    ASSERT(count == 0, "Agent with no reads should return 0");
    return 1;
}

static int test_record_read_null_path(void) {
    setup();
    /* Should not crash on NULL path */
    fs_record_read("agent_1", NULL, false, 0.0);
    char paths[256][256];
    int count = 0;
    fs_known_reads("agent_1", paths, &count);
    ASSERT(count == 0, "NULL path should not add an entry");
    return 1;
}

int main(void) {
    printf("=== file_state tests ===\n\n");

    TEST(record_read_basic);
    TEST(note_write_update_self);
    TEST(check_stale_no_read);
    TEST(sibling_write_stale);
    TEST(partial_read_warning);
    TEST(writes_since);
    TEST(writes_since_exclude_self);
    TEST(clear);
    TEST(lock_path);
    TEST(disabled);
    TEST(disabled_with_env);
    TEST(known_reads_empty);
    TEST(record_read_null_path);

    printf("\n%s\n", tests_failed > 0 ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    printf("Results: %d/%d passed, %d failed, %d total (sum %d)\n",
           tests_passed, tests_run, tests_failed,
           tests_run, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
