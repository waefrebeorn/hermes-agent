/*
 * test_vault.c — O11: Credential encryption at rest tests
 *
 * Tests vault encrypt/decrypt lifecycle:
 *   - Set master key, store/retrieve/delete credentials
 *   - Save to encrypted file, reload, verify data survives
 *   - List services, lock/unlock
 *   - NULL safety, edge cases
 */
#include "hermes.h"
#include "hermes_crypto.h"
#include "hermes_json.h"
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

int main(void) {
    printf("=== O11: Credential encryption at rest ===\n\n");

    /* Use temp path for vault file */
    char tmpdir[] = "/tmp/hermes_vault_test_XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "FAIL: mkdtemp failed\n");
        return 1;
    }
    char vault_path[512];
    snprintf(vault_path, sizeof(vault_path), "%s/vault.enc", tmpdir);

    printf("--- Master key management ---\n");

    /* Initially no master key */
    TEST("no master key initially", !vault_has_master_key());

    /* Set master key */
    TEST("set master key", vault_set_master_key("test-passphrase-123"));
    TEST("has master key after set", vault_has_master_key());

    /* Lock vault */
    vault_lock();
    TEST("vault locked", vault_has_master_key());  /* key remains, data cleared */

    printf("\n--- Store and retrieve ---\n");

    /* Re-set key (lock only clears data, key persists) ... actually it
       doesn't clear the key. Let me verify the flow works: set key, load
       empty vault, store, retrieve. */
    vault_set_master_key("test-passphrase-123");
    vault_set_path(vault_path);

    /* Load empty vault (file doesn't exist yet, should fail gracefully) */
    TEST("load non-existent vault", !vault_load());

    /* Store credentials */
    TEST("store api_key for openai", vault_store("openai", "api_key", "sk-test123"));
    TEST("store api_key for anthropic", vault_store("anthropic", "api_key", "sk-ant-test456"));
    TEST("store token for github", vault_store("github", "token", "ghp_test789"));

    /* Retrieve */
    {
        const char *v = vault_retrieve("openai", "api_key");
        TEST("retrieve openai api_key", v && strcmp(v, "sk-test123") == 0);
    }
    {
        const char *v = vault_retrieve("anthropic", "api_key");
        TEST("retrieve anthropic api_key", v && strcmp(v, "sk-ant-test456") == 0);
    }
    {
        const char *v = vault_retrieve("github", "token");
        TEST("retrieve github token", v && strcmp(v, "ghp_test789") == 0);
    }

    /* Non-existent key */
    TEST("retrieve non-existent returns NULL", !vault_retrieve("openai", "nonexistent"));

    printf("\n--- Persistence: save and reload ---\n");

    /* Save to encrypted file */
    TEST("save vault", vault_save());
    TEST("vault file exists", access(vault_path, F_OK) == 0);

    /* Verify file is binary (encrypted), not plain JSON */
    {
        FILE *f = fopen(vault_path, "rb");
        char buf[32] = {0};
        fread(buf, 1, 16, f);
        fclose(f);
        TEST("file has header", strncmp(buf, "HERMES_VAULT_V1", 15) == 0);
        TEST("file is not plain JSON", strstr(buf, "entries") == NULL);
    }

    /* Verify file permissions are 0600 */
    {
        struct stat st;
        stat(vault_path, &st);
        TEST("vault file 0600", (st.st_mode & 0777) == 0600);
    }

    /* Lock and reload from file */
    vault_lock();
    TEST("lock clears data", vault_retrieve("openai", "api_key") == NULL);

    /* Verify load works */
    TEST("load vault from file", vault_load());
    {
        const char *v = vault_retrieve("openai", "api_key");
        TEST("reloaded openai api_key matches", v && strcmp(v, "sk-test123") == 0);
    }
    {
        const char *v = vault_retrieve("anthropic", "api_key");
        TEST("reloaded anthropic api_key matches", v && strcmp(v, "sk-ant-test456") == 0);
    }

    printf("\n--- Update existing credential ---\n");
    vault_store("openai", "api_key", "sk-updated");
    {
        const char *v = vault_retrieve("openai", "api_key");
        TEST("updated credential", v && strcmp(v, "sk-updated") == 0);
    }
    TEST("re-save after update", vault_save());

    /* Reload and verify update persisted */
    vault_lock();
    vault_load();
    {
        const char *v = vault_retrieve("openai", "api_key");
        TEST("persisted update after reload", v && strcmp(v, "sk-updated") == 0);
    }

    printf("\n--- Delete credential ---\n");
    TEST("delete github token", vault_delete("github", "token"));
    TEST("deleted credential gone", vault_retrieve("github", "token") == NULL);
    TEST("other credentials survive", vault_retrieve("openai", "api_key") != NULL);

    printf("\n--- List services ---\n");
    {
        char services[10][128];
        int count = vault_list_services(services, 10);
        TEST("list returns 2 services", count == 2);
        bool found_openai = false, found_anthropic = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(services[i], "openai") == 0) found_openai = true;
            if (strcmp(services[i], "anthropic") == 0) found_anthropic = true;
        }
        TEST("openai in services list", found_openai);
        TEST("anthropic in services list", found_anthropic);
    }

    printf("\n--- Edge cases: NULL safety ---\n");
    TEST("vault_set_master_key(NULL)", !vault_set_master_key(NULL));
    TEST("vault_store(NULL, k, v)", !vault_store(NULL, "k", "v"));
    TEST("vault_store(s, NULL, v)", !vault_store("s", NULL, "v"));
    TEST("vault_store(s, k, NULL)", !vault_store("s", "k", NULL));
    TEST("vault_retrieve(NULL, k)", vault_retrieve(NULL, "k") == NULL);
    TEST("vault_retrieve(s, NULL)", vault_retrieve("s", NULL) == NULL);
    TEST("vault_delete(NULL, k)", !vault_delete(NULL, "k"));
    TEST("vault_delete(s, NULL)", !vault_delete("s", NULL));
    TEST("vault_set_path(NULL) no crash", (vault_set_path(NULL), 1));

    printf("\n--- Edge cases: wrong key ---\n");
    {
        /* Save with one key, try to load with different key */
        vault_set_master_key("original-key");
        vault_set_path(vault_path);
        vault_store("test", "key", "secret-value");
        vault_save();
        vault_lock();

        /* Try to load with wrong key */
        vault_set_master_key("wrong-key");
        TEST("wrong key fails to decrypt", !vault_load());

        /* Original key still works */
        vault_set_master_key("original-key");
        TEST("original key reloads correctly", vault_load());
        const char *v = vault_retrieve("test", "key");
        TEST("data intact after wrong-key attempt", v && strcmp(v, "secret-value") == 0);
    }

    /* Cleanup */
    vault_lock();
    unlink(vault_path);
    rmdir(tmpdir);

    printf("\n=== Results: %s ===\n", failures ? "SOME FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
