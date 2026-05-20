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

/* Streaming output callback — prints tokens directly to stdout */
static int cli_stream_cb(const char *token, void *userdata) {
    (void)userdata;
    printf("%s", token);
    fflush(stdout);
    return 0;
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
    /* Set streaming callback for interactive mode */
    if (g_cli.interactive) {
        g_cli.agent.stream_cb = cli_stream_cb;
        g_cli.agent.stream_data = &g_cli;
    }
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
                printf("  /sessions     — List saved sessions\n");
                printf("  /save         — Save current session\n");
                printf("  /load <id>    — Load a session\n");
                printf("  /stats        — Show session stats\n");
                printf("  /conv         — Show recent conversation\n");
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
            if (strcmp(input, "/sessions") == 0) {
                if (!g_cli.agent.db) {
                    printf("No session database.\n");
                } else {
                    size_t count = 0;
                    char **list = db_list(g_cli.agent.db, &count);
                    if (list && count > 0) {
                        printf("Sessions (%zu):\n", count);
                        for (size_t i = 0; i < count; i++) {
                            printf("  %s\n", list[i]);
                            free(list[i]);
                        }
                        free(list);
                    } else {
                        printf("No saved sessions.\n");
                    }
                }
                continue;
            }
            if (strcmp(input, "/save") == 0) {
                if (agent_save_session(&g_cli.agent))
                    printf("Session saved: %s\n", g_cli.agent.session_id);
                else
                    printf("Failed to save session.\n");
                continue;
            }
            if (strncmp(input, "/load ", 6) == 0) {
                const char *sid = input + 6;
                if (agent_load_session(&g_cli.agent, sid)) {
                    printf("Session loaded: %s (%zu messages)\n", sid, g_cli.agent.message_count);
                } else {
                    printf("Failed to load session: %s\n", sid);
                }
                continue;
            }
            if (strcmp(input, "/stats") == 0) {
                printf("Messages: %zu\n", g_cli.agent.message_count);
                printf("Iterations: %d\n", g_cli.agent.iteration_count);
                printf("Session: %s\n", g_cli.agent.session_id);
                printf("Tools registered: %zu\n", g_cli.agent.tools.count);
                continue;
            }
            if (strcmp(input, "/conv") == 0) {
                size_t n = g_cli.agent.message_count;
                size_t start = (n > 10) ? n - 10 : 0;
                printf("Recent conversation (%zu-%zu of %zu):\n", start + 1, n, n);
                for (size_t i = start; i < n; i++) {
                    const char *role_str;
                    switch (g_cli.agent.messages[i]->role) {
                        case MSG_SYSTEM:    role_str = "sys";  break;
                        case MSG_USER:      role_str = "usr";  break;
                        case MSG_ASSISTANT: role_str = "asm";  break;
                        case MSG_TOOL:      role_str = "tool"; break;
                        default:            role_str = "?";    break;
                    }
                    const char *content = g_cli.agent.messages[i]->content;
                    if (content) {
                        char preview[81];
                        size_t clen = strlen(content);
                        if (clen > 80) {
                            memcpy(preview, content, 77);
                            preview[77] = '.';
                            preview[78] = '.';
                            preview[79] = '.';
                            preview[80] = '\0';
                        } else {
                            memcpy(preview, content, clen + 1);
                        }
                        printf("  [%s] %s\n", role_str, preview);
                    } else {
                        printf("  [%s] (no content)\n", role_str);
                    }
                }
                continue;
            }
        }

        /* Run agent */
        if (g_cli.interactive)
            display_printf(DISPLAY_WHITE, DISPLAY_DIM, "\n");
        char *resp = agent_chat(&g_cli.agent, input);
        if (resp) {
            /* In streaming mode, content was already printed by callback.
             * Only print final newline and handle errors. */
            if (g_cli.interactive && g_cli.agent.stream_cb) {
                printf("\n");
                if (strncmp(resp, "Error:", 6) == 0)
                    printf("%s\n", resp);
            } else {
                printf("%s\n", resp);
            }
            free(resp);
        } else {
            printf("Error: No response\n");
        }
    }

    agent_close_db(&g_cli.agent);
    agent_free(&g_cli.agent);
    return 0;
}
