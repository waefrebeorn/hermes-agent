/*
 * uuid.h — C UUID library (J08: Python uuid port).
 *
 * Public API for UUID generation (v4 random, v5 name-based) and
 * parsing/formatting. Thread-safe — no global state beyond /dev/urandom.
 * All returned strings are malloc'd — caller must free().
 */

#ifndef HERMES_UUID_H
#define HERMES_UUID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────────── */

#define UUID_LEN       16    /* UUID binary length (128 bits) */
#define UUID_STR_LEN   36    /* UUID string length (16 hex + 4 hyphens) */
#define UUID_STR_MAX   37    /* UUID string buffer size (incl. null) */

/* Pre-defined UUID v5 namespaces (binary, UUID_LEN bytes each) */
#define UUID_NS_DNS  "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8"
#define UUID_NS_URL  "\x6b\xa7\xb8\x11\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8"

/* ─── UUID v4 (Random) ──────────────────────────────────── */

/** uuid_v4() — Generate a UUID v4 string (36 chars + null). Caller free(). */
char *uuid_v4(void);

/** uuid_v4_bytes(out) — Generate UUID v4 into 16-byte buffer. Returns true on success. */
bool uuid_v4_bytes(uint8_t out[UUID_LEN]);

/* ─── UUID v5 (SHA-1 Name-based) ────────────────────────── */

/** uuid_v5(ns, name, name_len) — Generate UUID v5 from namespace + name. Caller free(). */
char *uuid_v5(const uint8_t ns[UUID_LEN], const char *name, size_t name_len);

/** uuid_v5_bytes(ns, name, name_len, out) — UUID v5 into 16-byte buffer. Returns true. */
bool uuid_v5_bytes(const uint8_t ns[UUID_LEN], const char *name, size_t name_len,
                   uint8_t out[UUID_LEN]);

/* ─── Parsing / Formatting ───────────────────────────────── */

/** uuid_parse(str, out) — Parse 36-char UUID string to 16 bytes. Returns true. */
bool uuid_parse(const char *str, uint8_t out[UUID_LEN]);

/** uuid_unparse(bytes) — Format 16 bytes to 36-char UUID string. Caller free(). */
char *uuid_unparse(const uint8_t bytes[UUID_LEN]);

/* ─── Validation ─────────────────────────────────────────── */

/** uuid_is_valid(str) — Check if string is a valid UUID format (36 chars, hex + hyphens). */
bool uuid_is_valid(const char *str);

/** uuid_version(str) — Extract UUID version (1-8) from string. Returns 0 if invalid. */
int  uuid_version(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_UUID_H */
