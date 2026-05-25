/*
 * test_base64.c — Tests for libbase64 (RFC 4648 base64/url encoding).
 *
 * Known-answer tests against RFC 4648 test vectors.
 */
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void) {
    /* RFC 4648 §10 test vectors */
    /* "": "" */
    /* "f": "Zg==" */
    /* "fo": "Zm8=" */
    /* "foo": "Zm9v" */
    /* "foob": "Zm9vYg==" */
    /* "fooba": "Zm9vYmE=" */
    /* "foobar": "Zm9vYmFy" */

    const unsigned char *empty = (const unsigned char *)"";
    const unsigned char *f = (const unsigned char *)"f";
    const unsigned char *foo = (const unsigned char *)"foo";
    const unsigned char *foobar = (const unsigned char *)"foobar";

    char *got;

    /* Standard encode */
    got = base64_encode(empty, 0);
    TEST("encode empty", got && strcmp(got, "") == 0);
    free(got);

    got = base64_encode(f, 1);
    TEST("encode 'f'", got && strcmp(got, "Zg==") == 0);
    free(got);

    got = base64_encode((const unsigned char *)"fo", 2);
    TEST("encode 'fo'", got && strcmp(got, "Zm8=") == 0);
    free(got);

    got = base64_encode(foo, 3);
    TEST("encode 'foo'", got && strcmp(got, "Zm9v") == 0);
    free(got);

    got = base64_encode((const unsigned char *)"foob", 4);
    TEST("encode 'foob'", got && strcmp(got, "Zm9vYg==") == 0);
    free(got);

    got = base64_encode((const unsigned char *)"fooba", 5);
    TEST("encode 'fooba'", got && strcmp(got, "Zm9vYmE=") == 0);
    free(got);

    got = base64_encode(foobar, 6);
    TEST("encode 'foobar'", got && strcmp(got, "Zm9vYmFy") == 0);
    free(got);

    /* Standard decode */
    size_t dlen;
    unsigned char *dec;

    dec = base64_decode("", &dlen);
    TEST("decode empty", dec && dlen == 0);
    free(dec);

    dec = base64_decode("Zg==", &dlen);
    TEST("decode Zg==", dec && dlen == 1 && dec[0] == 'f');
    free(dec);

    dec = base64_decode("Zm9v", &dlen);
    TEST("decode Zm9v", dec && dlen == 3 && memcmp(dec, "foo", 3) == 0);
    free(dec);

    dec = base64_decode("Zm9vYmFy", &dlen);
    TEST("decode Zm9vYmFy", dec && dlen == 6 && memcmp(dec, "foobar", 6) == 0);
    free(dec);

    /* Round-trip: encode then decode */
    const unsigned char test_data[] = "Hello, World! \x01\x02\xff\xfe";
    size_t tlen = sizeof(test_data) - 1; /* exclude null terminator */
    got = base64_encode(test_data, tlen);
    TEST("roundtrip encode not null", got != NULL);
    if (got) {
        dec = base64_decode(got, &dlen);
        TEST("roundtrip decode not null", dec != NULL);
        TEST("roundtrip length match", dec && dlen == tlen);
        TEST("roundtrip content match", dec && memcmp(dec, test_data, tlen) == 0);
        free(dec);
        free(got);
    }

    /* URL-safe encode */
    /* 0xff 0xfb → /2/7 → /w followed by base64 chars */
    unsigned char bin_data[] = {0xff, 0xfb};
    got = base64url_encode(bin_data, 2);
    TEST("base64url encode", got != NULL);
    /* Standard base64 of [0xff, 0xfb] = "/vsA" → no / in URL-safe */
    /* Let's just verify no + or / characters */
    if (got) {
        TEST("base64url no slash", strchr(got, '/') == NULL);
        TEST("base64url no plus", strchr(got, '+') == NULL);
        free(got);
    }

    /* URL-safe decode */
    dec = base64url_decode("Zm9vYmFy", &dlen);
    TEST("base64url decode foobar", dec && dlen == 6 && memcmp(dec, "foobar", 6) == 0);
    free(dec);

    /* URL-safe round-trip with binary data */
    unsigned char bin2[] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0};
    got = base64url_encode(bin2, 16);
    TEST("url encode binary not null", got != NULL);
    if (got) {
        dec = base64url_decode(got, &dlen);
        TEST("url decode binary not null", dec != NULL);
        TEST("url roundtrip length", dec && dlen == 16);
        TEST("url roundtrip content", dec && memcmp(dec, bin2, 16) == 0);
        free(dec);
        free(got);
    }

    /* Size helpers */
    TEST("encoded_len 0", base64_encoded_len(0) == 1);
    TEST("encoded_len 1", base64_encoded_len(1) == 5);
    TEST("encoded_len 3", base64_encoded_len(3) == 5);
    TEST("encoded_len 4", base64_encoded_len(4) == 9);

    /* Validation */
    TEST("valid base64", base64_is_valid("Zm9vYmFy"));
    TEST("valid base64 with pad", base64_is_valid("Zm9vYg=="));
    TEST("valid base64url", base64_is_valid("Zm9vYmFy"));
    TEST("invalid: NULL", !base64_is_valid(NULL));
    TEST("invalid: empty", !base64_is_valid(""));
    TEST("invalid: bad char", !base64_is_valid("Zm9v!mFy"));
    TEST("invalid: bad padding", !base64_is_valid("=Zm9vYmFy"));

    fprintf(stderr, "base64: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
