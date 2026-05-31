/*
 * test_telegram_thread_id_standalone.c — Tests for General topic thread_id mapping.
 * Standalone — no Telegram deps needed. Tests the logic embedded in send_message.c.
 * Compile: gcc -O2 -Wall -Wextra -o /tmp/t_tg_tid_sa test_telegram_thread_id_standalone.c -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Port of telegram_message_thread_id_for_send() — inline for testing */
static const char *message_thread_id_for_send(const char *thread_id) {
    if (!thread_id) return NULL;
    if (strcmp(thread_id, "1") == 0) return NULL;
    return thread_id;
}

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Telegram Thread ID for Send Tests ===\n");

    /* Test 1: NULL thread_id returns NULL */
    TEST("NULL -> NULL", message_thread_id_for_send(NULL) == NULL);

    /* Test 2: thread_id "1" (General topic) returns NULL */
    {
        const char *r = message_thread_id_for_send("1");
        TEST("'1' -> NULL (General topic)", r == NULL);
    }

    /* Test 3: thread_id "42" returns "42" */
    {
        const char *r = message_thread_id_for_send("42");
        TEST("'42' -> '42'", r && strcmp(r, "42") == 0);
    }

    /* Test 4: thread_id "0" returns "0" (valid topic ID) */
    {
        const char *r = message_thread_id_for_send("0");
        TEST("'0' -> '0'", r && strcmp(r, "0") == 0);
    }

    /* Test 5: thread_id "999" returns "999" */
    {
        const char *r = message_thread_id_for_send("999");
        TEST("'999' -> '999'", r && strcmp(r, "999") == 0);
    }

    /* Test 6: thread_id "" (empty string) returns "" (not "1") */
    {
        const char *r = message_thread_id_for_send("");
        TEST("'' -> ''", r && strcmp(r, "") == 0);
    }

    /* Edge cases */
    {
        const char *r = message_thread_id_for_send("01");
        TEST("'01' -> '01' (not same as '1')", r && strcmp(r, "01") == 0);
    }
    {
        const char *r = message_thread_id_for_send(" 1");
        TEST("' 1' -> ' 1' (leading space != '1')", r && strcmp(r, " 1") == 0);
    }
    {
        const char *r = message_thread_id_for_send("1 ");
        TEST("'1 ' -> '1 ' (trailing space != '1')", r && strcmp(r, "1 ") == 0);
    }
    {
        const char *r = message_thread_id_for_send("abc");
        TEST("'abc' -> 'abc' (non-numeric passes through)", r && strcmp(r, "abc") == 0);
    }
    {
        const char *r = message_thread_id_for_send("-1");
        TEST("'-1' -> '-1' (negative not '1')", r && strcmp(r, "-1") == 0);
    }
    {
        const char *r = message_thread_id_for_send("1.0");
        TEST("'1.0' -> '1.0' (float not '1')", r && strcmp(r, "1.0") == 0);
    }
    {
        const char *r = message_thread_id_for_send("0001");
        TEST("'0001' -> '0001' (leading zeros not '1')", r && strcmp(r, "0001") == 0);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All telegram thread_id tests PASSED");
    return failures ? 1 : 0;
}
