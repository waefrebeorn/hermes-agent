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

int main(void) {
    /* Test 1: SHA-256 hash */
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)"hello", 5, hash);
    /* Known SHA-256 of "hello": 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824 */
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
        TEST("base64url correct", strcmp(b64, "aGVsbG8") == 0); /* no padding */
        free(b64);
    }

    /* Base64url of empty string */
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

    /* Test 4: JWT encode (HS256) */
    char *token = crypto_jwt_encode("secret", "{\"sub\":\"123\",\"name\":\"test\"}");
    TEST("crypto_jwt_encode non-NULL", token != NULL);
    if (token) {
        /* JWT format: header.payload.signature (3 dot-separated parts) */
        int dots = 0;
        for (char *p = token; *p; p++) if (*p == '.') dots++;
        TEST("JWT has 3 parts", dots == 2);
        TEST("JWT not empty", strlen(token) > 0);

        /* Test 5: JWT decode + verify roundtrip */
        char *payload = crypto_jwt_decode("secret", token, NULL);
        TEST("crypto_jwt_decode non-NULL", payload != NULL);
        if (payload) {
            TEST("JWT payload correct", strstr(payload, "\"sub\":\"123\"") != NULL);
            free(payload);
        }

        /* Wrong secret should fail */
        char *bad_payload = crypto_jwt_decode("wrong-secret", token, NULL);
        TEST("crypto_jwt_decode wrong secret", bad_payload == NULL);

        free(token);
    }

    /* Test 6: random bytes */
    unsigned char buf[16];
    bool ok = crypto_random_bytes(buf, sizeof(buf));
    TEST("crypto_random_bytes success", ok == true);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All crypto tests PASSED");
    return failures ? 1 : 0;
}
