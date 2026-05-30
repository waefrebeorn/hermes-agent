/*
 * test_video_mime.c — Tests for video MIME type detection.
 * Expanded: edge cases for extension parsing, case variants, special chars.
 * Standalone — no deps beyond stdlib.
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

    /* Basic tests */
    TEST_MIME("NULL path", NULL, NULL);
    TEST_MIME("no extension", "video", NULL);
    TEST_MIME(".mp4", "video.mp4", "video/mp4");
    TEST_MIME(".MP4 uppercase", "video.MP4", "video/mp4");
    TEST_MIME(".webm", "video.webm", "video/webm");
    TEST_MIME(".mov", "video.mov", "video/mov");
    TEST_MIME(".avi -> mp4", "video.avi", "video/mp4");
    TEST_MIME(".mkv -> mp4", "video.mkv", "video/mp4");
    TEST_MIME(".mpeg", "video.mpeg", "video/mpeg");
    TEST_MIME(".mpg", "video.mpg", "video/mpeg");
    TEST_MIME(".png not video", "image.png", NULL);
    TEST_MIME("full path", "/tmp/videos/my_clip.mp4", "video/mp4");
    TEST_MIME("dot in dirname", "/tmp/v1.0/output.mov", "video/mov");
    TEST_MIME("trailing dot", "video.", NULL);

    /* ── Case variant edge cases ── */
    printf("\n--- Case variants ---\n");
    TEST_MIME(".Mp4 camel", "video.Mp4", "video/mp4");
    TEST_MIME(".MP4 all caps", "video.MP4", "video/mp4");
    TEST_MIME(".mP4 mixed", "video.mP4", "video/mp4");
    TEST_MIME(".WebM", "video.WebM", "video/webm");
    TEST_MIME(".WEBM", "video.WEBM", "video/webm");
    TEST_MIME(".Mov", "video.Mov", "video/mov");
    TEST_MIME(".MOV", "video.MOV", "video/mov");
    TEST_MIME(".AvI", "video.AvI", "video/mp4");
    TEST_MIME(".AVI", "video.AVI", "video/mp4");
    TEST_MIME(".Mkv", "video.Mkv", "video/mp4");
    TEST_MIME(".MKV", "video.MKV", "video/mp4");
    TEST_MIME(".Mpeg", "video.Mpeg", "video/mpeg");
    TEST_MIME(".MPEG", "video.MPEG", "video/mpeg");
    TEST_MIME(".Mpg", "video.Mpg", "video/mpeg");
    TEST_MIME(".MPG", "video.MPG", "video/mpeg");

    /* ── Edge case: multiple dots ── */
    printf("\n--- Multiple dots ---\n");
    TEST_MIME("double extension .tar.mp4", "video.tar.mp4", "video/mp4");
    TEST_MIME("double extension .mp4.bak", "video.mp4.bak", NULL);
    TEST_MIME("hidden file .video.mp4", ".video.mp4", "video/mp4");
    TEST_MIME("hidden no ext .video", ".video", NULL);
    TEST_MIME("just dot .", ".", NULL);
    TEST_MIME("double dot ..", "..", NULL);
    TEST_MIME("multiple dots .a.b.c.mp4", "file.a.b.c.mp4", "video/mp4");

    /* ── Path edge cases ── */
    printf("\n--- Path edge cases ---\n");
    TEST_MIME("empty string", "", NULL);
    TEST_MIME("just slash", "/", NULL);
    TEST_MIME("no dot absolute", "/tmp/video", NULL);
    TEST_MIME("space in path", "/tmp/my video.mp4", "video/mp4");
    TEST_MIME("underscore in path", "/tmp/my_video_file.mp4", "video/mp4");
    TEST_MIME("hyphen in path", "/tmp/my-video-2024.mp4", "video/mp4");
    TEST_MIME("parent dir reference", "../video.mp4", "video/mp4");
    TEST_MIME("very long name", "/tmp/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.mp4", "video/mp4");
    TEST_MIME("numbers in name", "video123.mp4", "video/mp4");

    /* ── Extension length edge cases ── */
    printf("\n--- Extension length ---\n");
    TEST_MIME("single char .m", "video.m", NULL);
    TEST_MIME("two char .m4", "video.m4", NULL);
    TEST_MIME("three char exact .m4v", "video.m4v", NULL);
    TEST_MIME("fifteen char .abcdefghijklmno", "video.abcdefghijklmno", NULL);

    /* ── Non-video formats ── */
    printf("\n--- Non-video formats ---\n");
    TEST_MIME(".jpg not video", "photo.jpg", NULL);
    TEST_MIME(".jpeg not video", "photo.jpeg", NULL);
    TEST_MIME(".gif not video", "anim.gif", NULL);
    TEST_MIME(".bmp not video", "img.bmp", NULL);
    TEST_MIME(".txt not video", "notes.txt", NULL);
    TEST_MIME(".pdf not video", "doc.pdf", NULL);
    TEST_MIME(".zip not video", "archive.zip", NULL);
    TEST_MIME(".html not video", "page.html", NULL);

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All video MIME detection tests PASSED");
    return failures;
}
