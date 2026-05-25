/*
 * main.c — WuBu Hermes C entry point.
 *
 * Phase 5 target: full hermes binary equivalent.
 * Current: dispatches to CLI or gateway based on argv.
 */

#include "hermes.h"
#include "plugin.h"
#include "acp/server.h"
#include "hermes_mcp_serve.h"
#include "hermes_secrets.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* Forward declaration for startup initialization */
extern void mcp_init_all(void);
extern void tools_init_all(void);

/* L03: Crash-resistant stdio — ignore SIGPIPE so broken pipe
 * (OSError in Python terms) doesn't crash the agent. */
static void install_safe_stdio(void) {
    signal(SIGPIPE, SIG_IGN);
}

static void print_banner(void) {
    printf("WuBu Hermes v%s\n", HERMES_VERSION);
    printf("C Translation — 221 gaps remaining\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char **argv) {
    /* L03: Install crash-resistant stdio before any output */
    install_safe_stdio();

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

    if (argc > 1 && strcmp(argv[1], "completions") == 0) {
        /* Print shell completion script */
        if (argc > 2) {
            if (strcmp(argv[2], "bash") == 0) {
                printf("# hermes bash completion — source: . <(hermes completions bash)\n");
                printf("_hermes_completions() {\n");
                printf("    local cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
                printf("    local opts=\"--help -h --version -v --session gateway cron --tui completions status dump logs tools plugins secrets skills model config history sessions usage insights copy\"\n");
                printf("    if [[ $COMP_CWORD -eq 1 ]]; then\n");
                printf("        COMPREPLY=($(compgen -W \"$opts\" -- \"$cur\"))\n");
                printf("    fi\n");
                printf("}\n");
                printf("complete -F _hermes_completions hermes\n");
                return 0;
            }
            if (strcmp(argv[2], "zsh") == 0) {
                printf("#compdef hermes\n");
                printf("_hermes() {\n");
                printf("    local -a opts\n");
                printf("    opts=(\n");
                printf("        '--help[Show help]' '-h[Show help]'\n");
                printf("        '--version[Show version]' '-v[Show version]'\n");
                printf("        '--session[Attach to session]:session:'\n");
                printf("        'gateway:Start gateway:->gateway'\n");
                printf("        'cron:Run scheduler:->cron'\n");
                printf("        'status:Show session status'\n");
                printf("        'dump:Dump system debug info'\n");
                printf("        'logs:View agent log files'\n");
                printf("        'tools:List registered tools'\n");
                printf("        'plugins:List installed plugins'\n");
                printf("        'secrets:Manage secrets'\n");
                printf("        'skills:Manage installed skills'\n");
                printf("        'model:Show current model'\n");
                printf("        'config:Show configuration'\n");
                printf("        'sessions:List sessions'\n");
                printf("        'usage:Show token usage'\n");
                printf("        '--tui[Start TUI]'\n");
                printf("        'completions:Generate completions:->completions'\n");
                printf("    )\n");
                printf("    _arguments $opts\n");
                printf("}\n");
                printf("_hermes \"$@\"\n");
                return 0;
            }
        }
        printf("Usage: hermes completions {bash|zsh}\n");
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "acp") == 0) {
        /* ACP server mode — JSON-RPC over stdio */
        acp_server_t *srv = acp_server_new();
        if (srv) {
            acp_server_run(srv);
            acp_server_free(srv);
        }
        return 0;
    }

    if (argc > 1 && (strcmp(argv[1], "mcp-serve") == 0 || strcmp(argv[1], "mcp_serve") == 0)) {
        /* MCP serve mode — expose Hermes tools as MCP HTTP server */
        int port = 9100;
        if (argc > 2) {
            char *end = NULL;
            long p = strtol(argv[2], &end, 10);
            if (end && *end == '\0' && p > 0 && p < 65536)
                port = (int)p;
        }

        hermes_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        hermes_config_load(&cfg, NULL);
        hermes_config_load_env(&cfg);
        hermes_secrets_init(&cfg);

        tools_init_all();
        mcp_init_all();
        mcp_serve_set_port(port);

        fprintf(stderr, "[mcp-serve] Starting MCP HTTP server on port %d...\n", port);
        if (!mcp_serve_start()) {
            fprintf(stderr, "[mcp-serve] Failed to start server\n");
            return 1;
        }

        fprintf(stderr, "[mcp-serve] Server running. Press Ctrl+C to stop.\n");
        fprintf(stderr, "[mcp-serve] Endpoint: POST http://localhost:%d/mcp\n", port);

        /* Wait until interrupted */
        while (mcp_serve_is_running()) {
            sleep(1);
        }

        mcp_serve_stop();
        fprintf(stderr, "[mcp-serve] Server stopped.\n");
        return 0;
    }

#ifdef HAS_NCURSES_TUI
    if (argc > 1 && (strcmp(argv[1], "--tui") == 0 || strcmp(argv[1], "tui") == 0)) {
        /* Agent state must be initialized before TUI can use it */
        hermes_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        hermes_config_load(&cfg, NULL);
        hermes_config_load_env(&cfg);

        /* L01: Initialize Bitwarden Secrets Manager */
        hermes_secrets_init(&cfg);

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
                /* Also scan source tree plugins directory for dev */
                char dev_pdir[4096];
                snprintf(dev_pdir, sizeof(dev_pdir), "%s/hermes-agent-dev/C/src/plugins", hermes_home);
                n += plugin_registry_discover(plugs, dev_pdir);

                if (n > 0) {
                    fprintf(stderr, "[plugin] loaded %d plugin(s)\n", n);
                    plugin_registry_init_all(plugs);
                }
                agent_state.plugin_reg = plugs;
                /* Pass to memory system for plugin-backed memory */
                extern void memory_set_plugin_registry(void *reg);
                memory_set_plugin_registry(plugs);
            }
        }

        /* Initialize MCP server connections from config */
        mcp_init_all();

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
        agent_state.llm.max_tool_calls = cfg.provider_cfg.max_tool_calls;
        agent_state.llm.n = cfg.provider_cfg.n;
        agent_state.llm.top_k = cfg.provider_cfg.top_k;
        agent_state.llm.candidate_count = cfg.provider_cfg.candidate_count;
        agent_state.llm.json_mode = cfg.provider_cfg.json_mode;
        agent_state.llm.response_format_strict = cfg.provider_cfg.response_format_strict;
        memcpy(agent_state.llm.safety_settings, cfg.provider_cfg.safety_settings,
               sizeof(agent_state.llm.safety_settings));
        memcpy(agent_state.llm.extra_body, cfg.provider_cfg.extra_body,
               sizeof(agent_state.llm.extra_body));
        agent_state.max_iterations = cfg.max_turns;
        if (cfg.config_path[0])
            snprintf(agent_state.hermes_home, sizeof(agent_state.hermes_home), "%s", cfg.config_path);

        extern int tui_fullscreen_run(agent_state_t *);
        int ret = tui_fullscreen_run(&agent_state);
        agent_free(&agent_state);
        return ret;
    }
#endif

    /* Initialize MCP server connections from config before CLI starts */
    mcp_init_all();

    return hermes_cli_main(argc, argv);
}
