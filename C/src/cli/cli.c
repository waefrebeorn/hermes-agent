/*
 * cli.c — CLI orchestrator for Hermes C.
 * Manages interactive CLI session: prompt, input, agent loop.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_display.h"
#include "hermes_skin.h"
#include "hermes_json.h"
#include "hermes_xai_retirement.h"
#include "ansi.h"
#include "plugin.h"
#include "line_edit.h"
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
    bool json_output;        /* H14: --json flag for JSON output mode */
    char session_arg[64];  /* --session id */
} cli_state_t;

/* Line editor instance (NULL in non-interactive mode) */
static line_edit_t *g_le = NULL;

/* Tab completion callback for line editor */
static char **cli_complete(const char *partial, void *user_data) {
    (void)user_data;
    int count = 0;
    int cap = 64;
    char **matches = malloc(sizeof(char*) * cap);
    if (!matches) return NULL;
    const command_def_t *cmd = commands_get_all();
    size_t plen = strlen(partial);
    for (int i = 0; cmd && cmd[i].name; i++) {
        const char *name = cmd[i].name;
        if (*name == '/') name++;
        if (strncmp(name, partial, plen) == 0) {
            if (count + 2 >= cap) {
                cap *= 2;
                char **tmp = realloc(matches, sizeof(char*) * cap);
                if (!tmp) { for (int j = 0; j < count; j++) free(matches[j]); free(matches); return NULL; }
                matches = tmp;
            }
            matches[count++] = strdup(cmd[i].name);
        }
        if (cmd[i].alias && strncmp(cmd[i].alias + 1, partial, plen) == 0) {
            if (count + 2 >= cap) {
                cap *= 2;
                char **tmp = realloc(matches, sizeof(char*) * cap);
                if (!tmp) { for (int j = 0; j < count; j++) free(matches[j]); free(matches); return NULL; }
                matches = tmp;
            }
            matches[count++] = strdup(cmd[i].alias);
        }
    }
    matches[count] = NULL;
    return matches;
}

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
 *  Tool event callback — wired to agent_loop for visual feedback
 * ================================================================ */

/* Tool event callback attached to agent_state_t.
 * Called by agent_loop for tool.started, tool.completed, tool.failed.
 * Returns non-zero to continue event propagation. */
static int cli_tool_event_cb(const char *event_type, const char *tool_name,
                              const char *args_or_result, void *userdata) {
    (void)userdata;
    if (!tool_name) return 1;

    if (strcmp(event_type, "tool.started") == 0) {
        /* Build preview from JSON args */
        char *preview = display_tool_preview(tool_name, args_or_result);
        display_tool_activity(tool_name, preview, DISPLAY_CYAN);
        free(preview);
    } else if (strcmp(event_type, "tool.completed") == 0) {
        /* Quick result summary */
        display_printf(DISPLAY_GREEN, DISPLAY_DIM, "  ┊ ✓ %s", tool_name);
        /* Check if result has a compact success indicator */
        if (args_or_result) {
            json_t *res = json_parse(args_or_result, NULL);
            if (res && res->type == JSON_OBJECT) {
                const char *summary = json_get_str(res, "success", NULL);
                if (!summary) summary = json_get_str(res, "output", NULL);
                if (summary) {
                    char truncated[80];
                    snprintf(truncated, sizeof(truncated), "%.77s", summary);
                    if (strlen(summary) > 77) strcat(truncated, "...");
                    printf(" %s", truncated);
                }
            }
            json_free(res);
        }
        printf("\n");
        fflush(stdout);
    } else if (strcmp(event_type, "tool.failed") == 0) {
        display_printf(DISPLAY_RED, DISPLAY_DIM, "  ┊ ✗ %s", tool_name);
        if (args_or_result) {
            json_t *res = json_parse(args_or_result, NULL);
            if (res && res->type == JSON_OBJECT) {
                const char *err = json_get_str(res, "error", NULL);
                if (err) {
                    char truncated[80];
                    snprintf(truncated, sizeof(truncated), " %.77s", err);
                    printf("%s", truncated);
                }
            }
            json_free(res);
        }
        printf("\n");
        fflush(stdout);
    }

    return 1; /* continue propagation */
}

/* ================================================================
 *  Display helpers
 * ================================================================ */

static void print_banner(void) {
    if (!g_cli.interactive) return;

    /* ASCII art logo: OSSIFRAG in block letters */
    const char *logo_lines[] = {
        " █████╗ ██████╗ ███████╗███████╗       █████╗  ██████╗ ███████╗███╗   ██╗████████╗",
        "██╔══██╗██╔══██╗██╔════╝██╔════╝      ██╔══██╗██╔════╝ ██╔════╝████╗  ██║╚══██╔══╝",
        "███████║██████╔╝█████╗  ███████╗█████╗███████║██║  ███╗█████╗  ██╔██╗ ██║   ██║",
        "██╔══██║██╔══██╗██╔══╝  ╚════██║╚════╝██╔══██║██║   ██║██╔══╝  ██║╚██╗██║   ██║",
        "██║  ██║██║  ██║███████╗███████║      ██║  ██║╚██████╔╝███████╗██║ ╚████║   ██║",
        "╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝      ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝  ╚═══╝   ╚═╝",
    };
    /* Skin-driven gradient: use banner_accent as base if available,
     * otherwise fall back to classic Hermes gold gradient */
    const char *accent_str = NULL;
    if (g_skin) accent_str = skin_get(g_skin, "colors.banner_accent", NULL);
    int start_r = 163, start_g = 38, start_b = 31;   /* #A3261F */
    int end_r = 235, end_g = 108, end_b = 50;         /* #EB6C32 */
    if (accent_str && accent_str[0] == '#') {
        /* Try to parse the accent color and derive a gradient */
        int ar, ag, ab;
        if (ansi_parse_hex(accent_str, &ar, &ag, &ab)) {
            /* Dim the accent for the start, brighten for the end */
            start_r = ar * 2 / 3; start_g = ag * 2 / 3; start_b = ab * 2 / 3;
            end_r = ar; end_g = ag; end_b = ab;
        }
    }
    for (int i = 0; i < 6; i++) {
        float t = (float)i / 5.0f;
        int r = (int)(start_r + (end_r - start_r) * t);
        int g = (int)(start_g + (end_g - start_g) * t);
        int b = (int)(start_b + (end_b - start_b) * t);
        display_set_fg_rgb(r, g, b);
        display_set_style(DISPLAY_BOLD);
        printf("%s", logo_lines[i]);
        display_reset();
        printf("\n");
    }

    /* Build banner content lines */
    char banner_lines[4096];
    int pos = 0;
    const char *title_color = skin_get(g_skin, "colors.banner_title", "#FFD700");
    const char *text_color = skin_get(g_skin, "colors.banner_text", "#FFF8DC");
    const char *dim_color = skin_get(g_skin, "colors.banner_dim", "#B8860B");

    /* Parse colors for ANSI embedding */
    int tr, tg, tb, dr, dg, db;
    if (!ansi_parse_hex(title_color, &tr, &tg, &tb)) { tr=255; tg=215; tb=0; }
    if (!ansi_parse_hex(dim_color, &dr, &dg, &db)) { dr=184; dg=134; db=11; }

    /* Title line */
    pos += snprintf(banner_lines + pos, sizeof(banner_lines) - (size_t)pos,
        "\x1B[1;38;2;%d;%d;%dm  WuBu Slermes v%s — C Translation\x1B[0m\n",
        tr, tg, tb, HERMES_VERSION);

    /* Model/Provider line */
    pos += snprintf(banner_lines + pos, sizeof(banner_lines) - (size_t)pos,
        "\x1B[2;38;2;%d;%d;%dm  Model: %s  Provider: %s\x1B[0m\n",
        dr, dg, db,
        g_cli.config.model[0] ? g_cli.config.model : "(default)",
        g_cli.config.provider[0] ? g_cli.config.provider : "(default)");

    /* Stats summary line */
    pos += snprintf(banner_lines + pos, sizeof(banner_lines) - (size_t)pos,
        "\x1B[2;38;2;%d;%d;%dm  Tools: 85  Gateways: 19  Providers: 10  Suite: 226/0/23\x1B[0m",
        dr, dg, db);

    /* Display as panel with skin border color */
    const char *border_color = skin_get(g_skin, "colors.banner_border", "#CD7F32");
    display_panel_hex(" Slermes C ", banner_lines, border_color);

    /* Hint line */
    display_printf_hex(text_color, DISPLAY_DIM,
        "  Type /help for commands, /exit to quit\n");

    /* Auto-load goal from mind-palace */
    if (g_cli.interactive) {
        char gp[4096];
        const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                           getenv("HOME") ? getenv("HOME") : ".";
        snprintf(gp, sizeof(gp), "%s/mind-palace/goal-mantra.md", home);
        FILE *gf = fopen(gp, "r");
        if (gf) {
            char line[256], first[256] = "";
            while (fgets(line, sizeof(line), gf)) {
                if (line[0] == '#' || line[0] == '\n') continue;
                strncpy(first, line, sizeof(first) - 1);
                break;
            }
            fclose(gf);
            if (first[0]) {
                char *nl = strchr(first, '\n');
                if (nl) *nl = '\0';
                display_printf(cli_skin_color("colors.dim", DISPLAY_WHITE), DISPLAY_DIM,
                               "  Goal: %s\n", first);
            }
        }
    }
    printf("\n");
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

/* ──────────────────────────────────────────────
 *  H14: JSON output helpers for --json mode
 * ────────────────────────────────────────────── */

/* Escape a string for JSON output (backslash, quote, newline, tab).
 * Caller must free() the result. Returns NULL on allocation failure. */
static char *cli_json_escape(const char *raw) {
    if (!raw) return strdup("null");
    size_t rlen = strlen(raw);
    size_t cap = rlen * 2 + 3;
    char *esc = (char *)malloc(cap);
    if (!esc) return strdup("null");
    size_t j = 0;
    esc[j++] = '"';
    for (size_t i = 0; i < rlen && j < cap - 4; i++) {
        if (raw[i] == '\\') { esc[j++] = '\\'; esc[j++] = '\\'; }
        else if (raw[i] == '"')  { esc[j++] = '\\'; esc[j++] = '"'; }
        else if (raw[i] == '\n') { esc[j++] = '\\'; esc[j++] = 'n'; }
        else if (raw[i] == '\t') { esc[j++] = '\\'; esc[j++] = 't'; }
        else if ((unsigned char)raw[i] < 0x20) { esc[j++] = '\\'; esc[j++] = 'u';
            snprintf(esc + j, cap - j, "%04x", (unsigned char)raw[i]); j += 4; }
        else esc[j++] = raw[i];
    }
    esc[j++] = '"';
    esc[j] = '\0';
    return esc;
}

/* Print a JSON-formatted response to stdout.
 * Fields: response, session_id, status (ok/error) */
static void cli_json_respond(const char *response, const char *session_id, const char *status) {
    char *esc_resp = cli_json_escape(response);
    printf("{\"response\":%s,\"session_id\":\"%s\",\"status\":\"%s\"}\n",
           esc_resp ? esc_resp : "null",
           session_id && session_id[0] ? session_id : "",
           status ? status : "ok");
    free(esc_resp);
}

int hermes_cli_main(int argc, char **argv) {
    display_init();
    memset(&g_cli, 0, sizeof(g_cli));
    g_cli.interactive = isatty(STDIN_FILENO);
    g_cli.running = true;

    /* Load config */
    hermes_config_load(&g_cli.config, NULL);
    hermes_config_load_env(&g_cli.config);
    /* P19: Enable SIGHUP-based config reload */
    hermes_config_setup_reload();

    /* L04: Check for retired xAI models and warn */
    {
        xai_retirement_result_t ret = hermes_xai_find_retired(&g_cli.config);
        for (int i = 0; i < ret.count; i++) {
            char *msg = hermes_xai_format_issue(&ret.issues[i]);
            if (msg) {
                fprintf(stderr, "\033[33mWarning:\033[0m %s\n", msg);
                free(msg);
            }
        }
    }

    /* O13: Initialize TIRITH policy engine with defaults + custom config */
    tirith_policy_global_init(&g_cli.config.security);

    /* Wire TIRITH scan settings from config */
    if (g_cli.config.security.tirith_path[0])
        tirith_set_path(g_cli.config.security.tirith_path);
    tirith_set_enabled(g_cli.config.security.tirith_enabled);

    /* Init audit logging */
    char log_dir[512];
    hermes_log_dir(log_dir, sizeof(log_dir));
    audit_init(log_dir);

    /* Initialize skin (loads from config.skin_path, falls back to default) */
    cli_skin_init();

    /* Initialize agent */
    agent_init(&g_cli.agent);
    /* Wire tool event callback for activity feed — after agent_init (memset) */
    g_cli.agent.tool_event_cb = cli_tool_event_cb;
    g_cli.agent.tool_event_data = &g_cli;
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

    /* Apply compression config */
    g_cli.agent.compress_enabled = g_cli.config.compress_enabled;

    /* Apply CDP URL for browser CDP tools */
    if (g_cli.config.cdp_url[0])
        cdp_set_url(g_cli.config.cdp_url);

    /* Trim /config.yaml suffix */
    char *slash = strrchr(g_cli.agent.hermes_home, '/');
    if (slash) *slash = '\0';

    /* O15: File permission hardening — secure sensitive files */
    {
        const char *home = g_cli.agent.hermes_home[0] ? g_cli.agent.hermes_home : NULL;
        if (!home) home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home)
            hermes_file_permissions_harden(home, NULL, NULL, geteuid());
    }

    /* O12: Audit log rotation — 10MB max, 5 rotated files, 30-day expiry */
    audit_set_rotation(10240, 5, 30);

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

    /* Check for -h/--help early for fast exit */
    bool arg_has_help = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            arg_has_help = true;
            break;
        }
    }

    /* Parse --json flag (order-independent) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            g_cli.json_output = true;
            for (int j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];
            argc--;
            i--;
        }
    }

    /* Check for help flag */
    if (arg_has_help || (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))) {
        printf("Usage: slermes [options] [message...]\n");
        printf("Options:\n");
        printf("  --help, -h         Show this help message\n");
        printf("  --version, -v      Show version information\n");
        printf("  --session <id>     Attach to a specific session\n");
        printf("  --json             JSON output mode (for scripting)\n");
        printf("  gateway            Start the multi-platform gateway\n");
        printf("  cron               Run the cron scheduler\n");
        printf("  status             Show session status\n");
        printf("  dump               Dump system debug info\n");
        printf("  logs               View agent log files\n");
        printf("  tools              List registered tools\n");
        printf("  plugins            List installed plugins\n");
        printf("  secrets            Manage secrets\n");
        printf("  skills             Manage installed skills\n");
        printf("  --tui              Start the terminal UI (requires ncurses)\n");
        printf("\nSlash commands (interactive mode): /help for full list\n");
        agent_free(&g_cli.agent);
        return 0;
    }

    /* Check for one-shot mode */
    if (argc > 1) {
        const char *session_id = NULL;
        int arg_start = 1;
        if (argc > 2 && strcmp(argv[1], "--session") == 0) {
            session_id = argv[2];
            snprintf(g_cli.session_arg, sizeof(g_cli.session_arg), "%s", session_id);
            arg_start = 3;
        } else if (argc == 2 && strcmp(argv[1], "--session") == 0) {
            fprintf(stderr, "Error: --session requires a session ID\n");
            fprintf(stderr, "Usage: slermes --session <session_id> [message...]\n");
            agent_free(&g_cli.agent);
            return 1;
        }

        /* Check for -e/--eval batch mode */
        if (arg_start < argc && (strcmp(argv[arg_start], "-e") == 0 || strcmp(argv[arg_start], "--eval") == 0)) {
            arg_start++; /* skip -e flag */
        }

        /* Check for known subcommands (dispatch as slash commands) */
        if (arg_start < argc && argv[arg_start][0] != '-' && argv[arg_start][0] != '/') {
            /* Special case: version prints banner */
            if (strcmp(argv[arg_start], "version") == 0) {
                printf("WuBu Slermes v%s\n", HERMES_VERSION);
                printf("C Translation — Phase 5 target\n");
                printf("Build: %s %s\n", __DATE__, __TIME__);
                agent_free(&g_cli.agent);
                return 0;
            }
            static const char *known_subcmds[] = {
                "status", "dump", "logs", "tools", "plugins", "secrets",
                "cron", "skills", "help", "commands", "model", "config",
                "history", "sessions", "usage", "insights", "copy", NULL
            };
            for (int i = 0; known_subcmds[i]; i++) {
                if (strcmp(argv[arg_start], known_subcmds[i]) == 0) {
                    /* Build full slash command string */
                    char cmd_input[8192];
                    snprintf(cmd_input, sizeof(cmd_input), "/%s", known_subcmds[i]);
                    /* Append remaining args */
                    for (int j = arg_start + 1; j < argc; j++) {
                        size_t clen = strlen(cmd_input);
                        snprintf(cmd_input + clen, sizeof(cmd_input) - clen, " %s", argv[j]);
                    }
                    commands_dispatch(cmd_input, &g_cli.agent);
                    /* Also try skill command if dispatch failed */
                    commands_try_skill(cmd_input, &g_cli.agent);
                    agent_close_db(&g_cli.agent);
                    agent_free(&g_cli.agent);
                    return 0;
                }
            }
        }

        /* I02: Reject unknown flags instead of sending to LLM */
        static const char *known_flags[] = {
            "--help", "-h", "--json", "--session", "--eval", "-e",
            "--version", "-v", NULL
        };
        for (int i = arg_start; i < argc; i++) {
            if (argv[i][0] == '-') {
                bool known = false;
                for (int k = 0; known_flags[k]; k++) {
                    if (strcmp(argv[i], known_flags[k]) == 0) {
                        known = true;
                        break;
                    }
                }
                if (!known) {
                    fprintf(stderr, "Error: unknown flag '%s'\n", argv[i]);
                    fprintf(stderr, "Run 'slermes --help' for usage.\n");
                    agent_free(&g_cli.agent);
                    return 1;
                }
            }
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
            if (g_cli.json_output) {
                cli_json_respond(resp, g_cli.agent.session_id, resp ? "ok" : "error");
            } else {
                display_panel("Response", resp ? resp : "(no response)", DISPLAY_CYAN);
            }
            free(resp);
            free(msg);

            agent_close_db(&g_cli.agent);
            agent_free(&g_cli.agent);
            return 0;
        }

        /* --session without prompt: fall through to interactive with session loaded */
    }

    /* H09: Pipe input mode — non-interactive, no args, read from stdin */
    if (!g_cli.interactive && argc <= 1) {
        size_t cap = 65536;
        size_t pos = 0;
        char *input = malloc(cap);
        if (!input) {
            fprintf(stderr, "Error: out of memory reading stdin\n");
            agent_close_db(&g_cli.agent);
            agent_free(&g_cli.agent);
            return 1;
        }
        int ch;
        while ((ch = fgetc(stdin)) != EOF) {
            if (pos >= cap - 1) {
                cap *= 2;
                char *tmp = realloc(input, cap);
                if (!tmp) {
                    fprintf(stderr, "Error: out of memory reading stdin\n");
                    free(input);
                    agent_close_db(&g_cli.agent);
                    agent_free(&g_cli.agent);
                    return 1;
                }
                input = tmp;
            }
            input[pos++] = (char)ch;
        }
        input[pos] = '\0';

        /* Strip trailing newlines */
        while (pos > 0 && (input[pos-1] == '\n' || input[pos-1] == '\r'))
            input[--pos] = '\0';

        if (pos > 0) {
            /* Check for slash commands */
            if (input[0] == '/') {
                if (!commands_dispatch(input, &g_cli.agent)) {
                    if (!commands_try_skill(input, &g_cli.agent)) {
                        if (g_cli.json_output) {
                            char err[512]; snprintf(err, sizeof(err), "Unknown command: %s", input);
                            cli_json_respond(err, g_cli.agent.session_id, "error");
                        } else {
                            printf("Unknown command: %s\n", input);
                        }
                    }
                }
            } else {
                char *resp = agent_chat(&g_cli.agent, input);
                if (g_cli.json_output) {
                    cli_json_respond(resp, g_cli.agent.session_id, resp ? "ok" : "error");
                } else {
                    display_panel("Response", resp ? resp : "(no response)", DISPLAY_CYAN);
                }
                free(resp);
            }
        }

        agent_close_db(&g_cli.agent);
        agent_free(&g_cli.agent);
        free(input);
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
    /* Initialize line editor for interactive mode */
    if (g_cli.interactive) {
        g_le = line_edit_create(cli_complete, NULL);
        if (g_le) {
            char hist_path[1024];
            snprintf(hist_path, sizeof(hist_path), "%s/.slermes_history",
                     g_cli.agent.hermes_home[0] ? g_cli.agent.hermes_home : getenv("HOME") ?: ".");
            line_edit_load_history(g_le, hist_path);
        }
    }

    while (g_cli.running) {
        /* P19: Check SIGHUP-based config reload before each input */
        hermes_config_check_reload(&g_cli.config, NULL);

        if (g_cli.interactive && g_le) {
            /* Use line editor with tab completion and history */
            char *line = line_edit_read(g_le, "\nhermes> ");
            if (!line) break;  /* EOF/Ctrl-D/Ctrl-C */
            strncpy(input, line, sizeof(input) - 1);
            input[sizeof(input) - 1] = '\0';
            free(line);
        } else {
            if (g_cli.interactive)
                display_printf(cli_skin_color("colors.prompt", DISPLAY_GREEN), DISPLAY_BOLD,
                               "\nhermes> ");
            fflush(stdout);
            if (!fgets(input, sizeof(input), stdin)) break;
        }

        /* Strip trailing newline */
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r'))
            input[--len] = '\0';

        if (len == 0) continue;

        /* Check for slash commands — dispatch via registry */
        if (input[0] == '/') {
            if (commands_dispatch(input, &g_cli.agent))
                continue;
            /* Try skill command before reporting unknown */
            if (commands_try_skill(input, &g_cli.agent))
                continue;
            /* Unknown command */
            if (g_cli.json_output) {
                char err[512]; snprintf(err, sizeof(err), "Unknown command: %s. Try /help", input);
                cli_json_respond(err, g_cli.agent.session_id, "error");
            } else {
                printf("Unknown command: %s. Try /help\n", input);
            }
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
                if (g_cli.json_output) {
                    cli_json_respond(resp, g_cli.agent.session_id,
                                     strncmp(resp, "Error:", 6) == 0 ? "error" : "ok");
                } else if (strncmp(resp, "Error:", 6) == 0) {
                    printf("%s\n", resp);
                }
            } else {
                if (g_cli.json_output) {
                    cli_json_respond(resp, g_cli.agent.session_id, "ok");
                } else {
                    display_panel("Response", resp, DISPLAY_CYAN);
                }
            }
            free(resp);
        } else {
            if (g_cli.json_output) {
                cli_json_respond("No response", g_cli.agent.session_id, "error");
            } else {
                printf("Error: No response\n");
            }
        }
    }

    agent_close_db(&g_cli.agent);
    agent_free(&g_cli.agent);
    return 0;
}
