/*
 * uuid.c — C UUID library (J08: Python uuid port).
 *
 * UUID v4 generation (/dev/urandom), UUID v5 (SHA-1), parse/format.
 * Thread-safe for distinct output buffers.
 */

#include "uuid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* For UUID v5 we need SHA-1 — include libhash if available */
#ifdef HERMES_HASH_H
#  error "Do not include hash.h here — use forward decl to avoid header coupling"
#endif

/* Forward declare hash_sha1 from libhash — linked symbol resolves at runtime */
extern unsigned char *hash_sha1(const unsigned char *data, size_t len);

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Read random bytes from /dev/urandom */
static bool get_random_bytes(uint8_t *buf, size_t len) {
    if (!buf || len == 0) return false;
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    return n == len;
}

/* Set UUID version (4 or 5) and variant bits */
static void set_uuid_variant(uint8_t bytes[UUID_LEN], int version) {
    bytes[6] = (bytes[6] & 0x0f) | (uint8_t)(version << 4);  /* set version */
    bytes[8] = (bytes[8] & 0x3f) | 0x80;                      /* set RFC 4122 variant */
}

/* Format 16 bytes into UUID string buffer — caller must provide UUID_STR_MAX buffer */
static void format_uuid(const uint8_t bytes[UUID_LEN], char out[UUID_STR_MAX]) {
    snprintf(out, UUID_STR_MAX,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
}

/* ================================================================
 *  UUID v4 (Random)
 * ================================================================ */

char *uuid_v4(void) {
    uint8_t bytes[UUID_LEN];
    if (!uuid_v4_bytes(bytes)) return NULL;
    return uuid_unparse(bytes);
}

bool uuid_v4_bytes(uint8_t out[UUID_LEN]) {
    if (!out) return false;
    if (!get_random_bytes(out, UUID_LEN)) return false;
    set_uuid_variant(out, 4);
    return true;
}

/* ================================================================
 *  UUID v5 (SHA-1 Name-based)
 * ================================================================ */

char *uuid_v5(const uint8_t ns[UUID_LEN], const char *name, size_t name_len) {
    uint8_t bytes[UUID_LEN];
    if (!uuid_v5_bytes(ns, name, name_len, bytes)) return NULL;
    return uuid_unparse(bytes);
}

bool uuid_v5_bytes(const uint8_t ns[UUID_LEN], const char *name, size_t name_len,
                   uint8_t out[UUID_LEN]) {
    if (!ns || !name || !out) return false;

    /* Build SHA-1 input: namespace (16 bytes) + name bytes */
    size_t input_len = UUID_LEN + name_len;
    uint8_t *input = (uint8_t *)malloc(input_len);
    if (!input) return false;
    memcpy(input, ns, UUID_LEN);
    if (name_len > 0) memcpy(input + UUID_LEN, name, name_len);

    /* SHA-1 produces 20 bytes — we use first 16 */
    unsigned char *sha1 = hash_sha1(input, input_len);
    free(input);
    if (!sha1) return false;

    memcpy(out, sha1, UUID_LEN);
    free(sha1);

    set_uuid_variant(out, 5);
    return true;
}

/* ================================================================
 *  Parsing / Formatting
 * ================================================================ */

bool uuid_parse(const char *str, uint8_t out[UUID_LEN]) {
    if (!str || !out) return false;
    if (!uuid_is_valid(str)) return false;

    /* Parse hex digits, skipping hyphens */
    unsigned int vals[UUID_LEN];
    int n = sscanf(str,
        "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
        &vals[0], &vals[1], &vals[2], &vals[3],
        &vals[4], &vals[5], &vals[6], &vals[7],
        &vals[8], &vals[9], &vals[10], &vals[11],
        &vals[12], &vals[13], &vals[14], &vals[15]);
    if (n != UUID_LEN) return false;

    for (int i = 0; i < UUID_LEN; i++)
        out[i] = (uint8_t)vals[i];
    return true;
}

char *uuid_unparse(const uint8_t bytes[UUID_LEN]) {
    if (!bytes) return NULL;
    char *str = (char *)malloc(UUID_STR_MAX);
    if (!str) return NULL;
    format_uuid(bytes, str);
    return str;
}

/* ================================================================
 *  Validation
 * ================================================================ */

bool uuid_is_valid(const char *str) {
    if (!str) return false;

    size_t len = strlen(str);
    if (len != UUID_STR_LEN) return false;

    /* Check pattern: 8-4-4-4-12 hex digits with hyphens */
    int hyphens[] = {8, 13, 18, 23};  /* positions 8, 13, 18, 23 */
    int hi = 0;

    for (size_t i = 0; i < len; i++) {
        if (hi < 4 && (int)i == hyphens[hi]) {
            if (str[i] != '-') return false;
            hi++;
        } else {
            if (!isxdigit((unsigned char)str[i])) return false;
        }
    }
    return hi == 4;
}

int uuid_version(const char *str) {
    if (!str) return 0;
    if (!uuid_is_valid(str)) return 0;
    /* Version is at position 14 (0-indexed: hex digit after 3rd group's hyphen) */
    char c = str[14];
    if (c >= '1' && c <= '8') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}
