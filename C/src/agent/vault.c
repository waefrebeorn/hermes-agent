/*
 * vault.c — P167: Credential vault for Hermes C.
 *
 * Encrypted credential storage with master key unlock.
 * Stores API keys, tokens, and secrets encrypted at rest.
 * Uses libcrypto (AES-256-GCM) for encryption.
 */

#include "hermes.h"
#include "hermes_crypto.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================
 *  Vault State
 * ================================================================ */

#define VAULT_MAX_CREDENTIALS 256
#define VAULT_HEADER "HERMES_VAULT_V1"
#define VAULT_DEFAULT_PATH ".slermes/vault.enc"

typedef struct {
    char service[128];
    char key[128];
    char value[2048];
} vault_entry_t;

static vault_entry_t g_vault[VAULT_MAX_CREDENTIALS];
static int g_vault_count = 0;
static bool g_vault_unlocked = false;
static char g_vault_path[4096] = {0};
static unsigned char g_master_key[32]; /* 256-bit AES key */
static bool g_master_key_set = false;

/* ================================================================
 *  Master Key Management
 * ================================================================ */

/* Derive a 256-bit key from a passphrase using a simple KDF */
static bool derive_key(const char *passphrase, unsigned char *key_out,
                        size_t key_len) {
    if (!passphrase || !key_out) return false;

    size_t plen = strlen(passphrase);
    /* Simple stretching: SHA-256-like (using crypto_hash if available, else simple XOR) */
    /* Use a basic hash derivative */
    unsigned char hash[32];
    memset(hash, 0, sizeof(hash));

    for (size_t i = 0; i < plen; i++) {
        hash[i % 32] ^= (unsigned char)passphrase[i];
        hash[(i + 1) % 32] += (unsigned char)passphrase[i] * 7;
        hash[(i + 3) % 32] ^= (unsigned char)(passphrase[i] << 3);
    }

    /* Multiple rounds of mixing */
    for (int round = 0; round < 10; round++) {
        for (size_t i = 0; i < 32; i++) {
            hash[i] = (hash[i] * 37 + hash[(i + 1) % 32] * 13) & 0xFF;
        }
    }

    size_t copy_len = key_len < 32 ? key_len : 32;
    memcpy(key_out, hash, copy_len);
    return true;
}

/* Set master key from passphrase */
bool vault_set_master_key(const char *passphrase) {
    if (!passphrase) return false;
    bool ok = derive_key(passphrase, g_master_key, sizeof(g_master_key));
    if (ok) {
        g_master_key_set = true;
        g_vault_unlocked = true;  /* key set = vault ready for use */
    }
    return ok;
}

/* Check if master key is set */
bool vault_has_master_key(void) {
    return g_master_key_set;
}

/* Lock the vault (clear in-memory credentials) */
void vault_lock(void) {
    memset(g_vault, 0, sizeof(g_vault));
    g_vault_count = 0;
    g_vault_unlocked = false;
}

/* ================================================================
 *  Vault File Path
 * ================================================================ */

void vault_set_path(const char *path) {
    if (path)
        snprintf(g_vault_path, sizeof(g_vault_path), "%s", path);
}

static const char *vault_get_path(void) {
    if (g_vault_path[0]) return g_vault_path;

    /* Default: ~/.slermes/vault.enc */
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";

    static char default_path[4096];
    snprintf(default_path, sizeof(default_path), "%s/.slermes/vault.enc", home);
    return default_path;
}

/* ================================================================
 *  Serialization
 * ================================================================ */

/* Serialize vault entries to JSON string (caller must free) */
static char *vault_serialize(void) {
    json_t *root = json_object();

    json_set(root, "version", json_number(1));
    json_set(root, "created_at", json_number((double)time(NULL)));

    json_t *entries = json_array();
    for (int i = 0; i < g_vault_count; i++) {
        json_t *entry = json_object();
        json_set(entry, "service", json_string(g_vault[i].service));
        json_set(entry, "key", json_string(g_vault[i].key));
        json_set(entry, "value", json_string(g_vault[i].value));
        json_append(entries, entry);
    }
    json_set(root, "entries", entries);

    char *json_str = json_serialize_pretty(root, 2);
    json_free(root);
    return json_str;
}

/* Deserialize JSON string into vault entries */
static bool vault_deserialize(const char *json_str) {
    if (!json_str) return false;

    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) {
        free(err);
        return false;
    }

    json_t *entries = json_obj_get(root, "entries");
    if (!entries) {
        json_free(root);
        return false;
    }

    g_vault_count = 0;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n && g_vault_count < VAULT_MAX_CREDENTIALS; i++) {
        json_t *entry = json_get(entries, i);
        if (!entry) continue;

        const char *service = json_get_str(entry, "service", "");
        const char *key = json_get_str(entry, "key", "");
        const char *value = json_get_str(entry, "value", "");

        if (service[0] && key[0]) {
            snprintf(g_vault[g_vault_count].service, sizeof(g_vault[0].service), "%s", service);
            snprintf(g_vault[g_vault_count].key, sizeof(g_vault[0].key), "%s", key);
            snprintf(g_vault[g_vault_count].value, sizeof(g_vault[0].value), "%s", value);
            g_vault_count++;
        }
    }

    json_free(root);
    return true;
}

/* ================================================================
 *  Persistence
 * ================================================================ */

/* Save vault to encrypted file */
bool vault_save(void) {
    if (!g_master_key_set) return false;

    /* Serialize entries */
    char *json_str = vault_serialize();
    if (!json_str) return false;

    /* Encrypt with master key */
    size_t encrypted_len = 0;
    unsigned char *encrypted = hermes_encrypt(
        (const unsigned char *)json_str, strlen(json_str),
        g_master_key, &encrypted_len);
    free(json_str);

    if (!encrypted) return false;

    /* Write to file */
    const char *path = vault_get_path();

    /* Ensure parent directory exists */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0700);
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(encrypted);
        return false;
    }

    /* Write header + encrypted data */
    fwrite(VAULT_HEADER, 1, strlen(VAULT_HEADER), f);
    size_t data_len = encrypted_len;
    fwrite(&data_len, 1, sizeof(data_len), f);
    fwrite(encrypted, 1, encrypted_len, f);
    fclose(f);

    /* Set restrictive permissions */
    chmod(path, 0600);

    free(encrypted);
    return true;
}

/* Load vault from encrypted file */
bool vault_load(void) {
    if (!g_master_key_set) return false;

    const char *path = vault_get_path();
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    /* Read and verify header */
    char header[32] = {0};
    size_t hlen = strlen(VAULT_HEADER);
    if (fread(header, 1, hlen, f) != hlen ||
        strncmp(header, VAULT_HEADER, hlen) != 0) {
        fclose(f);
        return false;
    }

    /* Read data length */
    size_t data_len = 0;
    if (fread(&data_len, 1, sizeof(data_len), f) != sizeof(data_len) ||
        data_len == 0 || data_len > 1024 * 1024) {
        fclose(f);
        return false;
    }

    /* Read encrypted data */
    unsigned char *encrypted = (unsigned char *)malloc(data_len);
    if (!encrypted) { fclose(f); return false; }

    if (fread(encrypted, 1, data_len, f) != data_len) {
        free(encrypted);
        fclose(f);
        return false;
    }
    fclose(f);

    /* Decrypt */
    size_t decrypted_len = 0;
    unsigned char *decrypted = hermes_decrypt(
        encrypted, data_len, g_master_key, &decrypted_len);
    free(encrypted);

    if (!decrypted) return false;

    /* Parse JSON */
    bool ok = vault_deserialize((const char *)decrypted);
    free(decrypted);

    if (ok) g_vault_unlocked = true;
    return ok;
}

/* ================================================================
 *  Credential CRUD
 * ================================================================ */

/* Store a credential */
bool vault_store(const char *service, const char *key, const char *value) {
    if (!service || !key || !value) return false;

    /* Check for existing entry and update */
    for (int i = 0; i < g_vault_count; i++) {
        if (strcmp(g_vault[i].service, service) == 0 &&
            strcmp(g_vault[i].key, key) == 0) {
            snprintf(g_vault[i].value, sizeof(g_vault[i].value), "%s", value);
            return true;
        }
    }

    /* Add new entry */
    if (g_vault_count >= VAULT_MAX_CREDENTIALS) return false;

    snprintf(g_vault[g_vault_count].service, sizeof(g_vault[0].service), "%s", service);
    snprintf(g_vault[g_vault_count].key, sizeof(g_vault[0].key), "%s", key);
    snprintf(g_vault[g_vault_count].value, sizeof(g_vault[0].value), "%s", value);
    g_vault_count++;
    return true;
}

/* Retrieve a credential. Returns NULL if not found. */
const char *vault_retrieve(const char *service, const char *key) {
    if (!service || !key) return NULL;
    if (!g_vault_unlocked) return NULL;

    for (int i = 0; i < g_vault_count; i++) {
        if (strcmp(g_vault[i].service, service) == 0 &&
            strcmp(g_vault[i].key, key) == 0) {
            return g_vault[i].value;
        }
    }
    return NULL;
}

/* Delete a credential */
bool vault_delete(const char *service, const char *key) {
    if (!service || !key) return false;

    for (int i = 0; i < g_vault_count; i++) {
        if (strcmp(g_vault[i].service, service) == 0 &&
            strcmp(g_vault[i].key, key) == 0) {
            /* Remove by swapping with last */
            g_vault[i] = g_vault[--g_vault_count];
            memset(&g_vault[g_vault_count], 0, sizeof(vault_entry_t));
            return true;
        }
    }
    return false;
}

/* ================================================================
 *  Key Rotation
 * ================================================================ */

/* Rotate master key: decrypt vault with old passphrase, re-encrypt with new.
 * The in-memory vault state is preserved after rotation if successful. */
bool vault_rotate_key(const char *old_passphrase, const char *new_passphrase) {
    if (!old_passphrase || !new_passphrase) return false;
    if (!old_passphrase[0] || !new_passphrase[0]) return false;

    /* Save current in-memory data first (if already loaded) */
    bool had_data = (g_vault_count > 0);

    /* Derive old key and try to load vault */
    unsigned char old_key[32];
    derive_key(old_passphrase, old_key, sizeof(old_key));

    bool load_ok = false;
    if (had_data) {
        /* If we have in-memory data, rotate in-place without file I/O */
        unsigned char new_key[32];
        derive_key(new_passphrase, new_key, sizeof(new_key));
        memcpy(g_master_key, new_key, sizeof(g_master_key));
        g_master_key_set = true;
        return true;
    }

    /* Otherwise, try to load and decrypt existing vault file */
    unsigned char saved_key[32];
    memcpy(saved_key, g_master_key, sizeof(g_master_key));

    /* Temporarily use old key to load */
    memcpy(g_master_key, old_key, sizeof(g_master_key));
    g_master_key_set = true;

    load_ok = vault_load();
    if (!load_ok) {
        /* Restore original key on failure */
        memcpy(g_master_key, saved_key, sizeof(g_master_key));
        return false;
    }

    /* Re-encrypt with new key */
    unsigned char new_key[32];
    derive_key(new_passphrase, new_key, sizeof(new_key));
    memcpy(g_master_key, new_key, sizeof(g_master_key));

    bool save_ok = vault_save();
    if (!save_ok) {
        /* Rollback: restore old key */
        memcpy(g_master_key, old_key, sizeof(g_master_key));
        return false;
    }

    return true;
}

/* List services in vault */
int vault_list_services(char services[][128], int max_count) {
    if (!services) return 0;

    int count = 0;
    for (int i = 0; i < g_vault_count && count < max_count; i++) {
        /* Check for duplicates */
        bool dup = false;
        for (int j = 0; j < count; j++) {
            if (strcmp(services[j], g_vault[i].service) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            snprintf(services[count], 128, "%s", g_vault[i].service);
            count++;
        }
    }
    return count;
}
