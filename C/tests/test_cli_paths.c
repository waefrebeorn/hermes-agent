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

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

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

int main(void) {
    printf("=== CLI Paths Tests ===\n\n");

    test_hermes_get_home();
    test_subdirs();
    test_profile();
    test_fallback_home();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
