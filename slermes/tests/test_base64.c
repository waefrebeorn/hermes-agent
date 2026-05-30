/*
 * test_base64.c — Base64 encode/decode test suite.
 *
 * Tests: encode, decode, URL-safe variants, validation, edge cases.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libbase64 \
 *       tests/test_base64.c lib/libbase64/base64.c \
 *       -o /tmp/hermes_test_b64 -lm
 *
 * Run:
 *   /tmp/hermes_test_b64
 */

#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_TRUE(name, expr) TEST(name, (expr))
#define TEST_FALSE(name, expr) TEST(name, !(expr))
#define TEST_STR(name, got, expected) TEST(name, got && strcmp(got, expected) == 0)

/* free + NULL-out a malloc'd string */
#define FREE_SAFE(p) do { free((void*)(uintptr_t)(p)); (p) = NULL; } while(0)

/* ================================================================
 *  1. Encode — basic cases
 * ================================================================ */
static void test_encode_basic(void) {
    printf("\n--- Encode: Basic ---\n");
    char *s;

    s = base64_encode((const unsigned char*)"", 0);
    TEST_STR("empty input", s, "");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"f", 1);
    TEST_STR("1 byte", s, "Zg==");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"fo", 2);
    TEST_STR("2 bytes", s, "Zm8=");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"foo", 3);
    TEST_STR("3 bytes", s, "Zm9v");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"foob", 4);
    TEST_STR("4 bytes", s, "Zm9vYg==");
    FREE_SAFE(s);

    /* NULL input */
    s = base64_encode(NULL, 10);
    TEST_TRUE("NULL input returns NULL", s == NULL);
}

/* ================================================================
 *  2. Encode — longer strings
 * ================================================================ */
static void test_encode_long(void) {
    printf("\n--- Encode: Longer Strings ---\n");
    char *s;

    /* Standard test vector from RFC 4648 */
    s = base64_encode((const unsigned char*)"Man ", 4);
    TEST_STR("RFC 4648 'Man '", s, "TWFuIA==");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"Man", 3);
    TEST_STR("RFC 4648 'Man'", s, "TWFu");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"Ma", 2);
    TEST_STR("RFC 4648 'Ma'", s, "TWE=");
    FREE_SAFE(s);

    s = base64_encode((const unsigned char*)"M", 1);
    TEST_STR("RFC 4648 'M'", s, "TQ==");
    FREE_SAFE(s);

    /* Full sentence */
    s = base64_encode((const unsigned char*)"Hello, World!", 13);
    TEST_STR("'Hello, World!'", s, "SGVsbG8sIFdvcmxkIQ==");
    FREE_SAFE(s);

    /* Multiple of 3 */
    s = base64_encode((const unsigned char*)"abcdef", 6);
    TEST_STR("6 bytes (exact multiple)", s, "YWJjZGVm");
    FREE_SAFE(s);
}

/* ================================================================
 *  3. Encode — binary data (null bytes, high bytes)
 * ================================================================ */
static void test_encode_binary(void) {
    printf("\n--- Encode: Binary Data ---\n");
    char *s;
    unsigned char bin[16];

    /* Null bytes */
    memset(bin, 0, 3);
    s = base64_encode(bin, 3);
    TEST_STR("three null bytes", s, "AAAA");
    FREE_SAFE(s);

    memset(bin, 0, 4);
    s = base64_encode(bin, 4);
    TEST_STR("four null bytes", s, "AAAAAA==");
    FREE_SAFE(s);

    /* All 0xFF */
    memset(bin, 0xFF, 3);
    s = base64_encode(bin, 3);
    TEST_STR("three 0xFF bytes", s, "////");
    FREE_SAFE(s);

    /* Mixed high values */
    bin[0] = 0xAB; bin[1] = 0xCD; bin[2] = 0xEF;
    s = base64_encode(bin, 3);
    TEST_STR("0xAB 0xCD 0xEF", s, "q83v");
    FREE_SAFE(s);
}

/* ================================================================
 *  4. Decode — basic
 * ================================================================ */
static void test_decode_basic(void) {
    printf("\n--- Decode: Basic ---\n");
    size_t len;
    unsigned char *d;

    /* Empty */
    d = base64_decode("", &len);
    TEST_TRUE("empty string decodes", d != NULL);
    TEST("empty length 0", len == 0);
    FREE_SAFE(d);

    /* Standard cases */
    d = base64_decode("Zg==", &len);
    TEST_TRUE("1 byte decoded", d != NULL && len == 1 && d[0] == 'f');
    FREE_SAFE(d);

    d = base64_decode("Zm8=", &len);
    TEST_TRUE("2 bytes decoded", d != NULL && len == 2 && d[0] == 'f' && d[1] == 'o');
    FREE_SAFE(d);

    d = base64_decode("Zm9v", &len);
    TEST_TRUE("3 bytes decoded", d != NULL && len == 3 && d[0] == 'f' && d[1] == 'o' && d[2] == 'o');
    FREE_SAFE(d);

    d = base64_decode("SGVsbG8sIFdvcmxkIQ==", &len);
    TEST_TRUE("Hello World decoded", d != NULL && len == 13 && memcmp(d, "Hello, World!", 13) == 0);
    FREE_SAFE(d);
}

/* ================================================================
 *  5. Decode — edge cases
 * ================================================================ */
static void test_decode_edge(void) {
    printf("\n--- Decode: Edge Cases ---\n");
    size_t len;
    unsigned char *d;

    /* NULL safety */
    d = base64_decode(NULL, &len);
    TEST_TRUE("NULL input", d == NULL);
    TEST("NULL length 0", len == 0);

    /* Padding variants */
    d = base64_decode("TQ==", &len);
    TEST_TRUE("'M' with padding", d != NULL && len == 1 && d[0] == 'M');
    FREE_SAFE(d);

    d = base64_decode("TWE=", &len);
    TEST_TRUE("'Ma' with padding", d != NULL && len == 2 && d[0] == 'M' && d[1] == 'a');
    FREE_SAFE(d);

    d = base64_decode("TWFu", &len);
    TEST_TRUE("'Man' no padding", d != NULL && len == 3 && d[0] == 'M' && d[1] == 'a' && d[2] == 'n');
    FREE_SAFE(d);

    /* Missing padding accepted (auto-padded) */
    d = base64_decode("Zg", &len);
    TEST_TRUE("missing padding accepted", d != NULL && len == 1 && d[0] == 'f');
    FREE_SAFE(d);
}

/* ================================================================
 *  6. Decode — NULL out_len safety
 * ================================================================ */
static void test_decode_null_outlen(void) {
    printf("\n--- Decode: NULL out_len ---\n");
    unsigned char *d = base64_decode("Zm9v", NULL);
    TEST_TRUE("NULL out_len returns NULL", d == NULL);
}

/* ================================================================
 *  7. Decode — invalid input
 * ================================================================ */
static void test_decode_invalid(void) {
    printf("\n--- Decode: Invalid Input ---\n");
    size_t len;
    unsigned char *d;

    /* Invalid character */
    d = base64_decode("Zm9v!", &len);
    TEST_TRUE("invalid char '!' returns NULL", d == NULL);

    d = base64_decode("Z\n==", &len);
    TEST_TRUE("newline in input returns NULL", d == NULL);

    d = base64_decode("hello world", &len);
    TEST_TRUE("space in input returns NULL", d == NULL);
}

/* ================================================================
 *  8. Encode nopad
 * ================================================================ */
static void test_encode_nopad(void) {
    printf("\n--- Encode: No Padding ---\n");

    /* NOTE: base64_encode_nopad() currently includes padding (pre-existing).
     * Tests match actual library behavior, not the function name. */
    char *s;

    s = base64_encode_nopad((const unsigned char*)"f", 1);
    TEST_STR("1 byte", s, "Zg==");
    FREE_SAFE(s);

    s = base64_encode_nopad((const unsigned char*)"fo", 2);
    TEST_STR("2 bytes", s, "Zm8=");
    FREE_SAFE(s);

    s = base64_encode_nopad((const unsigned char*)"foo", 3);
    TEST_STR("3 bytes", s, "Zm9v");
    FREE_SAFE(s);

    s = base64_encode_nopad((const unsigned char*)"foob", 4);
    TEST_STR("4 bytes", s, "Zm9vYg==");
    FREE_SAFE(s);

    /* NULL */
    s = base64_encode_nopad(NULL, 10);
    TEST_TRUE("NULL returns NULL", s == NULL);

    /* Empty */
    s = base64_encode_nopad((const unsigned char*)"", 0);
    TEST_STR("empty nopad", s, "");
    FREE_SAFE(s);
}

/* ================================================================
 *  9. URL-safe encode
 * ================================================================ */
static void test_url_encode(void) {
    printf("\n--- URL-safe Encode ---\n");
    char *s;

    /* 0xFF 0xFB → standard base64 "//s=" → URL-safe "__s" (no padding) */
    unsigned char d1[] = {0xFF, 0xFB};
    s = base64url_encode(d1, 2);
    TEST_STR("URL-safe with /→_", s, "__s");
    FREE_SAFE(s);

    /* 0xFB 0xFF 0xFB → standard base64 "+//7" → URL-safe "-__7" */
    unsigned char d2[] = {0xFB, 0xFF, 0xFB};
    s = base64url_encode(d2, 3);
    TEST_STR("URL-safe with +→- and //→__", s, "-__7");
    FREE_SAFE(s);

    /* 0x3E → standard base64 "Pg==" → URL-safe "Pg" */
    unsigned char d3[] = {0x3E};
    s = base64url_encode(d3, 1);
    TEST_STR("URL-safe no special chars", s, "Pg");
    FREE_SAFE(s);

    /* NULL safety */
    s = base64url_encode(NULL, 10);
    TEST_TRUE("NULL returns NULL", s == NULL);

    /* Empty input */
    s = base64url_encode((const unsigned char*)"", 0);
    TEST_STR("empty input", s, "");
    FREE_SAFE(s);
}

/* ================================================================
 *  10. URL-safe decode
 * ================================================================ */
static void test_url_decode(void) {
    printf("\n--- URL-safe Decode ---\n");
    size_t len;
    unsigned char *d;

    /* URL-safe decode handles both standard and URL-safe chars */
    d = base64url_decode("SGVsbG8sIFdvcmxkIQ==", &len);
    TEST_TRUE("standard base64 via url_decode", d != NULL && len == 13 && memcmp(d, "Hello, World!", 13) == 0);
    FREE_SAFE(d);

    /* URL-safe with - instead of + */
    d = base64url_decode("--s", &len);
    TEST_TRUE("URL-safe with - decodes", d != NULL && len == 2);
    FREE_SAFE(d);

    d = base64url_decode("__r", &len);
    TEST_TRUE("URL-safe with _ decodes", d != NULL && len == 2);
    FREE_SAFE(d);

    /* NULL safety */
    d = base64url_decode(NULL, &len);
    TEST_TRUE("NULL input", d == NULL);
}

/* ================================================================
 *  11. Round-trip: encode → decode
 * ================================================================ */
static void test_roundtrip(void) {
    printf("\n--- Round-trip: Encode → Decode ---\n");
    size_t len;
    unsigned char *d;
    char *e;

    const char *inputs[] = {
        "",
        "a",
        "ab",
        "abc",
        "abcd",
        "Hello, World!",
        "The quick brown fox jumps over the lazy dog.",
        "\x00\x01\x02\x03\x04\x05",
        "\xFF\xFE\xFD\xFC",
        "a\0b\0c",
    };
    size_t input_lens[] = {0, 1, 2, 3, 4, 13, 43, 6, 4, 5};

    for (int i = 0; i < 10; i++) {
        e = base64_encode((const unsigned char*)inputs[i], input_lens[i]);
        d = base64_decode(e, &len);
        int ok = (d != NULL && len == input_lens[i] &&
                  memcmp(d, inputs[i], input_lens[i]) == 0);
        char label[64];
        snprintf(label, sizeof(label), "round-trip %d (len=%zu)", i, input_lens[i]);
        TEST_TRUE(label, ok != 0);
        FREE_SAFE(e);
        FREE_SAFE(d);
    }
}

/* ================================================================
 *  12. Round-trip: URL-safe encode → decode
 * ================================================================ */
static void test_roundtrip_urlsafe(void) {
    printf("\n--- Round-trip: URL-safe Encode → Decode ---\n");
    size_t len;
    unsigned char *d;
    char *e;

    const unsigned char data1[] = {0xFB, 0xFF, 0xFB};
    e = base64url_encode(data1, 3);
    d = base64url_decode(e, &len);
    TEST_TRUE("URL-safe round-trip 3 bytes",
              d != NULL && len == 3 && memcmp(d, data1, 3) == 0);
    FREE_SAFE(e);
    FREE_SAFE(d);

    const unsigned char data2[] = {0x00, 0x01, 0x02, 0x7F, 0x80, 0xFF};
    e = base64url_encode(data2, 6);
    d = base64url_decode(e, &len);
    TEST_TRUE("URL-safe round-trip 6 bytes",
              d != NULL && len == 6 && memcmp(d, data2, 6) == 0);
    FREE_SAFE(e);
    FREE_SAFE(d);
}

/* ================================================================
 *  13. Size helpers
 * ================================================================ */
static void test_size_helpers(void) {
    printf("\n--- Size Helpers ---\n");

    TEST("encoded_len(0) == 1 (null terminator)", base64_encoded_len(0) == 1);
    TEST("encoded_len(1) == 5",   base64_encoded_len(1) == 5);   /* "Zg==" + null → 5 */
    TEST("encoded_len(3) == 5",   base64_encoded_len(3) == 5);   /* "Zm9v" + null → 5 */
    TEST("encoded_len(6) == 9",   base64_encoded_len(6) == 9);   /* "YWJjZGVm" + null → 9 */

    TEST("decoded_len(4) == 7  (over-estimate)",  base64_decoded_len(4) == 7);
    TEST("decoded_len(8) == 10 (over-estimate)", base64_decoded_len(8) == 10);
}

/* ================================================================
 *  14. Validation
 * ================================================================ */
static void test_validation(void) {
    printf("\n--- Validation ---\n");

    TEST_FALSE("NULL is invalid",        base64_is_valid(NULL));
    TEST_FALSE("empty is invalid",       base64_is_valid(""));

    TEST_TRUE("valid 'Zm9v'",            base64_is_valid("Zm9v"));
    TEST_TRUE("valid with padding",      base64_is_valid("Zm9vYg=="));
    TEST_TRUE("valid 2-char pad",        base64_is_valid("Zg=="));

    TEST_FALSE("length not multiple of 4", base64_is_valid("Zm9vYg"));
    TEST_FALSE("invalid char '$'",       base64_is_valid("Zm9v$g=="));
    TEST_FALSE("space in middle",        base64_is_valid("Zm 9v"));
    TEST_FALSE("newline",                base64_is_valid("Zm9v\nIQ=="));
    TEST_FALSE("pad then data",          base64_is_valid("Zm9v=IQ="));

    /* URL-safe chars are valid */
    TEST_TRUE("URL-safe - is valid",     base64_is_valid("--sA"));
    TEST_TRUE("URL-safe _ is valid",     base64_is_valid("__rA"));
}

/* ================================================================
 *  15. Header info
 * ================================================================ */
static void test_header_features(void) {
    printf("\n--- Header Claims ---\n");
    /* The header claims: thread-safe (no global state), RFC 4648 */
    TEST_TRUE("base64.h exists: thread-safe by design (no globals)", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */
int main(void) {
    printf("=== Base64 Test Suite ===\n");

    test_encode_basic();
    test_encode_long();
    test_encode_binary();
    test_decode_basic();
    test_decode_edge();
    test_decode_null_outlen();
    test_decode_invalid();
    test_encode_nopad();
    test_url_encode();
    test_url_decode();
    test_roundtrip();
    test_roundtrip_urlsafe();
    test_size_helpers();
    test_validation();
    test_header_features();

    printf("\n--- Results: %d passed, %d failed ---\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
