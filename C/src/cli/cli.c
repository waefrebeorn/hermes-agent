/*
 * cli.c — CLI orchestrator for Hermes C.
 * Manages interactive CLI session: prompt, input, agent loop.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_display.h"
#include "hermes_skin.h"
#include "plugin.h"
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
 *  Skin integration
 * ================================================================ */

static skin_t *g_skin = NULL;

/* Map a skin color name (e.g. "cyan", "red:bold") to display_color_t.
 * Returns fallback on unknown name. */
static display_color_t cli_skin_to_color(const char *name, display_color_t fallback) {
    if (!name) return fallback;
    if (strcmp(name, "black") == 0)   return DISPLAY_BLACK;
    if (strcmp(name, "red") == 0)     return DISPLAY_RED;
    if (strcmp(name, "green") == 0)   return DISPLAY_GREEN;
    if (strcmp(name, "yellow") == 0)  return DISPLAY_YELLOW;
    if (strcmp(name, "blue") == 0)    return DISPLAY_BLUE;
    if (strcmp(name, "magenta") == 0) return DISPLAY_MAGENTA;
    if (strcmp(name, "cyan") == 0)    return DISPLAY_CYAN;
    if (strcmp(name, "white") == 0)   return DISPLAY_WHITE;
    return fallback;
}

/* Get a skin color by dotted key, with hardcoded fallback. */
static display_color_t cli_skin_color(const char *key, display_color_t fallback) {
    if (!g_skin) return fallback;
    const char *name = skin_get(g_skin, key, NULL);
    return cli_skin_to_color(name, fallback);
}

/* Initialize skin from config. Falls back to skin_default() on error. */
static void cli_skin_init(void) {
    const char *path = g_cli.config.skin_path;
    if (path && path[0]) {
        g_skin = skin_load(path);
        if (!g_skin) {
            fprintf(stderr, "Warning: failed to load skin '%s': %s\n",
                    path, skin_error());
        }
    }
    if (!g_skin)
        g_skin = skin_default();
}

/* ================================================================
 *  Display helpers
 * ================================================================ */

static void print_banner(void) {
    if (!g_cli.interactive) return;
    display_printf(cli_skin_color("colors.banner", DISPLAY_CYAN), DISPLAY_BOLD,
                   "WuBu Hermes v%s — C Translation\n", HERMES_VERSION);
    display_printf(cli_skin_color("colors.dim", DISPLAY_WHITE), DISPLAY_DIM,
                   "  Model: %s  Provider: %s\n",
                   g_cli.config.model[0] ? g_cli.config.model : "(default)",
                   g_cli.config.provider[0] ? g_cli.config.provider : "(default)");
    display_printf(cli_skin_color("colors.dim", DISPLAY_WHITE), DISPLAY_DIM,
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

    /* Initialize skin (loads from config.skin_path, falls back to default) */
    cli_skin_init();

    /* Initialize agent */
    agent_init(&g_cli.agent);
    /* Set streaming callback for interactive mode */
    if (g_cli.interactive) {
        g_cli.agent.stream_cb = cli_stream_cb;
        g_cli.agent.stream_data = &g_cli;
    }
    /* Initialize tools */
    tools_init_all();

    /* Discover and load plugins from ~/.slermes/plugins/ */
    char plugin_dir[4096] = "";
    const char *hermes_home = getenv("SLERMES_HOME");
    if (!hermes_home) hermes_home = getenv("HOME");
    if (hermes_home)
        snprintf(plugin_dir, sizeof(plugin_dir), "%s/.slermes/plugins", hermes_home);
    if (plugin_dir[0]) {
        plugin_registry_t *plugins = plugin_registry_new();
        if (plugins) {
            int n = plugin_registry_discover(plugins, plugin_dir);
            if (n > 0) {
                plugin_registry_init_all(plugins);
                /* Store on agent for lifecycle management */
                g_cli.agent.plugin_reg = plugins;
            } else {
                plugin_registry_free(plugins);
            }
        }
    }

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

    /* Apply config values to runtime state */
    if (g_cli.config.personality[0])
        context_set_system(&g_cli.agent, g_cli.config.personality);
    commands_set_verbose(g_cli.config.verbose);
    approval_set_yolo(g_cli.config.yolo_mode);
    commands_set_fast(g_cli.config.fast_mode);

    /* Trim /config.yaml suffix */
    char *slash = strrchr(g_cli.agent.hermes_home, '/');
    if (slash) *slash = '\0';

    /* Set up persistent allowlist path and load saved approvals */
    {
        const char *home = g_cli.agent.hermes_home[0] ? g_cli.agent.hermes_home : NULL;
        if (!home) home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            char allowlist_path[HERMES_PATH_MAX];
            snprintf(allowlist_path, sizeof(allowlist_path), "%s/allowlist.json", home);
            approval_set_allowlist_path(allowlist_path);
            approval_load_allowlist();
        }
    }

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
            display_printf(cli_skin_color("colors.prompt", DISPLAY_GREEN), DISPLAY_BOLD,
                           "\nhermes> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        /* Strip trailing newline */
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r'))
            input[--len] = '\0';

        if (len == 0) continue;

        /* Check for slash commands — dispatch via registry */
        if (input[0] == '/') {
            if (commands_dispatch(input, &g_cli.agent))
                continue;
            /* Unknown command */
            printf("Unknown command: %s. Try /help\n", input);
            continue;
        }

        /* Run agent */
        if (g_cli.interactive)
            display_printf(cli_skin_color("colors.dim", DISPLAY_WHITE), DISPLAY_DIM, "\n");
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
