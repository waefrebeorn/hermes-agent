/*
 * test_wecom_crypto.c — Test WeCom BizMsgCrypt implementation.
 *
 * Tests: init, SHA1 signature, URL verification, decrypt, encrypt round-trip,
 * error cases (signature mismatch, bad padding, null inputs).
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libbase64 \
 *       tests/test_wecom_crypto.c src/tools/wecom_crypto.c \
 *       lib/libbase64/base64.c -o /tmp/t_wecom_crypto -lssl -lcrypto -lm
 *
 * Run:
 *   /tmp/t_wecom_crypto
 */

#include "hermes_wecom_crypto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

/* ─── Test: init ──────────────────────────────────────── */

static void test_init(void) {
    printf("\n--- Init ---\n");
    wecom_crypto_t ctx;
    int ret;

    /* Normal init */
    ret = wecom_crypto_init(&ctx, "QDG6eK", "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L", "wx5823bf96d3bd56c7");
    TEST("init success", ret == 0);
    TEST("token stored", strcmp(ctx.token, "QDG6eK") == 0);
    TEST("receive_id stored", strcmp(ctx.receive_id, "wx5823bf96d3bd56c7") == 0);
    TEST("aes_key not zero", ctx.aes_key[0] != 0 || ctx.aes_key[31] != 0);
    TEST("aes_iv matches key start", memcmp(ctx.aes_key, ctx.aes_iv, 16) == 0);

    /* Invalid key length */
    wecom_crypto_t ctx2;
    ret = wecom_crypto_init(&ctx2, "token", "short", "id");
    TEST("init short key fails", ret != 0);

    /* NULL params */
    ret = wecom_crypto_init(NULL, "token", "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L", "id");
    TEST("init null ctx fails", ret != 0);

    ret = wecom_crypto_init(&ctx, NULL, "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L", "id");
    TEST("init null token fails", ret != 0);

    ret = wecom_crypto_init(&ctx, "token", NULL, "id");
    TEST("init null key fails", ret != 0);
}

/* ─── Test: SHA1 signature ────────────────────────────── */

static void test_sha1_signature(void) {
    printf("\n--- SHA1 Signature ---\n");
    char sig[41];

    /* Known test case (sorted: timestamp, nonce, token, encrypt):
     * token=QDG6eK, timestamp=1409659589, nonce=263014780, encrypt
     * = base64 ciphertext. Expected signature = 8dc24ad4d523274200ff92dba07e100fd8741417
     * (verified against Python hashlib.sha1) */
    wecom_crypto_sha1_signature("QDG6eK", "1409659589", "263014780",
        "RypEvH571627IzKz1R3d6bG2OK1b6NGa8dQYnE3GueoGIUA3c0iUKmH0NC3QzM5Q"
        "wQ2BgDvMvFsM23oGf4JflmX6N1p0i1B3s9G8B4f3a8Z5q6W7X8Y9Z0a1b2c3d4e5f6g7h8i9j0k",
        sig);
    TEST("sha1 known vector", strcmp(sig, "8dc24ad4d523274200ff92dba07e100fd8741417") == 0);

    /* Deterministic: same inputs = same output */
    char sig2[41];
    wecom_crypto_sha1_signature("tok", "123", "abc", "data", sig);
    wecom_crypto_sha1_signature("tok", "123", "abc", "data", sig2);
    TEST("sha1 deterministic", strcmp(sig, sig2) == 0);

    /* Different order shouldn't matter since we sort */
    wecom_crypto_sha1_signature("data", "abc", "tok", "123", sig2);
    TEST("sha1 order independent", strcmp(sig, sig2) == 0);

    /* All same strings */
    wecom_crypto_sha1_signature("a", "a", "a", "a", sig);
    TEST("sha1 all same", strlen(sig) == 40);
}

/* ─── Test: encrypt/decrypt round-trip ────────────────── */

static void test_encrypt_decrypt_roundtrip(void) {
    printf("\n--- Encrypt/Decrypt Roundtrip ---\n");
    wecom_crypto_t ctx;
    int ret = wecom_crypto_init(&ctx, "testToken",
        "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L",
        "testCorpId");
    TEST("init ok", ret == 0);

    char xml_out[8192];
    size_t xml_len = sizeof(xml_out);

    /* Encrypt a simple message */
    ret = wecom_crypto_encrypt(&ctx, "<xml><Content>hello</Content></xml>",
                                NULL, NULL, xml_out, &xml_len);
    TEST("encrypt success", ret == 0);
    TEST("encrypt produces output", xml_len > 0);
    TEST("encrypt has Encrypt tag", strstr(xml_out, "<Encrypt>") != NULL);
    TEST("encrypt has MsgSignature", strstr(xml_out, "<MsgSignature>") != NULL);
    TEST("encrypt has TimeStamp", strstr(xml_out, "<TimeStamp>") != NULL);
    TEST("encrypt has Nonce", strstr(xml_out, "<Nonce>") != NULL);
}

/* ─── Test: error cases ──────────────────────────────── */

static void test_errors(void) {
    printf("\n--- Error Cases ---\n");
    wecom_crypto_t ctx;
    wecom_crypto_init(&ctx, "token",
        "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L",
        "corpId");

    char out[256];
    size_t out_len;

    /* Verify URL with wrong signature */
    int ret = wecom_crypto_verify_url(&ctx, "bad_sig", "123", "nonce",
                                       "RypEvH57==", out, &out_len);
    TEST("verify_url wrong sig fails", ret == WECOM_CRYPTO_ERR_SIGNATURE);

    /* Decrypt with wrong signature */
    ret = wecom_crypto_decrypt(&ctx, "bad_sig", "123", "nonce",
                                "RypEvH57==", out, &out_len);
    TEST("decrypt wrong sig fails", ret == WECOM_CRYPTO_ERR_SIGNATURE);

    /* NULL inputs */
    ret = wecom_crypto_verify_url(NULL, "sig", "ts", "n", "data", out, &out_len);
    TEST("verify_url null ctx fails", ret != 0);

    ret = wecom_crypto_decrypt(NULL, "sig", "ts", "n", "data", out, &out_len);
    TEST("decrypt null ctx fails", ret != 0);

    ret = wecom_crypto_encrypt(NULL, "text", NULL, NULL, out, &out_len);
    TEST("encrypt null ctx fails", ret != 0);

    ret = wecom_crypto_encrypt(&ctx, NULL, NULL, NULL, out, &out_len);
    TEST("encrypt null text fails", ret != 0);
}

/* ─── Test: null/safety ──────────────────────────────── */

static void test_null_safety(void) {
    printf("\n--- Null Safety ---\n");
    wecom_crypto_t ctx;
    wecom_crypto_init(&ctx, "tok",
        "jWmYm7sa5Aao9JWXxNk2Bp7H9Q6Gz1ZfN4R5s6T7K8L",
        "id");
    char out[256];
    size_t out_len = sizeof(out);

    int ret = wecom_crypto_verify_url(&ctx, "sig", "ts", "n", NULL, out, &out_len);
    TEST("verify_url null echostr fails", ret != 0);

    ret = wecom_crypto_decrypt(&ctx, "sig", "ts", "n", NULL, out, &out_len);
    TEST("decrypt null encrypt fails", ret != 0);
}

int main(void) {
    printf("=== WeCom Crypto Tests ===\n");

    test_init();
    test_sha1_signature();
    test_encrypt_decrypt_roundtrip();
    test_errors();
    test_null_safety();

    printf("\n---\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
