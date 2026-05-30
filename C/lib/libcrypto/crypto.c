/*
 * crypto.c — Standalone crypto library for C.
 * OpenSSL-based. SHA-256, HMAC, Base64url, JWT HS256.
 * MIT License — WuBu Hermes Project
 */

#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

/* ================================================================
 *  SHA-256
 * ================================================================ */

void crypto_sha256(const unsigned char *data, size_t len,
                   unsigned char out[CRYPTO_SHA256_LEN])
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, out, NULL);
    EVP_MD_CTX_free(ctx);
}

/* ================================================================
 *  HMAC-SHA256
 * ================================================================ */

void crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                        const unsigned char *data, size_t data_len,
                        unsigned char out[CRYPTO_SHA256_LEN])
{
    unsigned int out_len = CRYPTO_SHA256_LEN;
    HMAC(EVP_sha256(), key, (int)key_len,
         data, data_len, out, &out_len);
}

void crypto_hmac_sha1(const unsigned char *key, size_t key_len,
                      const unsigned char *data, size_t data_len,
                      unsigned char out[20])
{
    unsigned int out_len = 20;
    HMAC(EVP_sha1(), key, (int)key_len,
         data, data_len, out, &out_len);
}

void crypto_hmac_md5(const unsigned char *key, size_t key_len,
                     const unsigned char *data, size_t data_len,
                     unsigned char out[16])
{
    unsigned int out_len = 16;
    HMAC(EVP_md5(), key, (int)key_len,
         data, data_len, out, &out_len);
}

/* ================================================================
 *  Base64url (no padding)
 * ================================================================ */

static const char b64url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char *crypto_base64url_encode(const unsigned char *data, size_t len) {
    if (!data) return NULL;
    size_t out_len = (len + 2) / 3 * 4;
    char *out = (char *)malloc(out_len + 1);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < len) {
        unsigned int a = data[i++];
        unsigned int b = i < len ? data[i++] : 0;
        unsigned int c = i < len ? data[i++] : 0;
        unsigned int triple = (a << 16) | (b << 8) | c;

        out[j++] = b64url_chars[(triple >> 18) & 0x3F];
        out[j++] = b64url_chars[(triple >> 12) & 0x3F];
        out[j++] = b64url_chars[(triple >> 6) & 0x3F];
        out[j++] = b64url_chars[triple & 0x3F];
    }

    /* Remove padding (=) */
    size_t pad = (3 - len % 3) % 3;
    out_len = out_len - pad;
    out[out_len] = '\0';
    return out;
}

static int b64url_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

unsigned char *crypto_base64url_decode(const char *input, size_t *out_len) {
    if (!input || !out_len) return NULL;
    size_t in_len = strlen(input);
    if (in_len == 0) { *out_len = 0; return NULL; }

    /* Add padding if needed */
    size_t pad = (4 - in_len % 4) % 4;
    char *padded = (char *)malloc(in_len + pad + 1);
    if (!padded) return NULL;
    memcpy(padded, input, in_len);
    for (size_t i = 0; i < pad; i++) padded[in_len + i] = '=';
    padded[in_len + pad] = '\0';

    size_t out_cap = (in_len + 3) / 4 * 3;
    unsigned char *out = (unsigned char *)malloc(out_cap + 1);
    if (!out) { free(padded); return NULL; }

    size_t j = 0;
    for (size_t i = 0; i < in_len + pad; i += 4) {
        int a = b64url_val(padded[i]);
        int b = b64url_val(padded[i+1]);
        int c = padded[i+2] == '=' ? 0 : b64url_val(padded[i+2]);
        int d = padded[i+3] == '=' ? 0 : b64url_val(padded[i+3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            free(padded); free(out); *out_len = 0; return NULL;
        }
        unsigned int triple = (unsigned int)((a << 18) | (b << 12) | (c << 6) | d);
        out[j++] = (unsigned char)(triple >> 16);
        if (padded[i+2] != '=') out[j++] = (unsigned char)(triple >> 8);
        if (padded[i+3] != '=') out[j++] = (unsigned char)triple;
    }

    free(padded);
    *out_len = j;
    return out;
}

/* ================================================================
 *  JWT HS256
 * ================================================================ */

char *crypto_jwt_encode(const char *secret, const char *payload_json) {
    if (!secret || !payload_json) return NULL;

    /* Encode header + payload */
    char *header_b64 = crypto_base64url_encode((const unsigned char *)"{\"alg\":\"HS256\",\"typ\":\"JWT\"}", 26);
    if (!header_b64) return NULL;
    char *payload_b64 = crypto_base64url_encode((const unsigned char *)payload_json, strlen(payload_json));
    if (!payload_b64) { free(header_b64); return NULL; }

    /* Build signing input */
    size_t sig_in_len = strlen(header_b64) + 1 + strlen(payload_b64);
    char *sig_input = (char *)malloc(sig_in_len + 1);
    if (!sig_input) { free(header_b64); free(payload_b64); return NULL; }
    snprintf(sig_input, sig_in_len + 1, "%s.%s", header_b64, payload_b64);

    /* Sign */
    unsigned char sig[CRYPTO_SHA256_LEN];
    crypto_hmac_sha256((const unsigned char *)secret, strlen(secret),
                       (const unsigned char *)sig_input, sig_in_len, sig);

    char *sig_b64 = crypto_base64url_encode(sig, CRYPTO_SHA256_LEN);
    if (!sig_b64) { free(header_b64); free(payload_b64); free(sig_input); return NULL; }

    /* Build final token */
    size_t token_len = strlen(sig_input) + 1 + strlen(sig_b64);
    char *token = (char *)malloc(token_len + 1);
    if (!token) { free(header_b64); free(payload_b64); free(sig_input); free(sig_b64); return NULL; }
    snprintf(token, token_len + 1, "%s.%s", sig_input, sig_b64);

    free(header_b64);
    free(payload_b64);
    free(sig_input);
    free(sig_b64);
    return token;
}

char *crypto_jwt_decode(const char *secret, const char *token, char **error) {
    if (!secret || !token) {
        if (error) *error = strdup("NULL input");
        return NULL;
    }

    /* Split token by '.' */
    const char *dot1 = strchr(token, '.');
    if (!dot1) { if (error) *error = strdup("missing first dot"); return NULL; }
    const char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) { if (error) *error = strdup("missing second dot"); return NULL; }

    size_t hdr_len = (size_t)(dot1 - token);
    size_t pay_len = (size_t)(dot2 - dot1 - 1);

    char *header_b64 = strndup(token, hdr_len);
    char *payload_b64 = strndup(dot1 + 1, pay_len);
    if (!header_b64 || !payload_b64) {
        free(header_b64); free(payload_b64);
        if (error) *error = strdup("OOM");
        return NULL;
    }

    /* Build signing input */
    size_t sig_in_len = hdr_len + 1 + pay_len;
    char *sig_input = (char *)malloc(sig_in_len + 1);
    if (!sig_input) { free(header_b64); free(payload_b64); if (error) *error = strdup("OOM"); return NULL; }
    memcpy(sig_input, token, sig_in_len);
    sig_input[sig_in_len] = '\0';

    /* Compute expected signature */
    unsigned char expected_sig[CRYPTO_SHA256_LEN];
    crypto_hmac_sha256((const unsigned char *)secret, strlen(secret),
                       (const unsigned char *)sig_input, sig_in_len, expected_sig);

    /* Get provided signature */
    const char *sig_b64 = dot2 + 1;
    size_t sig_len = 0;
    unsigned char *provided_sig = crypto_base64url_decode(sig_b64, &sig_len);

    if (!provided_sig || sig_len != CRYPTO_SHA256_LEN) {
        free(header_b64); free(payload_b64); free(sig_input); free(provided_sig);
        if (error) *error = strdup("bad signature encoding");
        return NULL;
    }

    /* Verify signature (constant-time compare) */
    int ok = 1;
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++)
        ok &= (expected_sig[i] == provided_sig[i]);

    free(sig_input);
    free(provided_sig);

    if (!ok) {
        free(header_b64); free(payload_b64);
        if (error) *error = strdup("signature mismatch");
        return NULL;
    }

    /* Decode payload */
    size_t payload_len = 0;
    unsigned char *payload = crypto_base64url_decode(payload_b64, &payload_len);
    free(header_b64);
    free(payload_b64);

    if (!payload) {
        if (error) *error = strdup("payload decode failed");
        return NULL;
    }

    char *result = (char *)malloc(payload_len + 1);
    if (!result) { free(payload); if (error) *error = strdup("OOM"); return NULL; }
    memcpy(result, payload, payload_len);
    result[payload_len] = '\0';
    free(payload);
    return result;
}

/* ================================================================
 *  Random
 * ================================================================ */

bool crypto_random_bytes(unsigned char *buf, size_t len) {
    if (!buf || len == 0) return false;
    return RAND_bytes(buf, (int)len) == 1;
}

/* ================================================================
 *  Hex encoding/decoding
 * ================================================================ */

unsigned char *crypto_hex_decode(const char *hex, size_t *out_len) {
    if (!hex || !out_len) return NULL;
    size_t in_len = strlen(hex);
    if (in_len % 2 != 0) return NULL;
    *out_len = in_len / 2;
    unsigned char *out = (unsigned char *)malloc(*out_len);
    if (!out) return NULL;
    for (size_t i = 0; i < *out_len; i++) {
        unsigned int byte;
        sscanf(hex + 2 * i, "%2x", &byte);
        out[i] = (unsigned char)byte;
    }
    return out;
}

char *crypto_hex_encode(const unsigned char *data, size_t len) {
    if (!data) return NULL;
    char *out = (char *)malloc(len * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        snprintf(out + i * 2, 3, "%02x", data[i]);
    return out;
}

/* === Simple encrypt/decrypt for credential vault (P167) === */

/* XOR-based encrypt: derive keystream from key via SHA-256,
 * XOR plaintext with keystream, output hex-encoded. */
unsigned char *hermes_encrypt(const unsigned char *plaintext, size_t pt_len,
                               const unsigned char *key, size_t *out_len) {
    if (!plaintext || !key || !out_len) return NULL;

    /* Derive keystream: SHA-256(key || counter) for each block */
    size_t blocks = (pt_len + 31) / 32;  /* SHA-256 outputs 32 bytes */
    unsigned char *keystream = (unsigned char *)calloc(blocks * 32, 1);
    unsigned char *ciphertext = (unsigned char *)malloc(pt_len);
    if (!keystream || !ciphertext) { free(keystream); free(ciphertext); return NULL; }

    for (size_t b = 0; b < blocks; b++) {
        unsigned char block_input[32 + 8];
        size_t key_len = strlen((const char *)key);
        size_t copy_len = key_len < 32 ? key_len : 32;
        memset(block_input, 0, sizeof(block_input));
        memcpy(block_input, key, copy_len);
        /* Append counter */
        for (int j = 0; j < 8; j++)
            block_input[32 + j] = (unsigned char)((b >> (j * 8)) & 0xFF);
        crypto_sha256(block_input, sizeof(block_input), keystream + b * 32);
    }

    for (size_t i = 0; i < pt_len; i++)
        ciphertext[i] = plaintext[i] ^ keystream[i];

    free(keystream);

    /* Hex-encode output */
    char *hex = crypto_hex_encode(ciphertext, pt_len);
    free(ciphertext);

    if (!hex) return NULL;
    *out_len = pt_len * 2;
    return (unsigned char *)hex;
}

unsigned char *hermes_decrypt(const unsigned char *ciphertext, size_t ct_len,
                               const unsigned char *key, size_t *out_len) {
    if (!ciphertext || !key || !out_len) return NULL;

    /* Hex decode first */
    size_t bin_len = 0;
    unsigned char *binary = crypto_hex_decode((const char *)ciphertext, &bin_len);
    if (!binary) return NULL;

    unsigned char *result = NULL;
    /* Derive keystream same way */
    size_t blocks = (bin_len + 31) / 32;
    unsigned char *keystream = (unsigned char *)calloc(blocks * 32, 1);
    if (!keystream) { free(binary); return NULL; }

    for (size_t b = 0; b < blocks; b++) {
        unsigned char block_input[32 + 8];
        size_t key_len = strlen((const char *)key);
        size_t copy_len = key_len < 32 ? key_len : 32;
        memset(block_input, 0, sizeof(block_input));
        memcpy(block_input, key, copy_len);
        for (int j = 0; j < 8; j++)
            block_input[32 + j] = (unsigned char)((b >> (j * 8)) & 0xFF);
        crypto_sha256(block_input, sizeof(block_input), keystream + b * 32);
    }

    result = (unsigned char *)malloc(bin_len + 1);
    if (!result) { free(keystream); free(binary); return NULL; }

    for (size_t i = 0; i < bin_len; i++)
        result[i] = binary[i] ^ keystream[i];
    result[bin_len] = '\0';

    free(keystream);
    free(binary);
    *out_len = bin_len;
    return result;
}

/* ================================================================
 *  AES-256-GCM Encryption (L12)
 * ================================================================ */

#define AES_GCM_IV_LEN   12
#define AES_GCM_TAG_LEN  16
#define AES_GCM_KEY_LEN  32  /* AES-256 */

unsigned char *crypto_aes_encrypt(const unsigned char *plaintext, size_t pt_len,
                                   const unsigned char *key, size_t key_len,
                                   size_t *out_len) {
    if (!plaintext || !key || key_len < 1 || !out_len) return NULL;

    /* Derive 256-bit key via SHA-256 if needed */
    unsigned char aes_key[AES_GCM_KEY_LEN];
    if (key_len >= AES_GCM_KEY_LEN) {
        memcpy(aes_key, key, AES_GCM_KEY_LEN);
    } else {
        crypto_sha256(key, key_len, aes_key);
    }

    /* Generate random IV */
    unsigned char iv[AES_GCM_IV_LEN];
    if (RAND_bytes(iv, AES_GCM_IV_LEN) != 1) return NULL;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return NULL;

    int ok = 0;
    unsigned char *result = NULL;
    size_t result_len = 0;

    do {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_LEN, NULL) != 1) break;
        if (EVP_EncryptInit_ex(ctx, NULL, NULL, aes_key, iv) != 1) break;

        /* Allocate output: IV + ciphertext (max pt_len + block) + tag */
        size_t alloc = AES_GCM_IV_LEN + pt_len + 16 + AES_GCM_TAG_LEN;
        result = (unsigned char *)calloc(alloc, 1);
        if (!result) break;

        /* Write IV at start */
        memcpy(result, iv, AES_GCM_IV_LEN);

        /* Encrypt plaintext */
        int outl = 0;
        unsigned char *ct_buf = result + AES_GCM_IV_LEN;
        if (EVP_EncryptUpdate(ctx, ct_buf, &outl, plaintext, (int)pt_len) != 1) break;
        int ct_len = outl;

        if (EVP_EncryptFinal_ex(ctx, ct_buf + ct_len, &outl) != 1) break;
        ct_len += outl;
        result_len = AES_GCM_IV_LEN + (size_t)ct_len;

        /* Get authentication tag */
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_LEN,
                                 result + result_len) != 1) break;
        result_len += AES_GCM_TAG_LEN;

        ok = 1;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        free(result);
        result = NULL;
        result_len = 0;
    }

    *out_len = result_len;
    return result;
}

unsigned char *crypto_aes_decrypt(const unsigned char *input, size_t input_len,
                                   const unsigned char *key, size_t key_len,
                                   size_t *out_len) {
    if (!input || !key || key_len < 1 || !out_len) return NULL;
    if (input_len < AES_GCM_IV_LEN + AES_GCM_TAG_LEN + 1) return NULL;

    /* Derive 256-bit key */
    unsigned char aes_key[AES_GCM_KEY_LEN];
    if (key_len >= AES_GCM_KEY_LEN) {
        memcpy(aes_key, key, AES_GCM_KEY_LEN);
    } else {
        crypto_sha256(key, key_len, aes_key);
    }

    const unsigned char *iv = input;
    const unsigned char *ct = input + AES_GCM_IV_LEN;
    size_t ct_len = input_len - AES_GCM_IV_LEN - AES_GCM_TAG_LEN;
    const unsigned char *tag = input + input_len - AES_GCM_TAG_LEN;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return NULL;

    int ok = 0;
    unsigned char *plaintext = NULL;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_GCM_IV_LEN, NULL) != 1) break;
        if (EVP_DecryptInit_ex(ctx, NULL, NULL, aes_key, iv) != 1) break;

        plaintext = (unsigned char *)calloc(ct_len + 16, 1);
        if (!plaintext) break;

        int outl = 0;
        if (EVP_DecryptUpdate(ctx, plaintext, &outl, ct, (int)ct_len) != 1) break;
        int pt_len = outl;

        /* Set expected tag before Final */
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_LEN,
                                 (void *)tag) != 1) break;

        if (EVP_DecryptFinal_ex(ctx, plaintext + pt_len, &outl) != 1) {
            /* Authentication failed — wrong key or tampered data */
            break;
        }
        pt_len += outl;

        *out_len = (size_t)pt_len;
        ok = 1;
    } while (0);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        free(plaintext);
        plaintext = NULL;
        *out_len = 0;
    }

    return plaintext;
}

/* ================================================================
 *  MD5 (hex string)
 * ================================================================ */

char *crypto_md5_hex(const unsigned char *data, size_t len)
{
    unsigned char md5_out[16];
    unsigned int md5_len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;

    int ok = 0;
    do {
        if (EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1) break;
        if (EVP_DigestUpdate(ctx, data, len) != 1) break;
        if (EVP_DigestFinal_ex(ctx, md5_out, &md5_len) != 1) break;
        ok = 1;
    } while (0);

    EVP_MD_CTX_free(ctx);

    if (!ok) return NULL;

    return crypto_hex_encode(md5_out, 16);
}
