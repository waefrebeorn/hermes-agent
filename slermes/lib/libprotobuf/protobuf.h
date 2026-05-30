/*
 * protobuf.h — Minimal protobuf wire-format encoder/decoder for C.
 * Supports varint, fixed32/64, length-delimited, and field tags.
 * No .proto compiler needed — just raw wire format.
 * MIT License — WuBu Hermes Project
 */

#ifndef HERMES_PROTOBUF_H
#define HERMES_PROTOBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wire types */
#define PB_WIRE_VARINT    0
#define PB_WIRE_FIXED64   1
#define PB_WIRE_LENDELIM  2
#define PB_WIRE_FIXED32   5

/* --- Decode --- */

/* Decode a varint from buffer. Returns bytes consumed, 0 on error. */
int pb_decode_varint(const uint8_t *buf, size_t buf_len, uint64_t *value);

/* Skip a field of given wire type. Returns bytes consumed, 0 on error. */
int pb_skip_field(const uint8_t *buf, size_t buf_len, int wire_type);

/* Find a field by tag number and wire type. Returns pointer to field data,
 * sets *out_len to data length. Returns NULL if not found. */
const uint8_t *pb_find_field(const uint8_t *buf, size_t buf_len,
                              uint32_t tag, int wire_type,
                              size_t *out_len);

/* Read a varint field value. Returns true if found. */
bool pb_read_varint(const uint8_t *buf, size_t buf_len,
                    uint32_t tag, uint64_t *value);

/* Read a length-delimited field. Returns pointer to data, sets *out_len. */
const uint8_t *pb_read_delimited(const uint8_t *buf, size_t buf_len,
                                  uint32_t tag, size_t *out_len);

/* Parse a field tag from buffer at offset. Sets *tag_no and *wire_type.
 * Returns bytes consumed. */
int pb_parse_tag(const uint8_t *buf, size_t buf_len,
                 uint32_t *tag_no, int *wire_type);

/* --- Encode --- */

/* Get serialized size of a varint */
size_t pb_varint_size(uint64_t value);

/* Encode a varint into buffer at offset. Returns bytes written. */
int pb_encode_varint(uint8_t *buf, size_t buf_len, uint64_t value);

/* Encode a field tag + wire type. Returns bytes written. */
int pb_encode_tag(uint8_t *buf, size_t buf_len, uint32_t tag_no, int wire_type);

/* Encode a varint field (tag + value). Returns bytes written. */
int pb_encode_varint_field(uint8_t *buf, size_t buf_len,
                           uint32_t tag_no, uint64_t value);

/* Encode a length-delimited field (tag + length + data). Returns bytes written. */
int pb_encode_delimited_field(uint8_t *buf, size_t buf_len,
                              uint32_t tag_no,
                              const uint8_t *data, size_t data_len);

/* Encode a fixed32 field */
int pb_encode_fixed32_field(uint8_t *buf, size_t buf_len,
                            uint32_t tag_no, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PROTOBUF_H */
