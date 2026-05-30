/*
 * wecom_crypto.c — WeCom BizMsgCrypt-compatible AES-CBC encryption.
 *
 * Port of Python gateway/platforms/wecom_crypto.py (142 lines).
 * Uses OpenSSL for AES-256-CBC + SHA1, libbase64 for Base64.
 *
 * Wire format matches Tencent's official WXBizMsgCrypt SDK:
 *   Plaintext = random_16_bytes + network_byte_order_4(xml_len) + xml + receive_id
 *   PKCS7 padding (block_size=32)
 *   AES-256-CBC encryption
 *   Base64 encode
 */

#include "hermes_wecom_crypto.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <arpa/inet.h>  /* htonl, ntohl */

/* PKCS7 block size for WeCom */
#define PKCS7_BLOCK_SIZE 32

/* ─── PKCS7 padding ──────────────────────────────────── */

static void pkcs7_encode(const unsigned char *in, size_t in_len,
                         unsigned char *out, size_t *out_len)
{
    size_t pad = PKCS7_BLOCK_SIZE - (in_len % PKCS7_BLOCK_SIZE);
    if (pad == 0) pad = PKCS7_BLOCK_SIZE;
    memcpy(out, in, in_len);
    memset(out + in_len, (unsigned char)pad, pad);
    *out_len = in_len + pad;
}

static int pkcs7_decode(const unsigned char *in, size_t in_len,
                        unsigned char *out, size_t *out_len)
{
    if (in_len == 0) return WECOM_CRYPTO_ERR_DECRYPT;
    unsigned char pad = in[in_len - 1];
    if (pad < 1 || pad > PKCS7_BLOCK_SIZE)
        return WECOM_CRYPTO_ERR_DECRYPT;
    /* Verify all pad bytes match */
    for (size_t i = in_len - pad; i < in_len; i++) {
        if (in[i] != pad)
            return WECOM_CRYPTO_ERR_DECRYPT;
    }
    *out_len = in_len - pad;
    if (*out_len > 0)
        memcpy(out, in, *out_len);
    return 0;
}

/* ─── SHA1 signature ─────────────────────────────────── */

void wecom_crypto_sha1_signature(const char *token,
                                  const char *timestamp,
                                  const char *nonce,
                                  const char *encrypt,
                                  char out[41])
{
    /* Sorted list of 4 strings */
    const char *parts[4];
    parts[0] = token;
    parts[1] = timestamp;
    parts[2] = nonce;
    parts[3] = encrypt;

    /* Simple bubble sort by strcmp */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (strcmp(parts[i], parts[j]) > 0) {
                const char *tmp = parts[i];
                parts[i] = parts[j];
                parts[j] = tmp;
            }
        }
    }

    /* Concatenate */
    char buf[2048];
    size_t pos = 0;
    for (int i = 0; i < 4; i++) {
        size_t len = strlen(parts[i]);
        if (pos + len < sizeof(buf)) {
            memcpy(buf + pos, parts[i], len);
            pos += len;
        }
    }
    buf[pos] = '\0';

    /* SHA1 hex */
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)buf, pos, hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[40] = '\0';
}

/* ─── AES-256-CBC helpers ────────────────────────────── */

static int aes_cbc_decrypt(const unsigned char *key, const unsigned char *iv,
                           const unsigned char *ciphertext, size_t ct_len,
                           unsigned char *plaintext, size_t *pt_len)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return WECOM_CRYPTO_ERR_DECRYPT;

    int len = 0, total = 0;
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) ||
        1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, (int)ct_len)) {
        EVP_CIPHER_CTX_free(ctx);
        return WECOM_CRYPTO_ERR_DECRYPT;
    }
    total = len;

    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + total, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return WECOM_CRYPTO_ERR_DECRYPT;
    }
    total += len;
    *pt_len = (size_t)total;
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

static int aes_cbc_encrypt(const unsigned char *key, const unsigned char *iv,
                           const unsigned char *plaintext, size_t pt_len,
                           unsigned char *ciphertext, size_t *ct_len)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return WECOM_CRYPTO_ERR_ENCRYPT;

    int len = 0, total = 0;
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) ||
        1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int)pt_len)) {
        EVP_CIPHER_CTX_free(ctx);
        return WECOM_CRYPTO_ERR_ENCRYPT;
    }
    total = len;

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + total, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return WECOM_CRYPTO_ERR_ENCRYPT;
    }
    total += len;
    *ct_len = (size_t)total;
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

/* ─── Public API ─────────────────────────────────────── */

int wecom_crypto_init(wecom_crypto_t *ctx,
                       const char *token,
                       const char *encoding_aes_key,
                       const char *receive_id)
{
    if (!ctx || !token || !encoding_aes_key || !receive_id)
        return WECOM_CRYPTO_ERR_GENERIC;
    if (strlen(encoding_aes_key) != 43)
        return WECOM_CRYPTO_ERR_GENERIC;

    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->token, sizeof(ctx->token), "%s", token);
    snprintf(ctx->receive_id, sizeof(ctx->receive_id), "%s", receive_id);

    /* Base64 decode the 43-char key (add padding =) */
    char b64_buf[44];
    snprintf(b64_buf, sizeof(b64_buf), "%s=", encoding_aes_key);

    size_t key_len = 0;
    unsigned char *decoded = base64_decode(b64_buf, &key_len);
    if (!decoded || key_len < 32) {
        free(decoded);
        return WECOM_CRYPTO_ERR_GENERIC;
    }
    memcpy(ctx->aes_key, decoded, 32);
    memcpy(ctx->aes_iv, decoded, 16);  /* IV = first 16 bytes of key */
    free(decoded);
    return 0;
}

int wecom_crypto_verify_url(wecom_crypto_t *ctx,
                             const char *msg_signature,
                             const char *timestamp,
                             const char *nonce,
                             const char *echostr,
                             char *out, size_t *out_len)
{
    if (!ctx || !msg_signature || !timestamp || !nonce || !echostr || !out || !out_len)
        return WECOM_CRYPTO_ERR_GENERIC;

    /* Verify signature */
    char expected[41];
    wecom_crypto_sha1_signature(ctx->token, timestamp, nonce, echostr, expected);
    if (strcmp(expected, msg_signature) != 0)
        return WECOM_CRYPTO_ERR_SIGNATURE;

    /* Base64 decode */
    size_t ct_len = 0;
    unsigned char *ciphertext = base64_decode(echostr, &ct_len);
    if (!ciphertext || ct_len == 0) {
        free(ciphertext);
        return WECOM_CRYPTO_ERR_DECRYPT;
    }

    /* AES decrypt */
    unsigned char padded[8192];
    size_t padded_len = 0;
    int ret = aes_cbc_decrypt(ctx->aes_key, ctx->aes_iv,
                               ciphertext, ct_len,
                               padded, &padded_len);
    free(ciphertext);
    if (ret != 0) return ret;

    /* PKCS7 unpad */
    unsigned char unpadded[8192];
    size_t unpadded_len = 0;
    ret = pkcs7_decode(padded, padded_len, unpadded, &unpadded_len);
    if (ret != 0) return ret;

    /* Skip 16-byte random prefix */
    if (unpadded_len < 20) return WECOM_CRYPTO_ERR_DECRYPT;

    /* Extract network-order content length (4 bytes after random prefix) */
    uint32_t xml_net_len;
    memcpy(&xml_net_len, unpadded + 16, 4);
    uint32_t xml_len = ntohl(xml_net_len);

    /* Extract XML content */
    if (16 + 4 + xml_len > unpadded_len)
        return WECOM_CRYPTO_ERR_DECRYPT;

    memcpy(out, unpadded + 20, xml_len);
    out[xml_len] = '\0';
    *out_len = xml_len;

    /* Verify receive_id */
    const char *rid = (const char *)unpadded + 20 + xml_len;
    size_t rid_len = unpadded_len - (20 + xml_len);
    if (rid_len != strlen(ctx->receive_id) ||
        memcmp(rid, ctx->receive_id, rid_len) != 0)
        return WECOM_CRYPTO_ERR_DECRYPT;

    return 0;
}

int wecom_crypto_decrypt(wecom_crypto_t *ctx,
                          const char *msg_signature,
                          const char *timestamp,
                          const char *nonce,
                          const char *encrypt,
                          char *out, size_t *out_len)
{
    /* Same as verify_url but without the receive_id check at the end */
    if (!ctx || !msg_signature || !timestamp || !nonce || !encrypt || !out || !out_len)
        return WECOM_CRYPTO_ERR_GENERIC;

    char expected[41];
    wecom_crypto_sha1_signature(ctx->token, timestamp, nonce, encrypt, expected);
    if (strcmp(expected, msg_signature) != 0)
        return WECOM_CRYPTO_ERR_SIGNATURE;

    size_t ct_len = 0;
    unsigned char *ciphertext = base64_decode(encrypt, &ct_len);
    if (!ciphertext || ct_len == 0) {
        free(ciphertext);
        return WECOM_CRYPTO_ERR_DECRYPT;
    }

    unsigned char padded[8192];
    size_t padded_len = 0;
    int ret = aes_cbc_decrypt(ctx->aes_key, ctx->aes_iv,
                               ciphertext, ct_len,
                               padded, &padded_len);
    free(ciphertext);
    if (ret != 0) return ret;

    unsigned char unpadded[8192];
    size_t unpadded_len = 0;
    ret = pkcs7_decode(padded, padded_len, unpadded, &unpadded_len);
    if (ret != 0) return ret;

    if (unpadded_len < 20) return WECOM_CRYPTO_ERR_DECRYPT;

    uint32_t xml_net_len;
    memcpy(&xml_net_len, unpadded + 16, 4);
    uint32_t xml_len = ntohl(xml_net_len);

    if (16 + 4 + xml_len > unpadded_len)
        return WECOM_CRYPTO_ERR_DECRYPT;

    memcpy(out, unpadded + 20, xml_len);
    out[xml_len] = '\0';
    *out_len = xml_len;

    /* Verify receive_id */
    const char *rid = (const char *)unpadded + 20 + xml_len;
    size_t rid_len = unpadded_len - (20 + xml_len);
    if (rid_len != strlen(ctx->receive_id) ||
        memcmp(rid, ctx->receive_id, rid_len) != 0)
        return WECOM_CRYPTO_ERR_DECRYPT;

    return 0;
}

static void generate_nonce(char *out, size_t out_len)
{
    const char *alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t alen = strlen(alphabet);
    for (size_t i = 0; i < out_len - 1; i++) {
        unsigned char r = 0;
        if (1 != RAND_bytes(&r, 1)) r = (unsigned char)rand();
        out[i] = alphabet[r % alen];
    }
    out[out_len - 1] = '\0';
}

int wecom_crypto_encrypt(wecom_crypto_t *ctx,
                          const char *plaintext,
                          const char *nonce,
                          const char *timestamp,
                          char *out, size_t *out_len)
{
    if (!ctx || !plaintext || !out || !out_len)
        return WECOM_CRYPTO_ERR_GENERIC;

    char local_nonce[16];
    if (!nonce) {
        generate_nonce(local_nonce, sizeof(local_nonce));
        nonce = local_nonce;
    }

    char ts_buf[32];
    if (!timestamp) {
        snprintf(ts_buf, sizeof(ts_buf), "%ld", (long)time(NULL));
        timestamp = ts_buf;
    }

    /* Build raw payload: random_16 + network_len + plaintext + receive_id */
    unsigned char raw[8192];
    size_t raw_len = 0;

    /* 16 random bytes */
    unsigned char random_prefix[16];
    if (1 != RAND_bytes(random_prefix, 16)) {
        for (int i = 0; i < 16; i++)
            random_prefix[i] = (unsigned char)(rand() & 0xFF);
    }
    memcpy(raw, random_prefix, 16);
    raw_len = 16;

    /* Network-order content length */
    size_t text_len = strlen(plaintext);
    uint32_t net_len = htonl((uint32_t)text_len);
    memcpy(raw + raw_len, &net_len, 4);
    raw_len += 4;

    /* XML content */
    memcpy(raw + raw_len, plaintext, text_len);
    raw_len += text_len;

    /* Receive ID */
    size_t rid_len = strlen(ctx->receive_id);
    memcpy(raw + raw_len, ctx->receive_id, rid_len);
    raw_len += rid_len;

    /* PKCS7 pad */
    unsigned char padded[8192];
    size_t padded_len = 0;
    pkcs7_encode(raw, raw_len, padded, &padded_len);

    /* AES encrypt */
    unsigned char encrypted[8192];
    size_t encrypted_len = 0;
    int ret = aes_cbc_encrypt(ctx->aes_key, ctx->aes_iv,
                               padded, padded_len,
                               encrypted, &encrypted_len);
    if (ret != 0) return ret;

    /* Base64 encode */
    char *b64 = base64_encode(encrypted, encrypted_len);
    if (!b64) return WECOM_CRYPTO_ERR_ENCRYPT;

    /* Build XML response */
    char sig[41];
    wecom_crypto_sha1_signature(ctx->token, timestamp, nonce, b64, sig);

    int n = snprintf(out, *out_len,
        "<xml>\n"
        "<Encrypt><![CDATA[%s]]></Encrypt>\n"
        "<MsgSignature><![CDATA[%s]]></MsgSignature>\n"
        "<TimeStamp>%s</TimeStamp>\n"
        "<Nonce><![CDATA[%s]]></Nonce>\n"
        "</xml>",
        b64, sig, timestamp, nonce);

    free(b64);

    if (n < 0 || (size_t)n >= *out_len)
        return WECOM_CRYPTO_ERR_ENCRYPT;
    *out_len = (size_t)n;
    return 0;
}
