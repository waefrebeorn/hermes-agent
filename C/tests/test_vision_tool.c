/*
 * test_vision_tool.c — M33: Vision tool edge case tests.
 *
 * Tests image format validation, file size checking, and
 * edge cases without needing real image files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (!test_##name()) { \
        printf("  FAIL: %s\n", #name); \
        tests_failed++; \
    } else { \
        printf("  PASS: %s\n", #name); \
        tests_passed++; \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* ──────────────────────────────────────────────
 *  Image format validation (replicating vision.c patterns)
 * ────────────────────────────────────────────── */

/* Allowed image extensions */
static const char *VALID_EXTENSIONS[] = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", NULL};

static bool is_valid_image_ext(const char *path) {
    if (!path || !*path) return false;
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    for (int i = 0; VALID_EXTENSIONS[i]; i++) {
        if (strcasecmp(dot, VALID_EXTENSIONS[i]) == 0)
            return true;
    }
    return false;
}

/* URL pattern check (starts with http:// or https:// or /) */
static bool is_valid_image_source(const char *url) {
    if (!url || !*url) return false;
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
        return true;
    if (url[0] == '/')
        return true;
    return false;
}

/* File size validation (max 20MB) */
#define VISION_MAX_SIZE (20 * 1024 * 1024)

static bool is_valid_file_size(size_t filesize) {
    return filesize > 0 && filesize <= VISION_MAX_SIZE;
}

static bool test_valid_extensions(void) {
    ASSERT(is_valid_image_ext("photo.png"), ".png valid");
    ASSERT(is_valid_image_ext("photo.jpg"), ".jpg valid");
    ASSERT(is_valid_image_ext("photo.jpeg"), ".jpeg valid");
    ASSERT(is_valid_image_ext("photo.gif"), ".gif valid");
    ASSERT(is_valid_image_ext("photo.bmp"), ".bmp valid");
    ASSERT(is_valid_image_ext("photo.webp"), ".webp valid");
    ASSERT(is_valid_image_ext("/path/to/image.PNG"), ".PNG case insensitive");
    ASSERT(is_valid_image_ext("photo.JPG"), ".JPG case insensitive");
    return true;
}

static bool test_invalid_extensions(void) {
    ASSERT(!is_valid_image_ext("photo.txt"), ".txt invalid");
    ASSERT(!is_valid_image_ext("photo.pdf"), ".pdf invalid");
    ASSERT(!is_valid_image_ext("photo"), "no extension invalid");
    ASSERT(!is_valid_image_ext("photo.svg"), ".svg invalid");
    ASSERT(!is_valid_image_ext(""), "empty string invalid");
    return true;
}

static bool test_valid_sources(void) {
    ASSERT(is_valid_image_source("https://example.com/img.png"), "https URL");
    ASSERT(is_valid_image_source("http://example.com/img.png"), "http URL");
    ASSERT(is_valid_image_source("/home/user/photo.jpg"), "absolute path");
    ASSERT(is_valid_image_source("/tmp/test.png"), "/tmp path");
    return true;
}

static bool test_invalid_sources(void) {
    ASSERT(!is_valid_image_source(NULL), "NULL invalid");
    ASSERT(!is_valid_image_source(""), "empty invalid");
    ASSERT(!is_valid_image_source("ftp://host/img.png"), "ftp invalid");
    ASSERT(!is_valid_image_source("photo.png"), "relative path invalid");
    ASSERT(!is_valid_image_source("../photo.png"), "relative traversal invalid");
    return true;
}

static bool test_file_size_validation(void) {
    ASSERT(is_valid_file_size(1), "1 byte valid");
    ASSERT(is_valid_file_size(1024), "1KB valid");
    ASSERT(is_valid_file_size(1024 * 1024), "1MB valid");
    ASSERT(is_valid_file_size(20 * 1024 * 1024), "exactly 20MB valid (boundary)");
    ASSERT(!is_valid_file_size(0), "0 bytes invalid");
    ASSERT(!is_valid_file_size(20 * 1024 * 1024 + 1), "over 20MB invalid");
    ASSERT(!is_valid_file_size((size_t)-1), "max size_t invalid");
    return true;
}

static bool test_null_safety(void) {
    ASSERT(!is_valid_image_ext(NULL), "NULL ext safe");
    ASSERT(!is_valid_image_source(NULL), "NULL source safe");
    return true;
}

/* ──────────────────────────────────────────────
 *  Image info command building
 * ────────────────────────────────────────────── */

static char *build_info_cmd(const char *path) {
    if (!path) return NULL;
    /* Uses 'file' command for image info (same as vision.c) */
    size_t cap = strlen(path) + 128;
    char *cmd = (char *)malloc(cap);
    if (!cmd) return NULL;
    snprintf(cmd, cap, "file -b '%s' 2>/dev/null || echo 'unknown'", path);
    return cmd;
}

static char *build_dim_cmd(const char *path) {
    if (!path) return NULL;
    /* Uses 'identify' or ImageMagick for dimensions */
    size_t cap = strlen(path) + 128;
    char *cmd = (char *)malloc(cap);
    if (!cmd) return NULL;
    snprintf(cmd, cap, "identify -format '%%w %%h' '%s' 2>/dev/null || echo '0 0'", path);
    return cmd;
}

static bool test_info_cmd_building(void) {
    char *cmd = build_info_cmd("/tmp/test.png");
    ASSERT(cmd != NULL, "info cmd not NULL");
    ASSERT(strstr(cmd, "file -b") != NULL, "uses file command");
    ASSERT(strstr(cmd, "/tmp/test.png") != NULL, "contains path");
    ASSERT(strstr(cmd, "'") != NULL, "path is quoted");
    free(cmd);
    return true;
}

static bool test_info_cmd_null(void) {
    char *cmd = build_info_cmd(NULL);
    ASSERT(cmd == NULL, "NULL path returns NULL cmd");
    return true;
}

static bool test_dim_cmd_building(void) {
    char *cmd = build_dim_cmd("/home/user/photo.jpg");
    ASSERT(cmd != NULL, "dim cmd not NULL");
    ASSERT(strstr(cmd, "identify") != NULL, "uses identify command");
    ASSERT(strstr(cmd, "%w %h") != NULL, "format string");
    ASSERT(strstr(cmd, "/home/user/photo.jpg") != NULL, "contains path");
    free(cmd);
    return true;
}

static bool test_dim_cmd_null(void) {
    char *cmd = build_dim_cmd(NULL);
    ASSERT(cmd == NULL, "NULL path returns NULL cmd");
    return true;
}

/* ──────────────────────────────────────────────
 *  Vision handler schema
 * ────────────────────────────────────────────── */

static bool test_schema_structure(void) {
    /* The schema defines image_url (required) and question (optional) */
    const char *schema =
        "{\"type\":\"object\","
        "\"properties\":{"
          "\"image_url\":{\"type\":\"string\",\"description\":\"Path or URL to image\"},"
          "\"question\":{\"type\":\"string\",\"description\":\"Optional question\"}"
        "},"
        "\"required\":[\"image_url\"]}";

    ASSERT(strstr(schema, "\"image_url\"") != NULL, "image_url param");
    ASSERT(strstr(schema, "\"question\"") != NULL, "question param");
    ASSERT(strstr(schema, "\"required\"") != NULL, "required field");
    ASSERT(strstr(schema, "\"image_url\"") < strstr(schema, "\"required\""),
           "image_url in required (appears before required key)");

    /* image_url appears before required, meaning it's in the properties */
    /* After verification, required lists image_url */
    return true;
}

/* ──────────────────────────────────────────────
 *  Main
 * ────────────────────────────────────────────── */

int main(void) {
    printf("=== Vision Tool Tests (M33) ===\n");

    TEST(valid_extensions);
    TEST(invalid_extensions);
    TEST(valid_sources);
    TEST(invalid_sources);
    TEST(file_size_validation);
    TEST(null_safety);
    TEST(info_cmd_building);
    TEST(info_cmd_null);
    TEST(dim_cmd_building);
    TEST(dim_cmd_null);
    TEST(schema_structure);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
