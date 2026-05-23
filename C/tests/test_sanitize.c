/*
 * test_sanitize.c -- Tests for P166 output sanitization.
 *
 * Tests: truncation (normal/oversized), env var redaction (API_KEY, TOKEN,
 * PASSWORD patterns), sensitive file path redaction, NULL safety, combined pipeline.
 */

#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Forward declare */
char *hermes_sanitize_output(const char *tool_name, const char *raw_output);

static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static int contains(const char *haystack, const char *needle) {
    return haystack && needle && strstr(haystack, needle) != NULL;
}

int main(void) {
    printf("=== Sanitize Output Tests (P166) ===\n");

    /* --- NULL/Edge safety --- */
    printf("\n--- NULL/Edge Safety ---\n");
    {
        TEST("NULL input returns NULL");
        char *r = hermes_sanitize_output("test", NULL);
        if (r == NULL) { PASS; } else { FAIL("expected NULL"); free(r); }
    }
    {
        TEST("NULL tool_name returns copy of input");
        char *r = hermes_sanitize_output(NULL, "hello");
        if (r && strcmp(r, "hello") == 0) { PASS; free(r); }
        else { FAIL("unexpected result"); free(r); }
    }
    {
        TEST("empty input returns empty");
        char *r = hermes_sanitize_output("test", "");
        if (r && strcmp(r, "") == 0) { PASS; free(r); }
        else { FAIL("expected empty string"); free(r); }
    }
    {
        TEST("plain text passes through unchanged");
        char *r = hermes_sanitize_output("test", "Hello world\nThis is fine.");
        if (r && strcmp(r, "Hello world\nThis is fine.") == 0) { PASS; free(r); }
        else { FAIL("text changed"); free(r); }
    }

    /* --- Truncation --- */
    printf("\n--- Truncation ---\n");
    {
        TEST("small content not truncated");
        char input[200];
        memset(input, 'x', sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0';
        char *r = hermes_sanitize_output("test", input);
        if (r) {
            size_t len = strlen(r);
            if (len == 199) { PASS; } else { char b[64]; snprintf(b, sizeof(b), "len=%zu", len); FAIL(b); }
            free(r);
        } else { FAIL("null"); }
    }
    {
        TEST("very large content truncated");
        /* Create ~300KB input */
        size_t big = 300 * 1024;
        char *input = malloc(big + 1);
        memset(input, 'A', big);
        input[big] = '\0';
        char *r = hermes_sanitize_output("test", input);
        if (r) {
            size_t len = strlen(r);
            if (len < big && len < 100000) { PASS; }
            else { char b[64]; snprintf(b, sizeof(b), "len=%zu", len); FAIL(b); }
            free(r);
        } else { FAIL("null"); }
        free(input);
    }

    /* --- Env var redaction --- */
    printf("\n--- Env Var Redaction ---\n");
    {
        TEST("API_KEY=secret value redacted");
        char *r = hermes_sanitize_output("test", "export API_KEY=sk-1234567890abcdef");
        if (r && contains(r, "REDACTED") && !contains(r, "sk-1234567890abcdef")) { PASS; free(r); }
        else { FAIL("API_KEY not redacted"); free(r); }
    }
    {
        TEST("TOKEN=secret value redacted");
        char *r = hermes_sanitize_output("test", "DISCORD_TOKEN=abc123\nexport SLACK_TOKEN=xyz789");
        if (r && contains(r, "REDACTED") && !contains(r, "abc123")) { PASS; free(r); }
        else { FAIL("TOKEN not redacted"); free(r); }
    }
    {
        TEST("PASSWORD assignment redacted");
        char *r = hermes_sanitize_output("test", "DB_PASSWORD=supersecret");
        if (r && contains(r, "REDACTED") && !contains(r, "supersecret")) { PASS; free(r); }
        else { FAIL("PASSWORD not redacted"); free(r); }
    }
    {
        TEST("normal variable VALUE=something not redacted");
        char *r = hermes_sanitize_output("test", "MY_CONFIG_PATH=/home/user/config.yaml");
        if (r && !contains(r, "REDACTED")) { PASS; free(r); }
        else { FAIL("false positive"); free(r); }
    }

    /* --- File path sanitization --- */
    printf("\n--- File Path Sanitization ---\n");
    {
        TEST("sensitive path /.ssh/ redacted");
        char *r = hermes_sanitize_output("test", "contents of /.ssh/id_rsa: ...");
        if (r && contains(r, "REDACTED") && !contains(r, "id_rsa")) { PASS; free(r); }
        else { FAIL("not redacted"); free(r); }
    }
    {
        TEST("sensitive path .env redacted");
        char *r = hermes_sanitize_output("test", "loading file /.slermes/.env");
        if (r && contains(r, "REDACTED")) { PASS; free(r); }
        else { FAIL("not redacted"); free(r); }
    }
    {
        TEST("normal path not redacted");
        char *r = hermes_sanitize_output("test", "loading file /home/user/projects/code.c");
        if (r && !contains(r, "REDACTED")) { PASS; free(r); }
        else { FAIL("false positive"); free(r); }
    }

    /* --- Combined pipeline --- */
    printf("\n--- Combined Pipeline ---\n");
    {
        TEST("both env vars and file paths redacted");
        char *r = hermes_sanitize_output("test",
            "Config loaded from /.slermes/.env\nAPI_KEY=sk-abcdef");
        if (r && contains(r, "REDACTED") && !contains(r, "sk-abcdef") && !contains(r, "/.slermes/.env")) { PASS; free(r); }
        else { FAIL("not fully redacted"); free(r); }
    }

    /* Summary */
    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
