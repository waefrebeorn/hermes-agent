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
