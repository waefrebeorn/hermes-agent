#ifndef LIBCRYPTO_H
#define LIBCRYPTO_H

/*
 * libcrypto.h — Standalone crypto library for C.
 * OpenSSL-based. Replaces Python's cryptography + PyJWT.
 *
 * MIT License — WuBu Hermes Project
 *
 * Features:
 *   - SHA-256 hashing
 *   - HMAC-SHA256
 *   - Base64url (no padding)
 *   - JWT HS256 encode/decode
 *   - Random bytes generation
 *
 * Usage:
 *   // SHA-256 hash
 *   unsigned char hash[32];
 *   crypto_sha256("data", 4, hash);
 *
 *   // JWT encode
 *   char *token = crypto_jwt_encode("secret-key", "{\"sub\":\"123\"}");
 *   // token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjMifQ.xxx"
 *   free(token);
 *
 *   // JWT decode + verify
 *   char *payload = crypto_jwt_decode("secret-key", token, NULL);
 *   // payload = '{"sub":"123"}'
 *   free(payload);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Constants === */
#define CRYPTO_SHA256_LEN 32
#define CRYPTO_MAX_TOKEN  8192

/* === SHA-256 === */
void crypto_sha256(const unsigned char *data, size_t len, unsigned char out[CRYPTO_SHA256_LEN]);

/* === HMAC-SHA256 === */
void crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                        const unsigned char *data, size_t data_len,
                        unsigned char out[CRYPTO_SHA256_LEN]);

/* === Base64url (no padding) === */
char *crypto_base64url_encode(const unsigned char *data, size_t len);
unsigned char *crypto_base64url_decode(const char *input, size_t *out_len);

/* === JWT (HS256) === */
/* Encode JWT with HS256 signature. Returns malloc'd string. */
char *crypto_jwt_encode(const char *secret, const char *payload_json);

/* Decode and verify JWT with HS256. Returns NULL on fail. Caller free(). */
char *crypto_jwt_decode(const char *secret, const char *token, char **error);

/* === Random === */
/* Fill buffer with cryptographically secure random bytes. */
bool crypto_random_bytes(unsigned char *buf, size_t len);

/* === Utility === */
/* Convert hex string to bytes. Returns malloc'd buffer. */
unsigned char *crypto_hex_decode(const char *hex, size_t *out_len);

/* Convert bytes to hex string. Returns malloc'd string. */
char *crypto_hex_encode(const unsigned char *data, size_t len);

/* === Simple encrypt/decrypt for credential vault (P167) === */
/* XOR-based obfuscation using derived key. NOT production-grade security.
 * Uses SHA-256 to derive a keystream from the passphrase, then XORs
 * data with it. Output is hex-encoded. */
unsigned char *hermes_encrypt(const unsigned char *plaintext, size_t pt_len,
                               const unsigned char *key, size_t *out_len);
unsigned char *hermes_decrypt(const unsigned char *ciphertext, size_t ct_len,
                               const unsigned char *key, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* LIBCRYPTO_H */
