/*
 * test_crypto.c — Smoke test for crypto library (SHA-256, base64url, JWT).
 * Requires linking: -lssl -lcrypto
 */
#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Test 1: SHA-256 hash */
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)"hello", 5, hash);
    const unsigned char expected[32] = {
        0x2c,0xf2,0x4d,0xba,0x5f,0xb0,0xa3,0x0e,
        0x26,0xe8,0x3b,0x2a,0xc5,0xb9,0xe2,0x9e,
        0x1b,0x16,0x1e,0x5c,0x1f,0xa7,0x42,0x5e,
        0x73,0x04,0x33,0x62,0x93,0x8b,0x98,0x24
    };
    TEST("crypto_sha256 length correct", sizeof(hash) == CRYPTO_SHA256_LEN);
    TEST("crypto_sha256 correct", memcmp(hash, expected, 32) == 0);

    /* Test 2: base64url encode */
    char *b64 = crypto_base64url_encode((const unsigned char *)"hello", 5);
    TEST("crypto_base64url_encode non-NULL", b64 != NULL);
    if (b64) {
        TEST("base64url correct", strcmp(b64, "aGVsbG8") == 0);
        free(b64);
    }
    char *b64empty = crypto_base64url_encode((const unsigned char *)"", 0);
    TEST("crypto_base64url_encode empty", b64empty != NULL && strlen(b64empty) == 0);
    free(b64empty);

    /* Test 3: base64url decode roundtrip */
    char *enc = crypto_base64url_encode((const unsigned char *)"test data", 9);
    if (enc) {
        size_t dec_len = 0;
        unsigned char *dec = crypto_base64url_decode(enc, &dec_len);
        TEST("crypto_base64url_decode non-NULL", dec != NULL);
        if (dec) {
            TEST("base64url decode length", dec_len == 9);
            TEST("base64url decode roundtrip", memcmp(dec, "test data", 9) == 0);
            free(dec);
        }
        free(enc);
    }

    /* Test 4: JWT encode/decode */
    char *token = crypto_jwt_encode("secret", "{\"sub\":\"123\",\"name\":\"test\"}");
    TEST("crypto_jwt_encode non-NULL", token != NULL);
    if (token) {
        int dots = 0;
        for (char *p = token; *p; p++) if (*p == '.') dots++;
        TEST("JWT has 3 parts", dots == 2);
        char *payload = crypto_jwt_decode("secret", token, NULL);
        TEST("crypto_jwt_decode non-NULL", payload != NULL);
        if (payload) {
            TEST("JWT payload correct", strstr(payload, "\"sub\":\"123\"") != NULL);
            free(payload);
        }
        char *bad_payload = crypto_jwt_decode("wrong-secret", token, NULL);
        TEST("crypto_jwt_decode wrong secret", bad_payload == NULL);
        free(token);
    }

    /* Test 5: random bytes */
    unsigned char buf[16];
    bool ok = crypto_random_bytes(buf, sizeof(buf));
    TEST("crypto_random_bytes success", ok == true);

    /* Test 6-9: AES-256-GCM */
    {
    const unsigned char *plaintext = (const unsigned char *)"Hello, AES-256-GCM!";
    size_t pt_len = strlen((const char *)plaintext);
    const unsigned char *key = (const unsigned char *)"test-key-32-bytes-for-aes-256-gcm!!";

    size_t enc_len = 0;
    unsigned char *enc = crypto_aes_encrypt(plaintext, pt_len, key, strlen((const char *)key), &enc_len);
    if (!enc) { printf("  FAIL: AES encrypt returned NULL\n"); return 1; }
    if (enc_len != 12 + pt_len + 16) { printf("  FAIL: AES encrypt bad length %zu\n", enc_len); free(enc); return 1; }
    printf("  PASS: AES encrypt non-NULL with correct length\n");

    size_t dec_len = 0;
    unsigned char *dec = crypto_aes_decrypt(enc, enc_len, key, strlen((const char *)key), &dec_len);
    if (!dec) { printf("  FAIL: AES decrypt returned NULL\n"); free(enc); return 1; }
    if (dec_len != pt_len || memcmp(dec, plaintext, pt_len) != 0) {
        printf("  FAIL: AES decrypt mismatch (%zu vs %zu)\n", dec_len, pt_len);
        free(enc); free(dec); return 1;
    }
    printf("  PASS: AES decrypt matches plaintext\n");

    /* Test with wrong key — should fail authentication */
    const unsigned char *wrong_key = (const unsigned char *)"wrong-key-different!!!!!";
    unsigned char *bad_dec = crypto_aes_decrypt(enc, enc_len, wrong_key, strlen((const char *)wrong_key), &dec_len);
    if (bad_dec != NULL) {
        printf("  FAIL: AES decrypt with wrong key should return NULL\n");
        free(bad_dec); free(enc); free(dec); return 1;
    }
    printf("  PASS: AES decrypt with wrong key returns NULL (auth fail)\n");

    /* Test with short key (key derivation) */
    const unsigned char *short_key = (const unsigned char *)"short";
    unsigned char *enc2 = crypto_aes_encrypt(plaintext, pt_len, short_key, 5, &enc_len);
    if (!enc2) { printf("  FAIL: AES encrypt short key returned NULL\n"); free(enc); free(dec); return 1; }
    printf("  PASS: AES encrypt with short key works (SHA-256 derived)\n");
    unsigned char *dec2 = crypto_aes_decrypt(enc2, enc_len, short_key, 5, &dec_len);
    if (!dec2 || dec_len != pt_len || memcmp(dec2, plaintext, pt_len) != 0) {
        printf("  FAIL: AES decrypt short key roundtrip\n");
        free(enc); free(dec); free(enc2); free(dec2); return 1;
    }
    printf("  PASS: AES decrypt short key roundtrip correct\n");
    free(enc2); free(dec2);

    /* Test null args — should return NULL gracefully */
    unsigned char *null_enc = crypto_aes_encrypt(NULL, 0, key, strlen((const char *)key), &enc_len);
    if (null_enc != NULL) {
        printf("  FAIL: AES encrypt NULL plaintext should return NULL\n");
        free(null_enc); free(enc); free(dec); return 1;
    }
    printf("  PASS: AES encrypt NULL plaintext safe\n");

    free(enc);
    free(dec);
    } /* end AES test block */

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All crypto tests PASSED");
    return failures ? 1 : 0;
}
