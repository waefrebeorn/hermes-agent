/*
 * test_feishu_comment.c — Tests for Feishu comment helpers.
 * Standalone — no deps beyond stdlib.
 * Compile: gcc -O2 -Wall -Wextra -o /tmp/t_fc test_feishu_comment.c -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Inline version of feishu_sanitize_comment_text() for testing */
static char *sanitize_comment_text(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    size_t cap = len * 5 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < cap - 6; i++) {
        char c = text[i];
        switch (c) {
            case '&':  memcpy(out + pos, "&amp;", 5);  pos += 5; break;
            case '<':  memcpy(out + pos, "&lt;", 4);   pos += 4; break;
            case '>':  memcpy(out + pos, "&gt;", 4);   pos += 4; break;
            default:   out[pos++] = c; break;
        }
    }
    out[pos] = '\0';
    return out;
}

/* Inline version of feishu_get_reply_user_id() for testing — simplified to matched expected
 * user_id value from a JSON-like object represented as string pairs (key,value,key,value,...). */
#include <string.h>

static const char *get_reply_user_id(const char *user_id_type,
                                      const char *user_id_val,
                                      const char *nested_open_id,
                                      const char *nested_user_id) {
    if (strcmp(user_id_type, "direct") == 0) {
        return user_id_val;
    } else if (strcmp(user_id_type, "nested") == 0) {
        if (nested_open_id && nested_open_id[0]) return nested_open_id;
        if (nested_user_id && nested_user_id[0]) return nested_user_id;
    }
    return "";
}

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

#define TEST_STR(name, actual, expected) do { \
    bool pass = (actual == NULL && expected == NULL) || \
                (actual != NULL && expected != NULL && strcmp(actual, expected) == 0); \
    if (!pass) { fprintf(stderr, "  FAIL: %s (got=%s, expected=%s)\n", name, actual ? actual : "NULL", expected ? expected : "NULL"); failures++; } \
    else printf("  PASS: %s -> %s\n", name, actual ? actual : "NULL"); \
} while (0)

int main(void) {
    printf("=== Feishu Comment Helper Tests ===\n\n");

    printf("--- sanitize_comment_text ---\n");
    /* Test 1: NULL returns NULL */
    {
        char *r = sanitize_comment_text(NULL);
        TEST("NULL -> NULL", r == NULL);
    }

    /* Test 2: Empty string */
    {
        char *r = sanitize_comment_text("");
        TEST("empty string", r && strcmp(r, "") == 0);
        free(r);
    }

    /* Test 3: Plain text (no escaping needed) */
    {
        char *r = sanitize_comment_text("Hello World");
        TEST("plain text", r && strcmp(r, "Hello World") == 0);
        free(r);
    }

    /* Test 4: Ampersand escaping */
    {
        char *r = sanitize_comment_text("A & B");
        TEST("ampersand", r && strcmp(r, "A &amp; B") == 0);
        free(r);
    }

    /* Test 5: Less-than escaping */
    {
        char *r = sanitize_comment_text("<tag>");
        TEST("less/greater", r && strcmp(r, "&lt;tag&gt;") == 0);
        free(r);
    }

    /* Test 6: All special chars */
    {
        char *r = sanitize_comment_text("& < >");
        TEST("all special", r && strcmp(r, "&amp; &lt; &gt;") == 0);
        free(r);
    }

    /* Test 7: No special chars mixed */
    {
        char *r = sanitize_comment_text("x & y < z > w");
        TEST("mixed", r && strcmp(r, "x &amp; y &lt; z &gt; w") == 0);
        free(r);
    }

    printf("\n--- get_reply_user_id ---\n");

    /* Test 8: Direct string user_id */
    {
        const char *r = get_reply_user_id("direct", "ou_abc123", NULL, NULL);
        TEST_STR("direct user_id", r, "ou_abc123");
    }

    /* Test 9: Nested with open_id */
    {
        const char *r = get_reply_user_id("nested", "", "ou_open123", "ui_456");
        TEST_STR("nested open_id", r, "ou_open123");
    }

    /* Test 10: Nested with user_id (no open_id) */
    {
        const char *r = get_reply_user_id("nested", "", "", "ui_789");
        TEST_STR("nested user_id fallback", r, "ui_789");
    }

    /* Test 11: Neither direct nor nested */
    {
        const char *r = get_reply_user_id("unknown", "", NULL, NULL);
        TEST_STR("unknown type -> empty", r, "");
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All Feishu comment tests PASSED");
    return failures;
}
