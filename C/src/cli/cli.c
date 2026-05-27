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
#include "hermes_logger.h"
#include "hermes_markdown.h"
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
    display_kawaii_t spinner;  /* KawaiiSpinner for LLM wait animation */
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
        /* Check if result has a compact success indicator or diff */
        if (args_or_result) {
            json_t *res = json_parse(args_or_result, NULL);
            if (res && res->type == JSON_OBJECT) {
                /* V11: Show inline diff for patch tool */
                if (strcmp(tool_name, "patch") == 0) {
                    const char *diff_text = json_get_str(res, "diff", NULL);
                    if (diff_text) {
                        printf("\n");
                        char *formatted = display_inline_diff(diff_text);
                        if (formatted) {
                            printf("%s", formatted);
                            free(formatted);
                        }
                    }
                }
                const char *summary = json_get_str(res, "success", NULL);
                if (!summary) summary = json_get_str(res, "output", NULL);
                if (summary && strcmp(tool_name, "patch") != 0) {
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
    size_t tool_count = g_cli.agent.tools.count > 0 ? g_cli.agent.tools.count : registry_get_count();
    pos += snprintf(banner_lines + pos, sizeof(banner_lines) - (size_t)pos,
        "\x1B[2;38;2;%d;%d;%dm  Tools: %zu  Gateways: 19  Providers: 10  Suite: 227/0/23\x1B[0m",
        dr, dg, db, tool_count);

    /* Display as panel with skin border color */
    const char *border_color = skin_get(g_skin, "colors.banner_border", "#CD7F32");
    display_panel_hex(" Slermes C ", banner_lines, border_color);

    /* Hint line + random tip */
    display_printf_hex(text_color, DISPLAY_DIM,
        "  Type /help for commands, /exit to quit\n");
    if (g_cli.interactive) {
        display_show_tip();
    }

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
    /* Stop spinner on first token (if interactive) */
    if (g_cli.interactive && g_cli.spinner.active)
        display_kawaii_stop(&g_cli.spinner, NULL);
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
/* H14: --json flag for JSON output mode */
static bool g_json_cmd_active = false;
static FILE *g_json_cmd_fp = NULL;
static int g_json_cmd_saved_stdout = -1;

/* Start capturing stdout for JSON wrapping */
static void json_capture_start(void) {
    if (!g_json_cmd_active) return;
    fflush(stdout);
    g_json_cmd_saved_stdout = dup(STDOUT_FILENO);
    g_json_cmd_fp = tmpfile();
    if (g_json_cmd_fp)
        dup2(fileno(g_json_cmd_fp), STDOUT_FILENO);
}

/* Stop capture, restore stdout, return captured text (must free) */
static char *json_capture_stop(void) {
    if (!g_json_cmd_active || g_json_cmd_saved_stdout < 0) return NULL;
    fflush(stdout);
    dup2(g_json_cmd_saved_stdout, STDOUT_FILENO);
    close(g_json_cmd_saved_stdout);
    g_json_cmd_saved_stdout = -1;

    if (!g_json_cmd_fp) return NULL;
    rewind(g_json_cmd_fp);
    long sz = 0;
    char *buf = NULL;
    if (fseek(g_json_cmd_fp, 0, SEEK_END) == 0) {
        sz = ftell(g_json_cmd_fp);
        rewind(g_json_cmd_fp);
        if (sz > 0) {
            buf = (char *)calloc(1, sz + 1);
            if (buf) fread(buf, 1, sz, g_json_cmd_fp);
        }
    }
    fclose(g_json_cmd_fp);
    g_json_cmd_fp = NULL;
    return buf;
}

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
    hermes_log_init();
    hermes_log(LOG_INFO, "cli", "CLI initialized");
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
    display_set_skin(g_skin);

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
    /* Copy config fields to agent — agent_configure_from_config sets ALL
     * llm config fields (max_retries, temperature, top_p, fallback, etc.)
     * that were NEVER being wired before. This was the #1 linkage bug. */
    agent_configure_from_config(&g_cli.agent, &g_cli.config);
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

    /* V11: Set spinner type from config */
    g_cli.spinner.type = display_parse_spinner_type(g_cli.config.display.spinner_style);

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

    /* Parse --profile flag (order-independent, consumes value) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                hermes_set_profile(argv[i + 1]);
                /* Remove profile and its value from argv */
                for (int j = i; j < argc - 2; j++)
                    argv[j] = argv[j + 2];
                argc -= 2;
                i--; /* Re-check this position */
            } else {
                fprintf(stderr, "Error: --profile requires a profile name\n");
                fprintf(stderr, "Usage: slermes --profile <name> [message...]\n");
                agent_free(&g_cli.agent);
                return 1;
            }
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
        printf("  --profile <name>   Use named config profile\n");
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
            /* F04: "chat" subcommand enters interactive mode instead of sending "chat" to LLM */
            if (strcmp(argv[arg_start], "chat") == 0) {
                /* Don't send "chat" as a prompt — fall through to interactive mode below.
                 * Skip the one-shot message processing, go directly to interactive loop. */
                goto start_interactive;
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
            "--version", "-v", "--profile", NULL
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

            /* Show spinner during LLM call */
            if (g_cli.interactive)
                display_kawaii_start(&g_cli.spinner, "thinking", true);
            char *resp = agent_chat(&g_cli.agent, msg);
            if (g_cli.interactive)
                display_kawaii_stop(&g_cli.spinner, NULL);
            if (g_cli.json_output) {
                cli_json_respond(resp, g_cli.agent.session_id, resp ? "ok" : "error");
            } else {
                /* V10: Render markdown in responses */
                char *rendered = resp ? hermes_markdown_render(resp, 0) : NULL;
                const char *display_text = rendered ? rendered : (resp ? resp : "(no response)");
                const char *border = g_skin ? skin_get(g_skin, "colors.response_border", NULL) : NULL;
                if (border && border[0] == '#')
                    display_panel_hex("Response", display_text, border);
                else
                    display_panel("Response", display_text, DISPLAY_CYAN);
                free(rendered);
            }
            free(resp);
            free(msg);

            agent_close_db(&g_cli.agent);
            agent_free(&g_cli.agent);
            return 0;
        }

        /* --session without prompt: fall through to interactive with session loaded */
    }

    /* H09: Pipe input mode — non-interactive, no args, read from stdin line by line */
    if (!g_cli.interactive && argc <= 1) {
        char line[65536];
        bool db_open = false;

        while (fgets(line, sizeof(line), stdin)) {
            /* Trim trailing newline */
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len == 0) continue;

            /* Open DB on first input (lazy) */
            if (!db_open) {
                db_open = true;
                if (g_cli.agent.hermes_home[0])
                    agent_open_db(&g_cli.agent);
            }

            if (line[0] == '/') {
                /* Handle /exit and /quit specially */
                if (strcmp(line, "/exit") == 0 || strcmp(line, "/quit") == 0) {
                    if (g_cli.json_output)
                        cli_json_respond("Goodbye!", g_cli.agent.session_id, "ok");
                    break;
                }
                /* H14: In JSON mode, capture command output and wrap as JSON */
                if (g_cli.json_output) {
                    g_json_cmd_active = true;
                    json_capture_start();
                }
                bool handled = commands_dispatch(line, &g_cli.agent);
                if (g_cli.json_output) {
                    char *captured = json_capture_stop();
                    g_json_cmd_active = false;
                    if (handled) {
                        cli_json_respond(captured ? captured : "(ok)",
                                         g_cli.agent.session_id, "ok");
                    } else {
                        /* Try skill dispatch */
                        if (!commands_try_skill(line, &g_cli.agent)) {
                            char err[512];
                            snprintf(err, sizeof(err), "Unknown command: %s", line);
                            cli_json_respond(err, g_cli.agent.session_id, "error");
                        } else {
                            cli_json_respond("(skill invoked)", g_cli.agent.session_id, "ok");
                        }
                    }
                    free(captured);
                } else {
                    if (!handled) {
                        if (!commands_try_skill(line, &g_cli.agent)) {
                            printf("Unknown command: %s\n", line);
                        }
                    }
                }
            } else {
                /* Show spinner during LLM call (pipe mode) */
                if (g_cli.interactive && g_cli.running)
                    display_kawaii_start(&g_cli.spinner, "thinking", true);
                char *resp = agent_chat(&g_cli.agent, line);
                if (g_cli.interactive && g_cli.running)
                    display_kawaii_stop(&g_cli.spinner, NULL);
                if (g_cli.json_output) {
                    cli_json_respond(resp, g_cli.agent.session_id, resp ? "ok" : "error");
                } else {
                    /* V10: Render markdown in responses */
                    char *rendered = resp ? hermes_markdown_render(resp, 0) : NULL;
                    const char *display_text = rendered ? rendered : (resp ? resp : "(no response)");
                    const char *border = g_skin ? skin_get(g_skin, "colors.response_border", NULL) : NULL;
                    if (border && border[0] == '#')
                        display_panel_hex("Response", display_text, border);
                    else
                        display_panel("Response", display_text, DISPLAY_CYAN);
                    free(rendered);
                }
                free(resp);
            }
        }

        /* If json_output and no input was processed, emit a status */
        if (g_cli.json_output && !db_open) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "{\"response\":\"No input provided. Use pipe or args.\","
                     "\"session_id\":\"%s\",\"status\":\"idle\"}",
                     g_cli.agent.session_id ? g_cli.agent.session_id : "");
            printf("%s\n", buf);
        }

        if (db_open) {
            agent_close_db(&g_cli.agent);
        }
        agent_free(&g_cli.agent);
        return 0;
    }

start_interactive:
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

        /* Get prompt symbol from skin branding or fallback */
    const char *prompt_symbol = NULL;
    if (g_skin) prompt_symbol = skin_get_branding(g_skin, "prompt_symbol", NULL);
    if (!prompt_symbol) prompt_symbol = "hermes>";

    if (g_cli.interactive && g_le) {
        /* Use line editor with tab completion and history */
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "\n%s ", prompt_symbol);

        /* Multi-line support: read initial line, then continue if backslash-ending */
        char *line = line_edit_read(g_le, prompt);
        if (!line) break;  /* EOF/Ctrl-D/Ctrl-C */
        strncpy(input, line, sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0';
        free(line);

        /* Backslash-continuation: if line ends with \, strip it and read next line */
        size_t input_len = strlen(input);
        while (input_len > 0 && input[input_len - 1] == '\\') {
            input[input_len - 1] = '\0';  /* Remove trailing backslash */
            input_len--;                   /* Update length */

            /* Read continuation line with continuation prompt */
            char prompt2[64];
            snprintf(prompt2, sizeof(prompt2), "\n%s ", ">");
            char *cont = line_edit_read(g_le, prompt2);
            if (!cont) break;  /* EOF cancels continuation */

            size_t cont_len = strlen(cont);
            size_t remaining = sizeof(input) - input_len - 1;
            if (remaining > cont_len)
                remaining = cont_len;
            if (remaining > 0) {
                /* Add space to separate lines, then append continuation */
                input[input_len] = ' ';
                memcpy(input + input_len + 1, cont, remaining);
                input[input_len + 1 + remaining] = '\0';
                input_len = strlen(input);
            }
            free(cont);
        }
    } else {
            if (g_cli.interactive)
                display_printf(cli_skin_color("colors.prompt", DISPLAY_GREEN), DISPLAY_BOLD,
                               "\n%s ", prompt_symbol);
            fflush(stdout);
            if (!fgets(input, sizeof(input), stdin)) break;

        /* Backslash-continuation in fallback mode */
        size_t input_len = strlen(input);
        while (input_len > 0 && input[input_len - 1] == '\\') {
            input[input_len - 1] = '\0';
            char cont[8192];
            if (!fgets(cont, sizeof(cont), stdin)) break;
            size_t cont_len = strlen(cont);
            /* Strip trailing newline from continuation */
            while (cont_len > 0 && (cont[cont_len-1] == '\n' || cont[cont_len-1] == '\r'))
                cont[--cont_len] = '\0';
            size_t remaining = sizeof(input) - strlen(input) - 1;
            if (remaining > cont_len) remaining = cont_len;
            if (remaining > 0) {
                strcat(input, " ");
                strncat(input, cont, remaining);
            }
            input_len = strlen(input);
        }
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

        /* Run agent — show spinner until first token arrives */
        if (g_cli.interactive)
            display_kawaii_start(&g_cli.spinner, "thinking", true);
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
                    /* V10: Render markdown in responses */
                    char *rendered = resp ? hermes_markdown_render(resp, 0) : NULL;
                    const char *display_text = rendered ? rendered : (resp ? resp : "(no response)");
                    const char *border = g_skin ? skin_get(g_skin, "colors.response_border", NULL) : NULL;
                    if (border && border[0] == '#')
                        display_panel_hex("Response", display_text, border);
                    else
                        display_panel("Response", display_text, DISPLAY_CYAN);
                    free(rendered);
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

        /* Show status bar after each response (if enabled) */
        if (g_cli.interactive && g_cli.config.display.statusbar) {
            display_statusbar(
                g_cli.agent.llm.model,
                g_cli.agent.session_id,
                g_cli.agent.user_turn_count,
                0);
        }
    }

    agent_close_db(&g_cli.agent);
    agent_free(&g_cli.agent);
    return 0;
}
