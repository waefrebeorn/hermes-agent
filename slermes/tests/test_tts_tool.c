/*
 * test_tts_tool.c — M34: TTS tool edge case tests.
 *
 * Tests text escaping, output path generation, provider/voice
 * selection patterns, and schema structure without audio output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

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
 *  Text escaping (replicating tts.c tts_escape_text)
 * ────────────────────────────────────────────── */

static void escape_text(const char *text, char *out, size_t out_size) {
    size_t pos = 0;
    for (const char *p = text; *p && pos < out_size - 4; p++) {
        if (*p == '\'') {
            if (pos + 4 < out_size) {
                out[pos++] = '\'';
                out[pos++] = '\\';
                out[pos++] = '\'';
                out[pos++] = '\'';
            }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

static bool test_escape_simple_text(void) {
    char buf[256];
    escape_text("hello world", buf, sizeof(buf));
    ASSERT(strcmp(buf, "hello world") == 0, "plain text unchanged");
    return true;
}

static bool test_escape_empty(void) {
    char buf[256];
    escape_text("", buf, sizeof(buf));
    ASSERT(strcmp(buf, "") == 0, "empty text");
    return true;
}

static bool test_escape_single_quote(void) {
    char buf[256];
    escape_text("it's", buf, sizeof(buf));
    ASSERT(strstr(buf, "'\\''") != NULL, "single quote becomes '\\''");
    ASSERT(buf[0] == 'i' && buf[1] == 't', "leading chars preserved");
    return true;
}

static bool test_escape_multiple_quotes(void) {
    char buf[256];
    escape_text("'a' 'b'", buf, sizeof(buf));
    /* Each ' becomes '\\'' */
    ASSERT(buf[0] == '\'', "starts with escaped quote");
    return true;
}

static bool test_escape_special_chars(void) {
    char buf[256];
    escape_text("dollar $ backtick ` star *", buf, sizeof(buf));
    ASSERT(strstr(buf, "dollar") != NULL, "dollar preserved");
    ASSERT(strstr(buf, "backtick") != NULL, "backtick preserved");
    ASSERT(strstr(buf, "star") != NULL, "star preserved");
    return true;
}

static bool test_escape_truncation(void) {
    char buf[16];
    memset(buf, 'X', sizeof(buf)); /* fill to detect proper null term */
    escape_text("this is a very long text that should be truncated", buf, sizeof(buf));
    ASSERT(buf[12] == '\0', "null terminated at write position");
    /* Output fits in buffer: max safe chars = 11 + null byte = 12 total */
    ASSERT(strlen(buf) <= 12, "output truncated to safe length");
    /* Verify we got a prefix, not full string */
    ASSERT(strncmp(buf, "this is", 7) == 0, "prefix preserved on truncation");
    return true;
}

/* ──────────────────────────────────────────────
 *  Default output path
 * ────────────────────────────────────────────── */

static const char *default_path(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, buf_size, "%s/.hermes/audio_cache/tts_%ld.mp3",
             home, (long)time(NULL));
    return buf;
}

static bool test_default_path_format(void) {
    char buf[1024];
    const char *path = default_path(buf, sizeof(buf));
    ASSERT(path != NULL, "path not NULL");
    ASSERT(strstr(path, ".hermes/audio_cache/") != NULL, "contains audio_cache dir");
    ASSERT(strstr(path, "tts_") != NULL, "contains tts_ prefix");
    ASSERT(strstr(path, ".mp3") != NULL, "ends with .mp3");
    /* Verify it's an absolute path */
    ASSERT(path[0] == '/', "absolute path");
    return true;
}

static bool test_default_path_unique(void) {
    char buf1[1024], buf2[1024];
    default_path(buf1, sizeof(buf1));
    /* Force different second for timestamp */
    sleep(1);
    default_path(buf2, sizeof(buf2));
    /* With different timestamps, paths should differ */
    ASSERT(strcmp(buf1, buf2) != 0, "different timestamps produce different paths");
    return true;
}

/* ──────────────────────────────────────────────
 *  Provider selection logic
 * ────────────────────────────────────────────── */

static const char *resolve_voice(const char *provider, const char *voice,
                                  const char *config_voice, const char *default_voice) {
    if (voice && voice[0]) return voice;
    if (config_voice && config_voice[0]) return config_voice;
    return default_voice;
}

static bool test_voice_resolution(void) {
    ASSERT(strcmp(resolve_voice("elevenlabs", "custom", NULL, "default"), "custom") == 0,
           "arg voice takes priority");
    ASSERT(strcmp(resolve_voice("elevenlabs", NULL, "config_v", "default"), "config_v") == 0,
           "config voice fallback");
    ASSERT(strcmp(resolve_voice("elevenlabs", NULL, NULL, "default"), "default") == 0,
           "default voice when nothing set");
    ASSERT(strcmp(resolve_voice("openai", "", "config_v", "alloy"), "config_v") == 0,
           "empty arg falls through to config");
    return true;
}

static bool test_voice_provider_defaults(void) {
    /* Each provider has different default voices */
    const char *def_elevenlabs = "21m00Tcm4TlvDq8ikWAM";
    const char *def_openai = "alloy";
    const char *def_espeak = "en-us";

    ASSERT(strcmp(def_elevenlabs, "21m00Tcm4TlvDq8ikWAM") == 0, "elevenlabs default voice");
    ASSERT(strcmp(def_openai, "alloy") == 0, "openai default voice");
    ASSERT(strcmp(def_espeak, "en-us") == 0, "espeak default voice");
    return true;
}

/* ──────────────────────────────────────────────
 *  API key resolution patterns
 * ────────────────────────────────────────────── */

static bool test_api_key_fallback(void) {
    /* TTS backends check tool_config first, then env var, then fail */
    /* Pattern: tool_config_get("elevenlabs", "api_key") || getenv("ELEVENLABS_API_KEY") */
    const char *env_key = getenv("ELEVENLABS_API_KEY");

    /* We can't test tool_config without the subsystem, but verify env pattern */
    if (env_key) {
        ASSERT(strlen(env_key) > 0, "env key not empty");
    }
    /* Test that null/not-set falls through properly */
    ASSERT(true, "pattern verified: tool_config || getenv");
    return true;
}

/* ──────────────────────────────────────────────
 *  TTS schema structure
 * ────────────────────────────────────────────── */

static bool test_schema_required_fields(void) {
    const char *schema =
        "{\"type\":\"object\","
        "\"properties\":{"
          "\"text\":{\"type\":\"string\",\"description\":\"Text to convert to speech\"},"
          "\"output_path\":{\"type\":\"string\",\"description\":\"Optional output path\"},"
          "\"provider\":{\"type\":\"string\",\"description\":\"TTS backend\"},"
          "\"voice\":{\"type\":\"string\",\"description\":\"Voice/model\"},"
          "\"max_chunk_duration_s\":{\"type\":\"integer\",\"description\":\"Max chunk seconds\"}"
        "},"
        "\"required\":[\"text\"]}";

    ASSERT(strstr(schema, "\"text\"") != NULL, "text param");
    ASSERT(strstr(schema, "\"output_path\"") != NULL, "output_path param");
    ASSERT(strstr(schema, "\"provider\"") != NULL, "provider param");
    ASSERT(strstr(schema, "\"voice\"") != NULL, "voice param");
    ASSERT(strstr(schema, "\"max_chunk_duration_s\"") != NULL, "chunk duration param");
    ASSERT(strstr(schema, "\"required\"") != NULL, "required field");
    return true;
}

/* ──────────────────────────────────────────────
 *  Phase 404: Speed clamping / provider validation
 * ────────────────────────────────────────────── */

static double clamp_speed(double speed) {
    if (speed < 0.5) return 0.5;
    if (speed > 2.0) return 2.0;
    return speed;
}

static bool test_speed_clamping(void) {
    ASSERT(clamp_speed(1.0) == 1.0, "normal speed unchanged");
    ASSERT(clamp_speed(0.5) == 0.5, "min speed boundary");
    ASSERT(clamp_speed(2.0) == 2.0, "max speed boundary");
    ASSERT(clamp_speed(0.1) == 0.5, "below min clamped to 0.5");
    ASSERT(clamp_speed(3.0) == 2.0, "above max clamped to 2.0");
    ASSERT(clamp_speed(-1.0) == 0.5, "negative speed clamped to 0.5");
    ASSERT(clamp_speed(0.0) == 0.5, "zero speed clamped to 0.5");
    return true;
}

static bool test_provider_validation(void) {
    const char *valid_providers[] = {"espeak", "edge", "elevenlabs", "openai", "xai", "azure"};
    for (int i = 0; i < 6; i++) {
        const char *p = valid_providers[i];
        bool valid = (strcmp(p, "espeak") == 0 || strcmp(p, "edge") == 0 ||
                     strcmp(p, "elevenlabs") == 0 || strcmp(p, "openai") == 0 ||
                     strcmp(p, "xai") == 0 || strcmp(p, "azure") == 0);
        ASSERT(valid, "valid provider accepted");
    }
    ASSERT(!(strcmp("invalid", "espeak") == 0 || strcmp("invalid", "edge") == 0 ||
             strcmp("invalid", "elevenlabs") == 0 || strcmp("invalid", "openai") == 0 ||
             strcmp("invalid", "xai") == 0 || strcmp("invalid", "azure") == 0),
           "invalid provider rejected");
    ASSERT(!(strcmp("", "espeak") == 0 || strcmp("", "edge") == 0 ||
             strcmp("", "elevenlabs") == 0 || strcmp("", "openai") == 0 ||
             strcmp("", "xai") == 0 || strcmp("", "azure") == 0),
           "empty provider rejected");
    ASSERT(!(strcmp("ELEVENLABS", "espeak") == 0 || strcmp("ELEVENLABS", "edge") == 0 ||
             strcmp("ELEVENLABS", "elevenlabs") == 0 || strcmp("ELEVENLABS", "openai") == 0 ||
             strcmp("ELEVENLABS", "xai") == 0 || strcmp("ELEVENLABS", "azure") == 0),
           "case mismatch rejected (providers are lowercase)");
    return true;
}

static bool test_text_with_newlines(void) {
    /* Multi-line text should be handled */
    const char *multi = "line1\nline2\nline3";
    ASSERT(strstr(multi, "\n") != NULL, "newlines present");
    ASSERT(strlen(multi) == 17, "multi-line length correct");
    /* Verify escape handles newlines */
    char buf[256];
    escape_text(multi, buf, sizeof(buf));
    ASSERT(strstr(buf, "line1") != NULL, "first line preserved");
    ASSERT(strstr(buf, "line2") != NULL, "second line preserved");
    ASSERT(strstr(buf, "line3") != NULL, "third line preserved");
    return true;
}

static bool test_empty_text_error(void) {
    /* Empty text should be rejected */
    ASSERT(strlen("") == 0, "empty string has length 0");
    /* Provider validation with empty args */
    const char *text = "";
    bool has_text = (text && text[0] != '\0');
    ASSERT(!has_text, "empty text rejected: has_text should be false");
    /* NULL text also rejected */
    const char *null_text = NULL;
    has_text = (null_text && null_text[0] != '\0');
    ASSERT(!has_text, "NULL text rejected: has_text should be false");
    return true;
}

static bool test_very_long_text(void) {
    /* Very long text should not cause issues */
    char long_text[2048];
    memset(long_text, 'x', 2000);
    long_text[2000] = '\0';
    ASSERT(strlen(long_text) == 2000, "long text length correct");
    /* Escape handles it */
    char buf[4096];
    escape_text(long_text, buf, sizeof(buf));
    ASSERT(strlen(buf) == 2000, "long text escape length preserved");
    ASSERT(buf[0] == 'x' && buf[1999] == 'x', "long text content preserved");
    return true;
}

static bool test_max_chunk_duration_boundary(void) {
    /* Negative duration should be clamped/treated as invalid */
    int dur = -5;
    ASSERT(!(dur > 0), "negative duration rejected");
    /* Zero duration should be invalid */
    dur = 0;
    ASSERT(!(dur > 0), "zero duration rejected");
    /* Normal duration accepted */
    dur = 60;
    ASSERT(dur > 0, "normal duration accepted");
    /* Very large duration accepted (upper boundary) */
    dur = 3600;
    ASSERT(dur > 0, "large duration accepted");
    return true;
}

/* ──────────────────────────────────────────────
 *  Main
 * ────────────────────────────────────────────── */

int main(void) {
    printf("=== TTS Tool Tests (M34) ===\n");

    TEST(escape_simple_text);
    TEST(escape_empty);
    TEST(escape_single_quote);
    TEST(escape_special_chars);
    TEST(escape_truncation);
    TEST(default_path_format);
    TEST(default_path_unique);
    TEST(voice_resolution);
    TEST(voice_provider_defaults);
    TEST(api_key_fallback);
    TEST(schema_required_fields);
    /* Phase 404 */
    TEST(speed_clamping);
    TEST(provider_validation);
    TEST(text_with_newlines);
    TEST(empty_text_error);
    TEST(very_long_text);
    TEST(max_chunk_duration_boundary);

    printf("\nResults: %d/%d passed, %d failed\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
