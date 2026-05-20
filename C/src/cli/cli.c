/*
 * cli.c — CLI orchestrator for Hermes C.
 * Manages interactive CLI session: prompt, input, agent loop.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    agent_state_t agent;
    hermes_config_t config;
    bool interactive;
    bool running;
    char session_arg[64];  /* --session id */
} cli_state_t;

static cli_state_t g_cli;

/* ================================================================
 *  Display helpers
 * ================================================================ */

static void print_banner(void) {
    if (!g_cli.interactive) return;
    display_printf(DISPLAY_CYAN, DISPLAY_BOLD,
                   "WuBu Hermes v%s — C Translation\n", HERMES_VERSION);
    display_printf(DISPLAY_WHITE, DISPLAY_DIM,
                   "  Model: %s  Provider: %s\n",
                   g_cli.config.model[0] ? g_cli.config.model : "(default)",
                   g_cli.config.provider[0] ? g_cli.config.provider : "(default)");
    display_printf(DISPLAY_WHITE, DISPLAY_DIM,
                   "  Type /help for commands, /exit to quit\n\n");
}

/* ================================================================
 *  CLI main
 * ================================================================ */

int hermes_cli_main(int argc, char **argv) {
    display_init();
    memset(&g_cli, 0, sizeof(g_cli));
    g_cli.interactive = isatty(STDIN_FILENO);
    g_cli.running = true;

    /* Load config */
    hermes_config_load(&g_cli.config, NULL);
    hermes_config_load_env(&g_cli.config);

    /* Initialize agent */
    agent_init(&g_cli.agent);
    /* Initialize tools */
    tools_init_all();
    /* Copy tools registry to agent */
    g_cli.agent.tools = *registry_get();
    /* Copy config fields to agent */
    memcpy(g_cli.agent.llm.base_url, g_cli.config.base_url, sizeof(g_cli.agent.llm.base_url));
    memcpy(g_cli.agent.llm.api_key, g_cli.config.api_key, sizeof(g_cli.agent.llm.api_key));
    memcpy(g_cli.agent.llm.model, g_cli.config.model, sizeof(g_cli.agent.llm.model));
    memcpy(g_cli.agent.llm.provider, g_cli.config.provider, sizeof(g_cli.agent.llm.provider));
    g_cli.agent.max_iterations = g_cli.config.max_turns;
    memcpy(g_cli.agent.hermes_home, g_cli.config.config_path,
           sizeof(g_cli.agent.hermes_home));
    /* Trim /config.yaml suffix */
    char *slash = strrchr(g_cli.agent.hermes_home, '/');
    if (slash) *slash = '\0';

    /* Check for one-shot mode */
    if (argc > 1) {
        /* Check for --session flag */
        const char *session_id = NULL;
        int arg_start = 1;
        if (argc > 2 && strcmp(argv[1], "--session") == 0) {
            session_id = argv[2];
            snprintf(g_cli.session_arg, sizeof(g_cli.session_arg), "%s", session_id);
            arg_start = 3;
        }

        if (arg_start < argc) {
            /* Open DB and load session if specified */
            if (g_cli.agent.hermes_home[0]) {
                agent_open_db(&g_cli.agent);
                if (session_id) agent_load_session(&g_cli.agent, session_id);
            }

            /* Concatenate args as message */
            size_t total = 0;
            for (int i = arg_start; i < argc; i++)
                total += strlen(argv[i]) + 1;
            char *msg = (char *)malloc(total + 1);
            msg[0] = '\0';
            for (int i = arg_start; i < argc; i++) {
                if (i > arg_start) strcat(msg, " ");
                strcat(msg, argv[i]);
            }

            char *resp = agent_chat(&g_cli.agent, msg);
            printf("%s\n", resp ? resp : "(no response)");
            free(resp);
            free(msg);
        }

        agent_close_db(&g_cli.agent);
        agent_free(&g_cli.agent);
        return 0;
    }

    /* Interactive mode */
    print_banner();

    /* Open DB and optionally load session */
    if (g_cli.agent.hermes_home[0]) {
        agent_open_db(&g_cli.agent);
        if (g_cli.session_arg[0])
            agent_load_session(&g_cli.agent, g_cli.session_arg);
    }

    char input[65536];
    while (g_cli.running) {
        if (g_cli.interactive)
            display_printf(DISPLAY_GREEN, DISPLAY_BOLD, "\nhermes> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        /* Strip trailing newline */
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r'))
            input[--len] = '\0';

        if (len == 0) continue;

        /* Check for slash commands */
        if (input[0] == '/') {
            if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0) {
                g_cli.running = false;
                break;
            }
            if (strcmp(input, "/help") == 0) {
                printf("Commands:\n");
                printf("  /exit, /quit  — Exit\n");
                printf("  /help         — This help\n");
                printf("  /clear        — Clear context\n");
                printf("  /model        — Show current model\n");
                continue;
            }
            if (strcmp(input, "/clear") == 0) {
                context_clear(&g_cli.agent);
                display_printf(DISPLAY_YELLOW, DISPLAY_NORMAL,
                               "Context cleared.\n");
                continue;
            }
            if (strcmp(input, "/model") == 0) {
                printf("Model: %s\n", g_cli.config.model);
                printf("Provider: %s\n", g_cli.config.provider);
                printf("Base URL: %s\n", g_cli.config.base_url);
                continue;
            }
        }

        /* Run agent */
        if (g_cli.interactive)
            display_printf(DISPLAY_WHITE, DISPLAY_DIM, "\n");
        char *resp = agent_chat(&g_cli.agent, input);
        if (resp) {
            printf("%s\n", resp);
            free(resp);
        } else {
            printf("Error: No response\n");
        }
    }

    agent_close_db(&g_cli.agent);
    agent_free(&g_cli.agent);
    return 0;
}
