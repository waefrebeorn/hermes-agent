/*
 * test_credential_files.c — Tests for credential_files module.
 * Tests: file registration, path validation, config loading, mounts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "credential_files.h"

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

/* Helper: create a temp file */
static char *create_temp_file(const char *dir, const char *name)
{
    static char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "test content\n");
        fclose(f);
    }
    return path;
}

/* Helper: create temp directory */
static char *create_temp_dir(void)
{
    static char dir[4096];
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    snprintf(dir, sizeof(dir), "%s/hermes_test_cred_XXXXXX", tmp);
    if (mkdtemp(dir) == NULL) return NULL;
    return dir;
}

int main(void)
{
    printf("=== Credential Files Tests ===\n\n");

    /* ── Registration ── */
    printf("-- Registration --\n");
    {
        credfiles_clear();
        TEST("registry starts empty", credfiles_get_registry()->count == 0);

        /* Create temp dir with a test file */
        char *tmp_dir = create_temp_dir();
        TEST("temp dir created", tmp_dir != NULL);

        /* Set HERMES_HOME to temp dir */
        setenv("HERMES_HOME", tmp_dir, 1);

        /* Register a file that exists */
        char *test_file = create_temp_file(tmp_dir, "test_token.json");
        TEST("test file created", test_file != NULL);

        bool ok = credfiles_register_file("test_token.json", "/root/.hermes");
        TEST("register existing file", ok == true);
        TEST("registry count 1", credfiles_get_registry()->count == 1);

        /* Check registry contents */
        credfiles_registry_t *r = credfiles_get_registry();
        TEST("host path ends with test_token.json",
             strstr(r->pairs[0].host_path, "test_token.json") != NULL);
        TEST("container path = /root/.hermes/test_token.json",
             strcmp(r->pairs[0].container_path, "/root/.hermes/test_token.json") == 0);

        /* Register a non-existent file */
        ok = credfiles_register_file("does_not_exist.json", "/root/.hermes");
        TEST("register missing file fails", ok == false);
        TEST("registry still 1", credfiles_get_registry()->count == 1);

        /* Cleanup */
        unlink(test_file);
        rmdir(tmp_dir);
        credfiles_clear();
        TEST("registry cleared", credfiles_get_registry()->count == 0);
    }

    /* ── Security: absolute path rejection ── */
    printf("\n-- Security --\n");
    {
        char *tmp_dir = create_temp_dir();
        setenv("HERMES_HOME", tmp_dir, 1);

        bool ok = credfiles_register_file("/etc/passwd", "/root/.hermes");
        TEST("reject absolute path", ok == false);

        ok = credfiles_register_file("../../etc/passwd", "/root/.hermes");
        TEST("reject path traversal ../", ok == false);

        ok = credfiles_register_file("subdir/../../etc/passwd", "/root/.hermes");
        TEST("reject nested ../", ok == false);

        /* Valid relative path still works */
        char *test_file = create_temp_file(tmp_dir, "valid_file.txt");
        ok = credfiles_register_file("valid_file.txt", "/root/.hermes");
        TEST("valid relative path accepted", ok == true);

        unlink(test_file);
        rmdir(tmp_dir);
        credfiles_clear();
    }

    /* ── NULL / empty input ── */
    printf("\n-- Edge cases --\n");
    {
        bool ok = credfiles_register_file(NULL, "/root/.hermes");
        TEST("null path returns false", ok == false);

        ok = credfiles_register_file("", "/root/.hermes");
        TEST("empty path returns false", ok == false);

        /* Clear when already empty */
        credfiles_clear();
        TEST("clear empty is safe", credfiles_get_registry()->count == 0);
    }

    /* ── Multiple file registration ── */
    printf("\n-- Multiple registration --\n");
    {
        char d_buf[] = "/tmp/hermes_multi_XXXXXX";
        char *tmp_dir = mkdtemp(d_buf);
        TEST("multi tmp dir created", tmp_dir != NULL);
        setenv("HERMES_HOME", tmp_dir, 1);

        char p1[4096]; snprintf(p1, sizeof(p1), "%s/token1.json", tmp_dir);
        char p2[4096]; snprintf(p2, sizeof(p2), "%s/token2.json", tmp_dir);
        FILE *f1 = fopen(p1, "w"); fprintf(f1, "tok1\n"); fclose(f1);
        FILE *f2 = fopen(p2, "w"); fprintf(f2, "tok2\n"); fclose(f2);

        bool ok1 = credfiles_register_file("token1.json", "/root/.hermes");
        bool ok2 = credfiles_register_file("token2.json", "/root/.hermes");
        TEST("register token1", ok1);
        TEST("register token2", ok2);
        TEST("count = 2", credfiles_get_registry()->count == 2);

        /* Verify container paths */
        credfiles_registry_t *r = credfiles_get_registry();
        bool found1 = (strcmp(r->pairs[0].container_path, "/root/.hermes/token1.json") == 0) ||
                       (strcmp(r->pairs[1].container_path, "/root/.hermes/token1.json") == 0);
        bool found2 = (strcmp(r->pairs[0].container_path, "/root/.hermes/token2.json") == 0) ||
                       (strcmp(r->pairs[1].container_path, "/root/.hermes/token2.json") == 0);
        TEST("token1 path correct", found1);
        TEST("token2 path correct", found2);

        unlink(p1); unlink(p2);
        rmdir(tmp_dir);
        credfiles_clear();
    }

    /* ── register_files (batch) ── */
    printf("\n-- Batch register --\n");
    {
        char d_buf[] = "/tmp/hermes_batch_XXXXXX";
        char *tmp_dir = mkdtemp(d_buf);
        TEST("batch tmp dir created", tmp_dir != NULL);
        setenv("HERMES_HOME", tmp_dir, 1);

        char pa[4096]; snprintf(pa, sizeof(pa), "%s/a.json", tmp_dir);
        char pb[4096]; snprintf(pb, sizeof(pb), "%s/b.json", tmp_dir);
        FILE *fa = fopen(pa, "w"); fprintf(fa, "a\n"); fclose(fa);
        FILE *fb = fopen(pb, "w"); fprintf(fb, "b\n"); fclose(fb);

        const char *entries[] = {"a.json", "b.json", "missing.json"};
        char missing[3][CREDFILES_MAX_PATH];
        int n_missing = credfiles_register_files(entries, 3, "/root/.hermes",
                                                   missing, 3);
        TEST("1 missing file reported", n_missing == 1);
        TEST("missing file is missing.json",
             strcmp(missing[0], "missing.json") == 0);
        TEST("2 files registered", credfiles_get_registry()->count == 2);

        unlink(pa); unlink(pb);
        rmdir(tmp_dir);
        credfiles_clear();
    }

    /* ── Skills mount ── */
    printf("\n-- Skills mount --\n");
    {
        char d_buf[] = "/tmp/hermes_skills_XXXXXX";
        char *tmp_dir = mkdtemp(d_buf);
        TEST("skills tmp dir created", tmp_dir != NULL);
        setenv("HERMES_HOME", tmp_dir, 1);

        /* Create skills dir */
        char skills_dir[4096];
        snprintf(skills_dir, sizeof(skills_dir), "%s/skills", tmp_dir);
        mkdir(skills_dir, 0755);

        credfiles_path_list_t mounts = credfiles_get_skills_mount("/root/.hermes");
        TEST("skills mount found", mounts.count == 1);
        TEST("skills host path correct",
             strstr(mounts.entries[0].host_path, "skills") != NULL);
        TEST("skills container path",
             strcmp(mounts.entries[0].container_path, "/root/.hermes/skills") == 0);

        rmdir(skills_dir);
        rmdir(tmp_dir);
    }

    /* ── Cache mounts ── */
    printf("\n-- Cache mounts --\n");
    {
        /* Create dedicated temp dir for cache test */
        char dir_buf[] = "/tmp/hermes_cache_XXXXXX";
        char *tmp_dir = mkdtemp(dir_buf);
        TEST("cache tmp dir created", tmp_dir != NULL);
        setenv("HERMES_HOME", tmp_dir, 1);

        /* Create documents cache dir */
        char cache_parent[4096];
        snprintf(cache_parent, sizeof(cache_parent), "%s/cache", tmp_dir);
        mkdir(cache_parent, 0755);
        char cache_dir[4096];
        snprintf(cache_dir, sizeof(cache_dir), "%s/cache/documents", tmp_dir);
        mkdir(cache_dir, 0755);

        credfiles_path_list_t mounts = credfiles_get_cache_mounts("/root/.hermes");
        TEST("at least 1 cache mount", mounts.count >= 1);
        if (mounts.count >= 1) {
            TEST("docs cache host path",
                 strstr(mounts.entries[0].host_path, "cache/documents") != NULL);
            TEST("docs cache container path",
                 strcmp(mounts.entries[0].container_path,
                        "/root/.hermes/cache/documents") == 0);
        }

        rmdir(cache_dir);
        rmdir(cache_parent);
        rmdir(tmp_dir);
    }

    /* ── HERMES_HOME env var fallback ── */
    printf("\n-- HERMES_HOME resolution --\n");
    {
        /* Unset HERMES_HOME, should fall back to $HOME/.hermes */
        unsetenv("HERMES_HOME");
        setenv("HOME", "/tmp/fakehome", 1);
        const char *home = credfiles_get_hermes_home();
        TEST("fallback to /tmp/fakehome/.hermes",
             home != NULL && strcmp(home, "/tmp/fakehome/.hermes") == 0);
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
