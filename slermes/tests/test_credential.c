/*
 * test_credential.c — M-series tests for credential files library.
 *
 * Tests the credential file passthrough registry for remote terminal backends.
 * Uses a temp HERMES_HOME to avoid touching real config.
 */

#include "credential.h"
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
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    const char *_a = (a); \
    const char *_b = (b); \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        printf("    assertion failed: %s -- expected \"%s\" got \"%s\" (%s:%d)\n", \
               msg, _a ? _a : "(null)", _b ? _b : "(null)", __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* ─── Helpers ────────────────────────────────────────────────────────── */

static char g_tmp_home[4096];
static char g_tmp_skills[4096];

static void setup(void)
{
    credential_clear();

    /* Create a temp HERMES_HOME */
    snprintf(g_tmp_home, sizeof(g_tmp_home), "/tmp/hermes_test_cred_XXXXXX");
    if (!mkdtemp(g_tmp_home)) {
        fprintf(stderr, "Failed to create temp dir\n");
        exit(1);
    }

    /* Set HERMES_HOME env var */
    setenv("HERMES_HOME", g_tmp_home, 1);

    /* Create intermediate directories */
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/.hermes", g_tmp_home);
    mkdir(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/.hermes/cache", g_tmp_home);
    mkdir(buf, 0755);

    /* Create skills dir */
    snprintf(g_tmp_skills, sizeof(g_tmp_skills), "%s/.hermes/skills", g_tmp_home);
    mkdir(g_tmp_skills, 0755);

    /* Create cache dirs */
    snprintf(buf, sizeof(buf), "%s/.hermes/cache/documents", g_tmp_home);
    mkdir(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/.hermes/cache/images", g_tmp_home);
    mkdir(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/.hermes/cache/audio", g_tmp_home);
    mkdir(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/.hermes/cache/screenshots", g_tmp_home);
    mkdir(buf, 0755);
}

static void teardown(void)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp_home);
    system(cmd);
    credential_shutdown();
}

/* Write a file with content */
static void write_test_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }
}

/* ─── Tests ──────────────────────────────────────────────────────────── */

static bool test_register_no_home(void)
{
    char my_home[4096];
    snprintf(my_home, sizeof(my_home), "/tmp/hct_nohome_XXXXXX");
    if (!mkdtemp(my_home)) return false;

    unsetenv("HERMES_HOME");
    setenv("HOME", my_home, 1);  /* force HOME to avoid affecting other tests */

    bool ok = credential_register_file(".bashrc", NULL);

    /* cleanup */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", my_home);
    system(cmd);
    return !ok;  /* should be false because .bashrc not inside HERMES_HOME=~/.hermes context */
}

static bool test_register_single_file(void)
{
    /* Create a test file in skills dir */
    char testfile[4096];
    snprintf(testfile, sizeof(testfile), "%s/test_config.json", g_tmp_skills);
    write_test_file(testfile, "{\"key\": \"value\"}");

    /* Register relative to HERMES_HOME */
    bool ok = credential_register_file(".hermes/skills/test_config.json", NULL);
    ASSERT(ok, "register .hermes/skills/test_config.json");

    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 1, "get_mounts count == 1");
    ASSERT_STR_EQ(mounts[0].host_path, testfile, "host_path matches");
    ASSERT(n > 0 && strstr(mounts[0].container_path, ".hermes/skills/test_config.json") != NULL,
           "container_path contains skills/test_config.json");
    return true;
}

static bool test_register_relative_skills(void)
{
    /* Register path relative to .hermes */
    char skills_dir[4096];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.hermes", g_tmp_home);

    /* Create nested file */
    char testfile[4096];
    snprintf(testfile, sizeof(testfile), "%s/skills/myskill/SKILL.md", skills_dir);
    mkdir(g_tmp_skills, 0755);
    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/skills/myskill", skills_dir);
    mkdir(subdir, 0755);
    write_test_file(testfile, "# Test Skill");

    bool ok = credential_register_file(".hermes/skills/myskill/SKILL.md", NULL);
    ASSERT(ok, "register nested skill file");

    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 1, "get_mounts count == 1");
    return true;
}

static bool test_register_nonexistent_file(void)
{
    bool ok = credential_register_file(".hermes/skills/nonexistent.json", NULL);
    ASSERT(!ok, "register nonexistent returns false");
    return true;
}

static bool test_register_absolute_path(void)
{
    bool ok = credential_register_file("/etc/passwd", NULL);
    ASSERT(!ok, "register absolute path rejected");
    return true;
}

static bool test_register_traversal(void)
{
    bool ok = credential_register_file("../../etc/passwd", NULL);
    ASSERT(!ok, "register path traversal rejected");
    return true;
}

static bool test_register_empty(void)
{
    bool ok = credential_register_file("", NULL);
    ASSERT(!ok, "register empty path rejected");
    bool ok2 = credential_register_file(NULL, NULL);
    ASSERT(!ok2, "register NULL path rejected");
    return true;
}

static bool test_register_multiple(void)
{
    /* Create multiple test files */
    char f1[4096], f2[4096];
    snprintf(f1, sizeof(f1), "%s/.hermes/key1.json", g_tmp_home);
    snprintf(f2, sizeof(f2), "%s/.hermes/key2.json", g_tmp_home);
    write_test_file(f1, "key1");
    write_test_file(f2, "key2");

    const char *paths[] = {
        ".hermes/key1.json",
        ".hermes/nonexistent.json",  /* this one doesn't exist */
        ".hermes/key2.json",
    };

    char *missing[8];
    int missing_cnt = credential_register_files(paths, 3, NULL, missing, 8);

    ASSERT(missing_cnt == 1, "1 missing file");
    ASSERT(missing[0] && strcmp(missing[0], ".hermes/nonexistent.json") == 0,
           "missing file is nonexistent.json");
    for (int i = 0; i < missing_cnt; i++) free(missing[i]);

    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 2, "2 registered files");
    return true;
}

static bool test_clear(void)
{
    /* Register something */
    char f[4096];
    snprintf(f, sizeof(f), "%s/.hermes/test_clear.json", g_tmp_home);
    write_test_file(f, "clear_me");

    credential_register_file(".hermes/test_clear.json", NULL);
    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 1, "before clear: 1 mount");

    credential_clear();
    n = credential_get_mounts(mounts, 8);
    ASSERT(n == 0, "after clear: 0 mounts");
    return true;
}

static bool test_get_skills_mount(void)
{
    /* Skills dir already created by setup */
    credential_mount_t mounts[4];
    int n = credential_get_skills_mount(NULL, mounts, 4);
    ASSERT(n == 1, "skills mount found");
    ASSERT(strstr(mounts[0].container_path, "/skills") != NULL,
           "container path ends with /skills");
    return true;
}

static bool test_get_skills_mount_no_dir(void)
{
    /* Set HERMES_HOME to an empty temp dir with no skills */
    char empty_home[4096];
    snprintf(empty_home, sizeof(empty_home), "/tmp/hermes_test_cred_empty_XXXXXX");
    if (!mkdtemp(empty_home)) return false;
    setenv("HERMES_HOME", empty_home, 1);

    credential_mount_t mounts[4];
    int n = credential_get_skills_mount(NULL, mounts, 4);
    ASSERT(n == 0, "no skills mount when dir missing");

    /* Cleanup */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", empty_home);
    system(cmd);
    return true;
}

static bool test_iter_skills_files(void)
{
    /* Create a skill file */
    char subdir[4096];
    snprintf(subdir, sizeof(subdir), "%s/myskill", g_tmp_skills);
    mkdir(subdir, 0755);
    char sk_file[4096];
    snprintf(sk_file, sizeof(sk_file), "%s/SKILL.md", subdir);
    write_test_file(sk_file, "# My Test Skill");

    credential_mount_t mounts[64];
    int n = credential_iter_skills_files(NULL, mounts, 64);
    ASSERT(n >= 1, "at least 1 skill file found");

    bool found = false;
    for (int i = 0; i < n; i++) {
        if (strstr(mounts[i].host_path, "SKILL.md") != NULL) {
            found = true;
            break;
        }
    }
    ASSERT(found, "SKILL.md found in iter");
    return true;
}

static bool test_get_cache_mounts(void)
{
    credential_mount_t mounts[8];
    int n = credential_get_cache_mounts(NULL, mounts, 8);
    ASSERT(n == 4, "4 cache dirs mounted (all exist in temp)");

    /* Verify each cache dir */
    ASSERT(strstr(mounts[0].container_path, "cache/documents") != NULL,
           "first cache dir is documents");
    ASSERT(strstr(mounts[3].container_path, "cache/screenshots") != NULL,
           "third cache dir is screenshots");
    return true;
}

static bool test_get_cache_mounts_empty(void)
{
    /* Create a fake home with no cache dirs */
    char empty_home[4096];
    snprintf(empty_home, sizeof(empty_home), "/tmp/hermes_test_cred_nocache_XXXXXX");
    if (!mkdtemp(empty_home)) return false;
    setenv("HERMES_HOME", empty_home, 1);

    credential_mount_t mounts[8];
    int n = credential_get_cache_mounts(NULL, mounts, 8);
    ASSERT(n == 0, "0 cache mounts when no dirs exist");

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", empty_home);
    system(cmd);
    return true;
}

static bool test_to_visible_cache_path(void)
{
    credential_mount_t mounts[8];
    int n = credential_get_cache_mounts(NULL, mounts, 8);
    ASSERT(n > 0, "cache mounts exist");

    /* Create a file inside one cache dir */
    char testfile[4096];
    snprintf(testfile, sizeof(testfile), "%s/.hermes/cache/documents/report.pdf",
             g_tmp_home);
    write_test_file(testfile, "fake pdf");

    char *translated = credential_to_visible_cache_path(testfile, NULL);
    ASSERT(translated != NULL, "path translated");
    ASSERT(strstr(translated, "cache/documents/report.pdf") != NULL,
           "translated path contains cache/documents/report.pdf");
    free(translated);

    /* Non-cache path should return NULL */
    char *notrans = credential_to_visible_cache_path("/tmp/somefile.txt", NULL);
    ASSERT(notrans == NULL, "non-cache path returns NULL");
    return true;
}

static bool test_iter_cache_files(void)
{
    /* Create a file in cache/documents */
    char testfile[4096];
    snprintf(testfile, sizeof(testfile), "%s/.hermes/cache/documents/report.pdf",
             g_tmp_home);
    write_test_file(testfile, "fake pdf");

    credential_mount_t mounts[128];
    int n = credential_iter_cache_files(NULL, mounts, 128);
    ASSERT(n >= 1, "at least 1 cache file found");

    bool found = false;
    for (int i = 0; i < n; i++) {
        if (strstr(mounts[i].host_path, "report.pdf") != NULL &&
            strstr(mounts[i].container_path, "cache/documents/report.pdf") != NULL) {
            found = true;
            break;
        }
    }
    ASSERT(found, "report.pdf found in cache files");
    return true;
}

static bool test_duplicate_registration(void)
{
    char f[4096];
    snprintf(f, sizeof(f), "%s/.hermes/dup_test.json", g_tmp_home);
    write_test_file(f, "dup");

    /* Register same path twice */
    bool ok1 = credential_register_file(".hermes/dup_test.json", NULL);
    bool ok2 = credential_register_file(".hermes/dup_test.json", NULL);
    ASSERT(ok1, "first register ok");
    ASSERT(ok2, "second register ok (duplicate)");

    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 1, "only 1 mount (deduped)");
    return true;
}

static bool test_custom_container_base(void)
{
    char f[4096];
    snprintf(f, sizeof(f), "%s/.hermes/custom_base.json", g_tmp_home);
    write_test_file(f, "custom");

    bool ok = credential_register_file(".hermes/custom_base.json", "/custom/path");
    ASSERT(ok, "register with custom base");

    credential_mount_t mounts[8];
    int n = credential_get_mounts(mounts, 8);
    ASSERT(n == 1, "1 mount with custom base");
    ASSERT(strstr(mounts[0].container_path, "/custom/path/") != NULL,
           "container path uses custom base");
    return true;
}

static bool test_null_edge_cases(void)
{
    credential_mount_t mounts[4];

    /* Should not crash */
    int n = credential_get_mounts(NULL, 0);
    ASSERT(n == 0, "get_mounts(NULL, 0)");

    n = credential_get_mounts(mounts, 0);
    ASSERT(n == 0, "get_mounts(buf, 0)");

    n = credential_get_skills_mount(NULL, NULL, 0);
    ASSERT(n == 0, "get_skills_mount(NULL, NULL, 0)");

    n = credential_iter_skills_files(NULL, NULL, 0);
    ASSERT(n == 0, "iter_skills_files(NULL, NULL, 0)");

    n = credential_get_cache_mounts(NULL, NULL, 0);
    ASSERT(n == 0, "get_cache_mounts(NULL, NULL, 0)");

    n = credential_iter_cache_files(NULL, NULL, 0);
    ASSERT(n == 0, "iter_cache_files(NULL, NULL, 0)");

    char *t = credential_to_visible_cache_path(NULL, NULL);
    ASSERT(t == NULL, "to_visible_cache_path(NULL, NULL)");

    int mc = credential_register_files(NULL, 0, NULL, NULL, 0);
    ASSERT(mc == 0, "register_files(NULL, 0)");

    ASSERT(true, "all null edge cases pass");
    return true;
}

/* ─── Main ───────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== Credential Files Library Tests ===\n");

    /* Each test gets its own setup/teardown for isolation */
    setup();  TEST(register_no_home);      teardown();
    setup();  TEST(register_single_file);   teardown();
    setup();  TEST(register_relative_skills); teardown();
    setup();  TEST(register_nonexistent_file); teardown();
    setup();  TEST(register_absolute_path); teardown();
    setup();  TEST(register_traversal);     teardown();
    setup();  TEST(register_empty);         teardown();
    setup();  TEST(register_multiple);      teardown();
    setup();  TEST(clear);                  teardown();
    setup();  TEST(get_skills_mount);       teardown();
    setup();  TEST(get_skills_mount_no_dir);teardown();
    setup();  TEST(iter_skills_files);      teardown();
    setup();  TEST(get_cache_mounts);       teardown();
    setup();  TEST(get_cache_mounts_empty); teardown();
    setup();  TEST(to_visible_cache_path);  teardown();
    setup();  TEST(iter_cache_files);       teardown();
    setup();  TEST(duplicate_registration); teardown();
    setup();  TEST(custom_container_base);  teardown();
    setup();  TEST(null_edge_cases);        teardown();

    printf("\nResults: %d passed, %d failed, %d total\n",
           tests_passed, tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
