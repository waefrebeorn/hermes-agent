/*
 * main.c — WuBu Hermes C entry point.
 *
 * Phase 5 target: full hermes binary equivalent.
 * Current: dispatches to CLI or gateway based on argv.
 */

#include "hermes.h"
#include <stdio.h>
#include <string.h>

static void print_banner(void) {
    printf("WuBu Hermes v%s\n", HERMES_VERSION);
    printf("C Translation — Phase 5 target\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        print_banner();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "gateway") == 0) {
        return hermes_gateway_main(argc - 1, argv + 1);
    }

    if (argc > 1 && strcmp(argv[1], "cron") == 0) {
        /* Forward declare — defined in scheduler.c */
        extern int hermes_cron_main(int, char**);
        return hermes_cron_main(argc - 1, argv + 1);
    }

#ifdef HAS_NCURSES_TUI
    if (argc > 1 && (strcmp(argv[1], "--tui") == 0 || strcmp(argv[1], "tui") == 0)) {
        extern int tui_fullscreen_run(void);
        return tui_fullscreen_run();
    }
#endif

    return hermes_cli_main(argc, argv);
}
