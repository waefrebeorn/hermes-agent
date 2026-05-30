/*
 * test_binary_extensions.c — Tests for binary extension detection.
 * Port of Python tools/binary_extensions.py.
 *
 * Tests: known binary extensions, non-binary extensions, edge cases.
 */

#include "binary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static void test_image_extensions(void) {
    printf("\n--- Image extensions ---\n");
    TEST(".png", has_binary_extension("photo.png"));
    TEST(".jpg", has_binary_extension("image.jpg"));
    TEST(".jpeg", has_binary_extension("photo.jpeg"));
    TEST(".gif", has_binary_extension("anim.gif"));
    TEST(".bmp", has_binary_extension("bitmap.bmp"));
    TEST(".webp", has_binary_extension("pic.webp"));
    TEST(".ico", has_binary_extension("favicon.ico"));
    TEST(".tiff", has_binary_extension("scan.tiff"));
    TEST(".tif", has_binary_extension("old_scan.tif"));
}

static void test_video_extensions(void) {
    printf("\n--- Video extensions ---\n");
    TEST(".mp4", has_binary_extension("video.mp4"));
    TEST(".mov", has_binary_extension("clip.mov"));
    TEST(".avi", has_binary_extension("movie.avi"));
    TEST(".mkv", has_binary_extension("film.mkv"));
    TEST(".webm", has_binary_extension("stream.webm"));
    TEST(".flv", has_binary_extension("flash.flv"));
    TEST(".mpeg", has_binary_extension("old.mpeg"));
    TEST(".mpg", has_binary_extension("older.mpg"));
}

static void test_archive_extensions(void) {
    printf("\n--- Archive extensions ---\n");
    TEST(".zip", has_binary_extension("files.zip"));
    TEST(".tar", has_binary_extension("archive.tar"));
    TEST(".gz", has_binary_extension("data.tar.gz"));
    TEST(".bz2", has_binary_extension("data.tar.bz2"));
    TEST(".7z", has_binary_extension("packed.7z"));
    TEST(".rar", has_binary_extension("compressed.rar"));
    TEST(".xz", has_binary_extension("docs.tar.xz"));
}

static void test_executable_extensions(void) {
    printf("\n--- Executable extensions ---\n");
    TEST(".exe", has_binary_extension("installer.exe"));
    TEST(".dll", has_binary_extension("library.dll"));
    TEST(".so", has_binary_extension("plugin.so"));
    TEST(".dylib", has_binary_extension("lib.dylib"));
    TEST(".bin", has_binary_extension("firmware.bin"));
    TEST(".o", has_binary_extension("module.o"));
    TEST(".a", has_binary_extension("libstatic.a"));
    TEST(".deb", has_binary_extension("package.deb"));
    TEST(".rpm", has_binary_extension("package.rpm"));
}

static void test_non_binary(void) {
    printf("\n--- Non-binary extensions ---\n");
    TEST(".txt", !has_binary_extension("readme.txt"));
    TEST(".md", !has_binary_extension("README.md"));
    TEST(".c", !has_binary_extension("main.c"));
    TEST(".h", !has_binary_extension("header.h"));
    TEST(".py", !has_binary_extension("script.py"));
    TEST(".json", !has_binary_extension("data.json"));
    TEST(".yaml", !has_binary_extension("config.yaml"));
    TEST(".html", !has_binary_extension("index.html"));
    TEST(".css", !has_binary_extension("style.css"));
    TEST(".js", !has_binary_extension("app.js"));
    TEST(".ts", !has_binary_extension("app.ts"));
    TEST(".rs", !has_binary_extension("lib.rs"));
    TEST(".go", !has_binary_extension("main.go"));
    TEST(".pdf", !has_binary_extension("document.pdf"));
}

static void test_edge_cases(void) {
    printf("\n--- Edge cases ---\n");
    TEST("NULL path", !has_binary_extension(NULL));
    TEST("empty path", !has_binary_extension(""));
    TEST("no extension", !has_binary_extension("Makefile"));
    TEST("hidden file", has_binary_extension(".hidden.png"));  /* has .png */
    TEST("dot at start", !has_binary_extension(".gitignore"));  /* but no extension itself — the path starts with .gitignore, rfind('.') returns 0, extension would be ".gitignore" */
    TEST("uppercase", has_binary_extension("IMAGE.PNG"));
    TEST("mixed case", has_binary_extension("Photo.JpEg"));
    TEST("path with dirs", has_binary_extension("/usr/local/bin/data.zip"));
    TEST("relative path", has_binary_extension("./images/photo.png"));
    TEST("multi-dot", has_binary_extension("archive.tar.gz"));
}

int main(void) {
    printf("=== Binary Extension Tests ===\n");

    test_image_extensions();
    test_video_extensions();
    test_archive_extensions();
    test_executable_extensions();
    test_non_binary();
    test_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
