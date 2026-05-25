/*
 * protobuf.c — Minimal protobuf wire-format encoder/decoder for C.
 * MIT License — WuBu Hermes Project
 */

#include "protobuf.h"
#include <string.h>

int pb_decode_varint(const uint8_t *buf, size_t buf_len, uint64_t *value) {
    if (!buf || !value || buf_len == 0) return 0;
    uint64_t result = 0;
    int shift = 0;
    size_t i = 0;
    while (i < buf_len && i < 10) {
        uint8_t byte = buf[i];
        result |= ((uint64_t)(byte & 0x7F)) << shift;
        shift += 7;
        i++;
        if ((byte & 0x80) == 0) {
            *value = result;
            return (int)i;
        }
    }
    return 0;
}

int pb_parse_tag(const uint8_t *buf, size_t buf_len,
                 uint32_t *tag_no, int *wire_type) {
    if (!buf || !tag_no || !wire_type) return 0;
    uint64_t value;
    int n = pb_decode_varint(buf, buf_len, &value);
    if (n <= 0) return 0;
    *tag_no = (uint32_t)(value >> 3);
    *wire_type = (int)(value & 0x07);
    return n;
}

int pb_skip_field(const uint8_t *buf, size_t buf_len, int wire_type) {
    switch (wire_type) {
        case PB_WIRE_VARINT: {
            uint64_t tmp;
            return pb_decode_varint(buf, buf_len, &tmp);
        }
        case PB_WIRE_FIXED64:
            return 8;
        case PB_WIRE_FIXED32:
            return 4;
        case PB_WIRE_LENDELIM: {
            if (buf_len < 1) return 0;
            uint64_t len;
            int n = pb_decode_varint(buf, buf_len, &len);
            if (n <= 0) return 0;
            return n + (int)len;
        }
        default:
            return 0;
    }
}

const uint8_t *pb_find_field(const uint8_t *buf, size_t buf_len,
                              uint32_t target_tag, int target_wire,
                              size_t *out_len) {
    size_t offset = 0;
    while (offset < buf_len) {
        uint32_t tag;
        int wire;
        int n = pb_parse_tag(buf + offset, buf_len - offset, &tag, &wire);
        if (n <= 0) break;
        offset += (size_t)n;

        if (tag == target_tag && wire == target_wire) {
            switch (wire) {
                case PB_WIRE_VARINT: {
                    uint64_t v;
                    int m = pb_decode_varint(buf + offset, buf_len - offset, &v);
                    if (m <= 0) return NULL;
                    if (out_len) *out_len = (size_t)m;
                    return buf + offset;
                }
                case PB_WIRE_FIXED64:
                    if (out_len) *out_len = 8;
                    return buf + offset;
                case PB_WIRE_FIXED32:
                    if (out_len) *out_len = 4;
                    return buf + offset;
                case PB_WIRE_LENDELIM: {
                    uint64_t len;
                    int m = pb_decode_varint(buf + offset, buf_len - offset, &len);
                    if (m <= 0) return NULL;
                    if (out_len) *out_len = (size_t)len;
                    return buf + offset + m;
                }
                default:
                    return NULL;
            }
        }

        int skipped = pb_skip_field(buf + offset, buf_len - offset, wire);
        if (skipped <= 0) break;
        offset += (size_t)skipped;
    }
    return NULL;
}

bool pb_read_varint(const uint8_t *buf, size_t buf_len,
                    uint32_t tag, uint64_t *value) {
    const uint8_t *data = pb_find_field(buf, buf_len, tag, PB_WIRE_VARINT, NULL);
    if (!data) return false;
    int n = pb_decode_varint(data, buf_len - (size_t)(data - buf), value);
    return n > 0;
}

const uint8_t *pb_read_delimited(const uint8_t *buf, size_t buf_len,
                                  uint32_t tag, size_t *out_len) {
    return pb_find_field(buf, buf_len, tag, PB_WIRE_LENDELIM, out_len);
}

/* --- Encode --- */

size_t pb_varint_size(uint64_t value) {
    size_t sz = 0;
    do {
        sz++;
        value >>= 7;
    } while (value);
    return sz;
}

int pb_encode_varint(uint8_t *buf, size_t buf_len, uint64_t value) {
    size_t i = 0;
    do {
        if (i >= buf_len) return -1;
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value) byte |= 0x80;
        buf[i++] = byte;
    } while (value);
    return (int)i;
}

int pb_encode_tag(uint8_t *buf, size_t buf_len, uint32_t tag_no, int wire_type) {
    return pb_encode_varint(buf, buf_len, ((uint64_t)tag_no << 3) | (uint64_t)wire_type);
}

int pb_encode_varint_field(uint8_t *buf, size_t buf_len,
                           uint32_t tag_no, uint64_t value) {
    int pos = 0;
    int n = pb_encode_tag(buf + pos, buf_len - (size_t)pos, tag_no, PB_WIRE_VARINT);
    if (n <= 0) return -1;
    pos += n;
    n = pb_encode_varint(buf + pos, buf_len - (size_t)pos, value);
    if (n <= 0) return -1;
    return pos + n;
}

int pb_encode_delimited_field(uint8_t *buf, size_t buf_len,
                              uint32_t tag_no,
                              const uint8_t *data, size_t data_len) {
    int pos = 0;
    int n = pb_encode_tag(buf + pos, buf_len - (size_t)pos, tag_no, PB_WIRE_LENDELIM);
    if (n <= 0) return -1;
    pos += n;
    n = pb_encode_varint(buf + pos, buf_len - (size_t)pos, data_len);
    if (n <= 0) return -1;
    pos += n;
    if (data && data_len > 0) {
        if ((size_t)pos + data_len > buf_len) return -1;
        memcpy(buf + pos, data, data_len);
        pos += (int)data_len;
    }
    return pos;
}

int pb_encode_fixed32_field(uint8_t *buf, size_t buf_len,
                            uint32_t tag_no, uint32_t value) {
    int pos = 0;
    int n = pb_encode_tag(buf + pos, buf_len - (size_t)pos, tag_no, PB_WIRE_FIXED32);
    if (n <= 0) return -1;
    pos += n;
    if ((size_t)pos + 4 > buf_len) return -1;
    buf[pos++] = (uint8_t)(value & 0xFF);
    buf[pos++] = (uint8_t)((value >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((value >> 16) & 0xFF);
    buf[pos++] = (uint8_t)(value >> 24);
    return pos;
}
