/*
 * test_image_gen.c — Image generation tool unit tests
 * Tests: error handling, JSON validation
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External handlers from image_gen.c */
extern char *image_generate_handler(const char *args_json, const char *task_id);
extern void registry_init_image_gen(void);

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

int main(void) {
    printf("=== Image Generation Tests ===\n");

    /* Test 1: Null args → error string */
    {
        char *result = image_generate_handler(NULL, NULL);
        TEST("null args returns error string", result != NULL);
        TEST("null args has error key", strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 2: Empty JSON → error */
    {
        char *result = image_generate_handler("{}", NULL);
        TEST("empty json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 3: Missing prompt → error */
    {
        char *result = image_generate_handler("{\"aspect_ratio\":\"16:9\"}", NULL);
        TEST("missing prompt returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 4: Invalid JSON → error */
    {
        char *result = image_generate_handler("{not json}", NULL);
        TEST("invalid json returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 5: Empty prompt → error */
    {
        char *result = image_generate_handler("{\"prompt\":\"\"}", NULL);
        TEST("empty prompt returns error", result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 6: Valid args (no crash — may return API key error) */
    {
        char *result = image_generate_handler("{\"prompt\":\"a test image\"}", "test_id");
        TEST("valid prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 7: Registration pointer exists */
    {
        TEST("registration symbol exists", registry_init_image_gen != NULL);
    }

    /* ══════════════════════════════════════════════════
     *  X02: Edge case tests — image_gen
     * ══════════════════════════════════════════════════ */

    /* Test 8: Invalid provider */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"provider\":\"nonexistent\"}", NULL);
        TEST("invalid provider returns error",
             result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 9: Empty provider string */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"provider\":\"\"}", NULL);
        TEST("empty provider returns error",
             result != NULL && strstr(result, "error") != NULL);
        free(result);
    }

    /* Test 10: Negative aspect ratio width */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"aspect_ratio\":\"-16:9\"}", NULL);
        TEST("negative aspect ratio returns error or doesn't crash",
             result != NULL);
        free(result);
    }

    /* Test 11: Very long prompt (~4K chars) */
    {
        char prompt_data[4100];
        memset(prompt_data, 'A', 4000);
        prompt_data[4000] = '\0';
        char args[8192];
        snprintf(args, sizeof(args), "{\"prompt\":\"%s\"}", prompt_data);
        char *result = image_generate_handler(args, NULL);
        TEST("very long prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 12: JSON injection via prompt (embedded quotes) */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\\\"}'); DROP TABLE; --\"}", NULL);
        TEST("json injection in prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 13: Prompt with unicode/emoji */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"a cat 🐱 dancing 💃 in the rain 🌧️\"}", NULL);
        TEST("unicode prompt doesn't crash", result != NULL);
        free(result);
    }

    /* Test 14: All optional params set (n, size, quality, style) */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"n\":2,\"size\":\"1024x1024\","
            "\"quality\":\"hd\",\"style\":\"vivid\"}", NULL);
        TEST("all params set doesn't crash", result != NULL);
        free(result);
    }

    /* Test 15: N parameter — zero */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"n\":0}", NULL);
        TEST("n=0 returns error or doesn't crash", result != NULL);
        free(result);
    }

    /* Test 16: N parameter — very large */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"n\":999}", NULL);
        TEST("n=999 doesn't crash", result != NULL);
        free(result);
    }

    /* Test 17: Unknown extra parameters */
    {
        char *result = image_generate_handler(
            "{\"prompt\":\"test\",\"unknown_param\":\"value\",\"another\":123}", NULL);
        TEST("unknown params doesn't crash", result != NULL);
        free(result);
    }

    /* Results */
    printf("\n=== Image Gen Test Summary: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
