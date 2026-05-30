/*
 * test_result_storage.c -- Tests for P49-P50 tool result storage.
 *
 * Tests: inline storage (small data), truncation (data > max size),
 * fallback when config unavailable, NULL/edge safety, cleanup.
 */

#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* Forward declare */
char *tool_result_store(const char *data, size_t size, size_t max_inline);

static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

int main(void) {
    /* Set up minimal env for hermes_config_load */
    char tmpdir[] = "/tmp/hermes_test_rs_XXXXXX";
    if (!mkdtemp(tmpdir)) { printf("FAIL: mkdtemp\n"); return 1; }
    setenv("SLERMES_HOME", tmpdir, 1);

    printf("=== Tool Result Storage Tests (P49-P50) ===\n");

    /* --- NULL/Edge safety --- */
    printf("\n--- NULL/Edge Safety ---\n");
    {
        TEST("NULL data returns error JSON");
        char *r = tool_result_store(NULL, 0, 4096);
        if (r && strstr(r, "error")) { PASS; free(r); }
        else { FAIL("expected error JSON"); free(r); }
    }
    {
        TEST("empty string stored inline");
        char *r = tool_result_store("", 0, 4096);
        if (r && strcmp(r, "") == 0) { PASS; free(r); }
        else { FAIL("expected empty"); free(r); }
    }
    {
        TEST("zero max_inline uses default 4096");
        char *r = tool_result_store("hello", 0, 0);
        if (r && strcmp(r, "hello") == 0) { PASS; free(r); }
        else { FAIL("expected 'hello'"); free(r); }
    }

    /* --- Inline storage --- */
    printf("\n--- Inline Storage ---\n");
    {
        TEST("small data stored inline (under 4096)");
        char input[100];
        memset(input, 'x', sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0';
        char *r = tool_result_store(input, 0, 4096);
        if (r && strlen(r) == 99) { PASS; free(r); }
        else { FAIL("wrong size"); free(r); }
    }
    {
        TEST("data just under max_inline kept inline");
        char input[4000];
        memset(input, 'A', sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0';
        char *r = tool_result_store(input, 0, 4096);
        if (r && strlen(r) == 3999) { PASS; free(r); }
        else { char b[64]; snprintf(b, sizeof(b), "len=%zu", r ? strlen(r) : 0); FAIL(b); free(r); }
    }

    /* --- Truncation --- */
    printf("\n--- Truncation ---\n");
    {
        TEST("data over 50000 triggers truncation (no config)");
        size_t big = 100000;
        char *input = malloc(big + 1);
        memset(input, 'B', big);
        input[big] = '\0';
        char *r = tool_result_store(input, 0, 4096);
        if (r) {
            size_t len = strlen(r);
            /* Without config, defaults to 50000 max. Truncation keeps
               first max_result bytes + truncation msg */
            if (len < big && len > 1000) { PASS; }
            else { char b[64]; snprintf(b, sizeof(b), "len=%zu", len); FAIL(b); }
            free(r);
        } else { FAIL("null"); }
        free(input);
    }

    /* --- Large result temp file --- */
    printf("\n--- Temp File Storage ---\n");
    {
        /* Data > 4096 (default max_inline for large) but < 50000 (default max_result) */
        TEST("data over max_inline writes temp file");
        size_t med = 10000;
        char *input = malloc(med + 1);
        memset(input, 'C', med);
        input[med] = '\0';
        char *r = tool_result_store(input, 0, 4096);
        if (r) {
            /* Should return a JSON reference like {"result_file":"...","size":10000} */
            if (strstr(r, "result_file") && strstr(r, "10000")) { PASS; }
            else { FAIL("unexpected format"); }
            free(r);
        } else { FAIL("null"); }
        free(input);
    }
    {
        TEST("temp file actually created on disk");
        size_t med = 10000;
        char *input = malloc(med + 1);
        memset(input, 'D', med);
        input[med] = '\0';
        char *r = tool_result_store(input, 0, 4096);
        if (r) {
            /* Extract the file path from JSON */
            const char *fp = strstr(r, "\"result_file\":\"");
            if (fp) {
                fp += 15;
                const char *end = strchr(fp, '"');
                if (end) {
                    char path[512];
                    size_t plen = (size_t)(end - fp);
                    if (plen < sizeof(path)) {
                        memcpy(path, fp, plen);
                        path[plen] = '\0';
                        struct stat st;
                        if (stat(path, &st) == 0) { PASS; }
                        else { FAIL("temp file not found"); }
                        unlink(path);
                    } else { FAIL("path too long"); }
                } else { FAIL("parse error"); }
            } else { FAIL("no result_file field"); }
            free(r);
        } else { FAIL("null"); }
        free(input);
    }

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");

    unsetenv("SLERMES_HOME");
    char cmd[256]; snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    return failed > 0 ? 1 : 0;
}
