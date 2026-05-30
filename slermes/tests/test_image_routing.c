/*
 * test_image_routing.c — Tests for image_routing module
 *
 * Tests independent functions: MIME sniffing, guess_mime, file_to_data_url,
 * build_native_content_parts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

/* Stub for model_supports_vision — tested separately in integration tests */
bool model_supports_vision(const char *model, const void *provider_cfg) {
    (void)model; (void)provider_cfg;
    return false;
}
const char *sniff_mime_from_bytes(const unsigned char *data, size_t len);
const char *guess_mime(const char *path, const unsigned char *data, size_t data_len);
char *file_to_data_url(const char *path);
char *build_native_content_parts(const char *user_text,
                                  const char **image_paths,
                                  size_t num_paths,
                                  char ***skipped_out,
                                  size_t *skipped_cnt);

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* Create a temporary PNG file for data URL tests */
static char *create_temp_png(void) {
    unsigned char png[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
        0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
        0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
        0x00, 0x00, 0x03, 0x00, 0x01, 0x36, 0x28, 0x19,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
        0xAE, 0x42, 0x60, 0x82
    };
    const char *path = "/tmp/test_image_routing.png";
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    fwrite(png, 1, sizeof(png), f);
    fclose(f);
    return strdup(path);
}

static void test_sniff_mime(void) {
    unsigned char png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00};
    const char *m = sniff_mime_from_bytes(png, sizeof(png));
    TEST("sniff PNG", m && strcmp(m, "image/png") == 0);

    unsigned char jpg[] = {0xFF, 0xD8, 0xFF, 0xE0};
    m = sniff_mime_from_bytes(jpg, sizeof(jpg));
    TEST("sniff JPEG", m && strcmp(m, "image/jpeg") == 0);

    unsigned char gif[] = {'G', 'I', 'F', '8', '9', 'a'};
    m = sniff_mime_from_bytes(gif, sizeof(gif));
    TEST("sniff GIF", m && strcmp(m, "image/gif") == 0);

    unsigned char webp[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
    m = sniff_mime_from_bytes(webp, sizeof(webp));
    TEST("sniff WEBP", m && strcmp(m, "image/webp") == 0);

    unsigned char bmp[] = {'B', 'M', 0, 0};
    m = sniff_mime_from_bytes(bmp, sizeof(bmp));
    TEST("sniff BMP", m && strcmp(m, "image/bmp") == 0);

    unsigned char heic[] = {0,0,0,0, 'f','t','y','p', 'h','e','i','c'};
    m = sniff_mime_from_bytes(heic, sizeof(heic));
    TEST("sniff HEIC", m && strcmp(m, "image/heic") == 0);

    unsigned char unknown[] = {0, 1, 2, 3};
    m = sniff_mime_from_bytes(unknown, sizeof(unknown));
    TEST("sniff unknown", m == NULL);

    TEST("sniff null", sniff_mime_from_bytes(NULL, 0) == NULL);
}

static void test_guess_mime(void) {
    const char *m = guess_mime("photo.jpg", NULL, 0);
    TEST("guess .jpg", m && strcmp(m, "image/jpeg") == 0);

    m = guess_mime("photo.jpeg", NULL, 0);
    TEST("guess .jpeg", m && strcmp(m, "image/jpeg") == 0);

    m = guess_mime("image.png", NULL, 0);
    TEST("guess .png", m && strcmp(m, "image/png") == 0);

    m = guess_mime("animation.gif", NULL, 0);
    TEST("guess .gif", m && strcmp(m, "image/gif") == 0);

    m = guess_mime("image.webp", NULL, 0);
    TEST("guess .webp", m && strcmp(m, "image/webp") == 0);

    m = guess_mime("image.bmp", NULL, 0);
    TEST("guess .bmp", m && strcmp(m, "image/bmp") == 0);

    m = guess_mime("image.heic", NULL, 0);
    TEST("guess .heic", m && strcmp(m, "image/heic") == 0);

    m = guess_mime("image.xyz", NULL, 0);
    TEST("guess unknown suffix", m && strcmp(m, "image/jpeg") == 0);

    unsigned char jpg[] = {0xFF, 0xD8, 0xFF, 0xE0};
    m = guess_mime("fake.png", jpg, sizeof(jpg));
    TEST("guess bytes override", m && strcmp(m, "image/jpeg") == 0);

    m = guess_mime(NULL, NULL, 0);
    TEST("guess null path", m && strcmp(m, "image/jpeg") == 0);
}

static void test_file_to_data_url(void) {
    char *path = create_temp_png();
    TEST("create temp png", path != NULL);
    if (!path) return;

    char *url = file_to_data_url(path);
    TEST("data URL", url != NULL);
    if (url) {
        TEST("data URL prefix", strncmp(url, "data:image/png;base64,", 22) == 0);
        TEST("data URL non-empty", strlen(url) > 30);
        printf("  data_url len=%zu\n", strlen(url));
        free(url);
    }

    TEST("nonexistent file", file_to_data_url("/tmp/nonexistent_xyz.png") == NULL);
    TEST("null path", file_to_data_url(NULL) == NULL);

    free(path);
}

static void test_build_native_content_parts(void) {
    char *path = create_temp_png();
    TEST("create temp png for parts", path != NULL);
    if (!path) return;

    const char *paths[] = {path};
    char **skipped = NULL;
    size_t skip_cnt = 0;

    char *result = build_native_content_parts("What's in this image?", paths, 1, &skipped, &skip_cnt);
    TEST("content parts not null", result != NULL);
    if (result) {
        TEST("has text part", strstr(result, "\"type\":\"text\"") != NULL);
        TEST("has image_url part", strstr(result, "\"type\":\"image_url\"") != NULL);
        TEST("has base64 data", strstr(result, "data:image/png;base64,") != NULL);
        TEST("has path hint", strstr(result, "[Image attached at:") != NULL);
        printf("  content: %s\n", result);
        free(result);
    }
    TEST("no skipped paths", skip_cnt == 0);
    if (skipped) free(skipped);

    /* No image paths — plain text mode */
    char *no_img = build_native_content_parts("Hello", NULL, 0, NULL, NULL);
    TEST("no images not null", no_img != NULL);
    if (no_img) {
        TEST("no images has text type", strstr(no_img, "\"type\":\"text\"") != NULL);
        TEST("no images has hello", strstr(no_img, "Hello") != NULL);
        free(no_img);
    }

    free(path);
}

int main(void) {
    printf("=== Image Routing Tests ===\n");

    test_sniff_mime();
    test_guess_mime();
    test_file_to_data_url();
    test_build_native_content_parts();

    printf("\nResults: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
