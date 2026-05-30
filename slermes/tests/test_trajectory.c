/*
 * test_trajectory.c — Tests for trajectory saving utilities.
 * Port of Python agent/trajectory.py tests.
 */
#include "hermes.h"
#include "hermes_trajectory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

int main(void) {
    printf("=== Trajectory tests ===\n\n");

    /* ──── hermes_convert_scratchpad_to_think ──── */

    /* NULL input */
    {
        char *r = hermes_convert_scratchpad_to_think(NULL);
        TEST("convert: NULL returns NULL", r == NULL);
        free(r);
    }

    /* No tags — passthrough */
    {
        char *r = hermes_convert_scratchpad_to_think("Hello world");
        TEST("convert: no tags passthrough", r && strcmp(r, "Hello world") == 0);
        free(r);
    }

    /* Empty string */
    {
        char *r = hermes_convert_scratchpad_to_think("");
        TEST("convert: empty string", r && *r == '\0');
        free(r);
    }

    /* REASONING_SCRATCHPAD → think */
    {
        char *r = hermes_convert_scratchpad_to_think("<REASONING_SCRATCHPAD>hidden</REASONING_SCRATCHPAD>");
        TEST("convert: scratchpad to think", r && strcmp(r, "<think>hidden</think>") == 0);
        free(r);
    }

    /* Opening tag only */
    {
        char *r = hermes_convert_scratchpad_to_think("Let me <REASONING_SCRATCHPAD>think...");
        TEST("convert: open tag only", r && strcmp(r, "Let me <think>think...") == 0);
        free(r);
    }

    /* Multiple replacements */
    {
        char *r = hermes_convert_scratchpad_to_think(
            "<REASONING_SCRATCHPAD>Step1</REASONING_SCRATCHPAD> then <REASONING_SCRATCHPAD>Step2</REASONING_SCRATCHPAD>");
        TEST("convert: multiple replacements", r && strcmp(r, "<think>Step1</think> then <think>Step2</think>") == 0);
        free(r);
    }

    /* Mixed content */
    {
        char *r = hermes_convert_scratchpad_to_think("Before <REASONING_SCRATCHPAD>During</REASONING_SCRATCHPAD> After");
        TEST("convert: mixed content", r && strcmp(r, "Before <think>During</think> After") == 0);
        free(r);
    }

    /* ──── hermes_has_incomplete_scratchpad ──── */

    /* NULL */
    {
        TEST("incomplete: NULL returns false", !hermes_has_incomplete_scratchpad(NULL));
    }

    /* Empty */
    {
        TEST("incomplete: empty returns false", !hermes_has_incomplete_scratchpad(""));
    }

    /* No tags */
    {
        TEST("incomplete: no tags returns false", !hermes_has_incomplete_scratchpad("plain text"));
    }

    /* Complete pair */
    {
        TEST("incomplete: complete pair returns false",
             !hermes_has_incomplete_scratchpad("<REASONING_SCRATCHPAD>done</REASONING_SCRATCHPAD>"));
    }

    /* Opening only */
    {
        TEST("incomplete: opening only returns true",
             hermes_has_incomplete_scratchpad("<REASONING_SCRATCHPAD>no closing"));
    }

    /* Closing only */
    {
        TEST("incomplete: closing only returns false (no opening)",
             !hermes_has_incomplete_scratchpad("</REASONING_SCRATCHPAD>"));
    }

    /* Multiple — one incomplete: correct behavior — if any closing tag exists,
     * the function returns false because strstr finds the closing tag. */
    {
        TEST("incomplete: one incomplete with another complete returns false (correct — any closing tag found)",
             !hermes_has_incomplete_scratchpad(
                 "<REASONING_SCRATCHPAD>complete</REASONING_SCRATCHPAD> then <REASONING_SCRATCHPAD>incomplete"));
    }

    /* ──── hermes_save_trajectory ──── */

    /* Write to temp file, read back, verify content, clean up */
    {
        const char *traj = "[{\"role\":\"user\",\"content\":\"hello\"},{\"role\":\"assistant\",\"content\":\"hi\"}]";
        const char *tmp = "/tmp/test_traj_write.jsonl";

        /* Remove any previous file */
        remove(tmp);

        int ret = hermes_save_trajectory(traj, "test-model", true, tmp);
        TEST("save: returns 0 on success", ret == 0);

        /* Read and verify */
        FILE *f = fopen(tmp, "r");
        TEST("save: file exists", f != NULL);
        if (f) {
            char buf[4096] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';
            TEST("save: contains model", strstr(buf, "test-model") != NULL);
            TEST("save: contains completed true", strstr(buf, "\"completed\":true") != NULL);
            TEST("save: contains timestamp", strstr(buf, "timestamp") != NULL);
            TEST("save: contains conversations", strstr(buf, "conversations") != NULL);
        }
        remove(tmp);
    }

    /* Failed trajectory with default filename */
    {
        const char *traj = "[{\"role\":\"user\",\"content\":\"fail\"}]";
        const char *tmp = "/tmp/failed_trajectories.jsonl";

        remove(tmp);
        int ret = hermes_save_trajectory(traj, "fail-model", false, NULL);
        TEST("save: failed trajectory uses default name", ret == 0);

        FILE *f = fopen(tmp, "r");
        if (f) {
            char buf[4096] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';
            TEST("save failed: contains model", strstr(buf, "fail-model") != NULL);
            TEST("save failed: completed false", strstr(buf, "\"completed\":false") != NULL);
        }
        remove(tmp);
    }

    /* Invalid JSON — expect -1 */
    {
        int ret = hermes_save_trajectory("{invalid}", "model", true, "/tmp/test_bad.jsonl");
        TEST("save: invalid JSON returns -1", ret == -1);
    }

    /* Malformed: string instead of array */
    {
        int ret = hermes_save_trajectory("\"not an array\"", "model", true, "/tmp/test_bad2.jsonl");
        TEST("save: non-array returns -1", ret == -1);
    }

    printf("\n=== Results: %s ===\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures;
}
