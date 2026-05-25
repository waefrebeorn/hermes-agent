/*
 * test_transcription.c — Tests for libtranscribe (audio transcription).
 * Tests validation, format detection, and JSON result helpers.
 * Does NOT test actual HTTP API calls (requires network + API keys).
 *
 * Port of Python tools/transcription_tools.py test suite.
 */

#include "transcribe.h"
#include "http.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tests = 0;
static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

/* ================================================================
 *  Format detection tests
 * ================================================================ */

static void test_supported_formats(void)
{
    TEST("mp3 supported",        transcribe_is_supported_format(".mp3"));
    TEST("wav supported",        transcribe_is_supported_format(".wav"));
    TEST("ogg supported",        transcribe_is_supported_format(".ogg"));
    TEST("flac supported",       transcribe_is_supported_format(".flac"));
    TEST("aac supported",        transcribe_is_supported_format(".aac"));
    TEST("m4a supported",        transcribe_is_supported_format(".m4a"));
    TEST("mp4 supported",        transcribe_is_supported_format(".mp4"));
    TEST("webm supported",       transcribe_is_supported_format(".webm"));
    TEST("mpeg supported",       transcribe_is_supported_format(".mpeg"));
    TEST("mpga supported",       transcribe_is_supported_format(".mpga"));
}

static void test_unsupported_formats(void)
{
    TEST("txt unsupported",      !transcribe_is_supported_format(".txt"));
    TEST("pdf unsupported",      !transcribe_is_supported_format(".pdf"));
    TEST("exe unsupported",      !transcribe_is_supported_format(".exe"));
    TEST("png unsupported",      !transcribe_is_supported_format(".png"));
    TEST("no extension",         !transcribe_is_supported_format(""));
    TEST("null extension",       !transcribe_is_supported_format(NULL));
    TEST("empty string",         !transcribe_is_supported_format(""));
}

static void test_case_insensitive(void)
{
    TEST("uppercase MP3",        transcribe_is_supported_format(".MP3"));
    TEST("mixed case Wav",       transcribe_is_supported_format(".Wav"));
    TEST("uppercase FLAC",       transcribe_is_supported_format(".FLAC"));
    TEST("mixed Ogg",            transcribe_is_supported_format(".Ogg"));
}

/* ================================================================
 *  File validation tests (no actual file needed)
 * ================================================================ */

static void test_validate_nonexistent(void)
{
    char *result = transcribe_validate_file("/tmp/nonexistent_audio_file.xyz");
    TEST("nonexistent returns error", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("nonexistent is json", j != NULL);
        if (j) {
            TEST("nonexistent success=false", !json_get_bool(j, "success", false));
            const char *err = json_get_str(j, "error", "");
            TEST("nonexistent has error", err && *err);
            const char *transcript = json_get_str(j, "transcript", "");
            TEST("nonexistent empty transcript", transcript && *transcript == '\0');
            json_free(j);
        }
        free(result);
    }
}

static void test_validate_null_path(void)
{
    char *result = transcribe_validate_file(NULL);
    TEST("null path returns error", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("null path is json", j != NULL);
        if (j) {
            TEST("null success=false", !json_get_bool(j, "success", false));
            json_free(j);
        }
        free(result);
    }
}

static void test_validate_unsupported_format(void)
{
    /* Temp file with .txt extension (unsupported, but it doesn't exist anyway) */
    char *result = transcribe_validate_file("/tmp/fake_audio.txt");
    TEST("unsupported format returns error", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("unsupported format is json", j != NULL);
        if (j) {
            TEST("unsupported success=false", !json_get_bool(j, "success", false));
            const char *err = json_get_str(j, "error", "");
            TEST("unsupported format error mentions format or not found",
                 err && (strstr(err, "Unsupported format") || strstr(err, "not found")));
            json_free(j);
        }
        free(result);
    }
}

/* ================================================================
 *  Format constants test
 * ================================================================ */

static void test_format_constants(void)
{
    TEST("format count is 10", transcribe_supported_format_count == 10);
    int count = 0;
    for (const char **f = transcribe_supported_formats; *f; f++)
        count++;
    TEST("null-terminated list count matches", count == transcribe_supported_format_count);
}

/* ================================================================
 *  JSON result format test (via transcribe_audio on nonexistent file)
 * ================================================================ */

static void test_result_format(void)
{
    /* transcribe_audio on nonexistent file should fail gracefully */
    char *result = transcribe_audio("/tmp/nonexistent_transcribe_test.ogg", NULL);
    TEST("audio on nonexistent returns result", result != NULL);
    if (result) {
        json_t *j = json_parse(result, NULL);
        TEST("audio result is valid json", j != NULL);
        if (j) {
            TEST("result has success field", json_obj_get(j, "success") != NULL);
            TEST("result has transcript field", json_obj_get(j, "transcript") != NULL);
            TEST("result success=false", !json_get_bool(j, "success", false));
            json_free(j);
        }
        free(result);
    }
}

/* ================================================================
 *  Provider constants test
 * ================================================================ */

static void test_provider_constants(void)
{
    TEST("provider groq", strcmp(TRANSCRIBE_PROVIDER_GROQ, "groq") == 0);
    TEST("provider openai", strcmp(TRANSCRIBE_PROVIDER_OPENAI, "openai") == 0);
    TEST("provider xai", strcmp(TRANSCRIBE_PROVIDER_XAI, "xai") == 0);
    TEST("default groq model", strcmp(TRANSCRIBE_DEFAULT_MODEL_GROQ, "whisper-large-v3-turbo") == 0);
    TEST("default openai model", strcmp(TRANSCRIBE_DEFAULT_MODEL_OPENAI, "whisper-1") == 0);
    TEST("max file size 25MB", TRANSCRIBE_MAX_FILE_SIZE == 25 * 1024 * 1024);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void)
{
    printf("=== Transcription Library Tests ===\n");

    test_supported_formats();
    test_unsupported_formats();
    test_case_insensitive();
    test_validate_nonexistent();
    test_validate_null_path();
    test_validate_unsupported_format();
    test_format_constants();
    test_result_format();
    test_provider_constants();

    printf("\nResults: %d passed, %d failed, %d total\n", passed, failed, tests);
    return failed > 0 ? 1 : 0;
}
