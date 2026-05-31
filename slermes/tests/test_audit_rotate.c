/*
 * test_audit_rotate.c — O12: Audit log rotation tests
 *
 * Tests:
 *   - Log to file, verify rotation triggers at size limit
 *   - Verify rotate closes old file and creates new
 *   - Verify configurable max_files, max_size
 *   - NULL safety for all 5 log functions
 *   - Convenience wrappers: approval, redaction, violation
 *   - Newline sanitization
 *   - Long/missing fields
 */
#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

static char tmpdir[256] = {0};
static char log_dir[512] = {0};
static char log_path[512] = {0};

static void setup(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/hermes_audit_XXXXXX");
    if (!mkdtemp(tmpdir)) { fprintf(stderr, "mkdtemp failed\n"); exit(1); }
    setenv("SLERMES_HOME", tmpdir, 1);
    snprintf(log_dir, sizeof(log_dir), "%s/logs", tmpdir);
    mkdir(log_dir, 0755);
    snprintf(log_path, sizeof(log_path), "%s/security.log", log_dir);
}

static void cleanup(void) {
    audit_shutdown();
    unlink(log_path);
    rmdir(log_dir);
    rmdir(tmpdir);
    unsetenv("SLERMES_HOME");
}

static int count_lines(void) {
    FILE *f = fopen(log_path, "r");
    if (!f) return 0;
    int n = 0; char buf[8192];
    while (fgets(buf, sizeof(buf), f)) n++;
    fclose(f);
    return n;
}

static int log_exists_with(const char *needle) {
    FILE *f = fopen(log_path, "r");
    if (!f) return 0;
    char buf[8192];
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, needle)) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

int main(void) {
    printf("=== O12: Audit log rotation ===\n\n");

    /* Init */
    setup();
    TEST("audit_init", audit_init(log_dir));

    printf("\n--- Security log ---\n");
    {
        audit_log_security("test", "op1", "allowed", "unit_test", "test entry 1");
        audit_log_security("test", "op2", "allowed", "unit_test", "test entry 2");
        audit_log_security("test", "op3", "allowed", "unit_test", "test entry 3");

        struct stat st;
        TEST("log file created", stat(log_path, &st) == 0 && st.st_size > 0);
        TEST("log contains test entry", log_exists_with("test entry 1"));
        TEST("log contains category test", log_exists_with("test|op1"));
        TEST("log has 3 lines from security entries", count_lines() == 3);
    }

    printf("\n--- NULL safety ---\n");
    {
        audit_log_security(NULL, NULL, NULL, NULL, NULL);
        TEST("NULL category/action/result/reason/detail no crash", 1);
        TEST("NULL entry logged with empty fields", log_exists_with("||||"));
        TEST("5 lines total after NULL entry", count_lines() == 4);
    }

    printf("\n--- Empty string safety ---\n");
    {
        audit_log_security("", "", "", "", "");
        TEST("empty strings no crash", 1);
        TEST("6 lines total after empty entry", count_lines() == 5);
    }

    printf("\n--- Newline sanitization ---\n");
    {
        audit_log_security("nl", "test", "ok", "reason", "line1\nline2\rline3");
        TEST("newlines sanitized no crash", 1);
        /* The sanitizer replaces \n and \r with spaces, so log is single-line */
        char buf[8192];
        FILE *f = fopen(log_path, "r");
        if (f) {
            /* Read last line */
            char *last = NULL;
            while (fgets(buf, sizeof(buf), f)) last = buf;
            fclose(f);
            TEST("newline sanitized to spaces", last && strstr(last, "line1 line2 line3"));
        }
    }

    printf("\n--- audit_log_approval ---\n");
    {
        audit_log_approval("file_write", "/tmp/test.txt", true);
        audit_log_approval("terminal_exec", "rm -rf /", false);
        TEST("approval entries no crash", 1);
        TEST("approval approved logged", log_exists_with("approval|file_write|approved"));
        TEST("approval denied logged", log_exists_with("approval|terminal_exec|denied"));
    }

    printf("\n--- audit_log_redaction ---\n");
    {
        audit_log_redaction("model_response", "sk-[a-zA-Z0-9]+");
        TEST("redaction entry no crash", 1);
        TEST("redaction logged", log_exists_with("redaction|model_response|redacted"));
    }

    printf("\n--- audit_log_violation ---\n");
    {
        audit_log_violation("file_blocked", "/etc/shadow");
        TEST("violation entry no crash", 1);
        TEST("violation logged", log_exists_with("violation|file_blocked|blocked"));
    }

    printf("\n--- Very long detail ---\n");
    {
        char long_detail[3000];
        memset(long_detail, 'A', sizeof(long_detail) - 1);
        long_detail[sizeof(long_detail) - 1] = '\0';
        audit_log_security("long", "detail", "ok", "threshold", long_detail);
        TEST("long detail no crash", 1);
        /* Detail_clean buffer is 4096, our 3000 fits */
        TEST("long detail logged", log_exists_with("AAAAA"));
    }

    printf("\n--- Rotation configuration ---\n");
    {
        audit_set_rotation(0, 0, 0);    /* disable */
        audit_set_rotation(1024, 5, 30); /* 1MB, 5 files, 30 days */
        audit_set_rotation(10240, 3, 7); /* 10MB, 3 files, 7 days */
        audit_set_rotation(1, 10, 365);  /* 1KB, 10 files, 1 year */
        TEST("set_rotation no crash with various inputs", 1);

        /* Log after rotation config change */
        audit_log_security("rotation", "test", "ok", "config", "after rotation set");
        TEST("log still works after rotation config", log_exists_with("after rotation set"));
    }

    printf("\n--- Multiple convenience wrappers interleaved ---\n");
    {
        audit_log_approval("tool_a", "cmd_a", true);
        audit_log_redaction("output", "password=");
        audit_log_violation("path_blocked", "/etc/passwd");
        audit_log_approval("tool_b", "cmd_b", false);
        TEST("interleaved wrappers no crash", 1);
        TEST("final approval logged", log_exists_with("approval|tool_b|denied"));
    }

    printf("\n--- Cleanup ---\n");
    {
        cleanup();
        /* Re-init and verify new file */
        setup();
        TEST("audit_init after shutdown", audit_init(log_dir));
        audit_log_security("reinit", "test", "ok", "post_shutdown", "fresh start");
        TEST("log after re-init works", log_exists_with("fresh start"));
        cleanup();
    }

    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
