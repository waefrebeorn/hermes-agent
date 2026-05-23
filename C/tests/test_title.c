/*
 * test_title.c -- Tests for session title generation.
 *
 * Tests: normal message → first 6 words, short message, NULL/empty,
 * message with newlines, unprintable chars, single word, exactly 6 words.
 */

#include "hermes_agent.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(name) do { printf("  %s: ", name); } while(0)
#define PASS do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

int main(void) {
    printf("=== Session Title Generation Tests ===\n");

    printf("\n--- Basic Title ---\n");
    {
        TEST("NULL input returns 'New Session'");
        char *t = agent_generate_title(NULL, NULL);
        if (t && strcmp(t, "New Session") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("empty input returns 'New Session'");
        char *t = agent_generate_title(NULL, "");
        if (t && strcmp(t, "New Session") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("first 6 words extracted as title");
        char *t = agent_generate_title(NULL, "This is a test message for the session title");
        if (t && strcmp(t, "This is a test message for") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("short message (under 6 words) uses all words");
        char *t = agent_generate_title(NULL, "Hello world");
        if (t && strcmp(t, "Hello world") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("single word");
        char *t = agent_generate_title(NULL, "Help");
        if (t && strcmp(t, "Help") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("exactly 6 words");
        char *t = agent_generate_title(NULL, "one two three four five six");
        if (t && strcmp(t, "one two three four five six") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("message with newlines");
        char *t = agent_generate_title(NULL, "First line\nSecond line\nThird line");
        if (t && strcmp(t, "First line Second line Third line") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("message with trailing spaces");
        char *t = agent_generate_title(NULL, "  Trimmed   title   ");
        if (t && strcmp(t, "Trimmed title") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("message with non-printable chars");
        char *t = agent_generate_title(NULL, "Hello\x01\x02World");
        if (t && strcmp(t, "HelloWorld") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }

    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
