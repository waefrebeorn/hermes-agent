/*
 * main.c — WuBu Hermes C entry point.
 *
 * Phase 5 target: full hermes binary equivalent.
 * Currently: stub that prints version and exits.
 */
#include "hermes.h"
#include <stdio.h>

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

    return hermes_cli_main(argc, argv);
}

int hermes_cli_main(int argc, char **argv) {
    print_banner();
    printf("CLI mode — not yet implemented\n");
    printf("Run: %s --version\n", argv[0] ? argv[0] : "hermes");
    return 0;
}

int hermes_gateway_main(int argc, char **argv) {
    print_banner();
    printf("Gateway mode — not yet implemented\n");
    return 0;
}
