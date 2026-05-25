/* test_display.c — Display: color constants, style flags, basic progress/spinner lifecycle. */
#include "hermes_display.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Display Module Tests ===\n\n");

    /* --- Color constants --- */
    printf("--- Color constants ---\n");
    TEST("DISPLAY_BLACK 0", DISPLAY_BLACK == 0);
    TEST("DISPLAY_RED 1", DISPLAY_RED == 1);
    TEST("DISPLAY_GREEN 2", DISPLAY_GREEN == 2);
    TEST("DISPLAY_YELLOW 3", DISPLAY_YELLOW == 3);
    TEST("DISPLAY_BLUE 4", DISPLAY_BLUE == 4);
    TEST("DISPLAY_MAGENTA 5", DISPLAY_MAGENTA == 5);
    TEST("DISPLAY_CYAN 6", DISPLAY_CYAN == 6);
    TEST("DISPLAY_WHITE 7", DISPLAY_WHITE == 7);
    TEST("DISPLAY_DEFAULT 9", DISPLAY_DEFAULT == 9);

    /* --- Style flags --- */
    printf("\n--- Style flags ---\n");
    TEST("DISPLAY_NORMAL 0", DISPLAY_NORMAL == 0);
    TEST("DISPLAY_BOLD 1", DISPLAY_BOLD == 1);
    TEST("DISPLAY_DIM 2", DISPLAY_DIM == 2);
    TEST("DISPLAY_ITALIC 4", DISPLAY_ITALIC == 4);
    TEST("DISPLAY_UNDERLINE 8", DISPLAY_UNDERLINE == 8);

    /* --- Type sizes --- */
    printf("\n--- Type sizes ---\n");
    TEST("progress_t sizeof", sizeof(display_progress_t) > 0);
    TEST("spinner_t sizeof", sizeof(display_spinner_t) > 0);

    /* --- init (doesn't depend on TTY) --- */
    printf("\n--- init ---\n");
    display_init();
    TEST("init called (no crash)", 1);

    /* --- reset/clear (no TTY needed) --- */
    printf("\n--- clear/reset ---\n");
    display_reset();
    TEST("reset no crash", 1);

    /* --- progress init (only tests struct init, not update which needs TTY) --- */
    printf("\n--- progress init ---\n");
    display_progress_t bar;
    display_progress_init(&bar, "Loading", 100);
    TEST("init sets label", strcmp(bar.label, "Loading") == 0);
    TEST("init sets total", bar.total == 100);
    TEST("init sets current 0", bar.current == 0);
    TEST("init sets width", bar.width > 0);

    /* --- spinner lifecycle --- */
    printf("\n--- spinner lifecycle ---\n");
    display_spinner_t sp = {0};
    display_spinner_start(&sp, "Working");
    TEST("start sets label", strcmp(sp.label, "Working") == 0);
    TEST("start sets active", sp.active);

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
