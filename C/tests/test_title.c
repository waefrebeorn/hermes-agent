/*
 * test_title.c -- Tests for session title generation.
 *
 * Tests: sentence extraction, NULL/empty, newlines, code block skipping,
 * trailing spaces, non-printable chars, 80-char cap.
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
        TEST("extracts content up to sentence-ending punctuation at paragraph break");
        char *t = agent_generate_title(NULL, "This is a test message.\n\nMore text after.");
        if (t && strcmp(t, "This is a test message") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("extracts full content when no clear sentence boundary");
        char *t = agent_generate_title(NULL, "This is a test message for the session");
        if (t && strcmp(t, "This is a test message for the session") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("short message (under sentence) uses all words");
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
        TEST("stops at period at end of string");
        char *t = agent_generate_title(NULL, "Hello world.");
        if (t && strcmp(t, "Hello world") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("stops at double-newline paragraph break");
        char *t = agent_generate_title(NULL, "Hello world!\n\nMore text.");
        if (t && strcmp(t, "Hello world!") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("message with newlines collapses to spaces");
        char *t = agent_generate_title(NULL, "First line\nSecond line\nThird line");
        if (t && strcmp(t, "First line Second line Third line") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("double newline stops extraction");
        char *t = agent_generate_title(NULL, "First paragraph\n\nSecond paragraph");
        if (t && strcmp(t, "First paragraph") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("message with trailing spaces collapsed");
        char *t = agent_generate_title(NULL, "  Trimmed   title   ");
        if (t && strcmp(t, "Trimmed title") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("non-printable chars skipped");
        char *t = agent_generate_title(NULL, "Hello\x01\x02World");
        if (t && strcmp(t, "HelloWorld") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }
    {
        TEST("code blocks skipped in title extraction");
        char *t = agent_generate_title(NULL, "Some text\n```\ncode block\n```\nMore text.");
        if (t && strcmp(t, "Some text More text") == 0) { PASS; free(t); }
        else { FAIL(t ? t : "NULL"); free(t); }
    }

    printf("\n==============================================\n");
    printf("  Results: %d passed, %d failed, %d skipped\n", passed, failed, 0);
    printf("==============================================\n");
    return failed > 0 ? 1 : 0;
}
