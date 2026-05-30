/*
 * test_yuanbao_media.c — Tests for yuanbao_media utility functions.
 *
 * Tests: yuanbao_generate_file_id, yuanbao_build_image_msg, yuanbao_build_file_msg.
 * Also tests: crypto_md5_hex (new MD5 function in libcrypto).
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libcrypto \
 *       -I lib/libplugin tests/test_yuanbao_media.c src/tools/yuanbao_media.c \
 *       src/tools/url_safety.c lib/libjson/json.c lib/libcrypto/crypto.c \
 *       -o /tmp/hermes_test_yuanbao_media -lm -lssl -lcrypto
 *
 * Run:
 *   /tmp/hermes_test_yuanbao_media
 */

#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_yuanbao_media.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

static int tests = 0, passed = 0;

#define TEST(name) do { tests++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* Helper: parse JSON array, get first item */
static json_t *first_array_item(const char *json_str) {
    json_t *arr = json_parse(json_str, NULL);
    if (!arr || json_len(arr) == 0) { json_free(arr); return NULL; }
    json_t *item = json_get(arr, 0);
    if (!item) { json_free(arr); return NULL; }
    /* Return copy so we can free the array */
    json_t *copy = json_copy(item);
    json_free(arr);
    return copy;
}

/* ================================================================
 *  crypto_md5_hex tests
 * ================================================================ */

static void test_md5_hex_empty(void) {
    TEST("md5 hex of empty string");
    char *h = crypto_md5_hex((const unsigned char*)"", 0);
    if (h && strcmp(h, "d41d8cd98f00b204e9800998ecf8427e") == 0) { PASS(); free(h); return; }
    FAIL("expected d41d8cd98f00b204e9800998ecf8427e"); free(h);
}

static void test_md5_hex_hello(void) {
    TEST("md5 hex of 'hello'");
    char *h = crypto_md5_hex((const unsigned char*)"hello", 5);
    if (h && strcmp(h, "5d41402abc4b2a76b9719d911017c592") == 0) { PASS(); free(h); return; }
    FAIL("expected 5d41402abc4b2a76b9719d911017c592"); free(h);
}

static void test_md5_hex_random(void) {
    TEST("md5 hex returns 32-char hex string");
    char *h = crypto_md5_hex((const unsigned char*)"The quick brown fox", 19);
    if (h && strlen(h) == 32) {
        bool all_hex = true;
        for (int i = 0; i < 32; i++)
            if (!isxdigit((unsigned char)h[i])) all_hex = false;
        if (all_hex) { PASS(); free(h); return; }
    }
    FAIL("expected 32-char hex string"); free(h);
}

static void test_md5_hex_null(void) {
    TEST("md5 hex of NULL data is empty string hash");
    char *h = crypto_md5_hex(NULL, 0);
    /* Accept either NULL or empty-string hash for edge case */
    if (!h) { PASS(); return; }
    if (strcmp(h, "d41d8cd98f00b204e9800998ecf8427e") == 0) { PASS(); free(h); return; }
    FAIL("unexpected result"); free(h);
}

/* ================================================================
 *  yuanbao_generate_file_id tests
 * ================================================================ */

static void test_generate_file_id_exists(void) {
    TEST("generate_file_id returns non-NULL");
    char *id = yuanbao_generate_file_id();
    if (id) { PASS(); free(id); return; }
    FAIL("returned NULL");
}

static void test_generate_file_id_length(void) {
    TEST("generate_file_id is 32 hex chars");
    char *id = yuanbao_generate_file_id();
    if (!id) { FAIL("NULL"); return; }
    if (strlen(id) != 32) { FAIL("not 32 chars"); free(id); return; }
    bool all_hex = true;
    for (int i = 0; i < 32; i++)
        if (!isxdigit((unsigned char)id[i])) all_hex = false;
    if (all_hex) { PASS(); free(id); return; }
    FAIL("non-hex chars"); free(id);
}

static void test_generate_file_id_unique(void) {
    TEST("generate_file_id produces different values");
    char *id1 = yuanbao_generate_file_id();
    char *id2 = yuanbao_generate_file_id();
    if (!id1 || !id2) { FAIL("NULL"); free(id1); free(id2); return; }
    if (strcmp(id1, id2) != 0) { PASS(); free(id1); free(id2); return; }
    FAIL("identical values"); free(id1); free(id2);
}

/* ================================================================
 *  yuanbao_build_image_msg tests
 * ================================================================ */

static void test_build_image_msg_basic(void) {
    TEST("build_image_msg basic");
    char *json = yuanbao_build_image_msg("https://ex.com/img.png",
                                         "abc123", NULL, 1024, 200, 100, "image/png");
    if (!json) { FAIL("NULL"); return; }
    json_t *msg = first_array_item(json);
    if (!msg) { FAIL("no array item"); free(json); return; }
    const char *mt = json_get_str(msg, "msg_type", "");
    if (strcmp(mt, "TIMImageElem") != 0) {
        FAIL("wrong msg_type"); json_free(msg); free(json); return;
    }
    json_t *content = json_obj_get(msg, "msg_content");
    if (!content) { FAIL("no msg_content"); json_free(msg); free(json); return; }
    const char *uuid = json_get_str(content, "uuid", "");
    int fmt = (int)json_get_num(content, "image_format", -1);
    if (strcmp(uuid, "abc123") != 0) { FAIL("wrong uuid"); json_free(msg); free(json); return; }
    if (fmt != 3) { FAIL("wrong image_format"); json_free(msg); free(json); return; }
    json_t *arr = json_obj_get(content, "image_info_array");
    if (!arr || json_len(arr) != 1) { FAIL("no info array"); json_free(msg); free(json); return; }
    json_t *info = json_get(arr, 0);
    if (!info) { FAIL("no info item"); json_free(msg); free(json); return; }
    if ((int)json_get_num(info, "type", 0) != 1) { FAIL("wrong type"); json_free(msg); free(json); return; }
    if ((int)json_get_num(info, "size", 0) != 1024) { FAIL("wrong size"); json_free(msg); free(json); return; }
    if ((int)json_get_num(info, "width", 0) != 200) { FAIL("wrong width"); json_free(msg); free(json); return; }
    if ((int)json_get_num(info, "height", 0) != 100) { FAIL("wrong height"); json_free(msg); free(json); return; }
    PASS();
    json_free(msg);
    free(json);
}

static void test_build_image_msg_null_uuid(void) {
    TEST("build_image_msg null uuid uses filename");
    char *json = yuanbao_build_image_msg("https://ex.com/img.jpg",
                                         NULL, "photo.jpg", 500, 100, 100, "image/jpeg");
    if (!json) { FAIL("NULL"); return; }
    json_t *msg = first_array_item(json);
    if (!msg) { FAIL("no array item"); free(json); return; }
    json_t *content = json_obj_get(msg, "msg_content");
    const char *uuid = json_get_str(content, "uuid", "");
    int fmt = (int)json_get_num(content, "image_format", -1);
    if (strcmp(uuid, "photo.jpg") == 0 && fmt == 1) { PASS(); json_free(msg); free(json); return; }
    FAIL("wrong uuid or format"); json_free(msg); free(json);
}

static void test_build_image_msg_null_url(void) {
    TEST("build_image_msg null url returns NULL");
    char *json = yuanbao_build_image_msg(NULL, "abc", NULL, 0, 0, 0, NULL);
    if (!json) { PASS(); return; }
    FAIL("expected NULL"); free(json);
}

static void test_build_image_msg_unknown_mime(void) {
    TEST("build_image_msg unknown mime uses default format 255");
    char *json = yuanbao_build_image_msg("https://ex.com/f.xyz",
                                         "abc", NULL, 100, 50, 50, "application/octet-stream");
    if (!json) { FAIL("NULL"); return; }
    json_t *msg = first_array_item(json);
    if (!msg) { FAIL("no array item"); free(json); return; }
    json_t *content = json_obj_get(msg, "msg_content");
    int fmt = (int)json_get_num(content, "image_format", -1);
    if (fmt == 255) { PASS(); json_free(msg); free(json); return; }
    FAIL("expected 255 (unknown)"); json_free(msg); free(json);
}

/* ================================================================
 *  yuanbao_build_file_msg tests
 * ================================================================ */

static void test_build_file_msg_basic(void) {
    TEST("build_file_msg basic");
    char *json = yuanbao_build_file_msg("https://ex.com/doc.pdf",
                                        "report.pdf", "uuid-789", 2048);
    if (!json) { FAIL("NULL"); return; }
    json_t *msg = first_array_item(json);
    if (!msg) { FAIL("no array item"); free(json); return; }
    const char *mt = json_get_str(msg, "msg_type", "");
    if (strcmp(mt, "TIMFileElem") != 0) { FAIL("wrong msg_type"); json_free(msg); free(json); return; }
    json_t *content = json_obj_get(msg, "msg_content");
    if (!content) { FAIL("no msg_content"); json_free(msg); free(json); return; }
    const char *uuid = json_get_str(content, "uuid", "");
    const char *fn = json_get_str(content, "file_name", "");
    int sz = (int)json_get_num(content, "file_size", 0);
    const char *url = json_get_str(content, "url", "");
    if (strcmp(uuid, "uuid-789") != 0) { FAIL("wrong uuid"); json_free(msg); free(json); return; }
    if (strcmp(fn, "report.pdf") != 0) { FAIL("wrong file_name"); json_free(msg); free(json); return; }
    if (sz != 2048) { FAIL("wrong size"); json_free(msg); free(json); return; }
    if (strcmp(url, "https://ex.com/doc.pdf") != 0) { FAIL("wrong url"); json_free(msg); free(json); return; }
    PASS();
    json_free(msg);
    free(json);
}

static void test_build_file_msg_null_uuid(void) {
    TEST("build_file_msg null uuid uses filename");
    char *json = yuanbao_build_file_msg("https://ex.com/f.txt",
                                        "notes.txt", NULL, 100);
    if (!json) { FAIL("NULL"); return; }
    json_t *msg = first_array_item(json);
    if (!msg) { FAIL("no array item"); free(json); return; }
    json_t *content = json_obj_get(msg, "msg_content");
    const char *uuid = json_get_str(content, "uuid", "");
    if (strcmp(uuid, "notes.txt") == 0) { PASS(); json_free(msg); free(json); return; }
    FAIL("expected uuid=filename"); json_free(msg); free(json);
}

static void test_build_file_msg_null_url(void) {
    TEST("build_file_msg null url returns NULL");
    char *json = yuanbao_build_file_msg(NULL, "f.txt", NULL, 0);
    if (!json) { PASS(); return; }
    FAIL("expected NULL"); free(json);
}

static void test_build_file_msg_null_filename(void) {
    TEST("build_file_msg null filename returns NULL");
    char *json = yuanbao_build_file_msg("https://ex.com/f.txt", NULL, NULL, 0);
    if (!json) { PASS(); return; }
    FAIL("expected NULL"); free(json);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== crypto_md5_hex ===\n");
    test_md5_hex_empty();
    test_md5_hex_hello();
    test_md5_hex_random();
    test_md5_hex_null();

    printf("=== yuanbao_generate_file_id ===\n");
    test_generate_file_id_exists();
    test_generate_file_id_length();
    test_generate_file_id_unique();

    printf("=== yuanbao_build_image_msg ===\n");
    test_build_image_msg_basic();
    test_build_image_msg_null_uuid();
    test_build_image_msg_null_url();
    test_build_image_msg_unknown_mime();

    printf("=== yuanbao_build_file_msg ===\n");
    test_build_file_msg_basic();
    test_build_file_msg_null_uuid();
    test_build_file_msg_null_url();
    test_build_file_msg_null_filename();

    printf("\n%d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
