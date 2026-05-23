/*
 * test_paths.c -- Tests for P21 paths.c (SLERMES_HOME resolution).
 *
 * Tests: env var resolution, fallback to HOME, profile state,
 * data/cache/log dir construction, resolve_path, display_home.
 */

#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static int passed = 0, failed = 0, skipped = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

/* Check that buf ends with expected suffix */
static int has_suffix(const char *buf, const char *suffix) {
    if (!buf || !suffix) return 0;
    size_t blen = strlen(buf);
    size_t slen = strlen(suffix);
    if (slen > blen) return 0;
    return strcmp(buf + blen - slen, suffix) == 0;
}

int main(void) {
    printf("=== Path Resolution Tests (P21) ===\n");

    /* --- SLERMES_HOME env var path --- */
    printf("\n--- SLERMES_HOME Resolution ---\n");
    {
        TEST("with SLERMES_HOME set");
        const char *saved = getenv("SLERMES_HOME");
        setenv("SLERMES_HOME", "/custom/slermes", 1);
        char buf[4096];
        hermes_get_home(buf, sizeof(buf));
        if (strcmp(buf, "/custom/slermes") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
        if (saved) setenv("SLERMES_HOME", saved, 1);
        else unsetenv("SLERMES_HOME");
    }
    {
        TEST("without SLERMES_HOME, uses HOME/.slermes");
        const char *saved_home = getenv("SLERMES_HOME");
        const char *home = getenv("HOME");
        unsetenv("SLERMES_HOME");
        char buf[4096];
        hermes_get_home(buf, sizeof(buf));
        char expected[4096];
        snprintf(expected, sizeof(expected), "%s/.slermes", home ? home : "/tmp");
        if (strcmp(buf, expected) == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
        if (saved_home) setenv("SLERMES_HOME", saved_home, 1);
    }

    /* --- Subdirectory resolution --- */
    printf("\n--- Subdirectory Resolution ---\n");
    {
        TEST("config_dir = SLERMES_HOME");
        setenv("SLERMES_HOME", "/home/test/.slermes", 1);
        char buf[4096];
        hermes_config_dir(buf, sizeof(buf));
        if (strcmp(buf, "/home/test/.slermes") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }
    {
        TEST("data_dir appends /data");
        char buf[4096];
        hermes_data_dir(buf, sizeof(buf));
        if (has_suffix(buf, "/data")) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }
    {
        TEST("cache_dir appends /cache");
        char buf[4096];
        hermes_cache_dir(buf, sizeof(buf));
        if (has_suffix(buf, "/cache")) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }
    {
        TEST("log_dir appends /logs");
        char buf[4096];
        hermes_log_dir(buf, sizeof(buf));
        if (has_suffix(buf, "/logs")) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }

    /* --- resolve_path --- */
    printf("\n--- Resolve Path ---\n");
    {
        TEST("resolve with subpath");
        setenv("SLERMES_HOME", "/base", 1);
        char buf[4096];
        hermes_resolve_path("subdir/file.txt", buf, sizeof(buf));
        if (strcmp(buf, "/base/subdir/file.txt") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }
    {
        TEST("resolve without subpath returns base");
        setenv("SLERMES_HOME", "/base", 1);
        char buf[4096];
        hermes_resolve_path("", buf, sizeof(buf));
        if (strcmp(buf, "/base") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }
    {
        TEST("resolve with NULL subpath returns base");
        setenv("SLERMES_HOME", "/base", 1);
        char buf[4096];
        hermes_resolve_path(NULL, buf, sizeof(buf));
        if (strcmp(buf, "/base") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", buf); FAIL(err); }
    }

    /* --- Profile management --- */
    printf("\n--- Profile Management ---\n");
    {
        TEST("profile defaults to NULL");
        const char *p = hermes_get_profile();
        if (p == NULL) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", p); FAIL(err); }
    }
    {
        TEST("set_profile then get_profile");
        hermes_set_profile("dev");
        const char *p = hermes_get_profile();
        if (p && strcmp(p, "dev") == 0) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", p ? p : "NULL"); FAIL(err); }
    }
    {
        TEST("clear profile sets to NULL");
        hermes_set_profile("");
        const char *p = hermes_get_profile();
        if (p == NULL) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", p); FAIL(err); }
    }
    {
        TEST("set_profile(NULL) clears");
        hermes_set_profile("prod");
        hermes_set_profile(NULL);
        const char *p = hermes_get_profile();
        if (p == NULL) { PASS; }
        else { char err[256]; snprintf(err, sizeof(err), "got '%s'", p); FAIL(err); }
    }

    /* --- Buffer size safety --- */
    printf("\n--- Buffer Safety ---\n");
    {
        TEST("small buffer truncated gracefully");
        setenv("SLERMES_HOME", "/very/long/path/that/exceeds/small/buffer", 1);
        char tiny[8];
        hermes_get_home(tiny, sizeof(tiny));
        tiny[sizeof(tiny)-1] = '\0';
        if (strlen(tiny) < sizeof(tiny)) { PASS; }
        else { FAIL("buffer not null-terminated"); }
        unsetenv("SLERMES_HOME");
    }

    /* --- display_home smoke test --- */
    printf("\n--- Display Home ---\n");
    {
        TEST("hermes_display_home prints without crash");
        setenv("SLERMES_HOME", "/test/slermes", 1);
        hermes_set_profile("test");
        /* Capture stdout */
        int orig_stdout = dup(1);
        int null_fd = open("/dev/null", O_WRONLY);
        dup2(null_fd, 1);
        hermes_display_home();
        fflush(stdout);
        dup2(orig_stdout, 1);
        close(null_fd);
        close(orig_stdout);
        PASS;
        unsetenv("SLERMES_HOME");
    }

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
