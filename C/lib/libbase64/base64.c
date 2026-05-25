/*
 * base64.c — C base64 encoding/decoding library (RFC 4648).
 * Thread-safe. All returned buffers are malloc'd.
 */

#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Tables
 * ================================================================ */

static const char ENC_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const signed char DEC_TABLE[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59, 60,61,-1,-1,-1, 0,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6,  7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22, 23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32, 33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48, 49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
};

/* ================================================================
 *  Size helpers
 * ================================================================ */

size_t base64_encoded_len(size_t data_len) {
    return ((data_len + 2) / 3) * 4 + 1; /* includes null */
}

size_t base64_decoded_len(size_t encoded_len) {
    return (encoded_len / 4 + 1) * 3 + 1;
}

/* ================================================================
 *  Encode
 * ================================================================ */

char *base64_encode(const unsigned char *data, size_t len) {
    return base64_encode_nopad(data, len);
}

char *base64_encode_nopad(const unsigned char *data, size_t len) {
    if (!data) return NULL;
    if (len == 0) return strdup("");
    size_t cap = base64_encoded_len(len) + 4;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    size_t o = 0, i = 0;
    while (i + 2 < len) { /* full 3-byte blocks */
        unsigned a = data[i], b = data[i+1], c = data[i+2];
        out[o++] = ENC_TABLE[a >> 2];
        out[o++] = ENC_TABLE[((a & 3) << 4) | (b >> 4)];
        out[o++] = ENC_TABLE[((b & 15) << 2) | (c >> 6)];
        out[o++] = ENC_TABLE[c & 63];
        i += 3;
    }
    if (i < len) { /* 1 or 2 remaining bytes */
        unsigned a = data[i];
        out[o++] = ENC_TABLE[a >> 2];
        if (i + 1 < len) { /* 2 bytes */
            unsigned b = data[i+1];
            out[o++] = ENC_TABLE[((a & 3) << 4) | (b >> 4)];
            out[o++] = ENC_TABLE[(b & 15) << 2];
        } else { /* 1 byte */
            out[o++] = ENC_TABLE[(a & 3) << 4];
            out[o++] = '=';
        }
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/* ================================================================
 *  Decode single quad (4 chars, may include '=')
 *  Returns bytes written (1-3) or -1 on error.
 * ================================================================ */

static int decode_quad(const char q[4], unsigned char out[3]) {
    int v0 = DEC_TABLE[(unsigned char)q[0]];
    int v1 = DEC_TABLE[(unsigned char)q[1]];
    if (v0 < 0 || v1 < 0) return -1;
    out[0] = (unsigned char)((v0 << 2) | (v1 >> 4));
    if (q[2] == '=') return 1;   /* 1 data byte */
    int v2 = DEC_TABLE[(unsigned char)q[2]];
    if (v2 < 0) return -1;
    out[1] = (unsigned char)((v1 << 4) | (v2 >> 2));
    if (q[3] == '=') return 2;   /* 2 data bytes */
    int v3 = DEC_TABLE[(unsigned char)q[3]];
    if (v3 < 0) return -1;
    out[2] = (unsigned char)((v2 << 6) | v3);
    return 3;                     /* 3 data bytes */
}

/* ================================================================
 *  Decode
 * ================================================================ */

unsigned char *base64_decode(const char *encoded, size_t *out_len) {
    if (!encoded || !out_len) { if (out_len) *out_len = 0; return NULL; }
    size_t len = strlen(encoded);
    if (len == 0) { *out_len = 0; return (unsigned char *)strdup(""); }

    size_t padded_len = len;
    if (padded_len % 4 != 0)
        padded_len = ((padded_len / 4) + 1) * 4;

    /* Build padded buffer — allocate enough space for padding */
    char *padded = (char *)malloc(padded_len + 1);
    if (!padded) { *out_len = 0; return NULL; }
    memcpy(padded, encoded, len);
    if (padded_len > len)
        memset(padded + len, '=', padded_len - len);
    padded[padded_len] = '\0';

    /* Normalize URL-safe -> standard */
    size_t quads = padded_len / 4;
    unsigned char *out = (unsigned char *)malloc(quads * 3 + 1);
    if (!out) { free(padded); *out_len = 0; return NULL; }

    size_t o = 0;
    for (size_t q = 0; q < quads; q++) {
        char quad[4];
        memcpy(quad, padded + q * 4, 4);
        /* Normalize URL-safe chars in-place */
        for (int i = 0; i < 4; i++) {
            if (quad[i] == '-') quad[i] = '+';
            if (quad[i] == '_') quad[i] = '/';
        }

        unsigned char block[3];
        int n = decode_quad(quad, block);
        if (n < 0) { free(padded); free(out); *out_len = 0; return NULL; }
        memcpy(out + o, block, (size_t)n);
        o += (size_t)n;
    }

    free(padded);
    out[o] = '\0';
    *out_len = o;
    return out;
}

/* ================================================================
 *  URL-safe variants
 * ================================================================ */

char *base64url_encode(const unsigned char *data, size_t len) {
    char *s = base64_encode(data, len);
    if (!s) return NULL;
    size_t slen = strlen(s);
    while (slen > 0 && s[slen - 1] == '=') s[--slen] = '\0';
    for (size_t i = 0; i < slen; i++) {
        if (s[i] == '+') s[i] = '-';
        if (s[i] == '/') s[i] = '_';
    }
    return s;
}

unsigned char *base64url_decode(const char *encoded, size_t *out_len) {
    return base64_decode(encoded, out_len);
}

/* ================================================================
 *  Validation
 * ================================================================ */

bool base64_is_valid(const char *encoded) {
    if (!encoded || !*encoded) return false;
    size_t len = strlen(encoded);
    if (len % 4 != 0) return false;

    bool pad = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)encoded[i];
        if (c == '=') { pad = true; continue; }
        if (pad) return false;
        if (DEC_TABLE[c] < 0 && c != '-' && c != '_') return false;
    }
    return true;
}
