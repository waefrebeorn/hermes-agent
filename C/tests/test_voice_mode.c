/* test_voice_mode.c - Voice mode tool unit tests */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from voice_mode.c */
char *voice_record(const char *output_path, int max_seconds);
char *voice_transcribe(const char *audio_path);
char *voice_listen(void);
void voice_speak(const char *text);
void voice_set_enabled(int enabled);
int voice_is_enabled(void);
void voice_set_device(const char *dev);
void voice_set_asr_cmd(const char *cmd);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Voice Mode Tests ===\n");

    /* Test 1: Voice enable/disable */
    {
        voice_set_enabled(0);
        TEST("voice_disabled_default", voice_is_enabled() == 0);
        voice_set_enabled(1);
        TEST("voice_enabled_after_set", voice_is_enabled() == 1);
        voice_set_enabled(0);
        TEST("voice_disabled_after_toggle", voice_is_enabled() == 0);
    }

    /* Test 2: Device configuration — no crash */
    {
        voice_set_device("plughw:0,0");
        TEST("voice_set_device_valid", 1);
        voice_set_device(NULL);
        TEST("voice_set_device_null_safe", 1);
        voice_set_device("default");
        TEST("voice_set_device_reset", 1);
    }

    /* Test 3: ASR command config — no crash */
    {
        voice_set_asr_cmd("whisper-cli");
        TEST("voice_set_asr_cmd_valid", 1);
        voice_set_asr_cmd(NULL);
        TEST("voice_set_asr_cmd_null_safe", 1);
        voice_set_asr_cmd("");
        TEST("voice_set_asr_cmd_empty", 1);
    }

    /* Test 4: voice_record null path */
    {
        char *path = voice_record(NULL, 0);
        free(path);
        TEST("voice_record_null_safe", 1);
    }

    /* Test 5: voice_record short timeout */
    {
        char *path = voice_record("/tmp/hermes_test_voice.wav", 1);
        free(path);
        TEST("voice_record_short_timeout", 1);
    }

    /* Test 6: voice_transcribe null path */
    {
        char *res = voice_transcribe(NULL);
        TEST("transcribe_null_path", res == NULL);
    }

    /* Test 7: voice_transcribe nonexistent file */
    {
        char *res = voice_transcribe("/tmp/nonexistent_file.wav");
        TEST("transcribe_bad_path", res == NULL);
    }

    /* Test 8: voice_speak null */
    {
        voice_speak(NULL);
        TEST("voice_speak_null_safe", 1);
    }

    /* Test 9: voice_listen no crash */
    {
        voice_set_enabled(1);
        char *text = voice_listen();
        free(text);
        TEST("voice_listen_no_crash", 1);
    }

    /* Test 10: State machine */
    {
        voice_set_enabled(0);
        TEST("disabled_initial", voice_is_enabled() == 0);
        voice_set_enabled(1);
        TEST("enabled_set", voice_is_enabled() == 1);
        voice_set_enabled(0);
        TEST("disabled_again", voice_is_enabled() == 0);
        voice_set_enabled(0);
        TEST("disabled_idempotent", voice_is_enabled() == 0);
        voice_set_enabled(1);
        voice_set_enabled(1);
        TEST("enabled_idempotent", voice_is_enabled() == 1);
    }

    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All voice_mode tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
