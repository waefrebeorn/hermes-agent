/*
 * main.c — WuBu Hermes C entry point.
 *
 * Phase 5 target: full hermes binary equivalent.
 * Current: dispatches to CLI or gateway based on argv.
 */

#include "hermes.h"
#include "plugin.h"
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

        /* Discover and load plugins */
        const char *hermes_home = getenv("SLERMES_HOME");
        if (!hermes_home) hermes_home = getenv("HOME");
        if (hermes_home) {
            char pdir[4096];
            snprintf(pdir, sizeof(pdir), "%s/.slermes/plugins", hermes_home);
            plugin_registry_t *plugs = plugin_registry_new();
            if (plugs) {
                int n = plugin_registry_discover(plugs, pdir);
                if (n > 0) {
                    plugin_registry_init_all(plugs);
                    agent_state.plugin_reg = plugs;
                } else {
                    plugin_registry_free(plugs);
                }
            }
        }

        memcpy(agent_state.llm.base_url, cfg.base_url, sizeof(agent_state.llm.base_url));
        memcpy(agent_state.llm.api_key, cfg.api_key, sizeof(agent_state.llm.api_key));
        memcpy(agent_state.llm.model, cfg.model, sizeof(agent_state.llm.model));
        memcpy(agent_state.llm.provider, cfg.provider, sizeof(agent_state.llm.provider));
        agent_state.llm.max_tokens = cfg.provider_cfg.max_tokens;
        agent_state.llm.temperature = cfg.provider_cfg.temperature;
        agent_state.llm.top_p = cfg.provider_cfg.top_p;
        agent_state.llm.stop_count = cfg.provider_cfg.stop_count;
        memcpy(agent_state.llm.stop_sequences, cfg.provider_cfg.stop_sequences,
               sizeof(agent_state.llm.stop_sequences));
        agent_state.llm.presence_penalty = cfg.provider_cfg.presence_penalty;
        agent_state.llm.frequency_penalty = cfg.provider_cfg.frequency_penalty;
        agent_state.llm.seed = cfg.provider_cfg.seed;
        agent_state.llm.logprobs = cfg.provider_cfg.logprobs;
        agent_state.llm.top_logprobs = cfg.provider_cfg.top_logprobs;
        memcpy(agent_state.llm.user, cfg.provider_cfg.user, sizeof(agent_state.llm.user));
        memcpy(agent_state.llm.service_tier, cfg.provider_cfg.service_tier,
               sizeof(agent_state.llm.service_tier));
        memcpy(agent_state.llm.reasoning_effort, cfg.provider_cfg.reasoning_effort,
               sizeof(agent_state.llm.reasoning_effort));
        memcpy(agent_state.llm.response_format, cfg.provider_cfg.response_format,
               sizeof(agent_state.llm.response_format));
        memcpy(agent_state.llm.metadata, cfg.provider_cfg.metadata,
               sizeof(agent_state.llm.metadata));
        memcpy(agent_state.llm.tool_choice, cfg.provider_cfg.tool_choice,
               sizeof(agent_state.llm.tool_choice));
        agent_state.llm.parallel_tool_calls = cfg.provider_cfg.parallel_tool_calls;
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
