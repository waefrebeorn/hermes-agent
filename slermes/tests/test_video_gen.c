/*
 * test_video_gen.c — Video generation tool unit tests
 * Tests: error handling, JSON validation, null safety
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External handlers from video_gen.c */
extern char *video_generate_handler(const char *args_json, const char *task_id);
extern void registry_init_video_gen(void);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Video Generation Tests ===\n");

    /* Test 1: Null args → error */
    {
        char *result = video_generate_handler(NULL, NULL);
        TEST("null args returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 2: Empty JSON → error (missing prompt) */
    {
        char *result = video_generate_handler("{}", NULL);
        TEST("empty json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 3: Invalid JSON → error */
    {
        char *result = video_generate_handler("{not json}", NULL);
        TEST("invalid json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 4: Valid prompt (no crash — may return API key error) */
    {
        char *result = video_generate_handler("{\"prompt\":\"a boat on the ocean\"}", "test");
        TEST("valid prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 5: Registration pointer exists */
    {
        TEST("registration symbol exists", registry_init_video_gen != NULL);
    }

    /* ══════════════════════════════════════════════════
     *  X03: Edge case tests — video_gen
     * ══════════════════════════════════════════════════ */

    /* Test 6: Empty prompt */
    {
        char *result = video_generate_handler("{\"prompt\":\"\"}", NULL);
        TEST("empty prompt returns error",
             result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 7: Empty provider string */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\",\"provider\":\"\"}", NULL);
        TEST("empty provider returns error",
             result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 8: Invalid provider */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\",\"provider\":\"nonexistent\"}", NULL);
        TEST("invalid provider returns error",
             result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 9: Very long prompt (~4K chars) */
    {
        char prompt_data[4100];
        memset(prompt_data, 'A', 4000);
        prompt_data[4000] = '\0';
        char args[8192];
        snprintf(args, sizeof(args), "{\"prompt\":\"%s\"}", prompt_data);
        char *result = video_generate_handler(args, NULL);
        TEST("very long prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 10: JSON injection via prompt */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\\\"}'); --\"}", NULL);
        TEST("json injection doesn't crash", result != NULL);
        free(result);
    }

    /* Test 11: Unicode/emoji prompt */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"a boat 🚤 on the ocean 🌊\"}", NULL);
        TEST("unicode prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 12: All optional params set (duration, resolution, style) */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\",\"duration\":10,\"resolution\":\"720p\","
            "\"style\":\"cinematic\"}", NULL);
        TEST("all params set doesn't crash", result != NULL);
        free(result);
    }

    /* Test 13: Unknown extra params */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\",\"unknown\":\"value\"}", NULL);
        TEST("unknown params doesn't crash", result != NULL);
        free(result);
    }

    /* Test 14: Negative duration */
    {
        char *result = video_generate_handler(
            "{\"prompt\":\"test\",\"duration\":-5}", NULL);
        TEST("negative duration doesn't crash", result != NULL);
        free(result);
    }

    /* Results */
    printf("\n=== Video Gen Test Summary: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
