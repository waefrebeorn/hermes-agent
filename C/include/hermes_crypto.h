/**
 * @defgroup hermes_crypto Cryptography
 * @brief Cryptographic utilities.
 *
 *
Base64 encoding/decoding, SHA-256 hashing, HMAC-SHA256
signing. Lightweight wrappers around OpenSSL EVP API.
 *
 * @{
 */
#ifndef HERMES_CRYPTO_H
#define HERMES_CRYPTO_H

/*
 * hermes_crypto.h — Delegate wrapper for libcrypto.
 *
 * Includes the canonical lib/libcrypto/crypto.h declarations.
 * No shim/rename layer — the lib header IS the source of truth.
 */

#include "../lib/libcrypto/crypto.h"

/** @} */ /* end of hermes_crypto group */
#endif /* HERMES_CRYPTO_H */
