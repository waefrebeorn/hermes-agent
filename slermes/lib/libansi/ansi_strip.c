/*
 * ansi_strip.c — Strip ANSI escape sequences (ECMA-48) from text.
 * Port of Python tools/ansi_strip.py.
 *
 * State machine parsing covers:
 *   - CSI sequences: ESC [ params* intermediates* final-byte
 *   - OSC sequences: ESC ] ... (BEL or ESC \ terminator)
 *   - DCS/SOS/PM/APC: ESC P/X/^/_ ... ESC \
 *   - nF (2-byte): ESC 0x20-0x2F + 0x30-0x7E
 *   - Fp/Fe/Fs (1-byte): ESC 0x30-0x7E
 *   - 8-bit CSI (0x9B): params* intermediates* final-byte
 *   - 8-bit OSC (0x9D): ... BEL or 0x9C
 *   - C1 controls (0x80-0x9F): single-byte (except 0x9B, 0x9D handled above)
 *
 * MIT License — WuBu Hermes Project
 */

#include "ansi_strip.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* --- State machine states --- */
typedef enum {
    ST_NORMAL,         /* Copying normal text */
    ST_ESC,            /* Saw ESC (0x1B) — expecting next byte */
    ST_CSI_PARAMS,     /* ESC [ ... — collecting parameter bytes */
    ST_CSI_INTERMED,   /* ESC [ params? intermediates? — before final byte */
    ST_OSC,            /* ESC ] ... — collecting OSC string */
    ST_STRING_ESC,     /* ESC P/X/^/_ ... — collecting DCS/SOS/PM/APC string */
    ST_NF_PARAM,       /* ESC 0x20-0x2F ... — nF intermediate bytes */
    ST_C1_CSI,         /* 0x9B ... — 8-bit CSI params/intermediates/final */
    ST_C1_OSC,         /* 0x9D ... — 8-bit OSC, awaiting terminator */
} strip_state_t;

/* --- Byte classification helpers --- */

static inline bool is_csi_param(uint8_t b) {
    return b >= 0x30 && b <= 0x3F;  /* digits, ; : < = > ? */
}

static inline bool is_intermediate(uint8_t b) {
    return b >= 0x20 && b <= 0x2F;  /* space through / */
}

static inline bool is_final_byte(uint8_t b) {
    return b >= 0x40 && b <= 0x7E;  /* @ through ~ */
}

static inline bool is_escape(const char *s, size_t pos, size_t len) {
    /* Check for ESC within the string */
    if (pos < len && (unsigned char)s[pos] == 0x1B) return true;
    return false;
}

/* --- Fast-path check --- */
bool ansi_has_escape(const char *text) {
    if (!text) return false;
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == 0x1B || (c >= 0x80 && c <= 0x9F)) return true;
    }
    return false;
}

/* --- Main strip function (into caller buffer) --- */
char *ansi_strip_buf(const char *text, char *buf, size_t buf_size) {
    if (!text || !buf || buf_size == 0) return NULL;

    /* Fast path: no escape bytes, just copy */
    if (!ansi_has_escape(text)) {
        size_t len = strlen(text);
        if (len >= buf_size) {
            if (buf_size > 0) buf[0] = '\0';
            return NULL;
        }
        memcpy(buf, text, len + 1);
        return buf;
    }

    /* State machine strip */
    strip_state_t state = ST_NORMAL;
    size_t src_i = 0;
    size_t dst_i = 0;
    size_t text_len = strlen(text);

    while (src_i < text_len && dst_i < buf_size - 1) {
        unsigned char c = (unsigned char)text[src_i];

        switch (state) {
            case ST_NORMAL:
                if (c == 0x1B) {
                    state = ST_ESC;
                    src_i++;
                } else if (c == 0x9B) {
                    /* 8-bit CSI */
                    state = ST_C1_CSI;
                    src_i++;
                } else if (c == 0x9D) {
                    /* 8-bit OSC */
                    state = ST_C1_OSC;
                    src_i++;
                } else if (c >= 0x80 && c <= 0x9F) {
                    /* Other 8-bit C1 controls — skip single byte */
                    src_i++;
                } else {
                    buf[dst_i++] = (char)c;
                    src_i++;
                }
                break;

            case ST_ESC:
                /* After ESC: dispatch on next byte */
                if (c == '[') {
                    state = ST_CSI_PARAMS;
                } else if (c == ']') {
                    state = ST_OSC;
                } else if (c == 'P' || c == 'X' || c == '^' || c == '_') {
                    /* DCS (P), SOS (X), PM (^), APC (_) */
                    state = ST_STRING_ESC;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* nF escape — one or more intermediate bytes */
                    state = ST_NF_PARAM;
                } else {
                    /* Fp/Fe/Fs: ESC followed by a single byte 0x30-0x7E,
                     * or any other byte — consume ESC and this byte,
                     * but only count it as consumed if it's in range.
                     * If not, it's data loss, but ansi_strip is lossy by design. */
                    state = ST_NORMAL;
                }
                src_i++;
                break;

            case ST_CSI_PARAMS:
                if (is_csi_param(c)) {
                    src_i++; /* still params */
                } else if (is_intermediate(c)) {
                    state = ST_CSI_INTERMED;
                    src_i++;
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL; /* CSI complete */
                    src_i++;
                } else {
                    /* Invalid byte — abort sequence, start fresh */
                    state = ST_NORMAL;
                }
                break;

            case ST_CSI_INTERMED:
                if (is_intermediate(c)) {
                    src_i++; /* still intermediates */
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL; /* CSI complete */
                    src_i++;
                } else {
                    /* Invalid — abort */
                    state = ST_NORMAL;
                }
                break;

            case ST_OSC:
                /* Collect until BEL (0x07) or ESC \ (ST) */
                if (c == 0x07) {
                    state = ST_NORMAL; /* BEL terminator */
                    src_i++;
                } else if (c == 0x1B) {
                    /* Could be ST (ESC \) or embedded escape */
                    /* Peek forward for \ (0x5C) */
                    if (src_i + 1 < text_len && text[src_i + 1] == '\\') {
                        state = ST_NORMAL; /* ST terminator: ESC \ */
                        src_i += 2; /* skip ESC + \ */
                    } else {
                        /* Embedded escape inside OSC — skip */
                        src_i++;
                    }
                } else {
                    src_i++; /* regular OSC content, skip */
                }
                break;

            case ST_STRING_ESC:
                /* DCS/SOS/PM/APC: skip until ESC \ */
                if (c == 0x1B) {
                    if (src_i + 1 < text_len && text[src_i + 1] == '\\') {
                        state = ST_NORMAL; /* ST terminator */
                        src_i += 2;
                    } else {
                        src_i++;
                    }
                } else {
                    src_i++;
                }
                break;

            case ST_NF_PARAM:
                /* nF: one or more 0x20-0x2F bytes, then 0x30-0x7E */
                if (c >= 0x20 && c <= 0x2F) {
                    src_i++; /* still intermediates */
                } else if (c >= 0x30 && c <= 0x7E) {
                    state = ST_NORMAL; /* complete */
                    src_i++;
                } else {
                    /* Invalid byte — abort */
                    state = ST_NORMAL;
                }
                break;

            case ST_C1_CSI:
                /* 8-bit CSI: params* intermediates* final-byte */
                if (is_csi_param(c)) {
                    src_i++;
                } else if (is_intermediate(c)) {
                    src_i++;
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL;
                    src_i++;
                } else {
                    state = ST_NORMAL;
                }
                break;

            case ST_C1_OSC:
                /* 8-bit OSC: skip until BEL or 0x9C (ST) */
                if (c == 0x07 || c == 0x9C) {
                    state = ST_NORMAL;
                    src_i++;
                } else {
                    src_i++;
                }
                break;

            default:
                /* Should never reach — safety reset */
                state = ST_NORMAL;
                break;
        }
    }

    buf[dst_i] = '\0';
    return buf;
}

/* --- Allocating wrapper --- */
char *ansi_strip(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    return ansi_strip_buf(text, buf, len + 1);
}
