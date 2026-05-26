/*
 * cli_display.c — CLI display helpers for Hermes C.
 * Higher-level display wrappers over deps/cli_display.c.
 */

#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void cli_display_response(const char *role, const char *content,
                           const char *reasoning)
{
    if (!content && !reasoning) return;

    if (reasoning && reasoning[0]) {
        display_printf(DISPLAY_YELLOW, DISPLAY_ITALIC, "%s", reasoning);
        printf("\n\n");
    }

    if (content && content[0]) {
        display_printf(DISPLAY_WHITE, DISPLAY_NORMAL, "%s", content);
        printf("\n");
    }
}

void cli_display_error(const char *msg) {
    display_printf(DISPLAY_RED, DISPLAY_BOLD, "Error: ");
    display_printf(DISPLAY_RED, DISPLAY_NORMAL, "%s\n", msg);
}

void cli_display_status(const char *msg) {
    display_printf(DISPLAY_BLUE, DISPLAY_DIM, "  %s\n", msg);
}

void cli_display_thinking(void) {
    display_printf(DISPLAY_YELLOW, DISPLAY_DIM, "  thinking...\n");
}

/* ================================================================
 *  Output Helpers — colored print wrappers (Python cli_output.py parity)
 * ================================================================ */

void display_print_info(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_DEFAULT, DISPLAY_DIM, "  %s\n", text);
}

void display_print_success(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ %s\n", text);
}

void display_print_warning(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_YELLOW, DISPLAY_NORMAL, "⚠ %s\n", text);
}

void display_print_error(const char *text) {
    if (!text) return;
    display_printf(DISPLAY_RED, DISPLAY_NORMAL, "✗ %s\n", text);
}

void display_print_error_rich(const char *error_msg, const char *suggestion) {
    if (!error_msg) return;
    /* Error header — bold red */
    display_set_fg(DISPLAY_RED);
    display_set_style(DISPLAY_BOLD);
    printf("  ✗ Error: ");
    display_reset();
    /* Error message — red */
    display_set_fg(DISPLAY_RED);
    printf("%s\n", error_msg);
    display_reset();
    /* Suggestion — dim yellow if present */
    if (suggestion && suggestion[0]) {
        display_printf(DISPLAY_YELLOW, DISPLAY_DIM, "    ┊ %s\n", suggestion);
    }
    /* Separator line */
    display_printf(DISPLAY_RED, DISPLAY_DIM,
        "    └────────────────────────────────────\n");
}

void display_print_header(const char *text) {
    if (!text) return;
    printf("\n");
    display_printf(DISPLAY_YELLOW, DISPLAY_BOLD, "  %s\n", text);
}
