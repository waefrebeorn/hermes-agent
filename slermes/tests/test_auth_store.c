/*
 * test_auth_store.c — Auth store persistence unit tests.
 *
 * Tests: load existing file, save new entry, update existing,
 * remove entry, free, edge cases (missing file, invalid JSON, NULL args).
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_auth_store.c src/provider/token_exchange.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_auth_store -lm \
 *       -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_auth_store
 */

#include "hermes_auth.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 * Helpers
 * ================================================================ */

static char *make_temp_dir(void) {
    char template[] = "/tmp/hermes_auth_test_XXXXXX";
    char *tmp = mkdtemp(template);
    return tmp ? strdup(tmp) : NULL;
}

static void remove_temp_dir(const char *path) {
    if (!path) return;
    char *json_path = NULL;
    size_t len = strlen(path) + 16;
    json_path = (char *)malloc(len);
    if (json_path) {
        snprintf(json_path, len, "%s/auth.json", path);
        unlink(json_path);
        free(json_path);
    }
    rmdir(path);
}

static int write_auth_json(const char *dir, const char *content) {
    char *path = NULL;
    size_t len = strlen(dir) + 16;
    path = (char *)malloc(len);
    if (!path) return -1;
    snprintf(path, len, "%s/auth.json", dir);

    FILE *f = fopen(path, "w");
    if (!f) { free(path); return -1; }
    size_t written = fwrite(content, 1, strlen(content), f);
    fclose(f);
    free(path);
    return (int)written;
}

/* ================================================================
 * Tests
 * ================================================================ */

static void test_load_missing_file(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load from empty dir returns NULL");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries == NULL);
    assert(count == 0);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_invalid_json(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load from invalid JSON returns NULL");
    write_auth_json(tmp, "not valid json");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries == NULL);
    assert(count == 0);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_empty_object(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load from empty JSON object returns NULL");
    write_auth_json(tmp, "{}");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries == NULL);
    assert(count == 0);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_single_entry(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load single entry");
    write_auth_json(tmp, "{\"test-provider\":{"
        "\"access_token\":\"tok123\",\"refresh_token\":\"ref456\","
        "\"token_type\":\"Bearer\",\"expires_at\":999999.0,"
        "\"expires_in\":3600}}");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries != NULL);
    assert(count == 1);
    assert(strcmp(entries[0].provider, "test-provider") == 0);
    assert(strcmp(entries[0].token.access_token, "tok123") == 0);
    assert(strcmp(entries[0].token.refresh_token, "ref456") == 0);
    assert(strcmp(entries[0].token.token_type, "Bearer") == 0);
    assert(entries[0].token.expires_at == 999999.0);
    assert(entries[0].token.expires_in == 3600);
    auth_store_free(entries, count);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_minimal_entry(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load minimal entry (access_token only)");
    write_auth_json(tmp, "{\"minimal\":{\"access_token\":\"atok\"}}");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries != NULL);
    assert(count == 1);
    assert(strcmp(entries[0].provider, "minimal") == 0);
    assert(strcmp(entries[0].token.access_token, "atok") == 0);
    assert(entries[0].token.refresh_token == NULL ||
           entries[0].token.refresh_token[0] == '\0');
    assert(strcmp(entries[0].token.token_type, "Bearer") == 0);
    assert(entries[0].token.expires_at == 0.0);
    PASS();

    auth_store_free(entries, count);
    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_multiple_entries(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load multiple entries");
    write_auth_json(tmp, "{"
        "\"prov-a\":{\"access_token\":\"a1\"},"
        "\"prov-b\":{\"access_token\":\"b2\"}}");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries != NULL);
    assert(count == 2);
    assert(strcmp(entries[0].provider, "prov-a") == 0 ||
           strcmp(entries[0].provider, "prov-b") == 0);
    assert(strcmp(entries[1].provider, "prov-a") == 0 ||
           strcmp(entries[1].provider, "prov-b") == 0);
    assert(strcmp(entries[0].provider, entries[1].provider) != 0);
    PASS();

    auth_store_free(entries, count);
    remove_temp_dir(tmp);
    free(tmp);
}

static void test_load_null_out_count(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("load with NULL out_count");
    write_auth_json(tmp, "{\"x\":{\"access_token\":\"t\"}}");
    auth_entry_t *entries = auth_store_load(tmp, NULL);
    assert(entries != NULL);
    auth_store_free(entries, 1);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

/* ================================================================
 *  Auth Store Save
 * ================================================================ */

static void test_save_new_entry(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("save new entry creates auth.json");
    auth_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.provider, "saved-prov", sizeof(entry.provider) - 1);
    entry.token.access_token = strdup("saved-token");
    entry.token.token_type = strdup("Bearer");
    entry.token.expires_at = 12345.0;

    bool ok = auth_store_save(tmp, &entry);
    assert(ok);
    free(entry.token.access_token);
    free(entry.token.token_type);

    /* Verify file was created */
    char *path = NULL;
    size_t plen = strlen(tmp) + 16;
    path = (char *)malloc(plen);
    snprintf(path, plen, "%s/auth.json", tmp);
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    assert(sz > 0);
    free(path);
    PASS();

    /* Verify we can load it back */
    TEST("save then load back");
    int count = -1;
    auth_entry_t *loaded = auth_store_load(tmp, &count);
    assert(loaded != NULL);
    assert(count == 1);
    assert(strcmp(loaded[0].provider, "saved-prov") == 0);
    assert(strcmp(loaded[0].token.access_token, "saved-token") == 0);
    assert(loaded[0].token.expires_at == 12345.0);
    auth_store_free(loaded, count);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_save_updates_existing(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    /* Save first entry */
    auth_entry_t e1;
    memset(&e1, 0, sizeof(e1));
    strncpy(e1.provider, "provider1", sizeof(e1.provider) - 1);
    e1.token.access_token = strdup("token1");
    e1.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e1);
    free(e1.token.access_token);
    free(e1.token.token_type);

    /* Save second entry */
    auth_entry_t e2;
    memset(&e2, 0, sizeof(e2));
    strncpy(e2.provider, "provider2", sizeof(e2.provider) - 1);
    e2.token.access_token = strdup("token2");
    e2.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e2);
    free(e2.token.access_token);
    free(e2.token.token_type);

    TEST("save multiple entries preserved");
    int count = -1;
    auth_entry_t *loaded = auth_store_load(tmp, &count);
    assert(loaded != NULL);
    assert(count == 2);
    auth_store_free(loaded, count);
    PASS();

    /* Update first entry */
    auth_entry_t e1u;
    memset(&e1u, 0, sizeof(e1u));
    strncpy(e1u.provider, "provider1", sizeof(e1u.provider) - 1);
    e1u.token.access_token = strdup("updated-token");
    e1u.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e1u);
    free(e1u.token.access_token);
    free(e1u.token.token_type);

    TEST("save updates existing entry");
    count = -1;
    loaded = auth_store_load(tmp, &count);
    assert(loaded != NULL);
    assert(count == 2);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(loaded[i].provider, "provider1") == 0) {
            assert(strcmp(loaded[i].token.access_token, "updated-token") == 0);
            found++;
        }
    }
    assert(found == 1);
    auth_store_free(loaded, count);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_save_null_args(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("save with NULL hermes_home returns false");
    auth_entry_t e;
    memset(&e, 0, sizeof(e));
    assert(auth_store_save(NULL, &e) == false);
    PASS();

    TEST("save with NULL entry returns false");
    assert(auth_store_save(tmp, NULL) == false);
    PASS();

    TEST("save with empty provider returns false");
    memset(&e, 0, sizeof(e));
    assert(auth_store_save(tmp, &e) == false);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

/* ================================================================
 *  Auth Store Remove
 * ================================================================ */

static void test_remove_entry(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    /* Setup: save two entries */
    auth_entry_t e1, e2;
    memset(&e1, 0, sizeof(e1));
    strncpy(e1.provider, "keep", sizeof(e1.provider) - 1);
    e1.token.access_token = strdup("keep-token");
    e1.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e1);
    free(e1.token.access_token);
    free(e1.token.token_type);

    memset(&e2, 0, sizeof(e2));
    strncpy(e2.provider, "remove-me", sizeof(e2.provider) - 1);
    e2.token.access_token = strdup("gone-token");
    e2.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e2);
    free(e2.token.access_token);
    free(e2.token.token_type);

    TEST("remove one entry");
    bool ok = auth_store_remove(tmp, "remove-me");
    assert(ok);

    int count = -1;
    auth_entry_t *loaded = auth_store_load(tmp, &count);
    assert(loaded != NULL);
    assert(count == 1);
    assert(strcmp(loaded[0].provider, "keep") == 0);
    auth_store_free(loaded, count);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

static void test_remove_non_existent(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("remove non-existent returns false (nothing to remove)");
    auth_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.provider, "only-one", sizeof(e.provider) - 1);
    e.token.access_token = strdup("tok");
    e.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e);
    free(e.token.access_token);
    free(e.token.token_type);

    bool ok = auth_store_remove(tmp, "no-such-provider");
    assert(ok == false);  /* Function returns found flag */
    PASS();

    /* Verify existing entry is still intact */
    int count = -1;
    auth_entry_t *loaded = auth_store_load(tmp, &count);
    assert(loaded != NULL);
    assert(count == 1);
    assert(strcmp(loaded[0].provider, "only-one") == 0);
    auth_store_free(loaded, count);

    remove_temp_dir(tmp);
    free(tmp);
}

/* ================================================================
 *  Auth Store Free
 * ================================================================ */

static void test_free_null(void) {
    TEST("free NULL entries does not crash");
    auth_store_free(NULL, 0);
    PASS();

    TEST("free NULL with count > 0 does not crash");
    auth_store_free(NULL, 5);
    PASS();
}

static void test_free_loaded_entries(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    TEST("free after load does not leak");
    write_auth_json(tmp, "{\"p1\":{\"access_token\":\"a\"},\"p2\":{\"access_token\":\"b\"}}");
    int count = -1;
    auth_entry_t *entries = auth_store_load(tmp, &count);
    assert(entries != NULL);
    auth_store_free(entries, count);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

/* ================================================================
 *  Auth Store — Remove last entry (edge case)
 * ================================================================ */

static void test_remove_removes_file(void) {
    char *tmp = make_temp_dir();
    assert(tmp != NULL);

    auth_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.provider, "only", sizeof(e.provider) - 1);
    e.token.access_token = strdup("t");
    e.token.token_type = strdup("Bearer");
    auth_store_save(tmp, &e);
    free(e.token.access_token);
    free(e.token.token_type);

    TEST("remove last entry leaves empty JSON");
    bool ok = auth_store_remove(tmp, "only");
    assert(ok);

    int count = -1;
    auth_entry_t *loaded = auth_store_load(tmp, &count);
    assert(loaded == NULL);
    assert(count == 0);
    PASS();

    remove_temp_dir(tmp);
    free(tmp);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Auth Store Tests ===\n\n");

    printf("--- Load ---\n");
    test_load_missing_file();
    test_load_invalid_json();
    test_load_empty_object();
    test_load_single_entry();
    test_load_minimal_entry();
    test_load_multiple_entries();
    test_load_null_out_count();

    printf("\n--- Save ---\n");
    test_save_new_entry();
    test_save_updates_existing();
    test_save_null_args();

    printf("\n--- Remove ---\n");
    test_remove_entry();
    test_remove_non_existent();
    test_remove_removes_file();

    printf("\n--- Free ---\n");
    test_free_null();
    test_free_loaded_entries();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
