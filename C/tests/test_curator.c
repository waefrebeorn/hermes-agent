/*
 * test_curator.c — Curator state management tests.
 *
 * Tests curator_init_state, curator_record_run, curator_format_duration,
 * curator_format_reltime, and file-based save/load round-trips.
 *
 * Build (from C/):
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson \
 *       tests/test_curator.c src/agent/curator.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_curator -lm
 *
 * Run:
 *   /tmp/hermes_test_curator
 */

#include "hermes_curator.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* Create a temp dir and set HERMES_HOME to it */
static int setup_temp_home(char *tmpdir, size_t sz) {
    char tmpl[] = "/tmp/hermes_curator_test_XXXXXX";
    char *d = mkdtemp(tmpl);
    if (!d) return -1;
    snprintf(tmpdir, sz, "%s", d);
    /* Create the skills/ subdir */
    char skills_dir[1024];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", tmpdir);
    mkdir(skills_dir, 0755);
    setenv("HERMES_HOME", tmpdir, 1);
    return 0;
}

static void cleanup_temp_home(const char *tmpdir) {
    if (!tmpdir || !tmpdir[0]) return;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s 2>/dev/null", tmpdir);
    int rc = system(cmd);
    (void)rc;
}

int main(void)
{
    char tmpdir[1024] = "";
    char saved_env[1024] = "";
    const char *orig_home = getenv("HERMES_HOME");
    if (orig_home) snprintf(saved_env, sizeof(saved_env), "%s", orig_home);

    /* ================================================================== */
    /* 1. curator_init_state                                               */
    /* ================================================================== */

    curator_state_t state;
    memset(&state, 0xFF, sizeof(state));  /* fill with garbage */
    curator_init_state(&state);

    TEST("init: enabled", state.enabled == true);
    TEST("init: not paused", state.paused == false);
    TEST("init: run_count 0", state.run_count == 0);
    TEST("init: last_run_at 0", state.last_run_at == 0);
    TEST("init: duration 0", state.last_run_duration == 0.0);
    TEST("init: summary empty", state.last_run_summary[0] == '\0');

    /* NULL input should not crash */
    curator_init_state(NULL);
    TEST("init: NULL no-op", 1);

    /* ================================================================== */
    /* 2. curator_record_run — in-memory state update                      */
    /* ================================================================== */

    curator_state_t rs;
    curator_init_state(&rs);
    /* Need a temp home so save_state doesn't segfault */
    if (setup_temp_home(tmpdir, sizeof(tmpdir)) != 0) {
        fprintf(stderr, "FAIL: could not create temp dir\n");
        return 1;
    }

    time_t before = time(NULL);
    curator_record_run(&rs, 42.5, "Updated 3 skills, removed 2 stale");
    time_t after = time(NULL);

    TEST("record: run_count 1", rs.run_count == 1);
    TEST("record: duration 42.5", rs.last_run_duration == 42.5);
    TEST("record: summary stored",
         strcmp(rs.last_run_summary, "Updated 3 skills, removed 2 stale") == 0);
    TEST("record: last_run_at set",
         rs.last_run_at >= before && rs.last_run_at <= after);

    /* Second record_run */
    sleep(1);  /* ensure time advances */
    curator_record_run(&rs, 10.0, "Quick check");
    TEST("record: run_count 2", rs.run_count == 2);
    TEST("record: new duration", rs.last_run_duration == 10.0);
    TEST("record: new summary",
         strcmp(rs.last_run_summary, "Quick check") == 0);

    /* NULL input */
    curator_record_run(NULL, 0, NULL);
    TEST("record: NULL no-op", 1);

    /* ================================================================== */
    /* 3. curator_format_duration                                          */
    /* ================================================================== */

    char buf[64];

    curator_format_duration(0, buf, sizeof(buf));
    TEST("duration 0s", strcmp(buf, "0s") == 0);

    curator_format_duration(5, buf, sizeof(buf));
    TEST("duration 5s", strcmp(buf, "5s") == 0);

    curator_format_duration(59, buf, sizeof(buf));
    TEST("duration 59s", strcmp(buf, "59s") == 0);

    curator_format_duration(60, buf, sizeof(buf));
    TEST("duration 1m 0s", strcmp(buf, "1m 0s") == 0);

    curator_format_duration(125, buf, sizeof(buf));
    TEST("duration 2m 5s", strcmp(buf, "2m 5s") == 0);

    curator_format_duration(3599, buf, sizeof(buf));
    TEST("duration 59m 59s", strcmp(buf, "59m 59s") == 0);

    curator_format_duration(3600, buf, sizeof(buf));
    TEST("duration 1h 0m", strcmp(buf, "1h 0m") == 0);

    curator_format_duration(3661, buf, sizeof(buf));
    TEST("duration 1h 1m", strcmp(buf, "1h 1m") == 0);

    curator_format_duration(86399, buf, sizeof(buf));
    TEST("duration 23h 59m", strcmp(buf, "23h 59m") == 0);

    curator_format_duration(86400, buf, sizeof(buf));
    TEST("duration 1d 0h", strcmp(buf, "1d 0h") == 0);

    curator_format_duration(172800, buf, sizeof(buf));
    TEST("duration 2d 0h", strcmp(buf, "2d 0h") == 0);

    curator_format_duration(90061, buf, sizeof(buf));
    TEST("duration 1d 1h", strcmp(buf, "1d 1h") == 0);

    /* NULL buffer */
    curator_format_duration(10, NULL, 0);
    TEST("duration: NULL no-op", 1);

    /* Negative value (clamped via unsigned cast — should still produce output) */
    curator_format_duration(-5, buf, sizeof(buf));
    TEST("duration negative", buf[0] != '\0');

    /* ================================================================== */
    /* 4. curator_format_reltime                                           */
    /* ================================================================== */

    /* t=0 → "never" */
    curator_format_reltime(0, buf, sizeof(buf));
    TEST("reltime: never", strcmp(buf, "never") == 0);

    /* Very recent (now) */
    curator_format_reltime(time(NULL), buf, sizeof(buf));
    TEST("reltime: just now (0s ago)",
         strcmp(buf, "0s ago") == 0);

    /* 10 seconds ago */
    curator_format_reltime(time(NULL) - 10, buf, sizeof(buf));
    TEST("reltime: 10s ago",
         strcmp(buf, "10s ago") == 0);

    /* 5 minutes ago */
    curator_format_reltime(time(NULL) - 300, buf, sizeof(buf));
    TEST("reltime: 5m ago",
         strcmp(buf, "5m ago") == 0);

    /* 2 hours ago */
    curator_format_reltime(time(NULL) - 7200, buf, sizeof(buf));
    TEST("reltime: 2h ago",
         strcmp(buf, "2h ago") == 0);

    /* 3 days ago */
    curator_format_reltime(time(NULL) - 259200, buf, sizeof(buf));
    TEST("reltime: 3d ago",
         strcmp(buf, "3d ago") == 0);

    /* Future timestamp — clamped to 0s */
    curator_format_reltime(time(NULL) + 3600, buf, sizeof(buf));
    TEST("reltime: future clamped",
         strcmp(buf, "0s ago") == 0);

    /* NULL buffer */
    curator_format_reltime(1000, NULL, 0);
    TEST("reltime: NULL no-op", 1);

    /* ================================================================== */
    /* 5. Save → Load round-trip via JSON file                             */
    /* ================================================================== */

    curator_state_t saved;
    curator_init_state(&saved);
    saved.enabled = true;
    saved.paused = true;
    saved.run_count = 7;
    saved.last_run_at = 1234567890;
    saved.last_run_duration = 33.3;
    snprintf(saved.last_run_summary, sizeof(saved.last_run_summary),
             "Round-trip test summary");

    curator_save_state(&saved);

    /* Verify file exists */
    char state_file[1024];
    snprintf(state_file, sizeof(state_file), "%s/skills/.curator_state", tmpdir);
    struct stat st;
    TEST("save: file exists", stat(state_file, &st) == 0);

    /* Load back */
    curator_state_t loaded;
    curator_init_state(&loaded);
    bool ok = curator_load_state(&loaded);
    TEST("load: success", ok);
    TEST("load: enabled", loaded.enabled == true);
    TEST("load: paused", loaded.paused == true);
    TEST("load: run_count 7", loaded.run_count == 7);
    TEST("load: last_run_at", loaded.last_run_at == 1234567890);
    TEST("load: duration", loaded.last_run_duration == 33.3);
    TEST("load: summary",
         strcmp(loaded.last_run_summary, "Round-trip test summary") == 0);

    /* ================================================================== */
    /* 6. Load from non-existent file                                      */
    /* ================================================================== */

    /* Switch to a different temp dir with no state file */
    char empty_tmpl[] = "/tmp/hermes_curator_empty_XXXXXX";
    const char *d2 = mkdtemp(empty_tmpl);
    if (!d2) {
        fprintf(stderr, "FAIL: could not create empty dir\n");
        goto cleanup;
    }
    char empty_dir[1024];
    snprintf(empty_dir, sizeof(empty_dir), "%s", d2);
    char skills_empty[1024];
    snprintf(skills_empty, sizeof(skills_empty), "%s/skills", d2);
    mkdir(skills_empty, 0755);
    setenv("HERMES_HOME", d2, 1);

    curator_state_t nofile;
    curator_init_state(&nofile);
    ok = curator_load_state(&nofile);
    TEST("load: missing file returns false", !ok);
    TEST("load: missing file init defaults",
         nofile.enabled == true && nofile.run_count == 0);

    /* Clean up empty dir */
    (void)cleanup_temp_home(d2);

    /* ================================================================== */
    /* 7. Save NULL state (should not crash)                               */
    /* ================================================================== */

    curator_save_state(NULL);
    TEST("save: NULL no-op", 1);

    /* ================================================================== */
    /* 8. Load from corrupt JSON file                                      */
    /* ================================================================== */

    /* Write corrupt JSON */
    FILE *f = fopen(state_file, "w");
    if (f) {
        fprintf(f, "{this is not valid json}");
        fclose(f);
    }
    curator_state_t corrupt;
    curator_init_state(&corrupt);
    ok = curator_load_state(&corrupt);
    TEST("load: corrupt JSON returns false", !ok);
    /* State should be init defaults */
    TEST("load: corrupt fallback defaults",
         corrupt.enabled == true && corrupt.run_count == 0);

    /* ================================================================== */
    /* 9. Save with empty summary                                          */
    /* ================================================================== */

    curator_state_t empty_summary;
    curator_init_state(&empty_summary);
    empty_summary.run_count = 1;
    empty_summary.last_run_at = 999999999;
    empty_summary.last_run_duration = 5.0;
    empty_summary.last_run_summary[0] = '\0';  /* empty */
    curator_save_state(&empty_summary);

    curator_state_t loaded2;
    curator_init_state(&loaded2);
    ok = curator_load_state(&loaded2);
    TEST("load: empty summary ok", ok);
    TEST("load: empty summary preserved",
         loaded2.last_run_summary[0] == '\0');

    /* ================================================================== */
    /* Summary                                                             */
    /* ================================================================== */

cleanup:
    if (tmpdir[0]) {
        setenv("HERMES_HOME", saved_env[0] ? saved_env : "", 1);
        cleanup_temp_home(tmpdir);
    }

    printf("\n");
    printf("  Results: %d passed, %d failed, 0 skipped\n", passed, tests - passed);

    return (passed == tests) ? 0 : 1;
}
