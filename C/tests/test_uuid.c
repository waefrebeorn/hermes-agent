/*
 * test_uuid.c — Tests for libuuid (J08: UUID generation/parsing).
 *
 * Tests: v4 generation, v5 name-based, parse, unparse, validation.
 * Known-answer tests for v5 using RFC 4122 test vectors.
 */
#include "uuid.h"
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
    /* ================================================================ */
    /* UUID v4 generation */
    /* ================================================================ */

    char *u = uuid_v4();
    TEST("uuid_v4 not null", u != NULL);
    TEST("uuid_v4 length 36", u && strlen(u) == 36);
    TEST("uuid_v4 valid format", u && uuid_is_valid(u));
    TEST("uuid_v4 version 4", u && uuid_version(u) == 4);

    /* Generate two — they should differ */
    char *u2 = uuid_v4();
    TEST("uuid_v4 second not null", u2 != NULL);
    TEST("uuid_v4 unique", u && u2 && strcmp(u, u2) != 0);
    free(u);
    free(u2);

    /* UUID v4 via bytes API */
    uint8_t bytes[UUID_LEN];
    bool ok = uuid_v4_bytes(bytes);
    TEST("uuid_v4_bytes returns true", ok);
    /* Check version nibble at byte[6] top nibble = 4 */
    TEST("uuid_v4_bytes version nibble", (bytes[6] >> 4) == 4);
    /* Check variant bits at byte[8] top 2 bits = 10 */
    TEST("uuid_v4_bytes variant bits", (bytes[8] & 0xc0) == 0x80);

    /* Parse the first UUID string back */
    u = uuid_v4();
    TEST("uuid_v4 third not null", u != NULL);
    if (u) {
        uint8_t parsed[UUID_LEN];
        ok = uuid_parse(u, parsed);
        TEST("uuid_parse returns true", ok);
        if (ok) {
            /* Re-format and compare */
            char *re = uuid_unparse(parsed);
            TEST("round-trip match", re && strcmp(u, re) == 0);
            free(re);
        }
        free(u);
    }

    /* ================================================================ */
    /* UUID v5 (name-based) — RFC 4122 test vectors */
    /* ================================================================ */

    /* Test: UUID v5 of DNS namespace + name "www.example.com"
     * Expected: "2ed6657d-e927-568b-95e1-2665a8aea6a2" (from RFC 4122 Errata) */
    uint8_t ns_dns[UUID_LEN] = UUID_NS_DNS;
    char *v5 = uuid_v5(ns_dns, "www.example.com", 15);
    TEST("uuid_v5 www.example.com not null", v5 != NULL);
    if (v5) {
        TEST("uuid_v5 length 36", strlen(v5) == 36);
        TEST("uuid_v5 valid", uuid_is_valid(v5));
        TEST("uuid_v5 version 5", uuid_version(v5) == 5);
        /* Known value from RFC 4122 Errata */
        TEST("uuid_v5 DNS+www.example.com", strcmp(v5, "2ed6657d-e927-568b-95e1-2665a8aea6a2") == 0);
        free(v5);
    }

    /* Empty name */
    v5 = uuid_v5(ns_dns, "", 0);
    TEST("uuid_v5 empty name not null", v5 != NULL);
    if (v5) {
        TEST("uuid_v5 empty valid", uuid_is_valid(v5));
        free(v5);
    }

    /* NULL namespace */
    v5 = uuid_v5(NULL, "test", 4);
    TEST("uuid_v5 NULL ns is NULL", v5 == NULL);

    /* ================================================================ */
    /* UUID parsing */
    /* ================================================================ */

    uint8_t pbytes[UUID_LEN];
    ok = uuid_parse("550e8400-e29b-41d4-a716-446655440000", pbytes);
    TEST("parse standard UUID", ok);
    if (ok) {
        /* Check known bytes */
        TEST("parse byte[0]", pbytes[0] == 0x55);
        TEST("parse byte[4]", pbytes[4] == 0xe2);
        TEST("parse byte[15]", pbytes[15] == 0x00);
    }

    /* Round-trip with all-zeros UUID */
    ok = uuid_parse("00000000-0000-0000-0000-000000000000", pbytes);
    TEST("parse nil UUID", ok);
    if (ok) {
        for (int i = 0; i < UUID_LEN; i++)
            TEST("nil UUID byte zero", pbytes[i] == 0);
    }

    /* Invalid parse cases */
    TEST("parse NULL returns false", !uuid_parse(NULL, pbytes));
    TEST("parse empty returns false", !uuid_parse("", pbytes));
    TEST("parse short returns false", !uuid_parse("550e8400-e29b", pbytes));
    TEST("parse no hyphens returns false", !uuid_parse("550e8400e29b41d4a716446655440000", pbytes));
    TEST("parse bad hex returns false", !uuid_parse("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz", pbytes));

    /* ================================================================ */
    /* UUID unparse */
    /* ================================================================ */

    uint8_t zero_bytes[UUID_LEN] = {0};
    char *zp = uuid_unparse(zero_bytes);
    TEST("unparse zeros", zp && strcmp(zp, "00000000-0000-0000-0000-000000000000") == 0);
    free(zp);

    /* NULL safety */
    TEST("unparse NULL returns NULL", uuid_unparse(NULL) == NULL);

    /* ================================================================ */
    /* UUID validation */
    /* ================================================================ */

    TEST("valid UUID v4", uuid_is_valid("550e8400-e29b-41d4-a716-446655440000"));
    TEST("valid UUID v5", uuid_is_valid("2ed6657d-e927-568b-95e1-2665a8aea6a2"));
    TEST("invalid: NULL", !uuid_is_valid(NULL));
    TEST("invalid: empty", !uuid_is_valid(""));
    TEST("invalid: short", !uuid_is_valid("550e8400-e29b"));
    TEST("invalid: wrong hyphen pos", !uuid_is_valid("550e8400e-29b-41d4-a716-446655440000"));
    TEST("invalid: trailing char", !uuid_is_valid("550e8400-e29b-41d4-a716-446655440000x"));

    /* UUID version */
    TEST("version v4", uuid_version("550e8400-e29b-41d4-a716-446655440000") == 4);
    TEST("version v5", uuid_version("2ed6657d-e927-568b-95e1-2665a8aea6a2") == 5);
    TEST("version nil", uuid_version("00000000-0000-0000-0000-000000000000") == 0);
    TEST("version invalid", uuid_version("") == 0);
    TEST("version NULL", uuid_version(NULL) == 0);

    /* ================================================================ */
    /* Report */
    /* ================================================================ */
    fprintf(stderr, "J08 uuid: %d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
