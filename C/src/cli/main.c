/*
 * main.c — CLI-specific entry module for Hermes C.
 * CLI configuration and setup before delegating to hermes_cli_main().
 * Separate from src/main.c (the global entry point).
 */

#include "hermes.h"
#include "hermes_display.h"
#include <stdio.h>

/* Gateway stub (Phase 4) */
int hermes_gateway_main(int argc, char **argv) {
    (void)argc; (void)argv;
    fprintf(stderr, "Gateway mode not yet implemented (Phase 4)\n");
    return 1;
}

/* CLI-specific initialization */
void cli_init(void) {
    display_init();
}

/* CLI-specific cleanup */
void cli_cleanup(void) {
    display_reset();
}

/* Additional CLI entry point called from main.c */
int hermes_cli_entry(int argc, char **argv) {
    cli_init();
    int rc = hermes_cli_main(argc, argv);
    cli_cleanup();
    return rc;
}
