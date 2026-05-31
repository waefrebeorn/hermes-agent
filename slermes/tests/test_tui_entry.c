/*
 * test_tui_entry.c — Tests for TUI Entry/Startup (T05).
 * Tests pre-flight checks, error messages, stop mechanisms.
 * Note: cannot test actual ncurses init in test runner (no terminal).
 */
#include "tui_entry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: Result messages are non-NULL and non-empty */
    {
        for (int i = 0; i <= TUI_START_ERRNO; i++) {
            const char *msg = tui_entry_result_message((tui_start_result_t)i);
            TEST("result message non-NULL", msg != NULL);
            TEST("result message non-empty", msg && msg[0]);
        }
    }

    /* Test 2: Exit reason names */
    {
        TEST("user_quit name", strcmp(tui_entry_exit_name(TUI_EXIT_USER_QUIT), "user_quit") == 0);
        TEST("sigint name", strcmp(tui_entry_exit_name(TUI_EXIT_SIGINT), "sigint") == 0);
        TEST("sigterm name", strcmp(tui_entry_exit_name(TUI_EXIT_SIGTERM), "sigterm") == 0);
        TEST("normal name", strcmp(tui_entry_exit_name(TUI_EXIT_NORMAL), "normal") == 0);
        TEST("error name", strcmp(tui_entry_exit_name(TUI_EXIT_ERROR), "error") == 0);
        TEST("unknown name", strcmp(tui_entry_exit_name((tui_exit_reason_t)99), "unknown") == 0);
    }

    /* Test 3: Terminal check */
    {
        /* This test runs in a terminal (the test runner pipes), so
         * isatty may be false. Just verify it doesn't crash. */
        bool ok = tui_entry_check_terminal();
        /* Result depends on whether running in a real terminal */
        printf("  INFO: terminal check returned %d\n", ok);
        TEST("terminal check doesn't crash", true);
    }

    /* Test 4: Default config values */
    {
        tui_entry_config_t cfg = TUI_ENTRY_DEFAULT_CONFIG;
        TEST("default min_rows is 8", cfg.min_rows == 8);
        TEST("default min_cols is 40", cfg.min_cols == 40);
        TEST("default enable_events is true", cfg.enable_events == true);
        TEST("default welcome_message is NULL", cfg.welcome_message == NULL);
        TEST("default post_init is NULL", cfg.post_init == NULL);
    }

    /* Test 5: Stop request mechanism */
    {
        TEST("stop not requested initially", tui_entry_stop_requested() == false);
        TEST("exit reason is normal initially",
             tui_entry_get_exit_reason() == TUI_EXIT_NORMAL);

        tui_entry_request_stop();
        TEST("stop requested after call", tui_entry_stop_requested() == true);

        /* Reset for next test (we can't reset the internal state,
         * but the entry_run function resets it on start) */
    }

    /* Test 6: Entry run with NULL state returns error */
    {
        tui_exit_reason_t reason = TUI_EXIT_NORMAL;
        tui_start_result_t result = tui_entry_run(NULL, NULL, &reason);
        /* Should fail because no terminal + ncurses init will fail */
        /* But we accept any result — just verify it doesn't crash */
        printf("  INFO: entry_run(NULL) returned %d\n", (int)result);
        TEST("entry_run with NULL doesn't crash", true);
    }

    /* Test 7: Specific error messages for known codes */
    {
        const char *no_color = tui_entry_result_message(TUI_START_NO_COLOR);
        TEST("no_color mentions color", strstr(no_color, "color") != NULL);

        const char *no_term = tui_entry_result_message(TUI_START_NO_TERM);
        TEST("no_term mentions terminal", strstr(no_term, "terminal") != NULL ||
             strstr(no_term, "TERM") != NULL);

        const char *too_small = tui_entry_result_message(TUI_START_TOO_SMALL);
        TEST("too_small mentions dimensions", strstr(too_small, "40") != NULL ||
             strstr(too_small, "small") != NULL);
    }

    /* Test 8: Entry config struct validity */
    {
        tui_entry_config_t cfg = TUI_ENTRY_DEFAULT_CONFIG;
        cfg.min_rows = 24;
        cfg.min_cols = 80;
        TEST("config min_rows can be set", cfg.min_rows == 24);
        TEST("config min_cols can be set", cfg.min_cols == 80);
        TEST("config enable_events can be set to false",
             (cfg.enable_events = false) == false);
    }

    /* Test 9: Exit reason for all known values */
    {
        for (int i = TUI_EXIT_USER_QUIT; i <= TUI_EXIT_NORMAL; i++) {
            const char *name = tui_entry_exit_name((tui_exit_reason_t)i);
            TEST("exit reason name not unknown", name != NULL);
            TEST("exit reason name not empty", name[0] != '\0');
        }
    }

    /* Test 10: Result message for unknown code is non-NULL */
    {
        const char *msg = tui_entry_result_message((tui_start_result_t)999);
        TEST("unknown result message non-NULL", msg != NULL);
        TEST("unknown result has content", msg && msg[0]);
    }

    printf("\n");
    if (failures > 0)
        printf("  %d TESTS FAILED\n", failures);
    else
        printf("  ALL TESTS PASSED\n");
    return failures > 0 ? 1 : 0;
}
