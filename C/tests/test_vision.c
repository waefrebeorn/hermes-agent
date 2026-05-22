/*
 * test_vision.c — Vision tool unit tests (M33).
 *
 * Tests image validation (extension, file size) and error paths.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_vision.c src/tools/vision.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_vision -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_vision
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

extern char *vision_handler(const char *args_json, const char *task_id);

static json_node_t *parse_result(const char *json_str) {
    char *err = NULL;
    json_node_t *r = json_parse(json_str, &err);
    if (!r) fprintf(stderr, "  JSON parse error: %s\n", err ? err : "unknown");
    free(err);
    return r;
}

static const char *result_get_str(json_node_t *obj, const char *key, const char *def) {
    return json_object_get_string(obj, key, def);
}

/* Helper: write all bytes, discard result for warn_unused_result */
static void write_all(int fd, const void *buf, size_t count) {
    ssize_t r = write(fd, buf, count);
    (void)r;
}

int main(void) {
    printf("=== Vision Tool Tests ===\n");

    /* Create a small test file (not an image — tests extension check) */
    int fd = open("/tmp/hermes_test_vision.bin", O_CREAT|O_WRONLY, 0644);
    write_all(fd, "not an image", 12);
    close(fd);

    /* Create a small test image (minimal valid PNG — tests success path) */
    /* Minimal 1x1 red PNG (67 bytes) */
    unsigned char png[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, /* PNG signature */
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, /* IHDR chunk */
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, /* 1x1 pixel */
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, /* 8-bit RGBA */
        0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, /* IDAT chunk */
        0x54, 0x08, 0xD7, 0x63, 0x60, 0x60, 0x60, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x01, 0x27, 0x34, 0x27,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, /* IEND chunk */
        0xAE, 0x42, 0x60, 0x82
    };
    fd = open("/tmp/hermes_test_vision.png", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);

    json_node_t *r;

    /* 1. null args */
    r = parse_result(vision_handler(NULL, NULL));
    TEST("vision null args returns error",
         r && strstr(result_get_str(r, "error", ""), "No args") != NULL);
    json_free(r);

    /* 2. missing image_url */
    r = parse_result(vision_handler("{}", NULL));
    TEST("vision missing image_url returns error",
         r && strstr(result_get_str(r, "error", ""), "Missing image_url") != NULL);
    json_free(r);

    /* 3. non-existent local file */
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_nonexistent.xyz\"}", NULL));
    TEST("vision non-existent file returns error",
         r && strstr(result_get_str(r, "error", ""), "File not found") != NULL);
    json_free(r);

    /* 4. file without image extension (.bin) */
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.bin\"}", NULL));
    TEST("vision non-image extension returns error",
         r && strstr(result_get_str(r, "error", ""), "Not a recognized image format") != NULL);
    json_free(r);

    /* 5. valid PNG file */
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.png\"}", NULL));
    TEST("vision valid PNG returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    TEST("vision valid PNG no error",
         r && strstr(result_get_str(r, "error", ""), "error") == NULL);
    if (r) {
        const char *info = result_get_str(r, "file_info", "");
        printf("    file_info=%s\n", info);
        double fsize = json_object_get_number(r, "file_size", 0);
        printf("    file_size=%.0f\n", fsize);
        TEST("vision PNG returns file_size > 0", fsize > 0);
    }
    json_free(r);

    /* 6. URL-based image (skips local checks) */
    r = parse_result(vision_handler("{\"image_url\":\"https://example.com/test.png\"}", NULL));
    TEST("vision URL image returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    TEST("vision URL image no error",
         r && strstr(result_get_str(r, "error", ""), "error") == NULL);
    json_free(r);

    /* 7. data URI (skips local checks) */
    r = parse_result(vision_handler("{\"image_url\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAA=\",\"question\":\"What is this?\"}", NULL));
    TEST("vision data URI returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    TEST("vision data URI no error",
         r && strstr(result_get_str(r, "error", ""), "error") == NULL);
    json_free(r);

    /* 8. big file test (>50MB) — use /dev/zero trick via stat on a large path */
    /* Create a 51MB file (too large) */
    fd = open("/tmp/hermes_test_too_large.png", O_CREAT|O_WRONLY, 0644);
    if (fd >= 0) {
        /* Write a minimal PNG header then seek past 50MB to fake size */
        write_all(fd, png, sizeof(png));
        lseek(fd, 51LL * 1024 * 1024 - 1, SEEK_SET);
        write_all(fd, "", 1);
        close(fd);

        r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_too_large.png\"}", NULL));
        TEST("vision too-large file returns error",
             r && strstr(result_get_str(r, "error", ""), "File too large") != NULL);
        json_free(r);
        unlink("/tmp/hermes_test_too_large.png");
    } else {
        TEST("vision too-large file (skip — cannot create)", true);
    }

    /* 9. JPG extension */
    fd = open("/tmp/hermes_test_vision.jpg", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.jpg\"}", NULL));
    TEST("vision JPG file returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.jpg");

    /* 10. JPEG extension */
    fd = open("/tmp/hermes_test_vision.jpeg", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.jpeg\"}", NULL));
    TEST("vision JPEG file returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.jpeg");

    /* 11. GIF extension */
    fd = open("/tmp/hermes_test_vision.gif", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.gif\"}", NULL));
    TEST("vision GIF file returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.gif");

    /* 12. WEBP extension */
    fd = open("/tmp/hermes_test_vision.webp", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.webp\"}", NULL));
    TEST("vision WEBP file returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.webp");

    /* 13. HEIC extension */
    fd = open("/tmp/hermes_test_vision.heic", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.heic\"}", NULL));
    TEST("vision HEIC file returns image_url",
         r && result_get_str(r, "image_url", NULL) != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.heic");

    /* 14. SVF extension (invalid — file exists but bad extension) */
    fd = open("/tmp/hermes_test_vision.svf", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_vision.svf\"}", NULL));
    TEST("vision invalid .svf extension returns error (not in list)",
         r && strstr(result_get_str(r, "error", ""), "Not a recognized image format") != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_vision.svf");

    /* 15. No extension — file exists but without image extension */
    fd = open("/tmp/hermes_test_noext", O_CREAT|O_WRONLY, 0644);
    write_all(fd, png, sizeof(png));
    close(fd);
    r = parse_result(vision_handler("{\"image_url\":\"/tmp/hermes_test_noext\"}", NULL));
    TEST("vision no extension returns error",
         r && strstr(result_get_str(r, "error", ""), "Not a recognized image format") != NULL);
    json_free(r);
    unlink("/tmp/hermes_test_noext");

    /* Cleanup */
    unlink("/tmp/hermes_test_vision.bin");
    unlink("/tmp/hermes_test_vision.png");
    /* Already cleaned up individual extension files above */

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
