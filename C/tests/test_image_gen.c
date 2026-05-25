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

    /* Results */
    printf("\n=== Image Gen Test Summary: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
