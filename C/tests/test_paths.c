/* test_paths.c — Path resolution tests for paths.c module.
 * Tests: get_home, config_dir, data_dir, cache_dir, log_dir, resolve_path,
 * set_profile, get_profile, display_home. Compile with paths.c as dep.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Extern declarations for paths module functions */
extern void hermes_get_home(char *buf, size_t sz);
extern void hermes_config_dir(char *buf, size_t sz);
extern void hermes_data_dir(char *buf, size_t sz);
extern void hermes_cache_dir(char *buf, size_t sz);
extern void hermes_log_dir(char *buf, size_t sz);
extern void hermes_resolve_path(const char *sub, char *buf, size_t sz);
extern void hermes_display_home(void);
extern void hermes_set_profile(const char *name);
extern const char *hermes_get_profile(void);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

/* ================================================================
 *  hermes_get_home
 * ================================================================ */
static void test_get_home_custom_env(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_home", 1);
    char buf[512];
    hermes_get_home(buf, sizeof(buf));
    TEST("get_home respects SLERMES_HOME env var",
         strcmp(buf, "/tmp/slermes_test_home") == 0);
}

static void test_config_dir_routes(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_cfg", 1);
    char buf[512];
    hermes_config_dir(buf, sizeof(buf));
    TEST("config_dir returns HOME (home IS config dir)",
         strcmp(buf, "/tmp/slermes_test_cfg") == 0);
}

static void test_data_dir_routes(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_data", 1);
    char buf[512];
    hermes_data_dir(buf, sizeof(buf));
    TEST("data_dir returns HOME/data",
         strcmp(buf, "/tmp/slermes_test_data/data") == 0);
}

static void test_cache_dir_routes(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_cache", 1);
    char buf[512];
    hermes_cache_dir(buf, sizeof(buf));
    TEST("cache_dir returns HOME/cache",
         strcmp(buf, "/tmp/slermes_test_cache/cache") == 0);
}

static void test_log_dir_routes(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_log", 1);
    char buf[512];
    hermes_log_dir(buf, sizeof(buf));
    TEST("log_dir returns HOME/logs",
         strcmp(buf, "/tmp/slermes_test_log/logs") == 0);
}

static void test_resolve_path_absolute(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_rp", 1);
    char buf[512];
    hermes_resolve_path("skills/foo", buf, sizeof(buf));
    TEST("resolve_path joins HOME with subpath",
         strcmp(buf, "/tmp/slermes_test_rp/skills/foo") == 0);
}

static void test_resolve_path_null_sub(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_rpn", 1);
    char buf[512];
    hermes_resolve_path(NULL, buf, sizeof(buf));
    TEST("resolve_path returns HOME for NULL sub",
         strcmp(buf, "/tmp/slermes_test_rpn") == 0);
}

static void test_profile_set_and_get(void) {
    hermes_set_profile("test-profile");
    const char *p = hermes_get_profile();
    TEST("set_profile/get_profile round-trip",
         p && strcmp(p, "test-profile") == 0);
    hermes_set_profile("");
}

static void test_profile_max_length(void) {
    char long_name[128];
    memset(long_name, 'A', 63);
    long_name[63] = '\0';
    hermes_set_profile(long_name);
    const char *p = hermes_get_profile();
    TEST("profile name capped at 63 chars",
         p && strlen(p) <= 64);
    hermes_set_profile("");
}

static void test_display_home_no_crash(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_disp", 1);
    hermes_display_home();
    TEST("display_home does not crash", 1);
}

static void test_subdir_ordering(void) {
    setenv("SLERMES_HOME", "/tmp/slermes_test_ord", 1);
    char cfg[512], data[512], cache[512], log[512];
    hermes_config_dir(cfg, sizeof(cfg));
    hermes_data_dir(data, sizeof(data));
    hermes_cache_dir(cache, sizeof(cache));
    hermes_log_dir(log, sizeof(log));
    int all_distinct = strcmp(cfg, data) != 0 && strcmp(cfg, cache) != 0
        && strcmp(cfg, log) != 0 && strcmp(data, cache) != 0
        && strcmp(data, log) != 0 && strcmp(cache, log) != 0;
    TEST("all subdir routes are distinct", all_distinct);
}

int main(void) {
    printf("=== Path Resolution Tests ===\n\n");

    test_get_home_custom_env();
    test_config_dir_routes();
    test_data_dir_routes();
    test_cache_dir_routes();
    test_log_dir_routes();
    test_resolve_path_absolute();
    test_resolve_path_null_sub();
    test_profile_set_and_get();
    test_profile_max_length();
    test_display_home_no_crash();
    test_subdir_ordering();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
