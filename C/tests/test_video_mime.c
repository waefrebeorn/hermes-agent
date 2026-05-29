/*
 * test_video_mime.c — Tests for video MIME type detection.
 * Standalone — no deps beyond stdlib.
 * Compile: gcc -O2 -Wall -Wextra -o /tmp/t_vm test_video_mime.c -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* Inline version of vision_detect_video_mime_type() for testing */
static const char *detect_video_mime_type(const char *path) {
    if (!path) return NULL;
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    dot++;

    char ext[16];
    size_t i;
    for (i = 0; i < sizeof(ext) - 1 && dot[i]; i++)
        ext[i] = (char)tolower((unsigned char)dot[i]);
    ext[i] = '\0';

    if (strcmp(ext, "mp4") == 0)  return "video/mp4";
    if (strcmp(ext, "webm") == 0) return "video/webm";
    if (strcmp(ext, "mov") == 0)  return "video/mov";
    if (strcmp(ext, "avi") == 0)  return "video/mp4";
    if (strcmp(ext, "mkv") == 0)  return "video/mp4";
    if (strcmp(ext, "mpeg") == 0 || strcmp(ext, "mpg") == 0) return "video/mpeg";
    return NULL;
}

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

#define TEST_MIME(name, path, expected) do { \
    const char *r = detect_video_mime_type(path); \
    bool pass = (expected == NULL && r == NULL) || \
                (expected != NULL && r != NULL && strcmp(r, expected) == 0); \
    if (!pass) { fprintf(stderr, "  FAIL: %s (got=%s, expected=%s)\n", name, r ? r : "NULL", expected ? expected : "NULL"); failures++; } \
    else printf("  PASS: %s -> %s\n", name, r ? r : "NULL"); \
} while (0)

int main(void) {
    printf("=== Video MIME Type Detection Tests ===\n");

    /* Test 1: NULL path returns NULL */
    TEST_MIME("NULL path", NULL, NULL);

    /* Test 2: Empty string (no dot) returns NULL */
    TEST_MIME("no extension", "video", NULL);

    /* Test 3: .mp4 extension */
    TEST_MIME(".mp4", "video.mp4", "video/mp4");

    /* Test 4: .MP4 uppercase extension */
    TEST_MIME(".MP4 uppercase", "video.MP4", "video/mp4");

    /* Test 5: .webm extension */
    TEST_MIME(".webm", "video.webm", "video/webm");

    /* Test 6: .mov extension */
    TEST_MIME(".mov", "video.mov", "video/mov");

    /* Test 7: .avi extension (maps to video/mp4) */
    TEST_MIME(".avi -> mp4", "video.avi", "video/mp4");

    /* Test 8: .mkv extension (maps to video/mp4) */
    TEST_MIME(".mkv -> mp4", "video.mkv", "video/mp4");

    /* Test 9: .mpeg extension */
    TEST_MIME(".mpeg", "video.mpeg", "video/mpeg");

    /* Test 10: .mpg extension */
    TEST_MIME(".mpg", "video.mpg", "video/mpeg");

    /* Test 11: .png extension (not a video format) returns NULL */
    TEST_MIME(".png not video", "image.png", NULL);

    /* Test 12: Full path with directory */
    TEST_MIME("full path", "/tmp/videos/my_clip.mp4", "video/mp4");

    /* Test 13: Dot in directory name (should use last dot) */
    TEST_MIME("dot in dirname", "/tmp/v1.0/output.mov", "video/mov");

    /* Test 14: No extension, just a dot */
    TEST_MIME("trailing dot", "video.", NULL);

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All video MIME detection tests PASSED");
    return failures;
}
