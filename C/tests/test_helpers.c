/*
 * test_helpers.c — Tests for gateway helper utilities (G01).
 * Tests: msg_dedup, strip_markdown, redact_phone, thread_tracker.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_helpers.c ../src/gateway/helpers.c ../lib/libjson/json.c -o /tmp/t_helpers -lm -Wl,--unresolved-symbols=ignore-all
 */
#include "gateway_helpers.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Gateway Helpers Tests ===\n");

    /* ── msg_dedup ── */

    /* Test 1: init */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        TEST("msg_dedup_init sets count 0", d.count == 0);
        TEST("msg_dedup_init sets max_size 2000", d.max_size == 2000);
        TEST("msg_dedup_init sets ttl", d.ttl_seconds > 0);
        msg_dedup_destroy(&d);
    }

    /* Test 2: init_custom */
    {
        msg_dedup_t d;
        msg_dedup_init_custom(&d, 100, 60.0);
        TEST("msg_dedup_init_custom count 0", d.count == 0);
        TEST("msg_dedup_init_custom max_size 100", d.max_size == 100);
        TEST("msg_dedup_init_custom ttl 60", d.ttl_seconds == 60.0);
        msg_dedup_destroy(&d);
    }

    /* Test 3: first message is not duplicate */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        TEST("first msg not duplicate", !msg_dedup_is_duplicate(&d, "msg1"));
        msg_dedup_destroy(&d);
    }

    /* Test 4: same message twice is duplicate */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        msg_dedup_is_duplicate(&d, "dup_test");
        TEST("repeated msg is duplicate", msg_dedup_is_duplicate(&d, "dup_test"));
        msg_dedup_destroy(&d);
    }

    /* Test 5: different messages are not duplicates */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        msg_dedup_is_duplicate(&d, "msg_a");
        TEST("different msg not duplicate", !msg_dedup_is_duplicate(&d, "msg_b"));
        msg_dedup_destroy(&d);
    }

    /* Test 6: NULL msg_id returns false */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        TEST("NULL msg_id not duplicate", !msg_dedup_is_duplicate(&d, NULL));
        msg_dedup_destroy(&d);
    }

    /* Test 7: empty msg_id returns false */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        TEST("empty msg_id not duplicate", !msg_dedup_is_duplicate(&d, ""));
        msg_dedup_destroy(&d);
    }

    /* Test 8: clear resets */
    {
        msg_dedup_t d;
        msg_dedup_init(&d);
        msg_dedup_is_duplicate(&d, "clear_test");
        msg_dedup_clear(&d);
        TEST("after clear, duplicate not detected", !msg_dedup_is_duplicate(&d, "clear_test"));
        msg_dedup_destroy(&d);
    }

    /* Test 9: NULL dedup in is_duplicate */
    {
        TEST("NULL dedup returns false", !msg_dedup_is_duplicate(NULL, "test"));
    }

    /* ── strip_markdown ── */

    /* Test 10: NULL text */
    {
        char *res = strip_markdown(NULL);
        TEST("strip_markdown(NULL) returns NULL", res == NULL);
    }

    /* Test 11: plain text unchanged */
    {
        char *res = strip_markdown("hello world");
        TEST("strip_markdown plain text", res && strcmp(res, "hello world") == 0);
        free(res);
    }

    /* Test 12: bold removed */
    {
        char *res = strip_markdown("**bold** text");
        TEST("strip_markdown bold", res && strstr(res, "bold") != NULL);
        TEST("strip_markdown no **", res && strstr(res, "**") == NULL);
        free(res);
    }

    /* Test 13: inline code */
    {
        char *res = strip_markdown("text `code` more");
        TEST("strip_markdown code", res && strstr(res, "code") != NULL);
        TEST("strip_markdown no backticks", res && strstr(res, "`") == NULL);
        free(res);
    }

    /* Test 14: heading stripped */
    {
        char *res = strip_markdown("# Title");
        char *trimmed = res;
        while (*trimmed == ' ') trimmed++;
        TEST("strip_markdown heading", trimmed && strcmp(trimmed, "Title") == 0);
        free(res);
    }

    /* Test 15: link stripped to text — ]( replaced with space, ) removed */
    {
        char *res = strip_markdown("[click](https://example.com)");
        TEST("strip_markdown link text preserved", res && strstr(res, "click") != NULL);
        /* URL text remains as non-link text after ]( → space replacement */
        free(res);
    }

    /* Test 16: code block fenced */
    {
        char *res = strip_markdown("```\ncode\n```\nmore");
        TEST("strip_markdown code fence removed", res && strstr(res, "```") == NULL);
        TEST("strip_markdown code content preserved", res && strstr(res, "code") != NULL);
        free(res);
    }

    /* ── redact_phone ── */

    /* Test 17: NULL phone */
    {
        char *res = redact_phone(NULL);
        TEST("redact_phone(NULL) returns '<none>'", res && strcmp(res, "<none>") == 0);
        free(res);
    }

    /* Test 18: <none> */
    {
        char *res = redact_phone("<none>");
        TEST("redact_phone(<none>) returns '<none>'", res && strcmp(res, "<none>") == 0);
        free(res);
    }

    /* Test 19: short phone (<=4 chars) */
    {
        char *res = redact_phone("1234");
        TEST("redact_phone short returns ****", res && strcmp(res, "****") == 0);
        free(res);
    }

    /* Test 20: medium phone (5-8 chars) */
    {
        char *res = redact_phone("12345678");
        TEST("redact_phone medium shows first 2 + last 2", res && strcmp(res, "12****78") == 0);
        free(res);
    }

    /* Test 21: long phone (9+ chars) */
    {
        char *res = redact_phone("1234567890");
        TEST("redact_phone long shows first 4 + last 4", res && strcmp(res, "1234****7890") == 0);
        free(res);
    }

    /* ── thread_tracker ── */

    /* Test 22: init */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test_plat", "/tmp");
        TEST("thread_tracker_init count 0", t.count == 0);
        TEST("thread_tracker_init platform set", strcmp(t.platform, "test_plat") == 0);
        TEST("thread_tracker_init state_dir set", strcmp(t.state_dir, "/tmp") == 0);
        TEST("thread_tracker_init max_tracked 500", t.max_tracked == 500);
        thread_tracker_destroy(&t);
    }

    /* Test 23: mark adds thread */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        thread_tracker_mark(&t, "thread_1");
        TEST("thread_tracker_mark count 1", t.count == 1);
        TEST("thread_tracker_mark thread exists", thread_tracker_has(&t, "thread_1"));
        thread_tracker_destroy(&t);
    }

    /* Test 24: duplicate mark doesn't change count */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        thread_tracker_mark(&t, "dup");
        thread_tracker_mark(&t, "dup");
        TEST("thread_tracker duplicate mark count 1", t.count == 1);
        thread_tracker_destroy(&t);
    }

    /* Test 25: has returns false for unknown thread */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        thread_tracker_mark(&t, "known");
        TEST("thread_tracker unknown thread false", !thread_tracker_has(&t, "unknown"));
        thread_tracker_destroy(&t);
    }

    /* Test 26: NULL thread_id in has */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        TEST("thread_tracker NULL returns false", !thread_tracker_has(&t, NULL));
        thread_tracker_destroy(&t);
    }

    /* Test 27: multiple threads tracked */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        thread_tracker_mark(&t, "a");
        thread_tracker_mark(&t, "b");
        thread_tracker_mark(&t, "c");
        TEST("thread_tracker 3 threads count 3", t.count == 3);
        TEST("thread_tracker has a", thread_tracker_has(&t, "a"));
        TEST("thread_tracker has b", thread_tracker_has(&t, "b"));
        TEST("thread_tracker has c", thread_tracker_has(&t, "c"));
        thread_tracker_destroy(&t);
    }

    /* Test 28: destroy frees resources */
    {
        thread_tracker_t t;
        thread_tracker_init(&t, "test", "/tmp");
        thread_tracker_mark(&t, "x");
        thread_tracker_destroy(&t);
        TEST("thread_tracker_destroy count 0", t.count == 0);
        TEST("thread_tracker_destroy ids NULL", t.thread_ids == NULL);
    }

    /* Test 29: NULL tracker in mark */
    {
        thread_tracker_mark(NULL, "test");
        TEST("thread_tracker_mark(NULL) no crash", 1);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All helpers tests PASSED");
    return failures ? 1 : 0;
}
