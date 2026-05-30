/*
 * test_tts.c — TTS tool unit tests (M34).
 *
 * Tests: argument validation, chunking, provider dispatch.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_tts.c src/tools/tts.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_tts -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   /tmp/hermes_test_tts
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration from tts.c */
char *tts_handler(const char *args_json, const char *task_id);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static int has_error(const char *json_str) {
    if (!json_str) return 0;
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int rv = (e != NULL);
    json_free(root);
    return rv;
}

static int json_contains(const char *json_str, const char *sub) {
    if (!json_str || !sub) return 0;
    return strstr(json_str, sub) != NULL;
}

int main(void) {
    printf("=== TTS Tool Tests ===\n");

    /* Test 1: Null args */
    {
        char *res = tts_handler(NULL, NULL);
        TEST("null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = tts_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args (missing text) */
    {
        char *res = tts_handler("{}", NULL);
        /* Missing text puts error in root, not as "error" field */
        TEST("empty args returns non-NULL", res != NULL);
        if (res) {
            TEST("empty args has error indication", json_contains(res, "Missing text"));
        }
        free(res);
    }

    /* Test 4: Text with empty string */
    {
        char *res = tts_handler("{\"text\":\"\"}", NULL);
        TEST("empty text returns error indication", json_contains(res, "Missing text"));
        free(res);
    }

    /* Test 5: Valid text with default provider (espeak) */
    {
        char *res = tts_handler("{\"text\":\"hello world\"}", NULL);
        TEST("valid text returns non-NULL", res != NULL);
        if (res) {
            TEST("result has status field", json_contains(res, "\"status\""));
            TEST("result has provider field", json_contains(res, "\"provider\""));
            TEST("result has files array", json_contains(res, "\"files\""));
        }
        free(res);
    }

    /* Test 6: Explicit provider */
    {
        char *res = tts_handler("{\"text\":\"test\",\"provider\":\"elevenlabs\"}", NULL);
        TEST("elevenlabs provider returns non-NULL", res != NULL);
        if (res) {
            TEST("elevenlabs has provider field", json_contains(res, "elevenlabs"));
        }
        free(res);
    }

    /* Test 7: OpenAI provider */
    {
        char *res = tts_handler("{\"text\":\"test\",\"provider\":\"openai\"}", NULL);
        TEST("openai provider returns non-NULL", res != NULL);
        if (res) {
            TEST("openai has provider field", json_contains(res, "openai"));
        }
        free(res);
    }

    /* Test 8: xAI provider */
    {
        char *res = tts_handler("{\"text\":\"test\",\"provider\":\"xai\"}", NULL);
        TEST("xai provider returns non-NULL", res != NULL);
        if (res) {
            TEST("xai has provider field", json_contains(res, "xai"));
        }
        free(res);
    }

    /* Test 9: Voice override */
    {
        char *res = tts_handler("{\"text\":\"hello\",\"voice\":\"en-US-Wavenet-D\"}", NULL);
        TEST("voice param accepted", res != NULL);
        free(res);
    }

    /* Test 10: Max chunk duration */
    {
        char *res = tts_handler("{\"text\":\"hello world\",\"max_chunk_duration_s\":30}", NULL);
        TEST("max_chunk_duration_s accepted", res != NULL);
        free(res);
    }

    /* Test 11: Output path */
    {
        char *res = tts_handler("{\"text\":\"hello\",\"output_path\":\"/tmp/test_tts_out.wav\"}", NULL);
        TEST("output_path accepted", res != NULL);
        free(res);
    }

    /* Test 12: Long text (>60s ~900 chars) */
    {
        char long_text[2048];
        int pos = 0;
        for (int i = 0; i < 20 && pos < (int)sizeof(long_text) - 20; i++) {
            pos += snprintf(long_text + pos, sizeof(long_text) - pos,
                           "Sentence number %d about various things. ", i);
        }
        char args[4096];
        snprintf(args, sizeof(args), "{\"text\":\"%s\"}", long_text);
        char *res = tts_handler(args, NULL);
        TEST("long text returns non-NULL", res != NULL);
        if (res) {
            TEST("long text has chunk_count", json_contains(res, "\"chunk_count\""));
        }
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All TTS tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
