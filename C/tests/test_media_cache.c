/*
 * test_media_cache.c — Media cache module tests.
 * Tests media_cache_save and media_cache_cleanup.
 *
 * Build:
 *   gcc -O2 -g -Wall -I include -I lib/libncurses/include tests/test_media_cache.c \
 *       src/tools/media_cache.c -o /tmp/hermes_test_media_cache \
 *       -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes_media_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Media Cache Tests ===\n");

    /* 1. Basic save and retrieve */
    {
        /* A minimal valid PNG (8 bytes magic + minimal IHDR) */
        unsigned char png_data[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  /* PNG magic */
        };
        char *path = media_cache_save("images", png_data, sizeof(png_data), ".png", true);
        TEST("save PNG with image validation", path != NULL);
        if (path) {
            /* File should exist */
            TEST("saved file exists", access(path, F_OK) == 0);
            /* Path should end with .png */
            TEST("saved path ends with .png",
                 strlen(path) > 4 && strcmp(path + strlen(path) - 4, ".png") == 0);
            free(path);
        }
    }

    /* 2. Non-image data fails validation */
    {
        unsigned char text_data[] = "This is not an image file";
        char *path = media_cache_save("images", text_data, sizeof(text_data), ".jpg", true);
        TEST("save non-image with validation returns NULL", path == NULL);
        free(path);
    }

    /* 3. Save without image validation (audio) */
    {
        unsigned char audio_data[] = {0x4F, 0x67, 0x67, 0x53}; /* OggS magic */
        char *path = media_cache_save("audio", audio_data, sizeof(audio_data), ".ogg", false);
        TEST("save audio without validation", path != NULL);
        if (path) {
            TEST("audio file exists", access(path, F_OK) == 0);
            TEST("audio path ends with .ogg",
                 strlen(path) > 4 && strcmp(path + strlen(path) - 4, ".ogg") == 0);
            free(path);
        }
    }

    /* 4. NULL/empty safety */
    {
        char *path;
        path = media_cache_save(NULL, (unsigned char *)"data", 4, ".jpg", true);
        TEST("save NULL type returns NULL", path == NULL);
        free(path);

        path = media_cache_save("images", NULL, 4, ".jpg", true);
        TEST("save NULL data returns NULL", path == NULL);
        free(path);

        path = media_cache_save("images", (unsigned char *)"data", 0, ".jpg", true);
        TEST("save zero length returns NULL", path == NULL);
        free(path);

        path = media_cache_save("images", (unsigned char *)"data", 4, NULL, true);
        TEST("save NULL ext returns NULL", path == NULL);
        free(path);
    }

    /* 5. Cleanup */
    {
        int removed = media_cache_cleanup("images", 0);
        TEST("cleanup with max_age=0 returns 0", removed == 0);

        removed = media_cache_cleanup(NULL, 24);
        TEST("cleanup with NULL type returns 0", removed == 0);

        removed = media_cache_cleanup("nonexistent_type", 24);
        TEST("cleanup nonexistent type returns 0", removed == 0);

        /* Cleanup with very large negative age (remove everything old) */
        /* We need a file to test cleanup... Let's just verify it runs */
        removed = media_cache_cleanup("images", -1);
        TEST("cleanup with negative age returns 0", removed == 0);
    }

    /* 6. media_should_send_as_audio */
    {
        TEST("audio .mp3 send as audio (non-Telegram)",
             media_should_send_as_audio(".mp3", false, false));
        TEST("audio .ogg send as audio (non-Telegram)",
             media_should_send_as_audio(".ogg", false, false));
        TEST("audio .wav send as audio (non-Telegram)",
             media_should_send_as_audio(".wav", false, false));
        TEST("audio .opus send as audio (non-Telegram)",
             media_should_send_as_audio(".opus", false, false));
        TEST("audio .m4a send as audio (non-Telegram)",
             media_should_send_as_audio(".m4a", false, false));
        TEST("audio .flac send as audio (non-Telegram)",
             media_should_send_as_audio(".flac", false, false));
        TEST("Telegram mp3 send as audio attachment",
             media_should_send_as_audio(".mp3", true, false));
        TEST("Telegram m4a send as audio attachment",
             media_should_send_as_audio(".m4a", true, false));
        TEST("Telegram ogg NOT audio unless is_voice",
             !media_should_send_as_audio(".ogg", true, false));
        TEST("Telegram ogg IS audio when is_voice",
             media_should_send_as_audio(".ogg", true, true));
        TEST("Telegram opus NOT audio unless is_voice",
             !media_should_send_as_audio(".opus", true, false));
        TEST("Telegram opus IS audio when is_voice",
             media_should_send_as_audio(".opus", true, true));
        TEST("Telegram wav not audio (not in Telegram audio exts)",
             !media_should_send_as_audio(".wav", true, false));
        TEST("unknown extension not audio",
             !media_should_send_as_audio(".xyz", false, false));
        TEST(".pdf not audio",
             !media_should_send_as_audio(".pdf", false, false));
        TEST("NULL ext not audio",
             !media_should_send_as_audio(NULL, false, false));
        TEST("empty ext not audio",
             !media_should_send_as_audio("", false, false));
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
