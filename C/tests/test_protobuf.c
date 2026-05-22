#include "protobuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)

/* ================================================================
 * 1. pb_decode_varint
 * ================================================================ */
static void test_decode_varint(void) {
    uint8_t buf[16];
    uint64_t val;

    /* single-byte (value < 128) */
    buf[0] = 0x00; /* 0 */
    TEST("single byte: 0");
    assert(pb_decode_varint(buf, 1, &val) == 1);
    assert(val == 0);
    PASS();

    buf[0] = 0x01; /* 1 */
    TEST("single byte: 1");
    assert(pb_decode_varint(buf, 1, &val) == 1);
    assert(val == 1);
    PASS();

    buf[0] = 0x7F; /* 127 */
    TEST("single byte: 127");
    assert(pb_decode_varint(buf, 1, &val) == 1);
    assert(val == 127);
    PASS();

    /* multi-byte */
    buf[0] = 0x80 | 0x01; buf[1] = 0x01; /* 128 + 1 = 129? No: (0x01 << 7) | 0x01 = 129 */
    /* Actually: byte0 = 1000 0001 (continuation, value 1), byte1 = 0000 0001 (final, value 1) */
    /* Value = (1 << 7) | 1 = 129 */
    TEST("multi-byte: 129");
    assert(pb_decode_varint(buf, 2, &val) == 2);
    assert(val == 129);
    PASS();

    /* 300 = 0xAC 0x02 — bytes: 1010 1100, 0000 0010 */
    /* value = (0x2C << 7) | 0x2C? Let me calculate: */
    /* byte0 = 0xAC = 1010 1100 → cont=1, val=0x2C=44 */
    /* byte1 = 0x02 = 0000 0010 → cont=0, val=0x02=2 */
    /* result = (2 << 7) | 44 = 256 + 44 = 300 */
    buf[0] = 0xAC; buf[1] = 0x02;
    TEST("multi-byte: 300");
    assert(pb_decode_varint(buf, 2, &val) == 2);
    assert(val == 300);
    PASS();

    /* large: 1000000 */
    uint64_t large = 1000000;
    uint8_t enc[16];
    int n = 0;
    uint64_t tmp = large;
    do {
        uint8_t byte = tmp & 0x7F;
        tmp >>= 7;
        if (tmp) byte |= 0x80;
        enc[n++] = byte;
    } while (tmp);
    TEST("large: 1000000");
    assert(pb_decode_varint(enc, (size_t)n, &val) == n);
    assert(val == large);
    PASS();

    /* truncated (continuation bit set but no more bytes) */
    buf[0] = 0x80;
    TEST("truncated varint (no more bytes)");
    assert(pb_decode_varint(buf, 1, &val) == 0);
    PASS();

    /* null safety */
    TEST("NULL buf returns 0");
    assert(pb_decode_varint(NULL, 10, &val) == 0);
    PASS();

    TEST("NULL value returns 0");
    assert(pb_decode_varint(buf, 10, NULL) == 0);
    PASS();

    TEST("buf_len == 0");
    assert(pb_decode_varint(buf, 0, &val) == 0);
    PASS();
}

/* ================================================================
 * 2. pb_varint_size
 * ================================================================ */
static void test_varint_size(void) {
    TEST("size(0) == 1");
    assert(pb_varint_size(0) == 1);
    PASS();

    TEST("size(1) == 1");
    assert(pb_varint_size(1) == 1);
    PASS();

    TEST("size(127) == 1");
    assert(pb_varint_size(127) == 1);
    PASS();

    TEST("size(128) == 2");
    assert(pb_varint_size(128) == 2);
    PASS();

    TEST("size(16383) == 2");
    assert(pb_varint_size(16383) == 2);
    PASS();

    TEST("size(16384) == 3");
    assert(pb_varint_size(16384) == 3);
    PASS();

    TEST("size(UINT64_MAX) > 0");
    assert(pb_varint_size(UINT64_MAX) > 0);
    PASS();
}

/* ================================================================
 * 3. pb_encode_varint (round-trip)
 * ================================================================ */
static void test_encode_varint(void) {
    uint8_t buf[16];
    uint64_t val;

    /* Simple values round-trip */
    uint64_t cases[] = {0, 1, 127, 128, 255, 300, 16383, 16384, 1000000, UINT64_MAX};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        memset(buf, 0, sizeof(buf));
        TEST("encode/decode round-trip");
        int n = pb_encode_varint(buf, sizeof(buf), cases[i]);
        assert(n > 0);
        int m = pb_decode_varint(buf, (size_t)n, &val);
        assert(m == n);
        assert(val == cases[i]);
        PASS();
    }

    /* Buffer too small */
    TEST("buffer too small returns -1");
    assert(pb_encode_varint(buf, 0, 100) == -1);
    PASS();
}

/* ================================================================
 * 4. pb_parse_tag / pb_encode_tag
 * ================================================================ */
static void test_tag(void) {
    uint8_t buf[16];
    uint32_t tag_no;
    int wire_type;

    /* Encode tag 1, wire varint */
    int n = pb_encode_tag(buf, sizeof(buf), 1, PB_WIRE_VARINT);
    TEST("encode tag 1/varint");
    assert(n > 0);
    PASS();

    int m = pb_parse_tag(buf, (size_t)n, &tag_no, &wire_type);
    TEST("parse tag 1/varint matches");
    assert(m == n);
    assert(tag_no == 1);
    assert(wire_type == PB_WIRE_VARINT);
    PASS();

    /* Tag 15, wire lendelim */
    n = pb_encode_tag(buf, sizeof(buf), 15, PB_WIRE_LENDELIM);
    m = pb_parse_tag(buf, (size_t)n, &tag_no, &wire_type);
    TEST("tag 15/lendelim");
    assert(m == n);
    assert(tag_no == 15);
    assert(wire_type == PB_WIRE_LENDELIM);
    PASS();

    /* NULL safety */
    TEST("parse_tag NULL buf");
    assert(pb_parse_tag(NULL, 10, &tag_no, &wire_type) == 0);
    PASS();

    TEST("parse_tag NULL tag_no");
    assert(pb_parse_tag(buf, 10, NULL, &wire_type) == 0);
    PASS();

    TEST("parse_tag NULL wire_type");
    assert(pb_parse_tag(buf, 10, &tag_no, NULL) == 0);
    PASS();
}

/* ================================================================
 * 5. pb_skip_field
 * ================================================================ */
static void test_skip_field(void) {
    uint8_t buf[16];

    /* Varint: byte 0x01 → skip 1 byte */
    buf[0] = 0x01;
    TEST("skip varint");
    assert(pb_skip_field(buf, sizeof(buf), PB_WIRE_VARINT) == 1);
    PASS();

    /* Fixed32: skip 4 bytes */
    TEST("skip fixed32");
    assert(pb_skip_field(buf, sizeof(buf), PB_WIRE_FIXED32) == 4);
    PASS();

    /* Fixed64: skip 8 bytes */
    TEST("skip fixed64");
    assert(pb_skip_field(buf, sizeof(buf), PB_WIRE_FIXED64) == 8);
    PASS();

    /* Length-delimited: varint length followed by data */
    buf[0] = 0x05; /* length = 5 */
    TEST("skip lendelim (length 5)");
    assert(pb_skip_field(buf, sizeof(buf), PB_WIRE_LENDELIM) == 1 + 5); /* 1 byte varint + 5 data */
    PASS();

    /* Invalid wire type */
    TEST("skip invalid wire type");
    assert(pb_skip_field(buf, sizeof(buf), 99) == 0);
    PASS();

    /* Empty buffer for lendelim */
    TEST("skip lendelim with buf_len=0");
    assert(pb_skip_field(buf, 0, PB_WIRE_LENDELIM) == 0);
    PASS();
}

/* ================================================================
 * 6. pb_encode_varint_field (encode + find)
 * ================================================================ */
static void test_encode_varint_field(void) {
    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));

    /* Encode field 1 = value 42 */
    int n = pb_encode_varint_field(buf, sizeof(buf), 1, 42);
    TEST("encode varint field 1 = 42");
    assert(n > 0);
    PASS();

    /* Find and read back */
    uint64_t val;
    TEST("read varint field 1 back");
    assert(pb_read_varint(buf, (size_t)n, 1, &val));
    assert(val == 42);
    PASS();

    /* Not found for wrong tag */
    TEST("wrong tag not found");
    assert(!pb_read_varint(buf, (size_t)n, 2, &val));
    PASS();

    /* Encode multiple fields and find second */
    uint8_t multi[64];
    int pos = 0;
    pos += pb_encode_varint_field(multi + pos, sizeof(multi) - (size_t)pos, 1, 100);
    pos += pb_encode_varint_field(multi + pos, sizeof(multi) - (size_t)pos, 2, 200);
    pos += pb_encode_varint_field(multi + pos, sizeof(multi) - (size_t)pos, 3, 300);

    TEST("multi-field: find field 2");
    assert(pb_read_varint(multi, (size_t)pos, 2, &val));
    assert(val == 200);
    PASS();

    TEST("multi-field: find field 3");
    assert(pb_read_varint(multi, (size_t)pos, 3, &val));
    assert(val == 300);
    PASS();
}

/* ================================================================
 * 7. pb_encode_delimited_field (encode + find)
 * ================================================================ */
static void test_encode_delimited_field(void) {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));

    const char *hello = "Hello, World!";
    size_t hello_len = strlen(hello);

    int n = pb_encode_delimited_field(buf, sizeof(buf), 1,
                                       (const uint8_t*)hello, hello_len);
    TEST("encode delimited field 1 = 'Hello, World!'");
    assert(n > 0);
    PASS();

    size_t out_len;
    const uint8_t *data = pb_read_delimited(buf, (size_t)n, 1, &out_len);
    TEST("read delimited field 1 back");
    assert(data != NULL);
    assert(out_len == hello_len);
    assert(memcmp(data, hello, hello_len) == 0);
    PASS();

    /* Empty data */
    memset(buf, 0, sizeof(buf));
    n = pb_encode_delimited_field(buf, sizeof(buf), 2, NULL, 0);
    TEST("encode delimited with empty data");
    assert(n > 0);
    PASS();

    data = pb_read_delimited(buf, (size_t)n, 2, &out_len);
    TEST("read delimited with empty data");
    assert(data != NULL);
    assert(out_len == 0);
    PASS();

    /* Buffer too small */
    TEST("buffer too small for field tag");
    assert(pb_encode_delimited_field(buf, 0, 1, NULL, 0) == -1);
    PASS();
}

/* ================================================================
 * 8. pb_encode_fixed32_field (encode + find)
 * ================================================================ */
static void test_encode_fixed32_field(void) {
    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));

    int n = pb_encode_fixed32_field(buf, sizeof(buf), 1, 0xDEADBEEF);
    TEST("encode fixed32 field 1 = 0xDEADBEEF");
    assert(n > 0);
    PASS();

    /* Find field */
    size_t out_len;
    const uint8_t *data = pb_find_field(buf, (size_t)n, 1, PB_WIRE_FIXED32, &out_len);
    TEST("find fixed32 field 1");
    assert(data != NULL);
    assert(out_len == 4);
    PASS();

    /* Read value */
    uint32_t val = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                   ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    TEST("fixed32 value correct");
    assert(val == 0xDEADBEEF);
    PASS();

    /* Buffer too small for value */
    TEST("buffer too small for value");
    assert(pb_encode_fixed32_field(buf, 1, 1, 42) == -1);
    PASS();
}

/* ================================================================
 * 9. pb_find_field — not found, multiple types
 * ================================================================ */
static void test_find_field(void) {
    uint8_t multi[64];
    int pos = 0;

    /* field 1 varint = 100 */
    pos += pb_encode_varint_field(multi + pos, sizeof(multi) - (size_t)pos, 1, 100);
    /* field 2 delimited = "test" */
    pos += pb_encode_delimited_field(multi + pos, sizeof(multi) - (size_t)pos, 2,
                                      (const uint8_t*)"test", 4);
    /* field 3 fixed32 = 0xFF */
    pos += pb_encode_fixed32_field(multi + pos, sizeof(multi) - (size_t)pos, 3, 0xFF);

    TEST("find field 1 (varint)");
    size_t out_len;
    const uint8_t *d = pb_find_field(multi, (size_t)pos, 1, PB_WIRE_VARINT, &out_len);
    assert(d != NULL);
    uint64_t val;
    assert(pb_decode_varint(d, out_len, &val) > 0);
    assert(val == 100);
    PASS();

    TEST("find field 2 (delimited)");
    d = pb_find_field(multi, (size_t)pos, 2, PB_WIRE_LENDELIM, &out_len);
    assert(d != NULL);
    assert(out_len == 4);
    assert(memcmp(d, "test", 4) == 0);
    PASS();

    TEST("find field 3 (fixed32)");
    d = pb_find_field(multi, (size_t)pos, 3, PB_WIRE_FIXED32, &out_len);
    assert(d != NULL);
    assert(out_len == 4);
    PASS();

    TEST("non-existent field returns NULL");
    d = pb_find_field(multi, (size_t)pos, 99, PB_WIRE_VARINT, &out_len);
    assert(d == NULL);
    PASS();

    TEST("wrong wire type returns NULL");
    d = pb_find_field(multi, (size_t)pos, 1, PB_WIRE_FIXED32, &out_len);
    assert(d == NULL);
    PASS();

    TEST("empty buffer returns NULL");
    d = pb_find_field(multi, 0, 1, PB_WIRE_VARINT, &out_len);
    assert(d == NULL);
    PASS();
}

/* ================================================================
 * 10. pb_read_varint — null safety
 * ================================================================ */
static void test_read_varint_null(void) {
    uint64_t val;

    TEST("read_varint with NULL buf");
    assert(!pb_read_varint(NULL, 10, 1, &val));
    PASS();

    TEST("read_delimited with NULL buf");
    size_t out_len;
    assert(pb_read_delimited(NULL, 10, 1, &out_len) == NULL);
    PASS();

    TEST("find_field with NULL buf");
    assert(pb_find_field(NULL, 10, 1, PB_WIRE_VARINT, &out_len) == NULL);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void) {
    printf("=== Protobuf Library Tests ===\n");

    test_decode_varint();
    test_varint_size();
    test_encode_varint();
    test_tag();
    test_skip_field();
    test_encode_varint_field();
    test_encode_delimited_field();
    test_encode_fixed32_field();
    test_find_field();
    test_read_varint_null();

    printf("\n=== Results: %d/%d passed ===\n", passed, tests);
    return passed == tests ? 0 : 1;
}
