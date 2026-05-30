/*
 * test_media_validation.c — Tests for validate_media_path() (B08/G02).
 * Tests: path validation, denied files, edge cases.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_media_validation.c
 *         ../src/tools/send_message.c ../src/agent/file_safety.c
 *         ../lib/libjson/json.c -o /tmp/t_media_val -lm
 *         -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes.h"
#include "hermes_json.h"
#include "hermes_file_safety.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Forward declaration from send_message.c */
char *validate_media_path(const char *path);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Media Path Validation Tests ===\n");

    /* Test 1: NULL path returns NULL (no error — nothing to validate) */
    {
        char *res = validate_media_path(NULL);
        TEST("NULL path returns NULL (allowed)", res == NULL);
        free(res);
    }

    /* Test 2: Empty path returns NULL */
    {
        char *res = validate_media_path("");
        TEST("empty path returns NULL (allowed)", res == NULL);
        free(res);
    }

    /* Test 3: Non-existent file returns error */
    {
        char *res = validate_media_path("/tmp/__nonexistent_file_xyz__");
        TEST("non-existent file returns error", res != NULL);
        if (res) {
            TEST("error mentions file not found", strstr(res, "not found") != NULL);
        }
        free(res);
    }

    /* Test 4: Directory returns error */
    {
        char *res = validate_media_path("/tmp");
        TEST("directory returns error", res != NULL);
        if (res) {
            TEST("error mentions not a regular file", strstr(res, "not a regular file") != NULL);
        }
        free(res);
    }

    /* Test 5: Valid existing regular file returns NULL (allowed) */
    {
        FILE *f = fopen("/tmp/test_media_val_valid.txt", "w");
        if (f) { fprintf(f, "test content"); fclose(f); }
        char *res = validate_media_path("/tmp/test_media_val_valid.txt");
        TEST("valid file returns NULL (allowed)", res == NULL);
        free(res);
        remove("/tmp/test_media_val_valid.txt");
    }

    /* Test 6: File in test-mode denied Hermes control path (.env) is blocked */
    {
        /* Set test paths so file_safety can resolve hermes home */
        file_safety_set_test_paths("/tmp/hermes-media-test", "/tmp/hermes-media-test");
        mkdir("/tmp/hermes-media-test", 0755);

        FILE *env = fopen("/tmp/hermes-media-test/.env", "w");
        if (env) { fprintf(env, "API_KEY=test"); fclose(env); }

        char *res = validate_media_path("/tmp/hermes-media-test/.env");
        TEST(".env file in hermes home is blocked", res != NULL);
        if (res) {
            TEST("block message mentions access denied or credential",
                 strstr(res, "denied") != NULL || strstr(res, "credential") != NULL);
        }
        free(res);
        remove("/tmp/hermes-media-test/.env");
        rmdir("/tmp/hermes-media-test");
    }

    /* Test 7: File in test-mode denied MCP tokens directory is blocked */
    {
        file_safety_set_test_paths("/tmp/hermes-media-test3", "/tmp/hermes-media-test3");
        mkdir("/tmp/hermes-media-test3", 0755);
        mkdir("/tmp/hermes-media-test3/mcp-tokens", 0755);
        FILE *tok = fopen("/tmp/hermes-media-test3/mcp-tokens/sample.json", "w");
        if (tok) { fprintf(tok, "{}"); fclose(tok); }

        char *res = validate_media_path("/tmp/hermes-media-test3/mcp-tokens/sample.json");
        TEST("MCP token file is blocked", res != NULL);
        if (res) {
            TEST("block message mentions denied or access",
                 strstr(res, "denied") != NULL);
        }
        free(res);
        remove("/tmp/hermes-media-test3/mcp-tokens/sample.json");
        rmdir("/tmp/hermes-media-test3/mcp-tokens");
        rmdir("/tmp/hermes-media-test3");
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All media validation tests PASSED");
    return failures ? 1 : 0;
}
