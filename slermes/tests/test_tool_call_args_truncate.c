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

    /* === estimate_payload_context_tokens tests === */

    /* 11. NULL input returns 0 */
    TEST("payload NULL returns 0", estimate_payload_context_tokens(NULL) == 0);

    /* 12. Empty input returns 0 */
    TEST("payload empty returns 0", estimate_payload_context_tokens("") == 0);

    /* 13. Invalid JSON returns raw chars/4 */
    {
        size_t t = estimate_payload_context_tokens("hello world");
        TEST("payload invalid JSON falls back", t == strlen("hello world") / 4);
    }

    /* 14. Bare array (Chat Completions messages) */
    {
        size_t t = estimate_payload_context_tokens(
            "[\"msg1\", \"msg2\"]");
        /* "msg1"(4) + "msg2"(4) = 8, 8/4 = 2 */
        TEST("payload bare array", t == 2);
    }

    /* 15. Dict with messages (Chat Completions) */
    {
        size_t t = estimate_payload_context_tokens(
            "{\"messages\":[{\"role\":\"user\",\"content\":\"hello\"}]}");
        /* Count chars: "role"(4) + "user"(4) + "content"(7) + "hello"(5) = 20 */
        /* 20 / 4 = 5 */
        TEST("payload CC messages shape", t == 5);
    }

    /* 16. Dict with messages + tools */
    {
        size_t t = estimate_payload_context_tokens(
            "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}],"
            "\"tools\":[{\"name\":\"test_tool\"}]}");
        /* messages: "role"(4)+"user"(4)+"content"(7)+"hi"(2) = 17 */
        /* tools: "name"(4)+"test_tool"(9) = 13 */
        /* total = 30, 30/4 = 7 */
        TEST("payload CC with tools", t == 7);
    }

    /* 17. Dict with input (Responses API) */
    {
        size_t t = estimate_payload_context_tokens(
            "{\"input\":\"Hello, how are you?\",\"instructions\":\"Be helpful\"}");
        /* input: "Hello, how are you?" = 19 */
        /* instructions: "Be helpful" = 10 */
        /* total = 29, 29/4 = 7 */
        TEST("payload Responses API shape", t == 7);
    }

    /* 18. Dict with input + tools */
    {
        size_t t = estimate_payload_context_tokens(
            "{\"input\":\"Hi\",\"instructions\":\"Reply\",\"tools\":[{}]}");
        /* input(2) + instructions(5) + {} empty object (maybe 2? depends on serialization) */
        /* json_count_chars: "input"(5)* + "Hi"(2) + "instructions"(12) + "Reply"(5) + "tools"(5) + {}=0 */
        /* Actually json_count_chars counts key+value pairs for objects */
        /* Keys: "input"(5) + "instructions"(12) + "tools"(5) = 22 */
        /* Values: "Hi"(2) + "Reply"(5) + {} empty object = 0 */
        /* Total = 29, 29/4 = 7 */
        /* But actually for input, instructions, tools — json_count_chars on each value counts the node's content */
        /* input is string "Hi" → 2, instructions is "Reply" → 5, tools is array [{}] → 0 (empty object) */
        /* So total_chars = 2 + 5 + 0 = 7, 7/4 = 1 */
        /* Wait, actually json_count_chars on the obj would count keys too... Let me think again. */
        /* The code does total_chars = json_count_chars(input) + json_count_chars(instructions) + json_count_chars(tools) */
        /* json_count_chars on a string → strlen(str) = 2 for "Hi" */
        /* json_count_chars on [{}] → array with 1 element, an object with 0 items → total = 0 */
        /* So total = 2 + 5 + 0 = 7, 7/4 = 1 (integer division) */
        TEST("payload Responses with tools", t == 1);
    }

    /* 19. Scalar string (non-JSON object) */
    {
        size_t t = estimate_payload_context_tokens("\"just a string\"");
        /* strlen("\"just a string\"") = 15 including quotes? No, parsed: "just a string" = 13 chars */
        /* Wait: payload is "\"just a string\"" which parses to a JSON string with content "just a string" = 13 */
        /* json_count_chars on string → 13, 13/4 = 3 */
        TEST("payload scalar string", t == 3);
    }

    /* 20. Empty object returns 0 */
    TEST("payload empty object", estimate_payload_context_tokens("{}") == 0);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
