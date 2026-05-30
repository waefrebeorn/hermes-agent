/**
 * @file hermes_wecom_crypto.h
 * @brief WeCom BizMsgCrypt-compatible AES-CBC encryption for callback mode.
 *
 * Implements the same wire format as Tencent's official WXBizMsgCrypt SDK
 * so that WeCom can verify, encrypt, and decrypt callback payloads.
 *
 * Port of Python gateway/platforms/wecom_crypto.py (142 lines).
 */

#ifndef HERMES_WECOM_CRYPTO_H
#define HERMES_WECOM_CRYPTO_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error codes ────────────────────────────────────── */

/** Generic WeCom crypto error. */
#define WECOM_CRYPTO_ERR_GENERIC   -1
/** Signature mismatch during decrypt/verify. */
#define WECOM_CRYPTO_ERR_SIGNATURE -2
/** Decryption failed (padding, format, or receive_id mismatch). */
#define WECOM_CRYPTO_ERR_DECRYPT   -3
/** Encryption failed. */
#define WECOM_CRYPTO_ERR_ENCRYPT   -4

/* ─── WXBizMsgCrypt context ──────────────────────────── */

typedef struct {
    char token[256];
    char receive_id[256];
    unsigned char aes_key[32];  /* 256-bit AES key */
    unsigned char aes_iv[16];   /* first 16 bytes of aes_key */
} wecom_crypto_t;

/**
 * Initialize a WXBizMsgCrypt context.
 *
 * @param ctx       Uninitialized context (zeroed first)
 * @param token     WeCom callback token
 * @param encoding_aes_key  43-char base64 encoding AES key
 * @param receive_id  WeCom Corp ID
 * @return 0 on success, negative on error (invalid key length, etc.)
 */
int wecom_crypto_init(wecom_crypto_t *ctx,
                      const char *token,
                      const char *encoding_aes_key,
                      const char *receive_id);

/**
 * Verify callback URL (echostr decryption).
 *
 * Decrypts the echostr, validates signature, returns plaintext.
 *
 * @param ctx           Initialized context
 * @param msg_signature Expected SHA1 signature
 * @param timestamp     WeCom timestamp
 * @param nonce         WeCom nonce
 * @param echostr       Encrypted echo string
 * @param out           Buffer for decrypted plaintext (at least 256 bytes)
 * @param out_len       Output: length of decrypted plaintext
 * @return 0 on success, negative on error
 */
int wecom_crypto_verify_url(wecom_crypto_t *ctx,
                            const char *msg_signature,
                            const char *timestamp,
                            const char *nonce,
                            const char *echostr,
                            char *out, size_t *out_len);

/**
 * Decrypt an encrypted WeCom message.
 *
 * Verifies signature, then AES-CBC decrypts and extracts XML content.
 *
 * @param ctx           Initialized context
 * @param msg_signature Expected SHA1 signature
 * @param timestamp     WeCom timestamp
 * @param nonce         WeCom nonce
 * @param encrypt       Base64-encoded ciphertext
 * @param out           Buffer for decrypted XML content (at least 4096 bytes)
 * @param out_len       Output: length of decrypted content
 * @return 0 on success, negative on error
 */
int wecom_crypto_decrypt(wecom_crypto_t *ctx,
                         const char *msg_signature,
                         const char *timestamp,
                         const char *nonce,
                         const char *encrypt,
                         char *out, size_t *out_len);

/**
 * Encrypt a plaintext message into WeCom XML response format.
 *
 * @param ctx       Initialized context
 * @param plaintext UTF-8 plaintext to encrypt
 * @param nonce     Optional nonce (NULL = auto-generated)
 * @param timestamp Optional timestamp (NULL = current time)
 * @param out       Buffer for XML output (at least 8192 bytes)
 * @param out_len   Output: length of XML string
 * @return 0 on success, negative on error
 */
int wecom_crypto_encrypt(wecom_crypto_t *ctx,
                         const char *plaintext,
                         const char *nonce,
                         const char *timestamp,
                         char *out, size_t *out_len);

/**
 * Compute SHA1 signature: sort(token, timestamp, nonce, encrypt), SHA1 hex.
 *
 * @param token     WeCom token
 * @param timestamp Timestamp string
 * @param nonce     Nonce string
 * @param encrypt   Encrypted text (or echostr)
 * @param out       Buffer for 40-char hex digest + null (41 bytes)
 */
void wecom_crypto_sha1_signature(const char *token,
                                 const char *timestamp,
                                 const char *nonce,
                                 const char *encrypt,
                                 char out[41]);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WECOM_CRYPTO_H */
