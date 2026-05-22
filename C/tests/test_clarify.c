/*
 * test_clarify.c — Clarify tool unit tests (M40).
 *
 * Tests: argument validation, response parsing.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       tests/test_clarify.c src/tools/clarify.c lib/libjson/json.c \
 *       -o /tmp/hermes_test_clarify -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   echo "test answer" | /tmp/hermes_test_clarify
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration from clarify.c */
char *clarify_handler(const char *args_json, const char *task_id);

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
    printf("=== Clarify Tool Tests ===\n");

    /* Test 1: Null args */
    {
        char *res = clarify_handler(NULL, NULL);
        TEST("null args returns error", has_error(res));
        free(res);
    }

    /* Test 2: Invalid JSON */
    {
        char *res = clarify_handler("{bad json}", NULL);
        TEST("invalid JSON returns error", has_error(res));
        free(res);
    }

    /* Test 3: Empty args (missing question) */
    {
        char *res = clarify_handler("{}", NULL);
        TEST("empty args returns error (missing question)", has_error(res));
        free(res);
    }

    /* Test 4: Extra field without question */
    {
        char *res = clarify_handler("{\"choices\":[\"a\",\"b\"]}", NULL);
        TEST("choices without question returns error", has_error(res));
        free(res);
    }

    /* Test 5: Valid question with stdin response */
    {
        /* Ensure stdin has input available (run with echo "answer" | test) */
        char *res = clarify_handler("{\"question\":\"What is your name?\"}", NULL);
        TEST("valid question returns non-NULL", res != NULL);
        if (res) {
            TEST("response contains response field", json_contains(res, "\"response\""));
        }
        free(res);
    }

    /* Test 6: Question with choices and selection */
    {
        /* Run with: echo "2" | test */
        char *res = clarify_handler(
            "{\"question\":\"Pick one\",\"choices\":[\"Option A\",\"Option B\",\"Option C\"]}", NULL);
        TEST("question with choices returns non-NULL", res != NULL);
        if (res) {
            TEST("choices response has response field", json_contains(res, "\"response\""));
        }
        free(res);
    }

    /* Test 7: Existing question key (not in JSON) */
    {
        char *res = clarify_handler("{\"non_question\":\"value\"}", NULL);
        TEST("missing 'question' key returns error", has_error(res));
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failed ? "SOME TESTS FAILED" : "All clarify tests PASSED");
    printf("  %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
