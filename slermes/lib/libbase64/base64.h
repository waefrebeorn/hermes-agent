/*
 * base64.h — C base64 encoding/decoding library (RFC 4648).
 *
 * Public API for base64 encode/decode with optional padding.
 * Thread-safe — no global state. All returned buffers are malloc'd.
 * Also provides base64url (URL-safe variant) for JWT/URI contexts.
 */

#ifndef HERMES_BASE64_H
#define HERMES_BASE64_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Standard Base64 (RFC 4648) ─────────────────────────── */

/** base64_encode(data, len) — Base64 encode. Caller free(). */
char *base64_encode(const unsigned char *data, size_t len);

/** base64_decode(encoded, *out_len) — Base64 decode. Caller free(). */
unsigned char *base64_decode(const char *encoded, size_t *out_len);

/** base64_encode_nopad(data, len) — Base64 encode without padding. */
char *base64_encode_nopad(const unsigned char *data, size_t len);

/* ─── Base64 URL-safe (RFC 4648 §5) ──────────────────────── */

/** base64url_encode(data, len) — URL-safe base64 (no padding). */
char *base64url_encode(const unsigned char *data, size_t len);

/** base64url_decode(encoded, *out_len) — URL-safe base64 decode. */
unsigned char *base64url_decode(const char *encoded, size_t *out_len);

/* ─── Utilities ──────────────────────────────────────────── */

/** base64_encoded_len(data_len) — Length of encoded output (including null). */
size_t base64_encoded_len(size_t data_len);

/** base64_decoded_len(encoded_len) — Max decoded length (including null). */
size_t base64_decoded_len(size_t encoded_len);

/** base64_is_valid(encoded) — Check if string is valid base64. */
bool base64_is_valid(const char *encoded);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_BASE64_H */
