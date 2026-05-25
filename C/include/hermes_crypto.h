#ifndef HERMES_CRYPTO_H
#define HERMES_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SHA-256 hash. out must be 32 bytes. Returns false on error. */
bool crypto_sha256(const unsigned char *data, size_t len, unsigned char out[32]);

/* HMAC-SHA256. out must be 32 bytes. */
bool crypto_hmac_sha256(const unsigned char *key, size_t key_len,
                        const unsigned char *data, size_t data_len,
                        unsigned char out[32]);

/* Base64 encode. Caller must free result. */
char *crypto_base64_encode(const unsigned char *data, size_t len);

/* Base64 decode. Caller must free result. Sets *out_len. */
unsigned char *crypto_base64_decode(const char *in, size_t *out_len);

/* Fill buffer with cryptographically random bytes. */
bool crypto_random_bytes(unsigned char *buf, size_t len);

/* Base64url encode (URL-safe chars, no padding). Caller must free result. */
char *crypto_base64url_encode(const unsigned char *data, size_t len);

/* Generate PKCE code verifier: random string, 43-128 chars (RFC 7636 §4.1). */
/* Returns malloc'd string. Caller must free. Returns NULL on failure. */
char *crypto_pkce_verifier(void);

/* Generate PKCE code challenge S256: SHA-256(verifier) → base64url (RFC 7636 §4.2). */
/* Returns malloc'd string. Caller must free. Returns NULL on failure. */
char *crypto_pkce_challenge(const char *code_verifier);

/* Get last OpenSSL error string. */
const char *crypto_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CRYPTO_H */
