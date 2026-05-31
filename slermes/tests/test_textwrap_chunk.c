/*
 * test_textwrap_chunk.c — Tests for textwrap_chunk().
 * Standalone — no deps beyond libc.
 * Compile: gcc -O2 -Wall -Wextra -I../include -I../lib/libtextwrap -o /tmp/t_twc test_textwrap_chunk.c ../lib/libtextwrap/textwrap.c -lm
 */
#include "textwrap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static void free_chunks(char **chunks, size_t count) {
    for (size_t i = 0; i < count; i++) free(chunks[i]);
    free(chunks);
}

int main(void) {
    printf("=== Textwrap Chunk Tests ===\n");

    /* Test 1: NULL input */
    {
        size_t count = 99;
        char **r = textwrap_chunk(NULL, 10, &count);
        TEST("NULL returns NULL", r == NULL);
        TEST("NULL sets count=0", count == 0);
    }

    /* Test 2: empty string */
    {
        size_t count = 99;
        char **r = textwrap_chunk("", 10, &count);
        TEST("empty returns non-NULL", r != NULL);
        TEST("empty sets count=0", count == 0);
        free_chunks(r, count);
    }

    /* Test 3: short text within limit */
    {
        size_t count = 0;
        char **r = textwrap_chunk("hello", 100, &count);
        TEST("short text returns 1 chunk", r && count == 1);
        if (r && count > 0) {
            TEST("short text correct", strcmp(r[0], "hello") == 0);
        }
        free_chunks(r, count);
    }

    /* Test 4: text exactly at limit */
    {
        size_t count = 0;
        char **r = textwrap_chunk("1234567890", 10, &count);
        TEST("exact limit returns 1 chunk", r && count == 1);
        if (r && count > 0) {
            TEST("exact limit correct", strcmp(r[0], "1234567890") == 0);
        }
        free_chunks(r, count);
    }

    /* Test 5: text exceeding limit, splits at newline */
    {
        size_t count = 0;
        char **r = textwrap_chunk("abc\ndef\nghi", 5, &count);
        TEST("newline-split returns 3 chunks", r && count == 3);
        if (r && count >= 3) {
            TEST("chunk 0 correct", strcmp(r[0], "abc") == 0);
            TEST("chunk 1 correct", strcmp(r[1], "def") == 0);
            TEST("chunk 2 correct", strcmp(r[2], "ghi") == 0);
        }
        free_chunks(r, count);
    }

    /* Test 6: text exceeding limit, no newline — hard cut */
    {
        size_t count = 0;
        char **r = textwrap_chunk("abcdefghijklmno", 5, &count);
        TEST("hard cut returns 3 chunks", r && count == 3);
        if (r && count >= 3) {
            TEST("hard cut chunk 0", strcmp(r[0], "abcde") == 0);
            TEST("hard cut chunk 1", strcmp(r[1], "fghij") == 0);
            TEST("hard cut chunk 2", strcmp(r[2], "klmno") == 0);
        }
        free_chunks(r, count);
    }

    /* Test 7: long line with newline break */
    {
        size_t count = 0;
        char **r = textwrap_chunk("hello world\nfoo bar baz", 12, &count);
        TEST("newline break returns 2 chunks", r && count == 2);
        if (r && count >= 2) {
            TEST("newline break chunk 0 ends at newline", strcmp(r[0], "hello world") == 0);
            TEST("newline break chunk 1", strcmp(r[1], "foo bar baz") == 0);
        }
        free_chunks(r, count);
    }

    /* Test 8: count_ptr=NULL returns NULL safely */
    {
        char **r = textwrap_chunk("test", 10, NULL);
        TEST("NULL count returns NULL", r == NULL);
    }

    /* ── Extended edge cases ── */
    printf("\n--- Extended edge cases ---\n");

    /* max_len=0 -> NULL (caught by max_len <= 0) */
    {
        size_t count = 99;
        char **r = textwrap_chunk("hello", 0, &count);
        TEST("max_len=0 returns NULL", r == NULL);
        TEST("max_len=0 sets count=0", count == 0);
    }

    /* max_len=1 — minimal boundary */
    {
        size_t count = 0;
        char **r = textwrap_chunk("ABCDE", 1, &count);
        TEST("max_len=1 returns 5 chunks", r && count == 5);
        if (r && count >= 5) {
            TEST("chunk 0 = A", r[0] && strcmp(r[0], "A") == 0);
            TEST("chunk 1 = B", r[1] && strcmp(r[1], "B") == 0);
            TEST("chunk 2 = C", r[2] && strcmp(r[2], "C") == 0);
        }
        free_chunks(r, count);
    }

    /* max_len negative -> NULL */
    {
        size_t count = 99;
        char **r = textwrap_chunk("hello", -5, &count);
        TEST("max_len=-5 returns NULL", r == NULL);
        TEST("max_len=-5 sets count=0", count == 0);
    }

    /* Text with consecutive newlines — must exceed max_len to trigger split */
    {
        size_t count = 0;
        /* "AAAAA\n\n\n\nbb" = 11 chars, max_len=8. Backward scan finds \n at pos 7.
         * this_chunk = 7 → chunk 0 = "AAAAA\n\n". Then skip 2 more \ns → chunk 1 = "bb". */
        char **r = textwrap_chunk("AAAAA\n\n\n\nbb", 8, &count);
        TEST("consecutive \\n returns 2 chunks", r && count == 2);
        if (r && count >= 2) {
            TEST("chunk 0 = AAAAA before \\n\\n", strcmp(r[0], "AAAAA\n\n") == 0);
            TEST("chunk 1 = bb after skipping \\n\\n", strcmp(r[1], "bb") == 0);
        }
        free_chunks(r, count);
    }

    /* Long text with max_len exactly matching newline positions */
    {
        size_t count = 0;
        /* "12345\nABCDE\nXYZ" = 14 chars, max_len=6. Backward scan finds \n at pos 5.
         * this_chunk = 5 (pos 0-4 = "12345"). Then skip \n at 5 → pos 6.
         * Second: remaining=8, this_chunk=6, scan finds \n at pos 11 → this_chunk = 11-6=5.
         * chunk 1 = "ABCDE", skip \n at 11 → pos 12.
         * Third: remaining=2, this_chunk=2. chunk 2 = "XY". No trailing \n after last char. */
        char **r = textwrap_chunk("12345\nABCDE\nXYZ", 6, &count);
        TEST("newline at exact boundary returns 3 chunks", r && count == 3);
        if (r && count >= 3) {
            TEST("chunk 0 = 12345", strcmp(r[0], "12345") == 0);
            TEST("chunk 1 = ABCDE", strcmp(r[1], "ABCDE") == 0);
            TEST("chunk 2 = XYZ", strcmp(r[2], "XYZ") == 0);
        }
        free_chunks(r, count);
    }

    /* Very short max_len with mixed content */
    {
        size_t count = 0;
        /* "ab\nc" = 4 chars, max_len=2. this_chunk=2, backward scan finds \n at pos 2? 
         * No: newline_pos = 0+2=2, scan back: text[1]='b', text[0]='a'. No \n. Hard cut at 2. "ab".
         * pos=2, skip \n at pos=2 → pos=3. Second: remaining=1, chunk="c". */
        char **r = textwrap_chunk("ab\nc", 2, &count);
        TEST("short mixed returns 2 chunks", r && count == 2);
        if (r && count >= 2) {
            TEST("chunk 0 = ab", strcmp(r[0], "ab") == 0);
            TEST("chunk 1 = c", strcmp(r[1], "c") == 0);
        }
        free_chunks(r, count);
    }

    /* Very long text with no newlines — hard cut many times */
    {
        char long_text[2048];
        memset(long_text, 'x', 2000);
        long_text[2000] = '\0';
        size_t count = 0;
        char **r = textwrap_chunk(long_text, 50, &count);
        TEST("long no-newline returns 40 chunks", r && count == 40);
        if (r && count > 0) {
            TEST("first chunk = 50 x's", r[0] && strlen(r[0]) == 50);
            TEST("last chunk = 50 x's", r[count-1] && strlen(r[count-1]) == 50);
        }
        free_chunks(r, count);
    }

    /* Mixed: more than one chunk with newline boundary */
    {
        size_t count = 0;
        char **r = textwrap_chunk("AAAAA\nBBBBB\nCCCCC", 10, &count);
        TEST("mixed split returns 3 chunks", r && count == 3);
        if (r && count >= 3) {
            TEST("chunk 0 = AAAAA", strcmp(r[0], "AAAAA") == 0);
            TEST("chunk 1 = BBBBB", strcmp(r[1], "BBBBB") == 0);
            TEST("chunk 2 = CCCCC", strcmp(r[2], "CCCCC") == 0);
        }
        free_chunks(r, count);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All textwrap chunk tests PASSED");
    return failures ? 1 : 0;
}
