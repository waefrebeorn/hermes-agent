/*
 * hash.c — C hashing library (J07: Python hashlib port).
 *
 * SHA-256, SHA-1, MD5, HMAC-SHA256 via OpenSSL EVP interface.
 * Thread-safe for distinct contexts (no global state beyond OpenSSL init).
 */

#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Generic hash via EVP — returns malloc'd binary of hash_len bytes, or NULL. */
static unsigned char *hash_digest(const unsigned char *data, size_t len,
                                   const EVP_MD *md, unsigned int hash_len) {
    if (!data || !md) return NULL;
    unsigned char *out = (unsigned char *)malloc(hash_len);
    if (!out) return NULL;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { free(out); return NULL; }

    unsigned int out_len = hash_len;
    int ok = (EVP_DigestInit_ex(ctx, md, NULL) == 1 &&
              EVP_DigestUpdate(ctx, data, len) == 1 &&
              EVP_DigestFinal_ex(ctx, out, &out_len) == 1);
    EVP_MD_CTX_free(ctx);

    if (!ok) { free(out); return NULL; }
    return out;
}

/* ================================================================
 *  SHA-256
 * ================================================================ */

unsigned char *hash_sha256(const unsigned char *data, size_t len) {
    return hash_digest(data, len, EVP_sha256(), HASH_SHA256_LEN);
}

char *hash_sha256_hex(const unsigned char *data, size_t len) {
    unsigned char *bin = hash_sha256(data, len);
    if (!bin) return NULL;
    char *hex = hash_bytes_to_hex(bin, HASH_SHA256_LEN);
    free(bin);
    return hex;
}

char *hash_sha256_file(const char *path) {
    if (!path || !*path) return NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(fp); return NULL; }

    unsigned char out[HASH_SHA256_LEN];
    unsigned int out_len = sizeof(out);
    char *result = NULL;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) goto done;

    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) goto done;
    }
    if (ferror(fp)) goto done;

    if (EVP_DigestFinal_ex(ctx, out, &out_len) == 1) {
        result = hash_bytes_to_hex(out, out_len);
    }

done:
    EVP_MD_CTX_free(ctx);
    fclose(fp);
    return result;
}

/* ================================================================
 *  SHA-1
 * ================================================================ */

unsigned char *hash_sha1(const unsigned char *data, size_t len) {
    return hash_digest(data, len, EVP_sha1(), HASH_SHA1_LEN);
}

char *hash_sha1_hex(const unsigned char *data, size_t len) {
    unsigned char *bin = hash_sha1(data, len);
    if (!bin) return NULL;
    char *hex = hash_bytes_to_hex(bin, HASH_SHA1_LEN);
    free(bin);
    return hex;
}

/* ================================================================
 *  MD5
 * ================================================================ */

unsigned char *hash_md5(const unsigned char *data, size_t len) {
    return hash_digest(data, len, EVP_md5(), HASH_MD5_LEN);
}

char *hash_md5_hex(const unsigned char *data, size_t len) {
    unsigned char *bin = hash_md5(data, len);
    if (!bin) return NULL;
    char *hex = hash_bytes_to_hex(bin, HASH_MD5_LEN);
    free(bin);
    return hex;
}

/* ================================================================
 *  HMAC-SHA256
 * ================================================================ */

unsigned char *hash_hmac_sha256(const unsigned char *key, size_t key_len,
                                 const unsigned char *data, size_t data_len) {
    if (!key || key_len == 0 || !data) return NULL;
    unsigned char *out = (unsigned char *)malloc(HASH_SHA256_LEN);
    if (!out) return NULL;

    unsigned int out_len = HASH_SHA256_LEN;
    unsigned char *result = HMAC(EVP_sha256(), key, (int)key_len,
                                  data, data_len, out, &out_len);
    if (!result) { free(out); return NULL; }
    return out;
}

char *hash_hmac_sha256_hex(const unsigned char *key, size_t key_len,
                            const unsigned char *data, size_t data_len) {
    unsigned char *bin = hash_hmac_sha256(key, key_len, data, data_len);
    if (!bin) return NULL;
    char *hex = hash_bytes_to_hex(bin, HASH_SHA256_LEN);
    free(bin);
    return hex;
}

/* ================================================================
 *  Utilities
 * ================================================================ */

char *hash_bytes_to_hex(const unsigned char *bytes, size_t len) {
    if (!bytes || len == 0) return NULL;
    char *hex = (char *)malloc(len * 2 + 1);
    if (!hex) return NULL;
    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

unsigned char *hash_hex_to_bytes(const char *hex, size_t *out_len) {
    if (!hex || !out_len) return NULL;
    size_t slen = strlen(hex);
    if (slen % 2 != 0) return NULL;
    size_t blen = slen / 2;
    unsigned char *bytes = (unsigned char *)malloc(blen);
    if (!bytes) return NULL;

    for (size_t i = 0; i < blen; i++) {
        char buf[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        char *end = NULL;
        long v = strtol(buf, &end, 16);
        if (end != buf + 2) { free(bytes); return NULL; }
        bytes[i] = (unsigned char)v;
    }
    *out_len = blen;
    return bytes;
}
