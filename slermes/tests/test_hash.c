/*
 * test_hash.c — Tests for libhash (J07: hashlib port).
 *
 * Tests: SHA-256, SHA-1, MD5, HMAC-SHA256, hex conversion, file hashing.
 * Known-answer tests against standard test vectors.
 */
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int total = 0, passed = 0;

#define TEST(name, expr) do { \
    total++; \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* Compare two byte arrays */
static int bytes_eq(const unsigned char *a, const unsigned char *b, size_t len) {
    return memcmp(a, b, len) == 0;
}

/* Hex string to expected lowercase hex — just use strcmp */
static int hex_eq(const char *got, const char *expected) {
    return got && expected && strcmp(got, expected) == 0;
}

int main(void) {
    /* ── Test vectors (NIST/FIPS known answers) ── */

    /* SHA-256: empty string */
    const unsigned char empty[] = "";
    const char *sha256_empty_hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    /* SHA-256: "abc" */
    const unsigned char abc[] = "abc";
    const char *sha256_abc_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    /* SHA-1: empty string */
    const char *sha1_empty_hex = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

    /* SHA-1: "abc" */
    const char *sha1_abc_hex = "a9993e364706816aba3e25717850c26c9cd0d89d";

    /* MD5: empty string */
    const char *md5_empty_hex = "d41d8cd98f00b204e9800998ecf8427e";

    /* MD5: "abc" */
    const char *md5_abc_hex = "900150983cd24fb0d6963f7d28e17f72";

    /* HMAC-SHA256: RFC 4231 Test Case 2 */
    const unsigned char hmac_key[] = "Jefe";
    const unsigned char hmac_data[] = "what do ya want for nothing?";
    const char *hmac_expected = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";

    /* ================================================================ */
    /* SHA-256 tests */
    /* ================================================================ */
    char *got;

    got = hash_sha256_hex(empty, 0);
    TEST("sha256 empty", hex_eq(got, sha256_empty_hex));
    free(got);

    got = hash_sha256_hex(abc, 3);
    TEST("sha256 abc", hex_eq(got, sha256_abc_hex));
    free(got);

    /* Binary SHA-256 test */
    unsigned char *bin = hash_sha256(empty, 0);
    TEST("sha256 binary empty not null", bin != NULL);
    if (bin) {
        unsigned char expected_bin[] = {
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
        };
        TEST("sha256 binary content", bytes_eq(bin, expected_bin, HASH_SHA256_LEN));
        free(bin);
    }

    /* NULL safety */
    TEST("sha256 NULL returns NULL", hash_sha256(NULL, 0) == NULL);
    TEST("sha256_hex NULL returns NULL", hash_sha256_hex(NULL, 0) == NULL);

    /* ================================================================ */
    /* SHA-1 tests */
    /* ================================================================ */

    got = hash_sha1_hex(empty, 0);
    TEST("sha1 empty", hex_eq(got, sha1_empty_hex));
    free(got);

    got = hash_sha1_hex(abc, 3);
    TEST("sha1 abc", hex_eq(got, sha1_abc_hex));
    free(got);

    /* Binary SHA-1 */
    unsigned char *bin1 = hash_sha1(abc, 3);
    TEST("sha1 binary not null", bin1 != NULL);
    if (bin1) {
        unsigned char expected_sha1[] = {
            0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
            0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
            0x9c, 0xd0, 0xd8, 0x9d
        };
        TEST("sha1 binary content", bytes_eq(bin1, expected_sha1, HASH_SHA1_LEN));
        free(bin1);
    }

    /* ================================================================ */
    /* MD5 tests */
    /* ================================================================ */

    got = hash_md5_hex(empty, 0);
    TEST("md5 empty", hex_eq(got, md5_empty_hex));
    free(got);

    got = hash_md5_hex(abc, 3);
    TEST("md5 abc", hex_eq(got, md5_abc_hex));
    free(got);

    /* Binary MD5 */
    unsigned char *bin_md5 = hash_md5(empty, 0);
    TEST("md5 binary not null", bin_md5 != NULL);
    if (bin_md5) {
        unsigned char expected_md5[] = {
            0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
            0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e
        };
        TEST("md5 binary content", bytes_eq(bin_md5, expected_md5, HASH_MD5_LEN));
        free(bin_md5);
    }

    /* ================================================================ */
    /* HMAC-SHA256 tests */
    /* ================================================================ */

    got = hash_hmac_sha256_hex(hmac_key, 4, hmac_data, 28);
    TEST("hmac-sha256 RFC 4231 case 2", hex_eq(got, hmac_expected));
    free(got);

    /* Binary HMAC */
    unsigned char *hmac_bin = hash_hmac_sha256(hmac_key, 4, hmac_data, 28);
    TEST("hmac binary not null", hmac_bin != NULL);
    if (hmac_bin) {
        unsigned char expected_hmac[] = {
            0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e,
            0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
            0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83,
            0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43
        };
        TEST("hmac binary content", bytes_eq(hmac_bin, expected_hmac, HASH_SHA256_LEN));
        free(hmac_bin);
    }

    /* HMAC NULL safety */
    TEST("hmac NULL key returns NULL", hash_hmac_sha256(NULL, 0, hmac_data, 28) == NULL);
    TEST("hmac NULL data returns NULL", hash_hmac_sha256(hmac_key, 4, NULL, 0) == NULL);

    /* ================================================================ */
    /* Hex conversion tests */
    /* ================================================================ */

    got = hash_bytes_to_hex((const unsigned char *)"", 0);
    TEST("bytes_to_hex empty returns NULL", got == NULL);

    unsigned char hex_in[] = {0xab, 0xcd, 0xef, 0x01, 0x23};
    got = hash_bytes_to_hex(hex_in, 5);
    TEST("bytes_to_hex abcdef0123", hex_eq(got, "abcdef0123"));
    free(got);

    /* hex_to_bytes */
    size_t hlen = 0;
    unsigned char *hbin = hash_hex_to_bytes("abcdef0123", &hlen);
    TEST("hex_to_bytes not null", hbin != NULL);
    TEST("hex_to_bytes length", hlen == 5);
    if (hbin) {
        unsigned char expected_hexb[] = {0xab, 0xcd, 0xef, 0x01, 0x23};
        TEST("hex_to_bytes content", bytes_eq(hbin, expected_hexb, 5));
        free(hbin);
    }

    /* Odd-length hex returns NULL */
    hbin = hash_hex_to_bytes("abc", &hlen);
    TEST("hex_to_bytes odd length returns NULL", hbin == NULL);

    /* NULL safety */
    TEST("hex_to_bytes NULL returns NULL", hash_hex_to_bytes(NULL, NULL) == NULL);

    /* ================================================================ */
    /* File hashing test */
    /* ================================================================ */

    /* Create temp file with known content */
    FILE *tf = tmpfile();
    TEST("tmpfile not null", tf != NULL);
    if (tf) {
        fwrite("hello world", 1, 11, tf);
        rewind(tf);

        /* Get fd and create /proc path for re-opening */
        int fd = fileno(tf);
        char procpath[64];
        snprintf(procpath, sizeof(procpath), "/proc/self/fd/%d", fd);

        got = hash_sha256_file(procpath);
        /* SHA-256 of "hello world" = known value */
        TEST("sha256_file not null", got != NULL);
        if (got) {
            TEST("sha256_file content", hex_eq(got, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"));
            free(got);
        }
    }

    /* File NULL/empty safety */
    TEST("sha256_file NULL returns NULL", hash_sha256_file(NULL) == NULL);
    TEST("sha256_file empty returns NULL", hash_sha256_file("") == NULL);
    TEST("sha256_file nonexistent returns NULL", hash_sha256_file("/nonexistent/file_xyz123") == NULL);

    /* ================================================================ */
    /* Report */
    /* ================================================================ */
    fprintf(stderr, "J07 hash: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
