/*
 * test_telegram_thread_not_found.c — Tests for Telegram thread-not-found detection.
 * Compile: gcc -O2 -Wall -Wextra -o /tmp/t_tg_tnf test_telegram_thread_not_found.c -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Port of telegram_is_thread_not_found() — inline for testing */
static bool is_thread_not_found(const char *error_text) {
    if (!error_text) return false;
    const char *p = error_text;
    while (*p) {
        if ((*p == 't' || *p == 'T') &&
            (p[1] == 'h' || p[1] == 'H') &&
            (p[2] == 'r' || p[2] == 'R') &&
            (p[3] == 'e' || p[3] == 'E') &&
            (p[4] == 'a' || p[4] == 'A') &&
            (p[5] == 'd' || p[5] == 'D') &&
            (p[6] == ' ' || p[6] == '_' || p[6] == '-') &&
            (p[7] == 'n' || p[7] == 'N') &&
            (p[8] == 'o' || p[8] == 'O') &&
            (p[9] == 't' || p[9] == 'T') &&
            (p[10] == ' ' || p[10] == '_' || p[10] == '-') &&
            (p[11] == 'f' || p[11] == 'F') &&
            (p[12] == 'o' || p[12] == 'O') &&
            (p[13] == 'u' || p[13] == 'U') &&
            (p[14] == 'n' || p[14] == 'N') &&
            (p[15] == 'd' || p[15] == 'D')) {
            return true;
        }
        p++;
    }
    return false;
}

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Telegram Thread-Not-Found Detection Tests ===\n");

    /* Test 1: NULL error text returns false */
    TEST("NULL -> false", !is_thread_not_found(NULL));

    /* Test 2: Empty string returns false */
    TEST("empty -> false", !is_thread_not_found(""));

    /* Test 3: Exact match "thread not found" */
    TEST("exact 'thread not found'", is_thread_not_found("thread not found"));

    /* Test 4: Case insensitive "THREAD NOT FOUND" */
    TEST("case insensitive UPPER", is_thread_not_found("THREAD NOT FOUND"));

    /* Test 5: Mixed case "Thread Not Found" */
    TEST("mixed case", is_thread_not_found("Thread Not Found"));

    /* Test 6: In a longer message */
    TEST("in sentence", is_thread_not_found("Error: Message thread not found in chat"));

    /* Test 7: With underscores (telegram_keyboard_update error format) */
    TEST("with underscores", is_thread_not_found("thread_not_found"));

    /* Test 8: With hyphens */
    TEST("with hyphens", is_thread_not_found("thread-not-found"));

    /* Test 9: Partial word "thread_no" should NOT match */
    TEST("partial 'thread_no'", !is_thread_not_found("thread_no"));

    /* Test 10: "thread_not_found_in_chat" should match */
    TEST("suffix after found", is_thread_not_found("thread_not_found_in_chat"));

    /* Test 11: Error about something else */
    TEST("unrelated error", !is_thread_not_found("Bad Request: chat not found"));

    /* Test 12: "thread not found" at start after prefix */
    TEST("prefixed message",
        is_thread_not_found("telegram error: thread not found (chat -100)"));

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All telegram thread-not-found tests PASSED");
    return failures;
}
