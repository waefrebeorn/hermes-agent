/*
 * test_paths.c — Tests for CLI path resolution module.
 *
 * Tests SLERMES_HOME env var, default paths, profile management,
 * and edge cases.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Forward declarations from paths.c */
void hermes_get_home(char *buf, size_t sz);
void hermes_config_dir(char *buf, size_t sz);
void hermes_data_dir(char *buf, size_t sz);
void hermes_cache_dir(char *buf, size_t sz);
void hermes_log_dir(char *buf, size_t sz);
void hermes_resolve_path(const char *sub, char *buf, size_t sz);
void hermes_display_home(void);
void hermes_set_profile(const char *name);
const char *hermes_get_profile(void);

static int pass = 0, fail = 0;
static int total_assertions = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; total_assertions++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; total_assertions++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } total_assertions++; } while(0)
#define PASS_OK() do { printf("OK\n"); pass++; total_assertions++; } while(0)

/* Save/restore environment around tests */
static char saved_slermes_home[4096] = "";
static char saved_home[4096] = "";

static void save_env(void) {
    const char *sh = getenv("SLERMES_HOME");
    if (sh) snprintf(saved_slermes_home, sizeof(saved_slermes_home), "%s", sh);
    const char *h = getenv("HOME");
    if (h) snprintf(saved_home, sizeof(saved_home), "%s", h);
}

static void restore_env(void) {
    if (saved_slermes_home[0])
        setenv("SLERMES_HOME", saved_slermes_home, 1);
    else
        unsetenv("SLERMES_HOME");
    if (saved_home[0])
        setenv("HOME", saved_home, 1);
    else
        unsetenv("HOME");
}

static void test_hermes_get_home(void) {
    TEST("hermes_get_home with SLERMES_HOME set");
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    char buf[4096];
    hermes_get_home(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes") == 0, "wrong path");
    PASS();

    TEST("hermes_get_home default without SLERMES_HOME");
    unsetenv("SLERMES_HOME");
    setenv("HOME", "/home/testuser", 1);
    hermes_get_home(buf, sizeof(buf));
    ASSERT(strstr(buf, "/home/testuser/.slermes") != NULL, "expected ~/.slermes");
    PASS();
}

static void test_subdirs(void) {
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    char buf[4096];

    TEST("hermes_config_dir");
    hermes_config_dir(buf, sizeof(buf));
    /* Config dir IS the home dir */
    ASSERT(strcmp(buf, "/tmp/test_slermes") == 0, "config dir should equal home");
    PASS();

    TEST("hermes_data_dir");
    hermes_data_dir(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes/data") == 0, "wrong data dir");
    PASS();

    TEST("hermes_cache_dir");
    hermes_cache_dir(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes/cache") == 0, "wrong cache dir");
    PASS();

    TEST("hermes_log_dir");
    hermes_log_dir(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes/logs") == 0, "wrong log dir");
    PASS();

    TEST("hermes_resolve_path");
    hermes_resolve_path("skills", buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes/skills") == 0, "wrong resolved path");
    PASS();

    TEST("hermes_resolve_path NULL sub");
    hermes_resolve_path(NULL, buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes") == 0, "NULL should return home");
    PASS();

    TEST("hermes_resolve_path empty sub");
    hermes_resolve_path("", buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes") == 0, "empty should return home");
    PASS();

    TEST("hermes_resolve_path nested");
    hermes_resolve_path("a/b/c/d", buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes/a/b/c/d") == 0, "wrong nested path");
    PASS();
}

static void test_profile(void) {
    TEST("hermes_get_profile default NULL");
    hermes_set_profile("");
    const char *p = hermes_get_profile();
    ASSERT(p == NULL, "default profile should be NULL");
    PASS();

    TEST("hermes_set_profile / hermes_get_profile round-trip");
    hermes_set_profile("work");
    p = hermes_get_profile();
    ASSERT(p != NULL, "profile should not be NULL");
    ASSERT(strcmp(p, "work") == 0, "wrong profile name");
    PASS();

    TEST("hermes_set_profile empty clears");
    hermes_set_profile("");
    p = hermes_get_profile();
    ASSERT(p == NULL, "empty should clear profile");
    PASS();

    TEST("hermes_set_profile NULL clears");
    hermes_set_profile(NULL);
    p = hermes_get_profile();
    ASSERT(p == NULL, "NULL should clear profile");
    PASS();

    TEST("hermes_set_profile overwrite");
    hermes_set_profile("first");
    hermes_set_profile("second");
    p = hermes_get_profile();
    ASSERT(p != NULL && strcmp(p, "second") == 0, "should overwrite");
    PASS();
}

static void test_fallback_home(void) {
    TEST("hermes_get_home fallback when no HOME");
    unsetenv("SLERMES_HOME");
    unsetenv("HOME");
    char buf[4096];
    hermes_get_home(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/.slermes") == 0, "expected /tmp/.slermes fallback");
    PASS();
}

/* ---- Additional edge case tests ---- */

static void test_small_buffer(void) {
    TEST("hermes_get_home tiny buffer (no crash)");
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    char buf[4];
    memset(buf, 0xAA, sizeof(buf));
    hermes_get_home(buf, 4);
    /* Must be NUL-terminated within buffer */
    ASSERT(buf[0] == '/' || buf[0] == '\0', "buffer not corrupted");
    PASS();
}

static void test_resolve_home_without_trailing_slash(void) {
    TEST("SLERMES_HOME without trailing slash");
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    char buf[4096];
    hermes_resolve_path("skills/extra", buf, sizeof(buf));
    /* Should NOT double-slash: /tmp/test_slermes//skills/extra */
    ASSERT(strstr(buf, "//") == NULL, "no double slash");
    ASSERT(strcmp(buf, "/tmp/test_slermes/skills/extra") == 0, "wrong path");
    PASS();
}

static void test_consistency_all_dirs(void) {
    TEST("all dirs under same home");
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    char home[4096], data[4096], cache[4096], logs[4096];
    hermes_get_home(home, sizeof(home));
    hermes_data_dir(data, sizeof(data));
    hermes_cache_dir(cache, sizeof(cache));
    hermes_log_dir(logs, sizeof(logs));

    size_t hlen = strlen(home);
    ASSERT(strncmp(data, home, hlen) == 0, "data not under home");
    ASSERT(strncmp(cache, home, hlen) == 0, "cache not under home");
    ASSERT(strncmp(logs, home, hlen) == 0, "logs not under home");
    ASSERT(strcmp(data + hlen, "/data") == 0, "data suffix wrong");
    ASSERT(strcmp(cache + hlen, "/cache") == 0, "cache suffix wrong");
    ASSERT(strcmp(logs + hlen, "/logs") == 0, "logs suffix wrong");
    PASS();
}

static void test_profile_does_not_affect_home(void) {
    TEST("profile set does not change home path");
    setenv("SLERMES_HOME", "/tmp/test_slermes", 1);
    hermes_set_profile("production");
    char buf[4096];
    hermes_get_home(buf, sizeof(buf));
    ASSERT(strcmp(buf, "/tmp/test_slermes") == 0, "home unchanged by profile");
    hermes_set_profile("");
    PASS();
}

static void test_long_slermes_home(void) {
    TEST("very long SLERMES_HOME path");
    /* Create a path slightly under PATH_MAX */
    char long_path[HERMES_PATH_MAX];
    memset(long_path, 'a', HERMES_PATH_MAX - 64);
    long_path[0] = '/';
    long_path[HERMES_PATH_MAX - 65] = '\0';
    setenv("SLERMES_HOME", long_path, 1);
    char buf[4096];
    hermes_get_home(buf, sizeof(buf));
    ASSERT(strncmp(buf, long_path, 300) == 0, "long path preserved");
    ASSERT(strlen(buf) < sizeof(buf), "fits in target buffer");
    PASS();
}

int main(void) {
    save_env();
    printf("=== CLI Paths Tests ===\n\n");

    test_hermes_get_home();
    test_subdirs();
    test_profile();
    test_fallback_home();
    test_small_buffer();
    test_resolve_home_without_trailing_slash();
    test_consistency_all_dirs();
    test_profile_does_not_affect_home();
    test_long_slermes_home();

    restore_env();

    printf("\n=== Results: %d passed, %d failed (%d assertions) ===\n",
           pass, fail, total_assertions);
    return fail > 0 ? 1 : 0;
}
