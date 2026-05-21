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
        /* Agent state must be initialized before TUI can use it */
        hermes_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        hermes_config_load(&cfg, NULL);
        hermes_config_load_env(&cfg);

        agent_state_t agent_state;
        agent_init(&agent_state);
        tools_init_all();
        agent_state.tools = *registry_get();
        memcpy(agent_state.llm.base_url, cfg.base_url, sizeof(agent_state.llm.base_url));
        memcpy(agent_state.llm.api_key, cfg.api_key, sizeof(agent_state.llm.api_key));
        memcpy(agent_state.llm.model, cfg.model, sizeof(agent_state.llm.model));
        memcpy(agent_state.llm.provider, cfg.provider, sizeof(agent_state.llm.provider));
        agent_state.max_iterations = cfg.max_turns;
        if (cfg.config_path[0])
            snprintf(agent_state.hermes_home, sizeof(agent_state.hermes_home), "%s", cfg.config_path);

        extern int tui_fullscreen_run(agent_state_t *);
        int ret = tui_fullscreen_run(&agent_state);
        agent_free(&agent_state);
        return ret;
    }
#endif

    return hermes_cli_main(argc, argv);
}
