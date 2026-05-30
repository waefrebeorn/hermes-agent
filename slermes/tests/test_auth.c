/*
 * test_auth.c — Smoke tests for PKCE + auth store
 *
 * Tests: crypto_pkce_verifier, crypto_pkce_challenge,
 *        base64url encoding, auth store round-trip.
 *
 * Build: gcc -O2 -g -I ../include -o test_auth test_auth.c \
 *        ../src/deps/crypto.o ../src/deps/json.o ../src/deps/http.o \
 *        ../src/deps/cli_display.o ../src/provider/token_exchange.o \
 *        -lssl -lcrypto -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

/* Mute unused warnings from deps */
#include "hermes.h"
#include "hermes_auth.h"

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name) do { \
    tests++; \
    printf("  TEST %s ... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    failed++; \
    printf("FAIL: %s\n", msg); \
} while(0)

/* ================================================================
 *  PKCE primitive tests
 * ================================================================ */

void test_pkce_verifier_length(void) {
    TEST("pkce_verifier length 43-128");
    char *v = crypto_pkce_verifier();
    if (!v) { FAIL("NULL"); return; }
    size_t len = strlen(v);
    if (len >= 43 && len <= 128) PASS();
    else FAIL("length out of range");
    free(v);
}

void test_pkce_verifier_unique(void) {
    TEST("pkce_verifier uniqueness");
    char *v1 = crypto_pkce_verifier();
    char *v2 = crypto_pkce_verifier();
    if (!v1 || !v2) { FAIL("NULL"); free(v1); free(v2); return; }
    if (strcmp(v1, v2) != 0) PASS();
    else FAIL("identical verifiers");
    free(v1); free(v2);
}

void test_pkce_challenge_not_null(void) {
    TEST("pkce_challenge non-NULL output");
    char *v = crypto_pkce_verifier();
    if (!v) { FAIL("verifier NULL"); return; }
    char *c = crypto_pkce_challenge(v);
    if (c && strlen(c) > 0) PASS();
    else FAIL("challenge NULL or empty");
    free(v); free(c);
}

void test_pkce_challenge_consistent(void) {
    TEST("pkce_challenge deterministic");
    char *v = crypto_pkce_verifier();
    if (!v) { FAIL("verifier NULL"); return; }
    char *c1 = crypto_pkce_challenge(v);
    char *c2 = crypto_pkce_challenge(v);
    if (!c1 || !c2) { FAIL("challenge NULL"); free(v); free(c1); free(c2); return; }
    if (strcmp(c1, c2) == 0) PASS();
    else FAIL("not deterministic");
    free(v); free(c1); free(c2);
}

void test_pkce_challenge_is_base64url(void) {
    TEST("pkce_challenge base64url format");
    char *v = crypto_pkce_verifier();
    if (!v) { FAIL("verifier NULL"); return; }
    char *c = crypto_pkce_challenge(v);
    if (!c) { FAIL("challenge NULL"); free(v); return; }
    /* Should be ~44 chars (32 bytes → base64url = 43 chars, no padding) */
    size_t len = strlen(c);
    if (len >= 40 && len <= 50) PASS();
    else FAIL("unexpected length");
    /* No padding chars allowed in base64url */
    if (strchr(c, '=')) {
        FAIL("contains padding '='");
    }
    free(v); free(c);
}

void test_base64url_known(void) {
    TEST("base64url known vector");
    unsigned char data[] = "Hello, World!";
    char *enc = crypto_base64url_encode(data, 13);
    if (!enc) { FAIL("NULL"); return; }
    /* Standard base64: "SGVsbG8sIFdvcmxkIQ==" */
    /* base64url (no padding): "SGVsbG8sIFdvcmxkIQ" */
    if (strcmp(enc, "SGVsbG8sIFdvcmxkIQ") == 0) PASS();
    else FAIL("unexpected encoding");
    free(enc);
}

void test_base64url_no_padding(void) {
    TEST("base64url no padding for 1-byte input");
    unsigned char data[] = {0xFF};
    char *enc = crypto_base64url_encode(data, 1);
    if (!enc) { FAIL("NULL"); return; }
    /* Standard base64: /w== */
    /* base64url (no padding): _w */
    size_t len = strlen(enc);
    if (len == 2 && strchr(enc, '=') == NULL) PASS();
    else FAIL("unexpected result");
    free(enc);
}

/* ================================================================
 *  Auth store tests
 * ================================================================ */

void test_auth_store_write_read(void) {
    TEST("auth store write/read round-trip");

    /* Use a temp dir */
    const char *tmpdir = "/tmp/hermes_auth_test";
    mkdir(tmpdir, 0700);

    auth_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.provider, "xai-oauth", 63);
    entry.token.access_token = strdup("test-access-token-123");
    entry.token.refresh_token = strdup("test-refresh-token-456");
    entry.token.token_type = strdup("Bearer");
    entry.token.expires_in = 3600;
    entry.token.expires_at = 9999999999.0;

    bool ok = auth_store_save(tmpdir, &entry);
    if (!ok) {
        FAIL("save failed");
        free(entry.token.access_token);
        free(entry.token.refresh_token);
        free(entry.token.token_type);
        return;
    }

    int count = 0;
    auth_entry_t *loaded = auth_store_load(tmpdir, &count);
    if (!loaded || count != 1) {
        FAIL("load failed or wrong count");
        free(entry.token.access_token);
        free(entry.token.refresh_token);
        free(entry.token.token_type);
        auth_store_free(loaded, count);
        return;
    }

    if (strcmp(loaded[0].provider, "xai-oauth") != 0) {
        FAIL("provider name mismatch");
    } else if (strcmp(loaded[0].token.access_token, "test-access-token-123") != 0) {
        FAIL("access_token mismatch");
    } else if (strcmp(loaded[0].token.refresh_token, "test-refresh-token-456") != 0) {
        FAIL("refresh_token mismatch");
    } else if (loaded[0].token.expires_in != 3600) {
        FAIL("expires_in mismatch");
    } else {
        PASS();
    }

    free(entry.token.access_token);
    free(entry.token.refresh_token);
    free(entry.token.token_type);
    auth_store_free(loaded, count);

    /* Cleanup */
    unlink("/tmp/hermes_auth_test/auth.json");
    rmdir(tmpdir);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("PKCE Token Exchange Tests\n");
    printf("=========================\n\n");

    test_pkce_verifier_length();
    test_pkce_verifier_unique();
    test_pkce_challenge_not_null();
    test_pkce_challenge_consistent();
    test_pkce_challenge_is_base64url();
    test_base64url_known();
    test_base64url_no_padding();
    test_auth_store_write_read();

    printf("\nResults: %d/%d passed, %d failed\n", passed, tests, failed);
    return failed > 0 ? 1 : 0;
}
