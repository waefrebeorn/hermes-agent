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

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All textwrap chunk tests PASSED");
    return failures ? 1 : 0;
}
