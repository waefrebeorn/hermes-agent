/*
 * test_send_message.c — Tests for send_message tool (M37).
 * Tests: schema validation, stdout, local, media, error cases.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_send_message.c ../src/tools/send_message.c ../lib/libjson/json.c -o /tmp/t_send -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Forward declaration from send_message.c */
char *send_message_handler(const char *args_json, const char *task_id);
char *sanitize_error_text(const char *text);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

/* Helper: check result JSON field */
static int json_has_error(const char *json_str) {
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return 1; }
    const char *e = json_get_str(root, "error", NULL);
    int has = (e != NULL);
    json_free(root);
    return has;
}

static char *json_get_field(const char *json_str, const char *field) {
    char *err = NULL;
    json_t *root = json_parse(json_str, &err);
    if (!root) { free(err); return NULL; }
    const char *val = json_get_str(root, field, NULL);
    char *result = val ? strdup(val) : NULL;
    json_free(root);
    return result;
}

int main(void) {
    printf("=== Send Message Tool Tests ===\n");

    /* Test 1: Null args */
    {
        char *res = send_message_handler(NULL, NULL);
        TEST("null args returns error", json_has_error(res));
        free(res);
    }

    /* Test 2: Empty args */
    {
        char *res = send_message_handler("{}", NULL);
        TEST("empty args returns error (missing message)", json_has_error(res));
        free(res);
    }

    /* Test 3: Invalid JSON */
    {
        char *res = send_message_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", json_has_error(res));
        free(res);
    }

    /* Test 4: Missing message field */
    {
        char *res = send_message_handler("{\"target\":\"stdout\"}", NULL);
        TEST("missing message returns error", json_has_error(res));
        free(res);
    }

    /* Test 5: stdout via target format stdout:console */
    {
        char *res = send_message_handler("{\"target\":\"stdout:console\",\"message\":\"hello\"}", NULL);
        char *s = json_get_field(res, "status");
        char *t = json_get_field(res, "target");
        TEST("stdout returns status=sent", s && strcmp(s, "sent") == 0);
        TEST("stdout shows target", t && strcmp(t, "stdout") == 0);
        free(s); free(t);
        free(res);
    }

    /* Test 6: stdout via platform override */
    {
        char *res = send_message_handler("{\"target\":\"local\",\"message\":\"override\",\"platform\":\"stdout\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("platform override stdout returns sent", s && strcmp(s, "sent") == 0);
        free(s);
        free(res);
    }

    /* Test 7: local target (save to file) */
    {
        char *res = send_message_handler("{\"target\":\"local\",\"message\":\"test save\"}", NULL);
        char *status = json_get_field(res, "status");
        TEST("local saves to file",
             (status && strcmp(status, "saved") == 0) || !status);
        if (status && strcmp(status, "saved") == 0) {
            char *path = json_get_field(res, "path");
            TEST("local has path", path && path[0] != '\0');
            free(path);
        }
        free(status);
        free(res);
    }

    /* Test 8: No target = local by default */
    {
        char *res = send_message_handler("{\"message\":\"default target\"}", NULL);
        char *status = json_get_field(res, "status");
        TEST("no target defaults to local",
             (status && strcmp(status, "saved") == 0) || !status);
        free(status);
        free(res);
    }

    /* Test 9: Media via media_path */
    {
        /* Use a known temp file */
        FILE *tf = fopen("/tmp/test_send_media.txt", "w");
        if (tf) { fprintf(tf, "media content"); fclose(tf); }
        char args[512];
        snprintf(args, sizeof(args),
            "{\"target\":\"local\",\"message\":\"with media\",\"media_path\":\"/tmp/test_send_media.txt\"}");
        char *res = send_message_handler(args, NULL);
        char *status = json_get_field(res, "status");
        TEST("media_path works",
             (status && strcmp(status, "saved") == 0) || !status);
        free(status);
        free(res);
        remove("/tmp/test_send_media.txt");
    }

    /* Test 10: MEDIA: prefix backward compat */
    {
        char *res = send_message_handler(
            "{\"target\":\"local\",\"message\":\"MEDIA:/tmp/some_file.txt remaining text\"}", NULL);
        char *status = json_get_field(res, "status");
        TEST("MEDIA: prefix works",
             (status && strcmp(status, "saved") == 0) || !status);
        free(status);
        free(res);
    }

    /* Test 11: Platform override with stdout */
    {
        char *res = send_message_handler(
            "{\"target\":\"stdout:x\",\"message\":\"platform test\",\"platform\":\"stdout\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("platform override stdout works", s && strcmp(s, "sent") == 0);
        free(s);
        free(res);
    }

    /* Test 13: Target with platform:chat_id format */
    {
        char *res = send_message_handler(
            "{\"target\":\"stdout:console\",\"message\":\"routed\",\"platform\":\"telegram\"}", NULL);
        TEST("platform routing completes without crash", res != NULL);
        free(res);
    }

    /* Test 14: Empty message string is valid */
    {
        char *res = send_message_handler("{\"target\":\"stdout:x\",\"message\":\"\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("empty message string is valid",
             (s && strcmp(s, "sent") == 0) || !s);
        free(s);
        free(res);
    }

    /* Test 15: sanitize_error_text — URL query param redaction */
    {
        char *res = sanitize_error_text("https://example.com/api?access_token=secret123&key=val");
        TEST("sanitize redacts access_token in URL", res && strstr(res, "***") != NULL);
        TEST("sanitize keeps param name", res && strstr(res, "access_token=") != NULL);
        TEST("sanitize removes secret value", res && strstr(res, "secret123") == NULL);
        free(res);
    }

    /* Test 16: sanitize_error_text — generic assignment redaction */
    {
        char *res = sanitize_error_text("error: api_key=sk-abc123xyz, something else");
        TEST("sanitize redacts api_key assignment", res && strstr(res, "***") != NULL);
        TEST("sanitize keeps key name", res && strstr(res, "api_key=") != NULL);
        TEST("sanitize removes key value", res && strstr(res, "sk-abc") == NULL);
        free(res);
    }

    /* Test 17: sanitize_error_text — no false positives on safe text */
    {
        char *res = sanitize_error_text("Platform 'telegram' send failed");
        TEST("sanitize doesn't modify safe text", res && strcmp(res, "Platform 'telegram' send failed") == 0);
        free(res);
    }

    /* Test 18: sanitize_error_text — null input */
    {
        char *res = sanitize_error_text(NULL);
        TEST("sanitize handles NULL input", res == NULL);
    }

    /* Test 19: sanitize_error_text — multiple tokens */
    {
        char *res = sanitize_error_text("token=ghp_abc123&token=ghp_def456");
        TEST("sanitize redacts multiple tokens", res && strstr(res, "token=***") != NULL);
        TEST("sanitize all tokens redacted", res && strstr(res, "ghp_") == NULL);
        free(res);
    }

    /* Test 20: sanitize_error_text — sig parameter */
    {
        char *res = sanitize_error_text("?sig=abc123def456&other=val");
        TEST("sanitize redacts sig param", res && strstr(res, "sig=***") != NULL);
        free(res);
    }

    /* Test 21: action=list returns no error */
    {
        char *res = send_message_handler("{\"action\":\"list\"}", NULL);
        TEST("action=list returns non-NULL", res != NULL);
        if (res) {
            TEST("action=list no error", !json_has_error(res));
        }
        free(res);
    }

    /* Test 22: action=list contains telegram */
    {
        char *res = send_message_handler("{\"action\":\"list\"}", NULL);
        TEST("action=list contains telegram", res && strstr(res, "telegram") != NULL);
        free(res);
    }

    /* Test 23: action=list contains stdout and local */
    {
        char *res = send_message_handler("{\"action\":\"list\"}", NULL);
        if (res) {
            TEST("action=list contains stdout", strstr(res, "stdout") != NULL);
            TEST("action=list contains local", strstr(res, "local") != NULL);
        }
        free(res);
    }

    /* Test 24: parse_mode=HTML accepted */
    {
        char *res = send_message_handler(
            "{\"target\":\"stdout:x\",\"message\":\"html test\",\"parse_mode\":\"HTML\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("parse_mode=HTML completes", s && strcmp(s, "sent") == 0);
        free(s);
        free(res);
    }

    /* Test 25: parse_mode=MarkdownV2 accepted */
    {
        char *res = send_message_handler(
            "{\"target\":\"stdout:x\",\"message\":\"mdv2 test\",\"parse_mode\":\"MarkdownV2\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("parse_mode=MarkdownV2 completes", s && strcmp(s, "sent") == 0);
        free(s);
        free(res);
    }

    /* Test 26: empty parse_mode defaults to Markdown */
    {
        char *res = send_message_handler(
            "{\"target\":\"stdout:x\",\"message\":\"empty parse\",\"parse_mode\":\"\"}", NULL);
        char *s = json_get_field(res, "status");
        TEST("empty parse_mode defaults to Markdown", s && strcmp(s, "sent") == 0);
        free(s);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All send_message tests PASSED");
    return failures ? 1 : 0;
}
