/*
 * binary.c — Binary file extension detection implementation.
 * Port of Python tools/binary_extensions.py.
 *
 * Uses a sorted array of binary extension strings with bsearch()
 * for O(log n) lookups.
 *
 * Extensions tracked in Python:
 *   Images: .png .jpg .jpeg .gif .bmp .ico .webp .tiff .tif
 *   Videos: .mp4 .mov .avi .mkv .webm .wmv .flv .m4v .mpeg .mpg
 *   Audio:  .mp3 .wav .ogg .flac .aac .m4a .wma .aiff .opus
 *   Archives: .zip .tar .gz .bz2 .7z .rar .xz .z .tgz .iso
 *   Executables: .exe .dll .so .dylib .bin .o .a .obj .lib
 *               .app .msi .deb .rpm
 *   Documents: .doc .docx .xls .xlsx .ppt .pptx .odt .ods .odp
 *   Fonts:   .ttf .otf .woff .woff2 .eot
 *   Bytecode: .pyc .pyo .class .jar .war .ear .node .wasm .rlib
 *   Database: .sqlite .sqlite3 .db .mdb .idx
 *   Design:  .psd .ai .eps .sketch .fig .xd .blend .3ds .max
 *   Flash:   .swf .fla
 *   Other:   .lockb .dat .data
 */

#include "binary.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ─── Binary Extension Set ───────────────────────────────── */

static const char *const BINARY_EXTENSIONS[] = {
    /* Numeric prefixes — sorted for bsearch */
    ".3ds", ".7z", ".a", ".aac", ".ai", ".aiff", ".app",
    ".avi", ".bin", ".blend", ".bmp", ".bz2",
    ".class", ".dat", ".data", ".db", ".deb", ".dll", ".doc", ".docx",
    ".dylib", ".ear", ".eot", ".eps", ".exe",
    ".fig", ".fla", ".flac", ".flv",
    ".gif", ".gz",
    ".ico", ".idx", ".iso",
    ".jar", ".jpeg", ".jpg",
    ".lib", ".lockb",
    ".m4a", ".m4v", ".max", ".mdb", ".mkv", ".mov", ".mp3", ".mp4",
    ".mpeg", ".mpg", ".msi",
    ".node",
    ".o", ".obj", ".odp", ".ods", ".odt", ".ogg", ".opus", ".otf",
    ".pdf", ".png", ".ppt", ".pptx", ".psd", ".pyc", ".pyo",
    ".rar", ".rlib", ".rpm",
    ".sketch", ".so", ".sqlite", ".sqlite3", ".swf",
    ".tar", ".tgz", ".tif", ".tiff", ".ttf",
    ".wad", ".war", ".wasm", ".wav", ".webm", ".webp", ".wma", ".wmv",
    ".woff", ".woff2",
    ".x", ".xd", ".xls", ".xlsx", ".xz",
    ".z", ".zip",
};

#define NUM_EXTENSIONS (sizeof(BINARY_EXTENSIONS) / sizeof(BINARY_EXTENSIONS[0]))

/* ─── Comparison Helper ──────────────────────────────────── */

static int ext_cmp(const void *a, const void *b)
{
    const char *key = *(const char **)a;
    const char *ext = *(const char **)b;
    return strcmp(key, ext);
}

/* ─── Public API ─────────────────────────────────────────── */

bool has_binary_extension(const char *path)
{
    if (!path || !*path)
        return false;

    /* Find last dot */
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return false;

    /* Lowercase the extension for comparison */
    static char ext[32];
    size_t len = 0;

    /* Copy and lowercase */
    while (*dot && len < sizeof(ext) - 1) {
        char c = *dot;
        ext[len++] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        dot++;
    }
    ext[len] = '\0';

    /* bsearch: key is pointer to ext string, array elements are const char * */
    const char *key = ext;
    return bsearch(&key, BINARY_EXTENSIONS, NUM_EXTENSIONS,
                   sizeof(const char *), ext_cmp) != NULL;
}
