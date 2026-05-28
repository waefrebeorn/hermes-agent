/* Trajectory tests — verify scratchpad conversion + save_trajectory.
 *
 * Tests:
 * 1. convert_scratchpad_to_think replaces <REASONING_SCRATCHPAD>
 * 2. Replaces close tag
 * 3. No-op when no scratchpad tag
 * 4. NULL input returns NULL
 * 5. has_incomplete_scratchpad true when open without close
 * 6. has_incomplete_scratchpad false when both open and close present
 * 7. has_incomplete_scratchpad false when no tags
 * 8. has_incomplete_scratchpad false on NULL
 * 9. save_trajectory writes valid JSONL
 * 10. save_trajectory with default filename
 */

#include "hermes_trajectory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void)
{
    /* Test 1: Convert open tag */
    {
        char *r = hermes_convert_scratchpad_to_think(
            "Let me <REASONING_SCRATCHPAD>reason</REASONING_SCRATCHPAD> out");
        TEST("scratchpad->think open", r && strcmp(r,
            "Let me <think>reason</think> out") == 0);
        free(r);
    }

    /* Test 2: No tags = no-op */
    {
        char *r = hermes_convert_scratchpad_to_think("plain text");
        TEST("no tags", r && strcmp(r, "plain text") == 0);
        free(r);
    }

    /* Test 3: NULL input */
    {
        char *r = hermes_convert_scratchpad_to_think(NULL);
        TEST("NULL input", r == NULL);
    }

    /* Test 4: Multiple occurrences */
    {
        char *r = hermes_convert_scratchpad_to_think(
            "<REASONING_SCRATCHPAD>A</REASONING_SCRATCHPAD> B <REASONING_SCRATCHPAD>C</REASONING_SCRATCHPAD>");
        TEST("multiple tags",
            r && strcmp(r, "<think>A</think> B <think>C</think>") == 0);
        free(r);
    }

    /* Test 5: has_incomplete_scratchpad — open without close */
    {
        bool r = hermes_has_incomplete_scratchpad(
            "start <REASONING_SCRATCHPAD>no close");
        TEST("incomplete scratchpad", r == true);
    }

    /* Test 6: has_incomplete_scratchpad — both present */
    {
        bool r = hermes_has_incomplete_scratchpad(
            "start <REASONING_SCRATCHPAD>mid</REASONING_SCRATCHPAD> end");
        TEST("complete scratchpad", r == false);
    }

    /* Test 7: No tags */
    {
        bool r = hermes_has_incomplete_scratchpad("plain text");
        TEST("no scratchpad tags", r == false);
    }

    /* Test 8: NULL */
    {
        bool r = hermes_has_incomplete_scratchpad(NULL);
        TEST("NULL scratchpad check", r == false);
    }

    /* Test 9: save_trajectory writes valid JSONL */
    {
        /* Write to a temp file */
        const char *arr = "[{\"role\":\"user\",\"content\":\"hi\"},{\"role\":\"assistant\",\"content\":\"hello\"}]";
        int rc = hermes_save_trajectory(arr, "test-model", true, "/tmp/test_traj.jsonl");
        TEST("save_trajectory success", rc == 0);

        /* Read back and verify */
        FILE *f = fopen("/tmp/test_traj.jsonl", "r");
        TEST("traj file exists", f != NULL);
        if (f) {
            char buf[4096];
            TEST("traj file non-empty", fgets(buf, sizeof(buf), f) != NULL);
            TEST("traj contains conversations", strstr(buf, "\"conversations\"") != NULL);
            TEST("traj contains timestamp", strstr(buf, "\"timestamp\"") != NULL);
            TEST("traj contains model", strstr(buf, "\"test-model\"") != NULL);
            TEST("traj contains completed:true", strstr(buf, "\"completed\":true") != NULL);
            fclose(f);
            remove("/tmp/test_traj.jsonl");
        }
    }

    /* Test 10: save_trajectory with completed=false uses failed filename */
    {
        const char *arr = "[{\"role\":\"user\",\"content\":\"fail\"}]";
        int rc = hermes_save_trajectory(arr, "model-f", false, NULL);
        /* Writes to failed_trajectories.jsonl — just verify no crash */
        TEST("trajectory failed no crash", rc == 0);
        remove("failed_trajectories.jsonl");
    }

    printf("trajectory: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
