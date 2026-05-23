/*
 * test_binary.c — Tests for binary extension detection.
 * Port of Python tools/binary_extensions.py test suite.
 */

#include "binary.h"
#include <stdio.h>
#include <string.h>

static int tests = 0;
static int passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ─── Binary extensions that should return true ────────── */

static void test_image_extensions(void)
{
    TEST("png",  has_binary_extension("image.png"));
    TEST("jpg",  has_binary_extension("photo.jpg"));
    TEST("jpeg", has_binary_extension("photo.jpeg"));
    TEST("gif",  has_binary_extension("animation.gif"));
    TEST("bmp",  has_binary_extension("bitmap.bmp"));
    TEST("ico",  has_binary_extension("icon.ico"));
    TEST("webp", has_binary_extension("image.webp"));
    TEST("tiff", has_binary_extension("scan.tiff"));
    TEST("tif",  has_binary_extension("scan.tif"));
}

static void test_video_extensions(void)
{
    TEST("mp4",  has_binary_extension("video.mp4"));
    TEST("mov",  has_binary_extension("video.mov"));
    TEST("avi",  has_binary_extension("video.avi"));
    TEST("mkv",  has_binary_extension("video.mkv"));
    TEST("webm", has_binary_extension("video.webm"));
    TEST("wmv",  has_binary_extension("video.wmv"));
    TEST("flv",  has_binary_extension("video.flv"));
    TEST("m4v",  has_binary_extension("video.m4v"));
    TEST("mpeg", has_binary_extension("video.mpeg"));
    TEST("mpg",  has_binary_extension("video.mpg"));
}

static void test_audio_extensions(void)
{
    TEST("mp3",  has_binary_extension("audio.mp3"));
    TEST("wav",  has_binary_extension("audio.wav"));
    TEST("ogg",  has_binary_extension("audio.ogg"));
    TEST("flac", has_binary_extension("audio.flac"));
    TEST("aac",  has_binary_extension("audio.aac"));
    TEST("m4a",  has_binary_extension("audio.m4a"));
    TEST("wma",  has_binary_extension("audio.wma"));
    TEST("aiff", has_binary_extension("audio.aiff"));
    TEST("opus", has_binary_extension("audio.opus"));
}

static void test_archive_extensions(void)
{
    TEST("zip", has_binary_extension("archive.zip"));
    TEST("tar", has_binary_extension("archive.tar"));
    TEST("gz",  has_binary_extension("archive.gz"));
    TEST("bz2", has_binary_extension("archive.bz2"));
    TEST("7z",  has_binary_extension("archive.7z"));
    TEST("rar", has_binary_extension("archive.rar"));
    TEST("xz",  has_binary_extension("archive.xz"));
    TEST("z",   has_binary_extension("archive.z"));
    TEST("tgz", has_binary_extension("archive.tgz"));
    TEST("iso", has_binary_extension("disc.iso"));
}

static void test_executable_extensions(void)
{
    TEST("exe",   has_binary_extension("program.exe"));
    TEST("dll",   has_binary_extension("library.dll"));
    TEST("so",    has_binary_extension("library.so"));
    TEST("dylib", has_binary_extension("library.dylib"));
    TEST("bin",   has_binary_extension("firmware.bin"));
    TEST("o",     has_binary_extension("object.o"));
    TEST("a",     has_binary_extension("archive.a"));
    TEST("obj",   has_binary_extension("object.obj"));
    TEST("lib",   has_binary_extension("library.lib"));
    TEST("app",   has_binary_extension("Application.app"));
    TEST("msi",   has_binary_extension("installer.msi"));
    TEST("deb",   has_binary_extension("package.deb"));
    TEST("rpm",   has_binary_extension("package.rpm"));
}

static void test_document_extensions(void)
{
    TEST("doc",   has_binary_extension("document.doc"));
    TEST("docx",  has_binary_extension("document.docx"));
    TEST("xls",   has_binary_extension("spreadsheet.xls"));
    TEST("xlsx",  has_binary_extension("spreadsheet.xlsx"));
    TEST("ppt",   has_binary_extension("slides.ppt"));
    TEST("pptx",  has_binary_extension("slides.pptx"));
    TEST("odt",   has_binary_extension("document.odt"));
    TEST("ods",   has_binary_extension("spreadsheet.ods"));
    TEST("odp",   has_binary_extension("presentation.odp"));
}

static void test_font_extensions(void)
{
    TEST("ttf",   has_binary_extension("font.ttf"));
    TEST("otf",   has_binary_extension("font.otf"));
    TEST("woff",  has_binary_extension("font.woff"));
    TEST("woff2", has_binary_extension("font.woff2"));
    TEST("eot",   has_binary_extension("font.eot"));
}

static void test_bytecode_extensions(void)
{
    TEST("pyc",   has_binary_extension("module.pyc"));
    TEST("pyo",   has_binary_extension("module.pyo"));
    TEST("class", has_binary_extension("Main.class"));
    TEST("jar",   has_binary_extension("app.jar"));
    TEST("war",   has_binary_extension("app.war"));
    TEST("ear",   has_binary_extension("app.ear"));
    TEST("node",  has_binary_extension("module.node"));
    TEST("wasm",  has_binary_extension("module.wasm"));
    TEST("rlib",  has_binary_extension("library.rlib"));
}

static void test_database_extensions(void)
{
    TEST("sqlite",  has_binary_extension("data.sqlite"));
    TEST("sqlite3", has_binary_extension("data.sqlite3"));
    TEST("db",      has_binary_extension("data.db"));
    TEST("mdb",     has_binary_extension("data.mdb"));
    TEST("idx",     has_binary_extension("index.idx"));
}

static void test_design_extensions(void)
{
    TEST("psd",   has_binary_extension("design.psd"));
    TEST("ai",    has_binary_extension("illustrator.ai"));
    TEST("eps",   has_binary_extension("vector.eps"));
    TEST("sketch", has_binary_extension("design.sketch"));
    TEST("fig",   has_binary_extension("design.fig"));
    TEST("xd",    has_binary_extension("design.xd"));
    TEST("blend", has_binary_extension("model.blend"));
    TEST("3ds",   has_binary_extension("model.3ds"));
    TEST("max",   has_binary_extension("model.max"));
}

static void test_flash_extensions(void)
{
    TEST("swf", has_binary_extension("animation.swf"));
    TEST("fla", has_binary_extension("source.fla"));
}

static void test_other_extensions(void)
{
    TEST("lockb", has_binary_extension("package-lock.lockb"));
    TEST("dat",   has_binary_extension("data.dat"));
    TEST("data",  has_binary_extension("output.data"));
}

/* ─── Text extensions that should return false ─────────── */

static void test_text_extensions(void)
{
    TEST("txt",      !has_binary_extension("readme.txt"));
    TEST("md",       !has_binary_extension("README.md"));
    TEST("c",        !has_binary_extension("main.c"));
    TEST("h",        !has_binary_extension("header.h"));
    TEST("py",       !has_binary_extension("script.py"));
    TEST("js",       !has_binary_extension("app.js"));
    TEST("ts",       !has_binary_extension("app.ts"));
    TEST("json",     !has_binary_extension("config.json"));
    TEST("yaml",     !has_binary_extension("config.yaml"));
    TEST("yml",      !has_binary_extension("config.yml"));
    TEST("toml",     !has_binary_extension("config.toml"));
    TEST("xml",      !has_binary_extension("data.xml"));
    TEST("html",     !has_binary_extension("index.html"));
    TEST("css",      !has_binary_extension("style.css"));
    TEST("sh",       !has_binary_extension("script.sh"));
    TEST("bash",     !has_binary_extension("script.bash"));
    TEST("rs",       !has_binary_extension("main.rs"));
    TEST("go",       !has_binary_extension("main.go"));
    TEST("rb",       !has_binary_extension("script.rb"));
    TEST("pl",       !has_binary_extension("script.pl"));
    TEST("lua",      !has_binary_extension("script.lua"));
    TEST("sql",      !has_binary_extension("query.sql"));
    TEST("cfg",      !has_binary_extension("config.cfg"));
    TEST("ini",      !has_binary_extension("settings.ini"));
    TEST("conf",     !has_binary_extension("nginx.conf"));
    TEST("env",      !has_binary_extension(".env"));
    TEST("gitignore",!has_binary_extension(".gitignore"));
}

/* ─── Edge cases ───────────────────────────────────────── */

static void test_edge_cases(void)
{
    TEST("no extension",         !has_binary_extension("Makefile"));
    TEST("hidden file no ext",   !has_binary_extension(".gitignore"));
    TEST("empty string",         !has_binary_extension(""));
    TEST("null pointer",         !has_binary_extension(NULL));
    TEST("dotfile",              !has_binary_extension(".profile"));
    TEST("double dot path",      has_binary_extension("archive.tar.gz"));
    TEST("uppercase PNG",        has_binary_extension("image.PNG"));
    TEST("uppercase JPG",        has_binary_extension("photo.JPG"));
    TEST("uppercase ZIP",        has_binary_extension("archive.ZIP"));
    TEST("mixed case",           has_binary_extension("image.Png"));
    TEST("full path binary",     has_binary_extension("/usr/bin/program.exe"));
    TEST("full path text",       !has_binary_extension("/home/user/readme.txt"));
    TEST("relative path binary", has_binary_extension("./images/photo.jpg"));
    TEST("relative path text",   !has_binary_extension("./src/main.c"));
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_image_extensions();
    test_video_extensions();
    test_audio_extensions();
    test_archive_extensions();
    test_executable_extensions();
    test_document_extensions();
    test_font_extensions();
    test_bytecode_extensions();
    test_database_extensions();
    test_design_extensions();
    test_flash_extensions();
    test_other_extensions();
    test_text_extensions();
    test_edge_cases();

    printf("binary: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
