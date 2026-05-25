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
    printf("WuBu Slermes v%s\n", HERMES_VERSION);
    printf("C Translation — Phase 5 target\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        print_banner();
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "completions") == 0) {
        if (argc > 2 && strcmp(argv[2], "install") == 0) {
            const char *shell = getenv("SHELL");
            if (!shell) shell = "/bin/bash";
            if (strstr(shell, "zsh"))
                printf("Add to ~/.zshrc:\n  source <(slermes completions zsh)\n");
            else
                printf("Add to ~/.bashrc:\n  source <(slermes completions bash)\n");
            return 0;
        }
        printf("Usage: slermes completions {bash|zsh|install}\n");
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

    if (argc > 1 && strcmp(argv[1], "init") == 0) {
        extern bool hermes_config_init(const char *);
        hermes_config_init(NULL);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "doctor") == 0) {
        /* Simple diagnostic */
        printf("=== Hermes Doctor ===\n\n");
        printf("-- Binary --\n");
        printf("  Version: %s\n", HERMES_VERSION);

        printf("\n-- Config --\n");
        hermes_config_t cfg;
        if (hermes_config_load(&cfg, NULL)) {
            printf("  Config: OK\n");
            printf("  Model: %s\n", cfg.model[0] ? cfg.model : "(not set)");
            printf("  Provider: %s\n", cfg.provider[0] ? cfg.provider : "(not set)");
            hermes_config_load_env(&cfg);
        } else {
            printf("  Config: NOT FOUND\n");
            printf("  Run: slermes init\n");
        }

        printf("\n-- API Keys --\n");
        const char *keys[] = {"OPENAI_API_KEY", "ANTHROPIC_API_KEY", "GOOGLE_API_KEY", 
                              "HERMES_API_KEY", NULL};
        int found = 0;
        for (int i = 0; keys[i]; i++) {
            const char *v = getenv(keys[i]);
            if (v && v[0]) { printf("  %s: set\n", keys[i]); found++; }
        }
        if (found == 0) printf("  No API keys found (check .env or env vars)\n");
        else printf("  %d key(s) found\n", found);

        printf("\n-- Summary --\n");
        printf("  Config: %s\n", cfg.model[0] ? "ready" : "needs setup");
        printf("  Run: slermes init if missing, then ./slermes\n");
        return 0;
    }

    return hermes_cli_main(argc, argv);
}
