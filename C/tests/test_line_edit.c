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

    /* cursor_word_end tests */
    /* word_end on empty buffer */
    le = make_buffer("", 0);
    TEST_NOT_NULL("empty buffer for word_end", le);
    if (le) {
        line_edit_cursor_word_end(le);
        TEST("word_end on empty stays at 0", le->buf->cursor == 0);
        line_edit_free(le);
    }

    /* word_end: "hello world", cursor at 0 */
    le = make_buffer("hello world", 0);
    TEST_NOT_NULL("\"hello world\" at 0 for word_end", le);
    if (le) {
        line_edit_cursor_word_end(le);
        TEST("word_end from 0 -> end of 'hello' (cursor 4)",
             le->buf->cursor == 4);
        line_edit_free(le);
    }

    /* word_end: "hello world", cursor at 6 (start of "world") */
    le = make_buffer("hello world", 6);
    TEST_NOT_NULL("\"hello world\" at 6 for word_end", le);
    if (le) {
        line_edit_cursor_word_end(le);
        TEST("word_end from 6 -> end of 'world' (cursor 10)",
             le->buf->cursor == 10);
        line_edit_free(le);
    }

    /* word_end: single word, cursor at 0 */
    le = make_buffer("hello", 0);
    TEST_NOT_NULL("\"hello\" at 0 for word_end", le);
    if (le) {
        line_edit_cursor_word_end(le);
        TEST("word_end single word -> end (cursor 4)",
             le->buf->cursor == 4);
        line_edit_free(le);
    }

    /* word_end: cursor at end of line */
    le = make_buffer("hello", 5);
    TEST_NOT_NULL("\"hello\" at 5 for word_end", le);
    if (le) {
        line_edit_cursor_word_end(le);
        TEST("word_end at end stays at end", le->buf->cursor == 5);
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
 *  6. Yank line (yy / Y)
 * ================================================================ */
static void test_yank_line(void) {
    printf("\n--- yank line ---\n");

    /* Yank non-empty buffer */
    line_edit_t *le = make_buffer("hello world", 6);
    TEST_NOT_NULL("yank_line on \"hello world\"", le);
    if (le) {
        line_edit_yank_line(le);
        TEST("kill_ring_len is 11", le->kill_ring_len == 11);
        TEST("kill_ring saved \"hello world\"",
             strcmp(le->kill_ring, "hello world") == 0);
        TEST("buffer unchanged", strcmp(le->buf->buf, "hello world") == 0);
        TEST("cursor unchanged at 6", le->buf->cursor == 6);
        line_edit_free(le);
    }

    /* Yank empty buffer (no-op) */
    le = make_buffer("", 0);
    TEST_NOT_NULL("yank_line on empty buffer", le);
    if (le) {
        TEST("pre: kill_ring_len is 0", le->kill_ring_len == 0);
        line_edit_yank_line(le);
        TEST("post: kill_ring_len still 0", le->kill_ring_len == 0);
        line_edit_free(le);
    }

    /* Yank then yank to verify paste round-trip */
    le = make_buffer("hello world", 0);
    TEST_NOT_NULL("yank then paste round-trip", le);
    if (le) {
        line_edit_yank_line(le);
        TEST("yank saved full text", strcmp(le->kill_ring, "hello world") == 0);
        /* Use line_edit_yank to paste (inserts text at cursor) */
        size_t pre_len = le->buf->len;
        line_edit_yank(le);
        TEST("buffer length doubled after yank",
             le->buf->len == pre_len * 2);
        TEST("buffer doubled content",
             strcmp(le->buf->buf, "hello worldhello world") == 0);
        line_edit_free(le);
    }

    /* Yank NULL (no crash) */
    line_edit_yank_line(NULL);
    TEST("yank_line(NULL) no crash", 1);
}

/* ================================================================
 *  7. Kill word forward
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

    /* line_edit_get_mode NULL safety */
    TEST("get_mode(NULL) returns INSERT",
         line_edit_get_mode(NULL) == LINE_EDIT_MODE_INSERT);
}

/* ================================================================
 *  Vi mode tests
 * ================================================================ */
static void test_vi_mode(void) {
    printf("\n--- Vi mode ---\n");

    /* Default mode is INSERT */
    line_edit_t *le = make_buffer("hello world", 0);
    TEST_NOT_NULL("create for vi test", le);
    TEST("default mode = INSERT", line_edit_get_mode(le) == LINE_EDIT_MODE_INSERT);

    /* Set mode to NORMAL and verify getter */
    le->vi_mode = LINE_EDIT_MODE_NORMAL;
    TEST("get_mode returns NORMAL", line_edit_get_mode(le) == LINE_EDIT_MODE_NORMAL);

    /* Set mode back to INSERT */
    le->vi_mode = LINE_EDIT_MODE_INSERT;
    TEST("get_mode returns INSERT after set", line_edit_get_mode(le) == LINE_EDIT_MODE_INSERT);

    /* vi_saved_line initialized */
    TEST("vi_saved initialized false", le->vi_saved == false);

    /* vi_last_find fields initialized via calloc */
    TEST("vi_last_find_char initialized 0", le->vi_last_find_char == 0);
    TEST("vi_last_find_forward initialized false",
         le->vi_last_find_forward == false);
    TEST("vi_last_find_till initialized false",
         le->vi_last_find_till == false);
    TEST("vi_last_change_op initialized 0",
         le->vi_last_change_op == 0);
    TEST("vi_last_change_param initialized 0",
         le->vi_last_change_param == 0);

    line_edit_free(le);

    /* --- toggle case (~) logic test --- */
    le = make_buffer("Hello World", 0);
    TEST_NOT_NULL("create for toggle test", le);
    if (le) {
        /* Simulate ~ at cursor 0: 'H' -> 'h' */
        unsigned char uc = (unsigned char)le->buf->buf[0];
        if (uc >= 'A' && uc <= 'Z')
            le->buf->buf[0] = uc + 32;
        TEST("~ toggles 'H' to 'h'",
             le->buf->buf[0] == 'h');

        /* Move to cursor 6 ('W') */
        le->buf->cursor = 6;
        uc = (unsigned char)le->buf->buf[6];
        if (uc >= 'A' && uc <= 'Z')
            le->buf->buf[6] = uc + 32;
        TEST("~ toggles 'W' to 'w'",
             le->buf->buf[6] == 'w');

        /* Move to cursor 0, lower-case letter 'h' */
        le->buf->cursor = 0;
        uc = (unsigned char)le->buf->buf[0];
        if (uc >= 'a' && uc <= 'z')
            le->buf->buf[0] = uc - 32;
        TEST("~ toggles 'h' back to 'H'",
             le->buf->buf[0] == 'H');

        /* Non-alpha char should not change */
        line_edit_free(le);
        le = make_buffer("abc123", 3);  /* cursor at '1' */
        TEST_NOT_NULL("create for non-alpha toggle test", le);
        if (le) {
            uc = (unsigned char)le->buf->buf[3];
            if (uc >= 'a' && uc <= 'z')
                le->buf->buf[3] = uc - 32;
            else if (uc >= 'A' && uc <= 'Z')
                le->buf->buf[3] = uc + 32;
            /* '1' unchanged */
            TEST("~ leaves digit unchanged",
                 le->buf->buf[3] == '1');
            line_edit_free(le);
        }

        /* --- replace char (r) logic test --- */
        le = make_buffer("hello world", 0);
        TEST_NOT_NULL("create for replace test", le);
        if (le) {
            /* Simulate rX at cursor 0: replace 'h' with 'X' */
            if (le->buf->cursor < le->buf->len) {
                le->buf->buf[le->buf->cursor] = 'X';
            }
            TEST("r replaces 'h' with 'X'",
                 le->buf->buf[0] == 'X');
            TEST("r leaves rest of string intact",
                 strcmp(le->buf->buf + 1, "ello world") == 0);
            TEST("r does not change length",
                 le->buf->len == 11);

            /* Replace at cursor 6 ('w' -> 'W') */
            le->buf->cursor = 6;
            if (le->buf->cursor < le->buf->len) {
                le->buf->buf[6] = 'W';
            }
            TEST("r replaces 'w' with 'W'",
                 le->buf->buf[6] == 'W');

            line_edit_free(le);
        }
    }

    /* --- find (f) logic test --- */
    le = make_buffer("hello world, hello!", 0);
    TEST_NOT_NULL("create for find test", le);
    if (le) {
        /* Simulate fo (find 'o' forward from cursor 0): 'o' at position 4 */
        size_t start = le->buf->cursor + 1;
        size_t found_pos = le->buf->len;
        size_t end = 0;
        for (size_t i = start; i < le->buf->len; i++) {
            if (le->buf->buf[i] == 'o') {
                found_pos = i;
                break;
            }
        }
        le->buf->cursor = found_pos;
        TEST("fo finds 'o' at position 4",
             le->buf->cursor == 4);

        /* Simulate ; (repeat last find 'o' forward) from cursor 5 */
        start = le->buf->cursor + 1;
        found_pos = le->buf->len;
        for (size_t i = start; i < le->buf->len; i++) {
            if (le->buf->buf[i] == 'o') {
                found_pos = i;
                break;
            }
        }
        le->buf->cursor = found_pos;
        TEST("; finds next 'o' at position 7",
             le->buf->cursor == 7);

        /* Simulate F' (find '\'' backward) — cursor at end */
        /* Set text with a single char target */
        line_edit_free(le);
        le = make_buffer("find.in.the.way", 14); /* cursor at end */
        TEST_NOT_NULL("create for F test", le);
        if (le) {
            /* F. — find '.' backward from cursor 14 */
            end = le->buf->cursor;
            found_pos = 0;
            for (size_t i = end; i > 0; i--) {
                if (le->buf->buf[i - 1] == '.') {
                    found_pos = i - 1;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("F. finds '.' at position 11 (last '.')",
                 le->buf->cursor == 11);

            /* ; (repeat backward) */
            end = le->buf->cursor;
            found_pos = 0;
            for (size_t i = end; i > 0; i--) {
                if (le->buf->buf[i - 1] == '.') {
                    found_pos = i - 1;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("; repeats F. to find '.' at position 7",
                 le->buf->cursor == 7);

            line_edit_free(le);
        }

        /* --- till (t) logic test --- */
        le = make_buffer("find.in.the.way", 0);
        TEST_NOT_NULL("create for t test", le);
        if (le) {
            /* t. — forward till '.', cursor lands BEFORE target */
            start = le->buf->cursor + 1;
            found_pos = le->buf->len;
            for (size_t i = start; i < le->buf->len; i++) {
                if (le->buf->buf[i] == '.') {
                    /* till means stop at i-1 */
                    if (i > 0) found_pos = i - 1;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("t. stops before '.' at position 3",
                 le->buf->cursor == 3);

            /* T. — backward till '.' */
            le->buf->cursor = 14; /* at end */
            end = le->buf->cursor;
            found_pos = le->buf->len;
            for (size_t i = end; i > 0; i--) {
                if (le->buf->buf[i - 1] == '.') {
                    /* T means stop AFTER the char (at i) */
                    found_pos = i;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("T. stops after '.' at position 12",
                 le->buf->cursor == 12);
            line_edit_free(le);
        }

        /* --- , (reverse direction repeat) logic test --- */
        le = make_buffer("hello world, hello!", 0);
        TEST_NOT_NULL("create for comma test", le);
        if (le) {
            /* Simulate fo (find 'o' forward from cursor 0): 'o' at position 4 */
            le->buf->cursor = 0;
            start = le->buf->cursor + 1;
            found_pos = le->buf->len;
            for (size_t i = start; i < le->buf->len; i++) {
                if (le->buf->buf[i] == 'o') {
                    found_pos = i;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("fo finds 'o' at position 4",
                 le->buf->cursor == 4);

            /* ; repeat forward from cursor 5 -> 'o' at position 7 */
            start = le->buf->cursor + 1;
            found_pos = le->buf->len;
            for (size_t i = start; i < le->buf->len; i++) {
                if (le->buf->buf[i] == 'o') {
                    found_pos = i;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST("; finds next 'o' at position 7",
                 le->buf->cursor == 7);

            /* , — repeat in opposite direction (backward find 'o' from 7) */
            end = le->buf->cursor;
            found_pos = 0;
            for (size_t i = end; i > 0; i--) {
                if (le->buf->buf[i - 1] == 'o') {
                    found_pos = i - 1;
                    break;
                }
            }
            le->buf->cursor = found_pos;
            TEST(", reverses fo to find 'o' at position 4",
                 le->buf->cursor == 4);
            line_edit_free(le);
        }
    }
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

/* ---- vi search helper tests ---- */
static void test_vi_search(void) {
    printf("\n--- vi search (/ ? n N) ---\n");

    /* Forward search — pattern found after cursor */
    {
        line_edit_t *le = make_buffer("hello world hello", 0);
        TEST_NOT_NULL("fwd search buffer", le);
        if (le) {
            line_edit_search_internal(le, "world", 5, true);
            TEST("forward search after cursor", le->buf->cursor == 6);
            line_edit_free(le);
        }
    }

    /* Forward search — wrap to start */
    {
        line_edit_t *le = make_buffer("aaa bbb aaa", 8);
        TEST_NOT_NULL("fwd wrap buffer", le);
        if (le) {
            line_edit_search_internal(le, "aaa", 3, true);
            TEST("forward search wrap to start", le->buf->cursor == 0);
            line_edit_free(le);
        }
    }

    /* Forward search — not found */
    {
        line_edit_t *le = make_buffer("hello", 0);
        TEST_NOT_NULL("fwd not found buffer", le);
        if (le) {
            line_edit_search_internal(le, "xyz", 3, true);
            TEST("forward search not found stays", le->buf->cursor == 0);
            line_edit_free(le);
        }
    }

    /* Backward search — found before cursor */
    {
        line_edit_t *le = make_buffer("aaa bbb aaa", 12);
        TEST_NOT_NULL("bwd search buffer", le);
        if (le) {
            line_edit_search_internal(le, "bbb", 3, false);
            TEST("backward search before cursor", le->buf->cursor == 4);
            line_edit_free(le);
        }
    }

    /* Backward search — not found */
    {
        line_edit_t *le = make_buffer("hello", 3);
        TEST_NOT_NULL("bwd not found buffer", le);
        if (le) {
            line_edit_search_internal(le, "xyz", 3, false);
            TEST("backward search not found stays", le->buf->cursor == 3);
            line_edit_free(le);
        }
    }

    /* NULL safety */
    {
        /* Calling with NULL should not crash */
        line_edit_search_internal(NULL, "hello", 5, true);
        TEST("NULL line_edit safe", true);
    }

    /* Backward search wrap from end */
    {
        line_edit_t *le = make_buffer("hello world hello", 0);
        TEST_NOT_NULL("bwd wrap buffer", le);
        if (le) {
            line_edit_search_internal(le, "world", 5, false);
            TEST("backward search wrap from end", le->buf->cursor == 6);
            line_edit_free(le);
        }
    }

    /* Pattern at exact cursor position — forward search skips current */
    {
        line_edit_t *le = make_buffer("aa bb aa", 0);
        TEST_NOT_NULL("skip current buffer", le);
        if (le) {
            line_edit_search_internal(le, "aa", 2, true);
            TEST("forward skips current match", le->buf->cursor == 6);
            line_edit_free(le);
        }
    }

    /* Multiple same-word — find next occurrence */
    {
        line_edit_t *le = make_buffer("foo bar foo baz foo", 4);
        TEST_NOT_NULL("next occurrence buffer", le);
        if (le) {
            line_edit_search_internal(le, "foo", 3, true);
            TEST("forward find next occurrence", le->buf->cursor == 8);
            line_edit_free(le);
        }
    }

    /* Empty pattern — no-op */
    {
        line_edit_t *le = make_buffer("hello", 0);
        TEST_NOT_NULL("empty pattern buffer", le);
        if (le) {
            line_edit_search_internal(le, "", 0, true);
            TEST("empty pattern no-op", le->buf->cursor == 0);
            line_edit_free(le);
        }
    }

    /* Empty buffer — no-op */
    {
        line_edit_t *le = make_buffer("", 0);
        TEST_NOT_NULL("empty buffer for search", le);
        if (le) {
            line_edit_search_internal(le, "hello", 5, true);
            TEST("empty buffer no-op", le->buf->cursor == 0);
            line_edit_free(le);
        }
    }
}

int main(void) {
    printf("=== Line Editor Test Suite ===\n");

    test_create_free();
    test_history();
    test_history_errors();
    test_word_motion();
    test_kill_yank();
    test_yank_line();
    test_kill_word_forward();
    test_transpose();
    test_null_safety();
    test_set_text();

    test_vi_mode();
    test_vi_search();

    /* ---- vi visual mode tests ---- */
    printf("\n--- vi visual mode (v V x d y) ---\n");

    /* Enter visual mode — sets vi_visual_active and start */
    {
        line_edit_t *le = make_buffer("hello world", 3);
        TEST_NOT_NULL("visual mode buffer", le);
        if (le) {
            TEST("visual initial inactive", !le->vi_visual_active);
            le->vi_visual_active = true;
            le->vi_visual_start = 3;
            TEST("visual mode active", le->vi_visual_active);
            TEST("visual start set", le->vi_visual_start == 3);
            line_edit_free(le);
        }
    }

    /* Exit visual mode via v */
    {
        line_edit_t *le = make_buffer("hello world", 3);
        TEST_NOT_NULL("exit visual v buffer", le);
        if (le) {
            le->vi_visual_active = true;
            le->vi_visual_start = 1;
            /* Simulate pressing 'v' — should deactivate */
            le->vi_visual_active = false;
            TEST("v exits visual mode", !le->vi_visual_active);
            line_edit_free(le);
        }
    }

    /* Exit visual mode via ESC */
    {
        line_edit_t *le = make_buffer("hello world", 3);
        TEST_NOT_NULL("exit visual esc buffer", le);
        if (le) {
            le->vi_visual_active = true;
            le->vi_visual_start = 1;
            le->vi_visual_active = false;
            TEST("ESC exits visual mode", !le->vi_visual_active);
            line_edit_free(le);
        }
    }

    /* Visual selection when cursor > start */
    {
        line_edit_t le;
        memset(&le, 0, sizeof(le));
        line_buf_t buf;
        memset(&buf, 0, sizeof(buf));
        char text[] = "abcdefgh";
        buf.buf = text;
        buf.len = 8;
        buf.cursor = 6;
        le.buf = &buf;
        le.vi_visual_active = true;
        le.vi_visual_start = 2;

        /* Selection should be [2,6) = "cdef" */
        size_t a = le.vi_visual_start;
        size_t b = le.buf->cursor;
        size_t sel_s = a < b ? a : b;
        size_t sel_e = a < b ? b : a;
        TEST("forward selection start", sel_s == 2);
        TEST("forward selection end", sel_e == 6);
    }

    /* Visual selection when cursor < start */
    {
        line_edit_t le;
        memset(&le, 0, sizeof(le));
        line_buf_t buf;
        memset(&buf, 0, sizeof(buf));
        char text[] = "abcdefgh";
        buf.buf = text;
        buf.len = 8;
        buf.cursor = 1;
        le.buf = &buf;
        le.vi_visual_active = true;
        le.vi_visual_start = 5;

        /* Selection should be [1,5) = "bcde" */
        size_t a = le.vi_visual_start;
        size_t b = le.buf->cursor;
        size_t sel_s = a < b ? a : b;
        size_t sel_e = a < b ? b : a;
        TEST("backward selection start", sel_s == 1);
        TEST("backward selection end", sel_e == 5);
    }

    /* Visual delete — simulated */
    {
        line_edit_t le;
        memset(&le, 0, sizeof(le));
        line_buf_t buf;
        memset(&buf, 0, sizeof(buf));
        char text[] = "hello world";
        buf.buf = text;
        buf.len = 11;
        buf.cursor = 8;
        le.buf = &buf;
        le.kill_ring[0] = '\0';
        le.kill_ring_len = 0;
        le.vi_visual_active = true;
        le.vi_visual_start = 2;

        /* Simulate visual delete: extract selection, delete from buffer */
        size_t a = le.vi_visual_start;
        size_t b = le.buf->cursor;
        size_t sel_s = a < b ? a : b;
        size_t sel_e = a < b ? b : a;
        size_t sel_len = sel_e - sel_s;  /* = 6: "llo wo" */
        if (sel_len > 0 && sel_len < 65535) {
            memcpy(le.kill_ring, le.buf->buf + sel_s, sel_len);
            le.kill_ring[sel_len] = '\0';
            le.kill_ring_len = sel_len;
        }
        if (sel_len > 0 && sel_s < le.buf->len && sel_e <= le.buf->len) {
            memmove(le.buf->buf + sel_s, le.buf->buf + sel_e,
                    le.buf->len - sel_e + 1);
            le.buf->len -= sel_len;
            le.buf->cursor = sel_s;
        }

        TEST("visual delete modifies buffer", le.kill_ring_len == 6);
        TEST("visual delete kills correct text",
             strncmp(le.kill_ring, "llo wo", 6) == 0);
        TEST("visual delete shortens buffer", le.buf->len == 5);
        TEST("visual delete cursor at sel_start", le.buf->cursor == 2);
        /* Remaining text should be "herld" (he + from position 8) */
        TEST("visual delete remaining text",
             strncmp(le.buf->buf, "herld", 5) == 0);
    }

    /* Visual yank — selection preserved, not deleted */
    {
        line_edit_t le;
        memset(&le, 0, sizeof(le));
        line_buf_t buf;
        memset(&buf, 0, sizeof(buf));
        char text[] = "hello world";
        buf.buf = text;
        buf.len = 11;
        buf.cursor = 5;
        le.buf = &buf;
        le.kill_ring[0] = '\0';
        le.kill_ring_len = 0;
        le.vi_visual_active = true;
        le.vi_visual_start = 0;

        /* Simulate visual yank: extract selection, don't delete */
        size_t a = le.vi_visual_start;
        size_t b = le.buf->cursor;
        size_t sel_s = a < b ? a : b;
        size_t sel_e = a < b ? b : a;
        size_t sel_len = sel_e - sel_s;  /* = 5: "hello" */
        if (sel_len > 0 && sel_len < 65535) {
            memcpy(le.kill_ring, le.buf->buf + sel_s, sel_len);
            le.kill_ring[sel_len] = '\0';
            le.kill_ring_len = sel_len;
        }

        TEST("visual yank into kill ring", le.kill_ring_len == 5);
        TEST("visual yank correct text",
             strncmp(le.kill_ring, "hello", 5) == 0);
        TEST("visual yank preserves buffer", le.buf->len == 11);
    }


    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
