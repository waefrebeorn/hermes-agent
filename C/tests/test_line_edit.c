/*
 * test_line_edit.c — Line editor test suite.
 *
 * Tests: create/free, history save/load, buffer operations,
 * word motion helpers, kill/yank, transpose.
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/liblineedit \
 *       tests/test_line_edit.c lib/liblineedit/line_edit.c \
 *       -o /tmp/hermes_test_line_edit -lm
 *
 * Run:
 *   /tmp/hermes_test_line_edit
 */

#include "line_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) TEST(name, a && b && strcmp(a, b) == 0)
#define TEST_NULL(name, p) TEST(name, p == NULL)
#define TEST_NOT_NULL(name, p) TEST(name, p != NULL)

/* Dummy completion callback */
static char **dummy_complete(const char *partial, void *user_data) {
    (void)partial;
    (void)user_data;
    return NULL;
}

/* Helper: create a line editor with pre-filled buffer and cursor position.
   Caller must free the returned pointer. */
static line_edit_t *make_buffer(const char *text, size_t cursor) {
    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    if (!le) return NULL;
    /* Set buffer content manually */
    size_t len = strlen(text);
    if (len >= le->buf->cap) {
        size_t new_cap = len + 256;
        if (new_cap > LINE_EDIT_MAX_LINE) new_cap = LINE_EDIT_MAX_LINE;
        char *new_buf = realloc(le->buf->buf, new_cap);
        if (!new_buf) { line_edit_free(le); return NULL; }
        le->buf->buf = new_buf;
        le->buf->cap = new_cap;
    }
    memcpy(le->buf->buf, text, len + 1);
    le->buf->len = len;
    le->buf->cursor = (cursor <= len) ? cursor : len;
    le->kill_ring[0] = '\0';
    le->kill_ring_len = 0;
    return le;
}

/* ================================================================
 *  1. Create / Free
 * ================================================================ */
static void test_create_free(void) {
    printf("\n--- create/free ---\n");

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create with completion callback", le);
    line_edit_free(le);

    le = line_edit_create(NULL, NULL);
    TEST_NOT_NULL("create with NULL completion callback", le);
    line_edit_free(le);

    line_edit_free(NULL);
    TEST("free(NULL) no crash", 1);
}

/* ================================================================
 *  2. History save/load
 * ================================================================ */
static void test_history(void) {
    printf("\n--- history save/load ---\n");

    char tmpfile[] = "/tmp/hermes_test_hist_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) { TEST("mkstemp failed", 0); return; }
    close(fd);

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for history test", le);
    if (!le) { unlink(tmpfile); return; }

    bool ok = line_edit_save_history(le, tmpfile);
    TEST("save empty history", ok);

    ok = line_edit_load_history(le, tmpfile);
    TEST("load empty history", ok);

    line_edit_free(le);
    unlink(tmpfile);
}

/* ================================================================
 *  3. History with NULL/bad paths
 * ================================================================ */
static void test_history_errors(void) {
    printf("\n--- history error paths ---\n");

    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for error test", le);
    if (!le) return;

    bool ok = line_edit_save_history(le, NULL);
    TEST("save with NULL path returns false", !ok);

    ok = line_edit_load_history(le, NULL);
    TEST("load with NULL path returns false", !ok);

    ok = line_edit_load_history(le, "/nonexistent/path/history.txt");
    TEST("load from nonexistent path succeeds (empty history)", ok);

    ok = line_edit_save_history(le, "/nonexistent/dir/history.txt");
    TEST("save to nonexistent dir returns false", !ok);

    line_edit_free(le);
}

/* ================================================================
 *  4. Word forward / backward
 * ================================================================ */
static void test_word_motion(void) {
    printf("\n--- word motion ---\n");

    /* cursor_word_forward on empty buffer */
    line_edit_t *le = make_buffer("", 0);
    TEST_NOT_NULL("empty buffer", le);
    if (le) {
        line_edit_cursor_word_forward(le);
        TEST("cursor_word_forward on empty buffer stays at 0",
             le->buf->cursor == 0);
        line_edit_cursor_word_backward(le);
        TEST("cursor_word_backward on empty buffer stays at 0",
             le->buf->cursor == 0);
        line_edit_free(le);
    }

    /* cursor_word_forward: "hello world", cursor at 0 */
    le = make_buffer("hello world", 0);
    TEST_NOT_NULL("\"hello world\" at 0", le);
    if (le) {
        line_edit_cursor_word_forward(le);
        TEST("word_forward from 0 -> start of next word \"world\"",
             le->buf->cursor == 6);
        line_edit_free(le);
    }

    /* cursor_word_forward: "hello world", cursor at 5 (at the space) */
    le = make_buffer("hello world", 5);
    TEST_NOT_NULL("\"hello world\" at 5", le);
    if (le) {
        line_edit_cursor_word_forward(le);
        TEST("word_forward from 5 (space) -> start of \"world\"",
             le->buf->cursor == 6);
        line_edit_free(le);
    }

    /* cursor_word_backward: "hello world", cursor at 11 (end) */
    le = make_buffer("hello world", 11);
    TEST_NOT_NULL("\"hello world\" at 11", le);
    if (le) {
        line_edit_cursor_word_backward(le);
        TEST("word_backward from 11 -> start of \"world\"",
             le->buf->cursor == 6);
        line_edit_free(le);
    }

    /* cursor_word_backward: "hello world", cursor at 6 (start of "world") */
    le = make_buffer("hello world", 6);
    TEST_NOT_NULL("\"hello world\" at 6", le);
    if (le) {
        line_edit_cursor_word_backward(le);
        TEST("word_backward from 6 -> start of \"hello\"",
             le->buf->cursor == 0);
        line_edit_free(le);
    }

    /* cursor_word_forward at end of line */
    le = make_buffer("hello", 5);
    TEST_NOT_NULL("\"hello\" at 5", le);
    if (le) {
        line_edit_cursor_word_forward(le);
        TEST("word_forward at end stays at 5", le->buf->cursor == 5);
        line_edit_free(le);
    }

    /* cursor_word_backward at start */
    le = make_buffer("hello", 0);
    TEST_NOT_NULL("\"hello\" at 0", le);
    if (le) {
        line_edit_cursor_word_backward(le);
        TEST("word_backward at 0 stays at 0", le->buf->cursor == 0);
        line_edit_free(le);
    }
}

/* ================================================================
 *  5. Kill line / Yank
 * ================================================================ */
static void test_kill_yank(void) {
    printf("\n--- kill/yank ---\n");

    /* Kill from middle */
    line_edit_t *le = make_buffer("hello world", 6);
    TEST_NOT_NULL("\"hello world\" at 6", le);
    if (le) {
        line_edit_kill_line(le);
        TEST_STR_EQ("kill_line truncates buffer", le->buf->buf, "hello ");
        TEST("kill_ring saved \"world\"",
             strcmp(le->kill_ring, "world") == 0);
        TEST("kill_ring_len is 5", le->kill_ring_len == 5);
        line_edit_free(le);
    }

    /* Kill from end (nothing to kill) */
    le = make_buffer("hello", 5);
    TEST_NOT_NULL("\"hello\" at 5 (end)", le);
    if (le) {
        line_edit_kill_line(le);
        TEST_STR_EQ("kill_line at end leaves buffer unchanged",
                    le->buf->buf, "hello");
        TEST("kill_ring not modified (stays empty)",
             le->kill_ring_len == 0);
        line_edit_free(le);
    }

    /* Kill entire line */
    le = make_buffer("hello world", 0);
    TEST_NOT_NULL("\"hello world\" at 0", le);
    if (le) {
        line_edit_kill_line(le);
        TEST_STR_EQ("kill_line from start empties buffer",
                    le->buf->buf, "");
        TEST("kill_ring saved full text",
             strcmp(le->kill_ring, "hello world") == 0);
        line_edit_free(le);
    }

    /* Yank after kill */
    le = make_buffer("hello world", 6);
    TEST_NOT_NULL("yank after kill", le);
    if (le) {
        line_edit_kill_line(le);
        TEST_STR_EQ("after kill: buffer is \"hello \"",
                    le->buf->buf, "hello ");
        line_edit_yank(le);
        TEST_STR_EQ("after yank: buffer restored",
                    le->buf->buf, "hello world");
        TEST("cursor after yank",
             le->buf->cursor == 11);
        line_edit_free(le);
    }

    /* Yank with empty kill ring */
    le = make_buffer("hello", 0);
    TEST_NOT_NULL("yank with empty ring", le);
    if (le) {
        size_t prev_len = le->buf->len;
        line_edit_yank(le);
        TEST("yank on empty ring does nothing",
             le->buf->len == prev_len);
        line_edit_free(le);
    }
}

/* ================================================================
 *  6. Kill word forward
 * ================================================================ */
static void test_kill_word_forward(void) {
    printf("\n--- kill word forward ---\n");

    /* Kill "world" from middle */
    line_edit_t *le = make_buffer("hello world foo", 6);
    TEST_NOT_NULL("\"hello world foo\" at 6", le);
    if (le) {
        line_edit_kill_word_forward(le);
        TEST_STR_EQ("kill_word from 6 removes \"world\"",
                    le->buf->buf, "hello  foo");
        TEST("kill_ring saved \"world\"",
             strcmp(le->kill_ring, "world") == 0);
        line_edit_free(le);
    }

    /* Kill word at end */
    le = make_buffer("hello world foo", 16);
    TEST_NOT_NULL("\"hello world foo\" at 16 (after 'foo')", le);
    if (le) {
        line_edit_kill_word_forward(le);
        TEST("kill_word at end works on empty (no crash)", 1);
        line_edit_free(le);
    }

    /* Kill word at start */
    le = make_buffer("hello world", 0);
    TEST_NOT_NULL("\"hello world\" at 0", le);
    if (le) {
        line_edit_kill_word_forward(le);
        TEST_STR_EQ("kill_word from 0 removes \"hello\"",
                    le->buf->buf, " world");
        TEST("kill_ring saved \"hello\"",
             strcmp(le->kill_ring, "hello") == 0);
        line_edit_free(le);
    }
}

/* ================================================================
 *  7. Transpose chars
 * ================================================================ */
static void test_transpose(void) {
    printf("\n--- transpose chars ---\n");

    /* Empty buffer */
    line_edit_t *le = make_buffer("", 0);
    TEST_NOT_NULL("empty buffer", le);
    if (le) {
        line_edit_transpose_chars(le);
        TEST("transpose on empty does nothing",
             le->buf->len == 0 && le->buf->cursor == 0);
        line_edit_free(le);
    }

    /* Single char */
    le = make_buffer("a", 1);
    TEST_NOT_NULL("single char", le);
    if (le) {
        line_edit_transpose_chars(le);
        TEST("transpose on single char does nothing",
             strcmp(le->buf->buf, "a") == 0);
        line_edit_free(le);
    }

    /* At end: swap last two chars */
    le = make_buffer("ab", 2);
    TEST_NOT_NULL("\"ab\" at 2 (end)", le);
    if (le) {
        line_edit_transpose_chars(le);
        TEST_STR_EQ("transpose at end swaps ab->ba",
                    le->buf->buf, "ba");
        line_edit_free(le);
    }

    /* At start: swap first two chars */
    le = make_buffer("abc", 0);
    TEST_NOT_NULL("\"abc\" at 0 (start)", le);
    if (le) {
        line_edit_transpose_chars(le);
        TEST_STR_EQ("transpose at start swaps first two",
                    le->buf->buf, "bac");
        line_edit_free(le);
    }

    /* In middle: swap cursor-1 and cursor */
    le = make_buffer("abcd", 2);
    TEST_NOT_NULL("\"abcd\" at 2", le);
    if (le) {
        line_edit_transpose_chars(le);
        TEST_STR_EQ("transpose at 2 swaps b<->c",
                    le->buf->buf, "acbd");
        line_edit_free(le);
    }
}

/* ================================================================
 *  8. Edge cases: NULL on all helpers
 * ================================================================ */
static void test_null_safety(void) {
    printf("\n--- NULL safety ---\n");

    line_edit_kill_line(NULL);
    TEST("kill_line(NULL) no crash", 1);

    line_edit_yank(NULL);
    TEST("yank(NULL) no crash", 1);

    line_edit_kill_word_forward(NULL);
    TEST("kill_word_forward(NULL) no crash", 1);

    line_edit_transpose_chars(NULL);
    TEST("transpose_chars(NULL) no crash", 1);

    line_edit_cursor_word_forward(NULL);
    TEST("cursor_word_forward(NULL) no crash", 1);

    line_edit_cursor_word_backward(NULL);
    TEST("cursor_word_backward(NULL) no crash", 1);
}

/* ================================================================
 *  9. set_text
 * ================================================================ */
static void test_set_text(void) {
    printf("\n--- set_text ---\n");

    /* Normal set */
    line_edit_t *le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for set_text", le);
    if (le) {
        line_edit_set_text(le, "hello world");
        TEST_STR_EQ("set_text sets buffer content",
                    le->buf->buf, "hello world");
        TEST("set_text cursor at end",
             le->buf->cursor == 11);
        line_edit_free(le);
    }

    /* NULL text (no crash) */
    le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for null text test", le);
    if (le) {
        line_edit_set_text(le, NULL);
        TEST("set_text(NULL) leaves buffer empty",
             le->buf->len == 0);
        line_edit_free(le);
    }

    /* NULL editor (no crash) */
    line_edit_set_text(NULL, "test");
    TEST("set_text on NULL editor no crash", 1);

    /* Overwrite existing content */
    le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for overwrite test", le);
    if (le) {
        line_edit_set_text(le, "first");
        line_edit_set_text(le, "second");
        TEST_STR_EQ("set_text overwrites existing content",
                    le->buf->buf, "second");
        line_edit_free(le);
    }

    /* Empty string */
    le = line_edit_create(dummy_complete, NULL);
    TEST_NOT_NULL("create for empty string test", le);
    if (le) {
        line_edit_set_text(le, "");
        TEST_STR_EQ("set_text empty string", le->buf->buf, "");
        TEST("set_text empty string cursor at 0",
             le->buf->cursor == 0);
        line_edit_free(le);
    }
}

int main(void) {
    printf("=== Line Editor Test Suite ===\n");

    test_create_free();
    test_history();
    test_history_errors();
    test_word_motion();
    test_kill_yank();
    test_kill_word_forward();
    test_transpose();
    test_null_safety();
    test_set_text();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
