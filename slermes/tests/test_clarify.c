/*
 * test_clarify.c — Tests for the clarify tool.
 * Tests: basic query, choices, edge cases.
 * Uses pipe to supply stdin input to fgets().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* We declare the handler directly (header not included in src/tools) */
extern char *clarify_handler(const char *args_json, const char *task_id);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

/* Helper: Call clarify_handler with piped stdin input */
static char *clarify_with_input(const char *args_json, const char *input)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    /* Write input to pipe write end */
    write(pipefd[1], input, strlen(input));
    close(pipefd[1]);

    /* Save old stdin, replace with pipe read end */
    int old_stdin = dup(STDIN_FILENO);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    /* Call handler (reads from pipe) */
    char *result = clarify_handler(args_json, "test_task");

    /* Restore stdin */
    dup2(old_stdin, STDIN_FILENO);
    close(old_stdin);

    return result;
}

/* Helper: check if JSON result contains a key with expected value */
static int json_has(const char *json, const char *key, const char *expected_val)
{
    if (!json) return 0;
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    const char *start = strstr(json, search_key);
    if (!start) return 0;
    start += strlen(search_key);
    /* Compare up to the closing quote */
    size_t elen = strlen(expected_val);
    return strncmp(start, expected_val, elen) == 0 && start[elen] == '"';
}

int main(void)
{
    printf("=== Clarify Tool Tests ===\n\n");

    /* ── Basic question ── */
    printf("-- Basic question --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"What is your name?\"}",
            "Alice\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response contains Alice",
                 json_has(result, "response", "Alice"));
            TEST("question echoed in result",
                 json_has(result, "question", "What is your name?"));
            TEST("no choices_offered without choices",
                 strstr(result, "\"choices_offered\"") == NULL);
            free(result);
        }
    }

    /* ── With choices ── */
    printf("\n-- With choices --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick one\",\"choices\":[\"Red\",\"Green\",\"Blue\"]}",
            "2\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response contains '2'", json_has(result, "response", "2"));
            TEST("selected is Green", json_has(result, "selected", "Green"));
            TEST("choices_offered present",
                 strstr(result, "\"choices_offered\"") != NULL);
            free(result);
        }
    }

    /* ── Invalid choice number ── */
    printf("\n-- Invalid choice --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick one\",\"choices\":[\"A\",\"B\"]}",
            "99\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response contains '99'", json_has(result, "response", "99"));
            /* No 'selected' should be present */
            TEST("no selected for invalid choice",
                 !strstr(result, "\"selected\""));
            free(result);
        }
    }

    /* ── Skip/cancel ── */
    printf("\n-- Skip --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Continue?\"}",
            "skip\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response is 'skip'", json_has(result, "response", "skip"));
            free(result);
        }
    }

    /* ── Null args ── */
    printf("\n-- Null args --\n");
    {
        char *result = clarify_handler(NULL, "test");
        TEST("result not null on NULL args", result != NULL);
        if (result) {
            TEST("error on NULL args", json_has(result, "error", "No args"));
            free(result);
        }
    }

    /* ── Missing question ── */
    printf("\n-- Missing question --\n");
    {
        char *result = clarify_handler("{}", "test");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("error for missing question",
                 json_has(result, "error", "Missing question"));
            free(result);
        }
    }

    /* ── Empty input ── */
    printf("\n-- Empty input --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Say something\"}",
            "\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("empty response handled",
                 strstr(result, "\"response\"") != NULL);
            free(result);
        }
    }

    /* ── Empty choices array ── */
    printf("\n-- Empty choices array --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick one\",\"choices\":[]}",
            "My answer\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("choices_offered absent for empty choices",
                 strstr(result, "\"choices_offered\"") == NULL);
            TEST("response present", json_has(result, "response", "My answer"));
            free(result);
        }
    }

    /* ── Single choice ── */
    printf("\n-- Single choice --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Continue?\",\"choices\":[\"Yes\"]}",
            "1\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("selected is Yes", json_has(result, "selected", "Yes"));
            TEST("choices_offered present",
                 strstr(result, "\"choices_offered\"") != NULL);
            free(result);
        }
    }

    /* ── Response '0' with choices (out of range) ── */
    printf("\n-- Response 0 (out of range) --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick one\",\"choices\":[\"A\",\"B\"]}",
            "0\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("no selected for index 0",
                 strstr(result, "\"selected\"") == NULL);
            free(result);
        }
    }

    /* ── Response negative with choices ── */
    printf("\n-- Response negative --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick one\",\"choices\":[\"X\",\"Y\"]}",
            "-1\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("no selected for negative",
                 strstr(result, "\"selected\"") == NULL);
            free(result);
        }
    }

    /* ── Response with JSON special chars ── */
    printf("\n-- Response with quotes and backslash --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"What's your name?\"}",
            "he said \"hello\" and then \\\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response contains quotes",
                 strstr(result, "said") != NULL);
            free(result);
        }
    }

    /* ── Whitespace-only response ── */
    printf("\n-- Whitespace-only response --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Say something\"}",
            "   \n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("whitespace response preserved",
                 strstr(result, "\"response\":\"   \"") != NULL);
            free(result);
        }
    }

    /* ── Skip with choices ── */
    printf("\n-- Skip with choices --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Continue?\",\"choices\":[\"Yes\",\"No\"]}",
            "skip\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response is skip", json_has(result, "response", "skip"));
            TEST("no selected for skip",
                 strstr(result, "\"selected\"") == NULL);
            free(result);
        }
    }

    /* ── Very long question ── */
    printf("\n-- Very long question --\n");
    {
        char long_q[1200];
        memset(long_q, 'x', 1100);
        memcpy(long_q, "{\"question\":\"", 13);
        long_q[13 + 1100 - 1] = '\0';
        strcat(long_q, "\"}");
        char *result = clarify_with_input(long_q, "ok\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("response present", json_has(result, "response", "ok"));
            TEST("question not lost", strstr(result, "\"question\"") != NULL);
            free(result);
        }
    }

    /* ── Choices with non-string items ── */
    printf("\n-- Choices with non-string items --\n");
    {
        char *result = clarify_with_input(
            "{\"question\":\"Pick\",\"choices\":[\"A\",null,\"C\"]}",
            "2\n");
        TEST("result not null", result != NULL);
        if (result) {
            TEST("null choice skipped gracefully — no selected",
                 strstr(result, "\"selected\"") == NULL);
            free(result);
        }
    }

    /* ── Summary ── */
    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
