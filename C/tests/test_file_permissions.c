/*
 * test_file_permissions.c — O15: File permission hardening tests
 *
 * Tests hermes_file_permissions_harden():
 *   - Sets 0700 on home dir
 *   - Sets 0600 on config.yaml, .env, session DB, vault, cron store
 *   - Skips when owner is root (uid 0)
 *   - Graceful on missing/non-existent files
 *   - NULL safety
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

static void create_file(const char *path, mode_t mode) {
    FILE *f = fopen(path, "w");
    if (f) { fputs("test", f); fclose(f); }
    chmod(path, mode);
}

int main(void) {
    printf("=== O15: File permission hardening ===\n\n");

    /* Setup: create temp dir with test files */
    char tmpdir[] = "/tmp/hermes_perm_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "FAIL: mkdtemp failed\n");
        return 1;
    }

    char config_path[512];
    char env_path[512];
    char db_path[512];
    char vault_path[512];
    char cron_path[512];

    snprintf(config_path, sizeof(config_path), "%s/config.yaml", tmpdir);
    snprintf(env_path, sizeof(env_path), "%s/.env", tmpdir);
    snprintf(db_path, sizeof(db_path), "%s/session.db", tmpdir);
    snprintf(vault_path, sizeof(vault_path), "%s/vault.dat", tmpdir);
    snprintf(cron_path, sizeof(cron_path), "%s/cron_store.json", tmpdir);

    /* Create test files with insecure permissions (0644) */
    chmod(tmpdir, 0755);  /* intentionally open */
    create_file(config_path, 0644);
    char vault_dir[512];
    snprintf(vault_dir, sizeof(vault_dir), "%s/data", tmpdir);
    mkdir(vault_dir, 0700);
    snprintf(vault_path, sizeof(vault_path), "%s/vault.dat", vault_dir);
    create_file(vault_path, 0644);
    snprintf(cron_path, sizeof(cron_path), "%s/cron_store.json", tmpdir);
    create_file(cron_path, 0644);

    /* Also create vaults in ~/.hermes/ and ~/.slermes/ locations */
    char vault_slermes[512], vault_hermes[512];
    snprintf(vault_hermes, sizeof(vault_hermes), "%s/.hermes/vault.dat", tmpdir);
    snprintf(vault_slermes, sizeof(vault_slermes), "%s/.slermes/vault.dat", tmpdir);
    create_file(vault_hermes, 0644);
    create_file(vault_slermes, 0644);

    printf("\n--- Basic hardening ---\n");

    /* Apply hardening */
    hermes_file_permissions_harden(tmpdir, db_path, cron_path, geteuid());

    /* Verify: home dir → 0700 */
    {
        struct stat st;
        stat(tmpdir, &st);
        TEST("home dir 0700", (st.st_mode & 0777) == 0700);
    }

    /* Verify: config.yaml → 0600 */
    {
        struct stat st;
        stat(config_path, &st);
        TEST("config.yaml 0600", (st.st_mode & 0777) == 0600);
    }

    /* Verify: .env → 0600 */
    {
        struct stat st;
        stat(env_path, &st);
        TEST(".env 0600", (st.st_mode & 0777) == 0600);
    }

    /* Verify: session DB → 0600 */
    {
        struct stat st;
        stat(db_path, &st);
        TEST("session DB 0600", (st.st_mode & 0777) == 0600);
    }

    /* Verify: vault.dat → 0600 */
    {
        struct stat st;
        stat(vault_path, &st);
        TEST("vault.dat 0600", (st.st_mode & 0777) == 0600);
    }

    /* Verify: cron store → 0600 */
    {
        struct stat st;
        stat(cron_path, &st);
        TEST("cron store 0600", (st.st_mode & 0777) == 0600);
    }

    /* Verify: ~/.hermes/vault.dat found and hardened */
    {
        struct stat st;
        if (stat(vault_hermes, &st) == 0)
            TEST("~/.hermes/vault.dat 0600", (st.st_mode & 0777) == 0600);
        else
            printf("SKIP: ~/.hermes/vault.dat not found\n");
    }

    /* Verify: ~/.slermes/vault.dat found and hardened */
    {
        struct stat st;
        if (stat(vault_slermes, &st) == 0)
            TEST("~/.slermes/vault.dat 0600", (st.st_mode & 0777) == 0600);
        else
            printf("SKIP: ~/.slermes/vault.dat not found\n");
    }

    printf("\n--- Root-skip behavior ---\n");

    /* Recreate with open perms */
    chmod(tmpdir, 0755);
    chmod(config_path, 0644);

    /* Call with uid 0 (simulating root) — should skip all hardening */
    hermes_file_permissions_harden(tmpdir, NULL, NULL, 0);

    /* Verify: NOT hardened when root */
    {
        struct stat st;
        stat(tmpdir, &st);
        TEST("root skips home dir hardening", (st.st_mode & 0777) == 0755);
    }
    {
        struct stat st;
        stat(config_path, &st);
        TEST("root skips config hardening", (st.st_mode & 0777) == 0644);
    }

    printf("\n--- NULL/edge case safety ---\n");

    /* NULL home */
    hermes_file_permissions_harden(NULL, NULL, NULL, geteuid());
    TEST("NULL home no crash", 1);

    /* Empty strings */
    hermes_file_permissions_harden("", "", "", geteuid());
    TEST("empty strings no crash", 1);

    /* Non-existent paths — should stat fail gracefully */
    hermes_file_permissions_harden("/nonexistent/path", "/nonexistent/db", "/nonexistent/cron", geteuid());
    TEST("non-existent paths no crash", 1);

    /* Cleanup */
    unlink(config_path);
    unlink(env_path);
    unlink(db_path);
    unlink(vault_path);
    unlink(cron_path);
    unlink(vault_hermes);
    unlink(vault_slermes);
    rmdir(tmpdir);

    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
