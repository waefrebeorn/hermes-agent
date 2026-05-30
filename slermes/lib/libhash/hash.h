/*
 * hash.h — C hashing library (J07: Python hashlib port).
 *
 * Public API for SHA-256, SHA-1, MD5 hashing and HMAC-SHA256.
 * Wraps OpenSSL EVP interface. All output buffers are malloc'd —
 * caller must free(). Hex outputs are null-terminated.
 */

#ifndef HERMES_HASH_H
#define HERMES_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────────── */

#define HASH_SHA256_LEN  32  /* SHA-256 output bytes */
#define HASH_SHA1_LEN    20  /* SHA-1 output bytes */
#define HASH_MD5_LEN     16  /* MD5 output bytes */

/* ─── SHA-256 ────────────────────────────────────────────── */

/** hash_sha256(data, len) — SHA-256 hash, returns malloc'd binary (32 bytes). */
unsigned char *hash_sha256(const unsigned char *data, size_t len);

/** hash_sha256_hex(data, len) — SHA-256 hex string, caller free(). */
char *hash_sha256_hex(const unsigned char *data, size_t len);

/** hash_sha256_file(path) — SHA-256 of file contents, returns hex string. NULL on error. */
char *hash_sha256_file(const char *path);

/* ─── SHA-1 ──────────────────────────────────────────────── */

/** hash_sha1(data, len) — SHA-1 hash, returns malloc'd binary (20 bytes). */
unsigned char *hash_sha1(const unsigned char *data, size_t len);

/** hash_sha1_hex(data, len) — SHA-1 hex string, caller free(). */
char *hash_sha1_hex(const unsigned char *data, size_t len);

/* ─── MD5 ────────────────────────────────────────────────── */

/** hash_md5(data, len) — MD5 hash, returns malloc'd binary (16 bytes). */
unsigned char *hash_md5(const unsigned char *data, size_t len);

/** hash_md5_hex(data, len) — MD5 hex string, caller free(). */
char *hash_md5_hex(const unsigned char *data, size_t len);

/* ─── HMAC-SHA256 ────────────────────────────────────────── */

/** hash_hmac_sha256(key, key_len, data, data_len) — HMAC-SHA256, malloc'd binary (32 bytes). */
unsigned char *hash_hmac_sha256(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len);

/** hash_hmac_sha256_hex(key, key_len, data, data_len) — HMAC-SHA256 hex string. */
char *hash_hmac_sha256_hex(const unsigned char *key, size_t key_len,
                            const unsigned char *data, size_t data_len);

/* ─── Utilities ──────────────────────────────────────────── */

/** hash_bytes_to_hex(bytes, len) — Convert binary to lowercase hex. Caller free(). */
char *hash_bytes_to_hex(const unsigned char *bytes, size_t len);

/** hash_hex_to_bytes(hex, *out_len) — Convert hex string to binary. Caller free(). */
unsigned char *hash_hex_to_bytes(const char *hex, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_HASH_H */
