/*
 * test_tool_call_args_truncate.c — Tests for tool_call_args_truncate().
 *
 * Build:
 *   gcc -O2 -g -Wall -I include -I lib/libjson tests/test_tool_call_args_truncate.c \
 *       src/tools/tool_result.c lib/libjson/json.c -o /tmp/hermes_test_truncate -lm
 */

#include "hermes_tool_result.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR(name, actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (_a && _e && strcmp(_a, _e) == 0) { passed++; printf("  PASS: %s\n", name); } \
    else if (!_a && !_e) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (got=%s, expected=%s)\n", name, _a ? _a : "NULL", _e ? _e : "NULL"); } \
} while(0)

int main(void) {
    printf("=== Tool Call Args Truncate Tests ===\n");

    /* 1. NULL input */
    {
        char *r = tool_call_args_truncate(NULL, 200);
        TEST("NULL returns NULL", r == NULL);
        free(r);
    }

    /* 2. Empty input */
    {
        char *r = tool_call_args_truncate("", 200);
        TEST("empty returns NULL", r == NULL);
        free(r);
    }

    /* 3. Invalid JSON returns NULL */
    {
        char *r = tool_call_args_truncate("not-json", 200);
        TEST("invalid JSON returns NULL", r == NULL);
        free(r);
    }

    /* 4. Short string — not truncated */
    {
        char *r = tool_call_args_truncate("{\"key\":\"short\"}", 200);
        TEST_STR("short string preserved",
            r, "{\"key\":\"short\"}");
        free(r);
    }

    /* 5. Long string value — truncated */
    {
        char long_val[300];
        memset(long_val, 'A', 250);
        long_val[250] = '\0';
        char input[512];
        snprintf(input, sizeof(input), "{\"content\":\"%s\"}", long_val);

        char *r = tool_call_args_truncate(input, 200);
        /* Should be truncated to 200 + "...[truncated]" */
        TEST("long string returns non-NULL", r != NULL);
        if (r) {
            /* Re-parse to check content */
            char *err = NULL;
            json_t *parsed = json_parse(r, &err);
            TEST("truncated result parses as valid JSON",
                 parsed != NULL && err == NULL);
            if (parsed) {
                json_t *content = json_obj_get(parsed, "content");
                TEST("truncated content key exists", content != NULL);
                if (content && content->type == JSON_STRING) {
                    size_t slen = strlen(content->str_val);
                    TEST("truncated length = 200 + suffix(14)",
                         slen == 214);
                    TEST("ends with ...[truncated]",
                         slen >= 14 && strcmp(content->str_val + slen - 14, "...[truncated]") == 0);
                }
                json_free(parsed);
            }
            free(err);
        }
        free(r);
    }

    /* 6. Nested object with long string */
    {
        char long_val[300];
        memset(long_val, 'B', 250);
        long_val[250] = '\0';
        char input[600];
        snprintf(input, sizeof(input),
            "{\"path\":\"/foo/bar\",\"details\":{\"content\":\"%s\",\"size\":42}}",
            long_val);

        char *r = tool_call_args_truncate(input, 100);
        TEST("nested long string returns non-NULL", r != NULL);
        if (r) {
            char *err = NULL;
            json_t *parsed = json_parse(r, &err);
            TEST("nested truncated result is valid JSON",
                 parsed != NULL && err == NULL);
            if (parsed) {
                json_t *details = json_obj_get(parsed, "details");
                TEST("details key exists", details != NULL);
                if (details) {
                    json_t *content = json_obj_get(details, "content");
                    TEST("nested content key exists", content != NULL);
                    if (content && content->type == JSON_STRING) {
                        size_t slen = strlen(content->str_val);
                        TEST("nested truncated length = 100 + suffix(14)",
                             slen == 114);
                    }
                }
                /* Non-string fields preserved inside details */
                json_t *size = json_obj_get(details, "size");
                TEST("nested size field preserved (number)",
                     size != NULL && size->type == JSON_NUMBER);
                TEST("path field preserved (exact)",
                     json_has(parsed, "path"));
                json_free(parsed);
            }
            free(err);
        }
        free(r);
    }

    /* 7. Array with long strings */
    {
        char long_val[300];
        memset(long_val, 'C', 250);
        long_val[250] = '\0';
        char input[600];
        snprintf(input, sizeof(input),
            "{\"messages\":[\"short\",\"%s\"]}", long_val);

        char *r = tool_call_args_truncate(input, 20);
        TEST("array long string returns non-NULL", r != NULL);
        if (r) {
            char *err = NULL;
            json_t *parsed = json_parse(r, &err);
            TEST("array truncated result is valid JSON",
                 parsed != NULL && err == NULL);
            if (parsed) {
                json_t *msgs = json_obj_get(parsed, "messages");
                TEST("messages array exists", msgs != NULL && msgs->type == JSON_ARRAY);
                if (msgs && msgs->type == JSON_ARRAY) {
                    TEST("array has 2 items", msgs->c.count == 2);
                    if (msgs->c.count >= 2) {
                        TEST("first item unchanged (short)",
                             msgs->c.items[0]->type == JSON_STRING &&
                             strcmp(msgs->c.items[0]->str_val, "short") == 0);
                        if (msgs->c.items[1]->type == JSON_STRING) {
                            size_t slen = strlen(msgs->c.items[1]->str_val);
                            TEST("second item truncated (20 + 14)",
                                 slen == 34);
                        }
                    }
                }
                json_free(parsed);
            }
            free(err);
        }
        free(r);
    }

    /* 8. Non-string values preserved (number, bool, null) */
    {
        char *r = tool_call_args_truncate(
            "{\"count\":42,\"flag\":true,\"empty\":null}", 200);
        TEST_STR("non-string values preserved",
            r, "{\"count\":42,\"flag\":true,\"empty\":null}");
        free(r);
    }

    /* 9. Exactly at boundary — not truncated */
    {
        char exact[201];
        memset(exact, 'D', 200);
        exact[200] = '\0';
        char input[512];
        snprintf(input, sizeof(input), "{\"val\":\"%s\"}", exact);

        char *r = tool_call_args_truncate(input, 200);
        TEST("exactly boundary returns non-NULL", r != NULL);
        if (r) {
            char *err = NULL;
            json_t *parsed = json_parse(r, &err);
            TEST("boundary result is valid JSON", parsed != NULL && err == NULL);
            if (parsed) {
                json_t *val = json_obj_get(parsed, "val");
                if (val && val->type == JSON_STRING) {
                    TEST("boundary string unchanged (200 chars)",
                         strlen(val->str_val) == 200);
                }
                json_free(parsed);
            }
            free(err);
        }
        free(r);
    }

    /* 10. Very short head_chars */
    {
        char *r = tool_call_args_truncate(
            "{\"msg\":\"hello world, this is a long message\"}", 5);
        TEST("small head_chars returns non-NULL", r != NULL);
        if (r) {
            TEST("small head_chars result is short",
                 strlen(r) < 50);
            /* Should contain ...[truncated] at the truncated point */
            TEST("small head_chars ends with truncated marker",
                 strstr(r, "...[truncated]") != NULL);
        }
        free(r);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
