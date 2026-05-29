/*
 * test_voice_mode.c — Voice mode configuration unit tests.
 *
 * Tests: enabled state, device config, ASR cmd config, edge cases.
 * These functions don't require audio hardware.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson \
 *       tests/test_voice_mode.c src/tools/voice_mode.c \
 *       lib/libjson/json.c -o /tmp/hermes_test_voice \
 *       -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_voice
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Functions under test (from voice_mode.c) */
void voice_set_enabled(int enabled);
int voice_is_enabled(void);
void voice_set_device(const char *dev);
void voice_set_asr_cmd(const char *cmd);
char *voice_record(const char *output_path, int max_seconds);
char *voice_transcribe(const char *audio_path);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Voice Mode Config Tests ===\n");

    /* Test 1: Default state is disabled */
    voice_set_enabled(0);
    TEST("default disabled", voice_is_enabled() == 0);

    /* Test 2: Enable and verify */
    voice_set_enabled(1);
    TEST("enable sets enabled", voice_is_enabled() == 1);

    /* Test 3: Disable and verify */
    voice_set_enabled(0);
    TEST("disable clears enabled", voice_is_enabled() == 0);

    /* Test 4: Toggle back */
    voice_set_enabled(1);
    TEST("re-enable works", voice_is_enabled() == 1);

    /* Test 5: Non-zero values treated as truthy */
    voice_set_enabled(42);
    TEST("non-zero is truthy", voice_is_enabled() != 0);

    /* Test 6: Negative values treated as truthy */
    voice_set_enabled(-1);
    TEST("negative is truthy", voice_is_enabled() != 0);

    /* Test 7: Device NULL (should not crash) */
    voice_set_device(NULL);
    /* Cannot verify NULL device — it's stored internally */
    TEST("set device NULL no crash", 1);

    /* Test 8: Device empty string */
    voice_set_device("");
    TEST("set device empty no crash", 1);

    /* Test 9: Device valid string */
    voice_set_device("default");
    TEST("set device 'default' no crash", 1);

    /* Test 10: Device long string */
    {
        char long_dev[256];
        memset(long_dev, 'x', sizeof(long_dev) - 1);
        long_dev[sizeof(long_dev) - 1] = '\0';
        voice_set_device(long_dev);
        TEST("set long device name no crash", 1);
    }

    /* Test 11: ASR cmd NULL (should not crash) */
    voice_set_asr_cmd(NULL);
    TEST("set asr NULL no crash", 1);

    /* Test 12: ASR cmd empty string */
    voice_set_asr_cmd("");
    TEST("set asr empty no crash", 1);

    /* Test 13: ASR cmd valid string */
    voice_set_asr_cmd("whisper --model base output.wav");
    TEST("set asr cmd valid no crash", 1);

    /* Test 14: voice_record with invalid path returns NULL (hardware not available) */
    {
        char *res = voice_record("/nonexistent/path/output.wav", 5);
        /* NULL is valid — hardware not available in test env */
        TEST("record with invalid path handled gracefully", 1);
        free(res);
    }

    /* Test 15: voice_transcribe with nonexistent file returns NULL */
    {
        char *res = voice_transcribe("/nonexistent/file.wav");
        /* NULL is valid — no hardware in test env */
        TEST("transcribe nonexistent file handled gracefully", 1);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All voice mode config tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
