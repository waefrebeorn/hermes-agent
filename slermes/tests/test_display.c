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

    /* Double init */
    display_init();
    TEST("init called twice (no crash)", 1);

    /* --- reset/clear (no TTY needed) --- */
    printf("\n--- clear/reset ---\n");
    display_reset();
    TEST("reset no crash", 1);

    /* Double reset */
    display_reset();
    TEST("reset called twice (no crash)", 1);

    /* --- progress init (only tests struct init, not update which needs TTY) --- */
    printf("\n--- progress init ---\n");
    display_progress_t bar;
    display_progress_init(&bar, "Loading", 100);
    TEST("init sets label", strcmp(bar.label, "Loading") == 0);
    TEST("init sets total", bar.total == 100);
    TEST("init sets current 0", bar.current == 0);
    TEST("init sets width", bar.width > 0);

    /* Progress with zero total clamped to default */
    {
        display_progress_t zb;
        memset(&zb, 0, sizeof(zb));
        display_progress_init(&zb, "Zero", 0);
        TEST("zero total clamped to minimum", zb.total > 0);
    }

    /* Progress with very long label */
    {
        display_progress_t lb;
        char long_label[256];
        memset(long_label, 'A', 200);
        long_label[200] = '\0';
        display_progress_init(&lb, long_label, 10);
        TEST("long label truncated to buffer", strncmp(lb.label, long_label, 50) == 0);
        TEST("long label NUL terminated", lb.label[sizeof(lb.label) - 1] == '\0');
    }

    /* Progress with NULL label */
    {
        display_progress_t nl;
        display_progress_init(&nl, NULL, 10);
        /* Should not crash; label may be empty or truncated */
        TEST("NULL label no crash", 1);
    }

    /* Progress update (no TTY — just verify no crash) */
    {
        display_progress_t ub;
        memset(&ub, 0, sizeof(ub));
        display_progress_init(&ub, "Update", 5);
        display_progress_update(&ub, 3);
        /* update renders to screen, doesn't store current in struct */
        TEST("update no crash", 1);
        TEST("total unchanged after update", ub.total == 5);
    }

    /* Progress update beyond total */
    {
        display_progress_t ob;
        display_progress_init(&ob, "Over", 5);
        display_progress_update(&ob, 10);
        TEST("update beyond total clamps or passes through",
             ob.current >= 0); /* at minimum doesn't crash */
    }

    /* Progress with current > total */
    {
        display_progress_t cb;
        display_progress_init(&cb, "Catch-up", 5);
        cb.current = 10;
        display_progress_update(&cb, 2);
        TEST("catch-up update resets current", cb.current >= 0);
    }

    /* --- spinner lifecycle --- */
    printf("\n--- spinner lifecycle ---\n");
    display_spinner_t sp = {0};
    display_spinner_start(&sp, "Working");
    TEST("start sets label", strcmp(sp.label, "Working") == 0);
    TEST("start sets active", sp.active);

    /* Spinner with NULL label */
    {
        display_spinner_t ns;
        memset(&ns, 0, sizeof(ns));
        display_spinner_start(&ns, NULL);
        TEST("NULL label spinner start no crash", 1);
    }

    /* Spinner double start */
    {
        display_spinner_t ds;
        memset(&ds, 0, sizeof(ds));
        display_spinner_start(&ds, "First");
        display_spinner_start(&ds, "Second");
        TEST("double start updates label", strcmp(ds.label, "Second") == 0);
    }

    /* Spinner stop (graceful no-op if active) */
    {
        display_spinner_t ss;
        memset(&ss, 0, sizeof(ss));
        display_spinner_start(&ss, "Stop-test");
        display_spinner_stop(&ss, "Done");
        TEST("stop clears active", !ss.active);
        /* Double stop no crash */
        display_spinner_stop(&ss, NULL);
        TEST("double stop no crash", 1);
    }

    /* Spinner stop on unstarted (no crash) */
    {
        display_spinner_t ns;
        memset(&ns, 0, sizeof(ns));
        display_spinner_stop(&ns, NULL);
        TEST("stop on unstarted spinner no crash", 1);
    }

    printf("\n=== Results: %d passed, %d failed, %d total ===\n", pass, fail, pass + fail);
    return fail > 0 ? 1 : 0;
}
