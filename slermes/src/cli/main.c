/*
 * main.c — CLI-specific entry module for Hermes C.
 * CLI configuration and setup before delegating to slermes_cli_main().
 * Separate from src/main.c (the global entry point).
 */

#include "slermes.h"
#include "slermes_display.h"
#include <stdio.h>

/* CLI-specific initialization */
void cli_init(void) {
    display_init();
}

/* CLI-specific cleanup */
void cli_cleanup(void) {
    display_reset();
}

/* Additional CLI entry point called from main.c */
int slermes_cli_entry(int argc, char **argv) {
    cli_init();
    int rc = slermes_cli_main(argc, argv);
    cli_cleanup();
    return rc;
}
