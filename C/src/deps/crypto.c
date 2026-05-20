/*
 * crypto.c — OpenSSL wrappers for Hermes C.
 *
 * Provides: SHA-256 hashing, HMAC-SHA256, base64, random bytes.
 * Uses libssl/libcrypto (available on system).
 */

#include "hermes_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OpenSSL */
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/err.h>

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "crypto: OOM\n"); exit(1); }
    return p;
}

/* ================================================================
 *  SHA-256
 * ================================================================ */

bool crypto_sha256(const unsigned char *data, size_t len,
                   unsigned char out[32])
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1
           && EVP_DigestUpdate(ctx, data, len) == 1
           && EVP_DigestFinal_ex(ctx, out, NULL) == 1;
    EVP_MD_CTX_free(ctx);
    return ok;
}

/* ================================================================
 *  HMAC-SHA256
 * ================================================================ */

bool crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                        const unsigned char *data, size_t data_len,
                        unsigned char out[32])
{
    unsigned int out_len = 32;
    HMAC(EVP_sha256(), key, (int)key_len,
         data, data_len, out, &out_len);
    return out_len == 32;
}

/* ================================================================
 *  Base64
 * ================================================================ */

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *crypto_base64_encode(const unsigned char *data, size_t len) {
    size_t out_len = 4 * ((len + 2) / 3) + 1;
    char *out = (char *)xmalloc(out_len);
    size_t i = 0, j = 0;
    while (i < len) {
        unsigned a = i < len ? data[i++] : 0;
        unsigned b = i < len ? data[i++] : 0;
        unsigned c = i < len ? data[i++] : 0;
        unsigned triplet = (a << 16) | (b << 8) | c;
        out[j++] = b64_chars[(triplet >> 18) & 0x3F];
        out[j++] = b64_chars[(triplet >> 12) & 0x3F];
        out[j++] = b64_chars[(triplet >> 6) & 0x3F];
        out[j++] = b64_chars[triplet & 0x3F];
    }
    /* Padding */
    size_t rem = len % 3;
    if (rem == 1) { out[j-2] = '='; out[j-1] = '='; }
    else if (rem == 2) out[j-1] = '=';
    out[j] = '\0';
    return out;
}

static int b64_char_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

unsigned char *crypto_base64_decode(const char *in, size_t *out_len) {
    size_t in_len = strlen(in);
    if (in_len % 4 != 0) return NULL;
    size_t pad = 0;
    if (in_len > 0 && in[in_len-1] == '=') pad++;
    if (in_len > 1 && in[in_len-2] == '=') pad++;
    size_t alloc = (in_len / 4) * 3 + 1;
    unsigned char *out = (unsigned char *)xmalloc(alloc);
    size_t j = 0;
    for (size_t i = 0; i < in_len; i += 4) {
        int a = b64_char_val(in[i]);
        int b = b64_char_val(in[i+1]);
        int c = b64_char_val(in[i+2]);
        int d = b64_char_val(in[i+3]);
        if (a < 0 || b < 0) { free(out); return NULL; }
        unsigned triplet = (a << 18) | (b << 12);
        if (c >= 0) triplet |= (c << 6);
        if (d >= 0) triplet |= d;
        out[j++] = (triplet >> 16) & 0xFF;
        if (c >= 0) out[j++] = (triplet >> 8) & 0xFF;
        if (d >= 0) out[j++] = triplet & 0xFF;
    }
    if (out_len) *out_len = j;
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  Random bytes
 * ================================================================ */

bool crypto_random_bytes(unsigned char *buf, size_t len) {
    return RAND_bytes(buf, (int)len) == 1;
}

/* ================================================================
 *  Last error string
 * ================================================================ */

const char *crypto_last_error(void) {
    return ERR_reason_error_string(ERR_get_error());
}
