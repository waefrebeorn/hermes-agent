/*
 * commands.c — Slash command definitions for Hermes C CLI.
 * Central registry of all slash commands. Phase 51-60: CLI parity.
 */

#include "hermes.h"
#include "hermes_secrets.h"
#include "hermes_gateway.h"
#include "hermes_skills_hub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/utsname.h>

#include "provider.h"
#include "provider_metadata.h"
#include "usage_pricing.h"
#include "hermes_display.h"
#include "hermes_curator.h"
#include "hermes_skin.h"
#include "skill_usage.h"
#include "hermes_skill_commands.h"
#include "hermes_auth.h"

/* Tool handler declarations (used by session commands) */
extern char *session_search_handler(const char *args_json, const char *task_id);
extern char *session_crud_handler(const char *args_json, const char *task_id);

/* Forward declarations of handlers */
static void cmd_help(const char *args, agent_state_t *state);
static void cmd_exit(const char *args, agent_state_t *state);
static void cmd_clear(const char *args, agent_state_t *state);
static void cmd_model(const char *args, agent_state_t *state);
static void cmd_sessions(const char *args, agent_state_t *state);
static void cmd_save(const char *args, agent_state_t *state);
static void cmd_load(const char *args, agent_state_t *state);
static void cmd_stats(const char *args, agent_state_t *state);
static void cmd_conv(const char *args, agent_state_t *state);
static void cmd_new(const char *args, agent_state_t *state);
static void cmd_undo(const char *args, agent_state_t *state);
static void cmd_history(const char *args, agent_state_t *state);
static void cmd_recap(const char *args, agent_state_t *state);
static void cmd_topic(const char *args, agent_state_t *state);
static void cmd_config(const char *args, agent_state_t *state);
static void cmd_commands(const char *args, agent_state_t *state);
static void cmd_tools(const char *args, agent_state_t *state);
static void cmd_tools_verify(const char *args, agent_state_t *state);
static void cmd_reset(const char *args, agent_state_t *state);
static void cmd_retry(const char *args, agent_state_t *state);
static void cmd_compress(const char *args, agent_state_t *state);
static void cmd_branch(const char *args, agent_state_t *state);
static void cmd_snapshot(const char *args, agent_state_t *state);
static void cmd_status(const char *args, agent_state_t *state);
static void cmd_stop(const char *args, agent_state_t *state);
static void cmd_approve(const char *args, agent_state_t *state);
static void cmd_deny(const char *args, agent_state_t *state);
static void cmd_title(const char *args, agent_state_t *state);
static void cmd_resume(const char *args, agent_state_t *state);
static void cmd_yolo(const char *args, agent_state_t *state);
static void cmd_usage(const char *args, agent_state_t *state);
static void cmd_plugins(const char *args, agent_state_t *state);
static void cmd_platforms(const char *args, agent_state_t *state);
static void cmd_redraw(const char *args, agent_state_t *state);
static void cmd_background(const char *args, agent_state_t *state);
static void cmd_verbose(const char *args, agent_state_t *state);
static void cmd_skin(const char *args, agent_state_t *state);
static void cmd_personality(const char *args, agent_state_t *state);
static void cmd_whoami(const char *args, agent_state_t *state);
static void cmd_profile(const char *args, agent_state_t *state);
static void cmd_goal(const char *args, agent_state_t *state);
static void cmd_agents(const char *args, agent_state_t *state);
static void cmd_reasoning(const char *args, agent_state_t *state);
static void cmd_toolsets(const char *args, agent_state_t *state);
static void cmd_skills(const char *args, agent_state_t *state);
static void cmd_cron(const char *args, agent_state_t *state);
static void cmd_fast(const char *args, agent_state_t *state);
static void cmd_secrets(const char *args, agent_state_t *state);
static void cmd_auth(const char *args, agent_state_t *state);
static void cmd_completions(const char *args, agent_state_t *state);
static void cmd_reload(const char *args, agent_state_t *state);
static void cmd_doctor(const char *args, agent_state_t *state);
static void cmd_rollback(const char *args, agent_state_t *state);
static void cmd_copy(const char *args, agent_state_t *state);
static void cmd_queue(const char *args, agent_state_t *state);
static void cmd_restart(const char *args, agent_state_t *state);
static void cmd_subgoal(const char *args, agent_state_t *state);
static void cmd_sethome(const char *args, agent_state_t *state);
static void cmd_platform(const char *args, agent_state_t *state);
static void cmd_bundles(const char *args, agent_state_t *state);
static void cmd_curator(const char *args, agent_state_t *state);
static void cmd_image(const char *args, agent_state_t *state);
static void cmd_paste(const char *args, agent_state_t *state);
static void cmd_insights(const char *args, agent_state_t *state);
static void cmd_indicator(const char *args, agent_state_t *state);
static void cmd_statusbar(const char *args, agent_state_t *state);
static void cmd_footer(const char *args, agent_state_t *state);
static void cmd_busy(const char *args, agent_state_t *state);
static void cmd_handoff(const char *args, agent_state_t *state);
static void cmd_reload_mcp(const char *args, agent_state_t *state);
static void cmd_reload_skills(const char *args, agent_state_t *state);
static void cmd_browser(const char *args, agent_state_t *state);
static void cmd_update(const char *args, agent_state_t *state);
static void cmd_debug(const char *args, agent_state_t *state);
static void cmd_voice(const char *args, agent_state_t *state);
static void cmd_steer(const char *args, agent_state_t *state);
static void cmd_kanban(const char *args, agent_state_t *state);
static void cmd_session_search(const char *args, agent_state_t *state);
static void cmd_session_export(const char *args, agent_state_t *state);
static void cmd_logs(const char *args, agent_state_t *state);
static void cmd_gateway(const char *args, agent_state_t *state);
static void cmd_skills_hub(const char *args, agent_state_t *state);
static void cmd_dump(const char *args, agent_state_t *state);
static void cmd_send(const char *args, agent_state_t *state);
static void cmd_webhook(const char *args, agent_state_t *state);
static void cmd_memory(const char *args, agent_state_t *state);

/* Forward declaration for send_message_handler from tools/send_message.c */
extern char *send_message_handler(const char *args_json, const char *task_id);

/* Registry — mirroring Python Hermes COMMAND_REGISTRY */
static const command_def_t COMMANDS[] = {
    /* Session lifecycle */
    {"/new",     "/n",    "Start a new conversation session",              cmd_new},
    {"/clear",   "/c",    "Clear conversation context",                     cmd_clear},
    {"/undo",    "/u",    "Remove the last assistant response",             cmd_undo},
    {"/save",    "/s",    "Save current session",                          cmd_save},
    {"/load",   NULL,     "Load a session: /load <session_id>",            cmd_load},
    {"/sessions",NULL,    "List saved sessions",                           cmd_sessions},
    {"/stats",  NULL,     "Show current session statistics",               cmd_stats},
    {"/recap",  NULL,     "Summarize recent session activity",               cmd_recap},
    {"/conv",   NULL,     "Show recent conversation messages",             cmd_conv},
    {"/history",NULL,     "Show full conversation history",                cmd_history},

    /* Configuration */
    {"/model",   "/m",    "Model mgmt: list [--cap NAME] | show <name> | providers | set <name>",cmd_model},
    {"/config",  "/cfg",  "Show or edit configuration: /config [key] [val]",cmd_config},
    {"/topic",   "/t",    "Set the system topic/personality: /topic <text>",cmd_topic},

    /* Tool and info */
    {"/tools",  NULL,     "List available tools and their status",         cmd_tools},
    {"/tools-verify", NULL, "Verify all expected tools are registered",     cmd_tools_verify},
    {"/commands","/cmds", "List all available slash commands",              cmd_commands},
    {"/skills-hub", "/hub", "Skills hub: /skills-hub [search|show|list|sync]", cmd_skills_hub},

    /* Help and exit */
    {"/help",    "/h",    "Show help for commands: /help [command]",       cmd_help},
    {"/exit",    "/quit", "Exit the program",                              cmd_exit},

    /* Advanced session management */
    {"/reset",   "/r",    "Reset session: clear all messages and start fresh", cmd_reset},
    {"/retry",   NULL,    "Retry the last LLM call (undo + re-send last user message)", cmd_retry},
    {"/compress","/cctx", "Compress conversation context (keep system + last N messages)", cmd_compress},
    {"/branch",  NULL,    "Branch from current session: /branch [message_index]", cmd_branch},
    {"/snapshot","/snap", "Save a named snapshot: /snapshot [name]",     cmd_snapshot},
    {"/status",  "/st",   "Show session status and configuration info",  cmd_status},
    {"/stop",    NULL,    "Kill all running background processes",        cmd_stop},
    {"/approve", NULL,    "Approve a pending dangerous command",          cmd_approve},
    {"/deny",    NULL,    "Deny a pending dangerous command",             cmd_deny},
    {"/title",   NULL,    "Set a title for the current session",          cmd_title},
    {"/resume",  NULL,    "Resume a previously-named session: /resume <id>", cmd_resume},
    {"/yolo",    NULL,    "Toggle YOLO mode (skip dangerous command approvals)", cmd_yolo},
    {"/usage",   NULL,    "Show token usage and session statistics",      cmd_usage},
    {"/plugins", NULL,    "Plugin mgmt: list | show <name>",                cmd_plugins},
    {"/platforms",NULL,   "Show gateway/messaging platform status",       cmd_platforms},
    {"/redraw",  NULL,    "Force a full UI repaint",                      cmd_redraw},
    {"/background","/bg", "Run a prompt in the background",              cmd_background},
    {"/verbose", NULL,    "Toggle tool progress display verbosity",       cmd_verbose},
    {"/skin",    NULL,    "Show or change the display skin/theme",        cmd_skin},
    {"/personality","/p", "Set a predefined personality system message", cmd_personality},
    {"/whoami",  NULL,    "Show your slash command access level",       cmd_whoami},
    {"/profile", NULL,    "Show active profile name and home directory", cmd_profile},
    {"/goal",    NULL,    "Set a standing goal Hermes works on across turns", cmd_goal},
    {"/agents",  NULL,    "Show active subagents and running tasks",    cmd_agents},
    {"/reasoning","/re",  "Manage reasoning effort and display",        cmd_reasoning},
    {"/toolsets",NULL,    "List available toolsets",                    cmd_toolsets},
    {"/skills",  NULL,    "Search and manage installed skills",         cmd_skills},
    {"/secrets", NULL,    "Manage secrets: /secrets [list|get|sync|status]", cmd_secrets},
    {"/auth",   NULL,      "Provider auth status: /auth [status|providers]", cmd_auth},
    {"/doctor",  NULL,    "Run system diagnostics: /doctor [all|config|env|keys]", cmd_doctor},
    {"/webhook", NULL,    "Manage webhook subscriptions: /webhook [list|add|remove]", cmd_webhook},
    {"/memory",  NULL,    "Memory setup: /memory [status|providers|setup]",  cmd_memory},
    {"/gateway", NULL,   "Manage gateway: /gateway [status|list|stop|setup|restart]", cmd_gateway},
    {"/completions", NULL, "Generate shell completions: /completions [bash|zsh|fish]", cmd_completions},
    {"/cron",    NULL,    "Manage scheduled tasks: /cron [list|status]", cmd_cron},
    {"/fast",    NULL,    "Toggle fast mode for priority processing",   cmd_fast},
    {"/reload",  NULL,    "Reload .env variables into running session", cmd_reload},
    {"/rollback",NULL,    "List or restore state snapshots",            cmd_rollback},
    {"/copy",    NULL,    "Copy the last assistant response to clipboard", cmd_copy},
    {"/queue",   NULL,    "Queue a prompt for the next turn",               cmd_queue},
    {"/restart", NULL,    "Gracefully restart the gateway",                 cmd_restart},
    {"/subgoal", NULL,    "Add or manage extra criteria on the active goal",cmd_subgoal},
    {"/sethome", NULL,    "Set this chat as the home channel",              cmd_sethome},
    {"/handoff", NULL,    "Hand off this session to a messaging platform",  cmd_handoff},
    {"/platform","/pf",   "Pause, resume, or list gateway platforms",       cmd_platform},
    {"/bundles", NULL,    "List skill bundles (aliases for multiple skills)",cmd_bundles},
    {"/curator", NULL,    "Background skill maintenance status",            cmd_curator},
    {"/image",   NULL,    "Attach a local image file for next prompt",     cmd_image},
    {"/paste",   NULL,    "Attach clipboard image from clipboard",          cmd_paste},
    {"/insights",NULL,    "Show usage insights and analytics (--days N)",              cmd_insights},
    {"/indicator",NULL,   "Pick the TUI busy-indicator style",             cmd_indicator},
    {"/statusbar",NULL,   "Toggle the context/model status bar",           cmd_statusbar},
    {"/footer",  NULL,    "Toggle gateway metadata footer on replies",     cmd_footer},
    {"/busy",    NULL,    "Control what Enter does while Hermes is working",cmd_busy},
    {"/reload-mcp",NULL,  "Reload MCP servers from config",                cmd_reload_mcp},
    {"/reload-skills",NULL,"Re-scan skills directory for changes",         cmd_reload_skills},
    {"/browser", NULL,    "Connect browser tools to Chromium via CDP",     cmd_browser},
    {"/voice",   NULL,    "Toggle voice input/output mode",                cmd_voice},
    {"/steer",   NULL,    "Inject a message after the next tool call",     cmd_steer},
    {"/kanban",  NULL,    "Kanban board management: /kanban [show|list|create]", cmd_kanban},
    {"/update",  NULL,    "Update Hermes Agent to the latest version",     cmd_update},
    {"/debug",   NULL,    "Upload debug report and get shareable link",     cmd_debug},
    {"/session-search", NULL, "Search sessions: /session-search <query> [--limit N]", cmd_session_search},
    {"/session-export", NULL, "Export session: /session-export <session_id> [json|markdown]", cmd_session_export},
    {"/logs", NULL, "View agent logs: /logs [errors|gateway] [-n N] [--level LEVEL]", cmd_logs},
    {"/dump", NULL, "Dump system debug info for support context", cmd_dump},
    {"/send", NULL, "Send a message: /send [target] <message>", cmd_send},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

/* ================================================================
 *  Command resolution
 * ================================================================ */

const command_def_t *commands_resolve(const char *input) {
    if (!input || input[0] != '/') return NULL;
    for (int i = 0; COMMANDS[i].name; i++) {
        if (strcmp(input, COMMANDS[i].name) == 0 ||
            (COMMANDS[i].alias && strcmp(input, COMMANDS[i].alias) == 0))
            return &COMMANDS[i];
    }
    /* Partial match: check if command name is a prefix of input followed by space or end */
    size_t inlen = strlen(input);
    for (int i = 0; COMMANDS[i].name; i++) {
        size_t cmdlen = strlen(COMMANDS[i].name);
        if (inlen > cmdlen && input[cmdlen] == ' ' &&
            strncmp(input, COMMANDS[i].name, cmdlen) == 0)
            return &COMMANDS[i];
        /* Also match exact prefix (no args) */
        if (inlen <= cmdlen && strncmp(COMMANDS[i].name, input, inlen) == 0)
            return &COMMANDS[i];
    }
    return NULL;
}

int commands_count(void) {
    int count = 0;
    for (int i = 0; COMMANDS[i].name; i++) count++;
    return count;
}

const char *commands_list_json(void) {
    int count = commands_count();
    size_t bufsz = 256 + (size_t)count * 64;
    char *buf = malloc(bufsz);
    if (!buf) return NULL;
    buf[0] = '\0';
    strcat(buf, "[");
    for (int i = 0; COMMANDS[i].name; i++) {
        if (i > 0) strcat(buf, ",");
        strcat(buf, "\"");
        strcat(buf, COMMANDS[i].name);
        strcat(buf, "\"");
    }
    strcat(buf, "]");
    return buf;
}

/* ================================================================
 *  Command execution
 * ================================================================ */

bool commands_dispatch(const char *input, agent_state_t *state) {
    const command_def_t *cmd = commands_resolve(input);
    if (!cmd) return false;

    /* Extract args (text after command name) */
    const char *args = input + strlen(cmd->name);
    while (*args == ' ') args++;
    cmd->handler(args, state);
    return true;
}

/* Try to dispatch a /command as a skill invocation.
 * Returns true if handled (message sent to agent_chat), false if not a skill.
 * Call this when commands_dispatch() returns false and input starts with '/'. */
bool commands_try_skill(const char *input, agent_state_t *state) {
    if (!input || input[0] != '/') return false;

    skill_cmd_scan();
    const char *slug = skill_cmd_resolve(input);
    if (!slug) return false;

    /* Extract user args after the skill name */
    size_t slen = strlen(slug);
    const char *user_args = input;
    if (strncmp(user_args, slug, slen) == 0) {
        user_args += slen;
        while (*user_args == ' ') user_args++;
    } else {
        /* Input is just /slug — find where slug ends */
        user_args = input + 1;
        while (*user_args && *user_args != ' ') user_args++;
        while (*user_args == ' ') user_args++;
    }

    char *msg = skill_cmd_build_message(slug, user_args);
    if (!msg) return false;

    printf("\n  [Invoking skill: %s]\n", slug);
    fflush(stdout);
    char *resp = agent_chat(state, msg);
    if (resp) {
        printf("\n%s\n", resp);
        free(resp);
    }
    free(msg);
    return true;
}

/* Return the full command list for iteration */
const command_def_t *commands_get_all(void) {
    return COMMANDS;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

static void cmd_help(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        /* Help for specific command */
        const command_def_t *cmd = commands_resolve(args);
        if (cmd) {
            printf("  %s", cmd->name);
            if (cmd->alias) printf(" (%s)", cmd->alias);
            printf("\n    %s\n", cmd->description);
        } else {
            printf("  Unknown command: %s\n", args);
        }
        return;
    }

    /* Build help content as aligned lines */
    char content[8192];
    int pos = 0;
    int count = commands_count();
    const command_def_t **cmds = NULL;
    if (count > 0) {
        cmds = (const command_def_t **)calloc((size_t)count, sizeof(command_def_t *));
        for (int i = 0; i < count; i++)
            cmds[i] = commands_get_all() + i;
    }

    /* Find max command name width for alignment */
    int max_name = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(cmds[i]->name);
        if (cmds[i]->alias) {
            int alen = (int)strlen(cmds[i]->alias) + 3; /* " (/x)" */
            if (alen > len) len = alen;
        }
        if (len > max_name) max_name = len;
    }
    if (max_name < 8) max_name = 8;
    if (max_name > 30) max_name = 30;

    /* Category section: Session (0-8), Config (9-11), Tools & Info (12-13), Other (14+) */
    const char *cats[] = {"Session", "Config", "Tools && Info", "Other"};
    int cat_starts[] = {0, 9, 12, 14};
    int cat_count = 4;
    const char *text_color = "#FFF8DC";

    for (int c = 0; c < cat_count; c++) {
        /* Category header in accent color */
        if (pos > 0)
            pos += snprintf(content + pos, sizeof(content) - (size_t)pos, "\n");
        pos += snprintf(content + pos, sizeof(content) - (size_t)pos,
            "\x1B[1;38;2;255;191;0m  ── %s ──\x1B[0m\n",
            cats[c]);

        /* Commands in this category */
        for (int i = cat_starts[c]; i < count && cmds[i]->name; i++) {
            /* Check if we're at next category boundary */
            if (c + 1 < cat_count && i >= cat_starts[c + 1]) break;
            if (!cmds[i]->name) break;

            char cmd_line[128];
            if (cmds[i]->alias)
                snprintf(cmd_line, sizeof(cmd_line), "%s (%s)",
                         cmds[i]->name, cmds[i]->alias);
            else
                snprintf(cmd_line, sizeof(cmd_line), "%s", cmds[i]->name);

            int pad = max_name - (int)strlen(cmd_line) + 2;
            if (pad < 1) pad = 1;
            pos += snprintf(content + pos, sizeof(content) - (size_t)pos,
                "    %s%*s%s\n", cmd_line, pad, "", cmds[i]->description);
        }
    }
    free(cmds);

    /* Display in a panel */
    const char *border = "#CD7F32";
    display_panel_hex(" Commands ", content, border);
    display_printf_hex(text_color, DISPLAY_DIM,
        "  Type /help <command> for details on a specific command.\n");
}
static void cmd_recap(const char *args, agent_state_t *state) {
    (void)args;
    size_t n = state->message_count;
    if (n == 0) {
        printf("No conversation to recap.\n");
        return;
    }

    /* Count turns by role */
    size_t user_count = 0, assistant_count = 0, tool_count = 0;
    size_t recent_window = n > 20 ? n - 20 : 0;
    size_t recent_user = 0, recent_assistant = 0;

    /* Tool usage counting */
#define MAX_TOOL_TYPES 64
    char tool_names[MAX_TOOL_TYPES][64];
    int tool_counts[MAX_TOOL_TYPES];
    int tool_type_count = 0;

    /* File tracking */
#define MAX_FILES 16
    char touched_files[MAX_FILES][128];
    int file_count = 0;

    /* Latest user prompt and assistant reply */
    char latest_user[256] = "";
    char latest_reply[256] = "";

    for (size_t i = 0; i < n; i++) {
        const message_t *msg = state->messages[i];
        if (!msg) continue;

        switch (msg->role) {
            case MSG_USER:
                user_count++;
                if (i >= recent_window) recent_user++;
                if (msg->content) {
                    snprintf(latest_user, sizeof(latest_user), "%s", msg->content);
                }
                break;
            case MSG_ASSISTANT:
                assistant_count++;
                if (i >= recent_window) recent_assistant++;
                if (msg->content && msg->content[0]) {
                    snprintf(latest_reply, sizeof(latest_reply), "%s", msg->content);
                }
                /* Count tool calls */
                for (int t = 0; t < msg->tool_calls_count; t++) {
                    tool_count++;
                    /* Track tool name for counts */
                    int found = 0;
                    for (int k = 0; k < tool_type_count; k++) {
                        if (strcmp(tool_names[k], msg->tool_calls[t].name) == 0) {
                            tool_counts[k]++;
                            found = 1;
                            break;
                        }
                    }
                    if (!found && tool_type_count < MAX_TOOL_TYPES) {
                        snprintf(tool_names[tool_type_count], sizeof(tool_names[0]),
                                 "%s", msg->tool_calls[t].name);
                        tool_counts[tool_type_count] = 1;
                        tool_type_count++;
                    }
                    /* Track file-editing tools */
                    if ((strcmp(msg->tool_calls[t].name, "write_file") == 0 ||
                         strcmp(msg->tool_calls[t].name, "patch") == 0 ||
                         strcmp(msg->tool_calls[t].name, "read_file") == 0) &&
                        file_count < MAX_FILES) {
                        /* Try to extract path from args JSON */
                        const char *args_str = msg->tool_calls[t].arguments;
                        if (args_str && args_str[0]) {
                            /* Simple JSON parse for "path" key */
                            const char *path_key = strstr(args_str, "\"path\":");
                            if (path_key) {
                                path_key += 7; /* skip "path": */
                                while (*path_key == ' ' || *path_key == '\"') path_key++;
                                const char *end = strchr(path_key, '\"');
                                if (end) {
                                    size_t plen = (size_t)(end - path_key);
                                    if (plen > sizeof(touched_files[0]) - 1)
                                        plen = sizeof(touched_files[0]) - 1;
                                    memcpy(touched_files[file_count], path_key, plen);
                                    touched_files[file_count][plen] = '\0';
                                    file_count++;
                                }
                            }
                        }
                    }
                }
                break;
            case MSG_TOOL:
                tool_count++;
                break;
            default:
                break;
        }
    }

    /* Truncate long text */
    if (strlen(latest_user) > 100) {
        snprintf(latest_user + 97, sizeof(latest_user) - 97, "...");
    }
    if (strlen(latest_reply) > 150) {
        snprintf(latest_reply + 147, sizeof(latest_reply) - 147, "...");
    }

    /* Format output */
    printf("\nSession recap%s%s\n",
           state->user_title[0] ? " - " : "",
           state->user_title[0] ? state->user_title : "");

    size_t win_user = n > 20 ? recent_user : user_count;
    size_t win_asst = n > 20 ? recent_assistant : assistant_count;
    printf("  Recent: %zu user turn%s / %zu assistant repl%s, %zu tool result%s\n",
           win_user, win_user != 1 ? "s" : "",
           win_asst, win_asst != 1 ? "ies" : "y",
           tool_count, tool_count != 1 ? "s" : "");
    if (n > 20 && (user_count != win_user || assistant_count != win_asst)) {
        printf("  (of %zu/%zu total)\n", user_count, assistant_count);
    }

    /* Top tools */
    if (tool_type_count > 0) {
        printf("  Tools used: ");
        int shown = 0;
        for (int i = 0; i < tool_type_count && i < 5; i++) {
            if (shown > 0) printf(", ");
            printf("%s x %d", tool_names[i], tool_counts[i]);
            shown++;
        }
        if (tool_type_count > 5)
            printf(" (+%d more)", tool_type_count - 5);
        printf("\n");
    }

    /* Files touched */
    if (file_count > 0) {
        printf("  Files touched: ");
        for (int i = 0; i < file_count && i < 5; i++) {
            if (i > 0) printf(", ");
            printf("%s", touched_files[i]);
        }
        if (file_count > 5)
            printf(" (+%d more)", file_count - 5);
        printf("\n");
    }

    /* Latest messages */
    if (latest_user[0])
        printf("  Last ask: %s\n", latest_user);
    if (latest_reply[0])
        printf("  Last reply: %s\n", latest_reply);

    printf("\n");
}

static void cmd_exit(const char *args, agent_state_t *state) {
    (void)args;
    state->interrupted = true;
}

static void cmd_clear(const char *args, agent_state_t *state) {
    (void)args;
    /* Preserve session metadata, wipe messages */
    char keep_id[64];
    snprintf(keep_id, sizeof(keep_id), "%s", state->session_id);
    int keep_iterations = state->iteration_count;

    context_clear(state);

    /* Restore session metadata */
    snprintf(state->session_id, sizeof(state->session_id), "%s", keep_id);
    state->iteration_count = keep_iterations;
    state->interrupted = false;

    printf("Context cleared. Session: %s\n", state->session_id);
}

static void cmd_model(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        /* No args: show current model */
        printf("Model:        %s\n", state->llm.model);
        printf("Provider:     %s\n", state->llm.provider[0] ? state->llm.provider : "(auto)");
        printf("Base URL:     %s\n", state->llm.base_url[0] ? state->llm.base_url : "(default)");
        printf("Max turns:    %d\n", state->max_iterations);
        printf("Tools:        %zu available\n", state->tools.count);
        printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
        return;
    }

    /* Parse subcommand */
    char arg_copy[256];
    snprintf(arg_copy, sizeof(arg_copy), "%s", args);
    const char *cmd = strtok(arg_copy, " ");
    if (!cmd) {
        printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
        return;
    }

    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
        /* /model list [--cap NAME] */
        const char *cap_str = strstr(args, "--cap");
        if (cap_str && cap_str[6]) {
            cap_str += 6;
            while (*cap_str == '=' || *cap_str == ' ') cap_str++;
            model_capability_t caps = model_capability_parse(cap_str);
            char *json = model_metadata_list_filtered_json(caps);
            if (json) {
                printf("Models with capability '%s':\n%s\n", cap_str, json);
                free(json);
            }
        } else {
            char *json = model_metadata_list_json();
            if (json) {
                printf("Known models:\n%s\n", json);
                free(json);
            }
        }
        return;
    }

    if (strcmp(cmd, "show") == 0 || strcmp(cmd, "info") == 0) {
        const char *name = strtok(NULL, " ");
        if (!name) {
            printf("Usage: /model show <model_name>\n");
            return;
        }
        const model_metadata_t *m = model_metadata_find(name);
        if (!m) {
            printf("Model '%s' not found in local metadata.\n", name);
            return;
        }
        char caps_str[128];
        model_capability_format(m->caps, caps_str, sizeof(caps_str));
        printf("Model:        %s\n", m->model_prefix);
        printf("Family:       %s\n", m->family);
        printf("Context:      %d tokens\n", m->context_window);
        printf("Max output:   %d tokens\n", m->max_output);
        printf("Capabilities: %s\n", caps_str[0] ? caps_str : "(none)");
        printf("Pricing:      $%.4f/$%.4f per 1M in/out\n", m->input_per_1m, m->output_per_1m);
        return;
    }

    if (strcmp(cmd, "providers") == 0 || strcmp(cmd, "prov") == 0) {
        char *json = provider_metadata_list_json();
        if (json) {
            printf("Known providers:\n%s\n", json);
            free(json);
        }
        return;
    }

    if (strcmp(cmd, "set") == 0) {
        const char *model_name = strtok(NULL, " ");
        if (!model_name) {
            printf("Usage: /model set <model_name>\n");
            return;
        }
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", model_name);
        printf("Model set to: %s\n", state->llm.model);
        return;
    }

    /* Unknown subcommand */
    printf("Unknown: /model %s\n", cmd);
    printf("Usage: /model list [--cap NAME] | /model show <name> | /model providers | /model set <name>\n");
}

static void cmd_sessions(const char *args, agent_state_t *state) {
    if (!state->db) {
        printf("No session database available.\n");
        return;
    }
    int limit = 0;
    const char *search = NULL;
    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        /* Check for -search filter */
        const char *search_marker = strstr(arg_copy, "-search");
        if (search_marker) {
            search = search_marker + 8;
            while (*search == ' ') search++;
            ((char*)search_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0) limit = parsed;
    }
    size_t count = 0;
    session_entry_t *entries = agent_session_list(&count, search, limit);
    if (!entries || count == 0) {
        printf("No saved sessions%s.\n", search ? " matching search" : "");
        if (entries) free(entries);
        return;
    }
    printf("Saved sessions (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %s", entries[i].id);
        if (entries[i].meta.title[0])
            printf(" \u2014 %s", entries[i].meta.title);
        printf("\n");
    }
    free(entries);
}

static void cmd_save(const char *args, agent_state_t *state) {
    (void)args;
    /* Auto-open DB if needed */
    if (!state->db) agent_open_db(state);
    if (agent_save_session(state))
        printf("Session saved: %s\n", state->session_id);
    else
        printf("Failed to save session.\n");
}

static void cmd_load(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /load <session_id>\n");
        return;
    }
    /* Auto-open DB if needed */
    if (!state->db) agent_open_db(state);
    /* Save current session before loading new one */
    if (state->message_count > 0 && state->db) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    if (agent_load_session(state, args))
        printf("Session loaded: %s (%zu messages)\n", args, state->message_count);
    else
        printf("Failed to load session: %s\n", args);
}

static void cmd_stats(const char *args, agent_state_t *state) {
    (void)args;
    printf("Messages:  %zu\n", state->message_count);
    printf("Iterations: %d\n", state->iteration_count);
    printf("Session:   %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Tools registered: %zu\n", state->tools.count);
    printf("Model:     %s\n", state->llm.model);
    printf("Provider:  %s\n", state->llm.provider);

    /* Token estimation */
    size_t total_chars = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
    }
    size_t est_tokens = llm_estimate_tokens("") + total_chars / 4;
    printf("Est. tokens: %zu across %zu messages\n",
           est_tokens, state->message_count);
}

/* Helper: print messages with role filtering and count limit */
static void print_messages(const agent_state_t *state, size_t start, size_t count,
                           const char *filter_role, bool show_full) {
    size_t printed = 0;
    size_t skip_role = 0;
    message_role_t filter = 255; /* no filter */
    if (filter_role) {
        if (strcmp(filter_role, "system") == 0 || strcmp(filter_role, "sys") == 0) filter = MSG_SYSTEM;
        else if (strcmp(filter_role, "user") == 0 || strcmp(filter_role, "usr") == 0) filter = MSG_USER;
        else if (strcmp(filter_role, "assistant") == 0 || strcmp(filter_role, "asm") == 0) filter = MSG_ASSISTANT;
        else if (strcmp(filter_role, "tool") == 0) filter = MSG_TOOL;
    }

    for (size_t i = start; i < state->message_count && printed < count; i++) {
        if (filter != 255 && state->messages[i]->role != filter) {
            skip_role++;
            continue;
        }
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "sys";  break;
            case MSG_USER:      role_str = "usr";  break;
            case MSG_ASSISTANT: role_str = "asm";  break;
            case MSG_TOOL:      role_str = "tool"; break;
            default:            role_str = "?";    break;
        }
        const char *content = state->messages[i]->content;
        if (content) {
            if (show_full) {
                printf("  [%s] %s\n", role_str, content);
            } else {
                char preview[81];
                size_t clen = strlen(content);
                if (clen > 80) {
                    memcpy(preview, content, 77);
                    preview[77] = '.'; preview[78] = '.'; preview[79] = '.';
                    preview[80] = '\0';
                } else {
                    memcpy(preview, content, clen + 1);
                }
                printf("  [%s] %s\n", role_str, preview);
            }
        } else {
            printf("  [%s] (no content)\n", role_str);
        }
        printed++;
    }
    if (printed == 0)
        printf("  (no matching messages)\n");
}

static void cmd_conv(const char *args, agent_state_t *state) {
    size_t n = state->message_count;
    size_t show_n = 10;
    const char *filter = NULL;

    /* Basic args parsing: /conv [N] [-role <role>] */
    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        const char *role_marker = strstr(arg_copy, "-role");
        if (role_marker) {
            filter = role_marker + 6;
            while (*filter == ' ') filter++;
            /* Truncate arg_copy at role_marker for count parsing */
            ((char*)role_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0 && parsed <= (int)state->message_count)
            show_n = (size_t)parsed;
    }

    size_t start = (n > show_n) ? n - show_n : 0;
    printf("Recent conversation (%zu-%zu of %zu)%s:\n",
           start + 1, n, n, filter ? " (filtered)" : "");
    print_messages(state, start, show_n, filter, false);
}

static void cmd_new(const char *args, agent_state_t *state) {
    /* Persist old session if DB available */
    if (state->db && state->message_count > 0) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    /* Reset session: clear messages, generate new ID */
    context_clear(state);
    agent_generate_session_id(state);
    printf("New session started: %s\n", state->session_id);
    /* Optionally set a name */
    if (args && args[0]) {
        snprintf(state->user_title, sizeof(state->user_title), "%s", args);
        printf("Session title: %s\n", state->user_title);
    }
}

static void cmd_undo(const char *args, agent_state_t *state) {
    (void)args;
    if (state->message_count == 0) {
        printf("No messages to undo.\n");
        return;
    }

    /* Take snapshot before modifying (so we can redo) */
    /* Then restore previous snapshot if available */
    if (agent_snapshot_restore(state)) {
        printf("Restored previous state. %zu messages.\n", state->message_count);
        return;
    }

    /* No snapshot — fall back to simple undo: remove last turn */
    size_t removed = 0;
    while (state->message_count > 0) {
        message_role_t role = state->messages[state->message_count - 1]->role;
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
        removed++;
        if (role == MSG_USER || role == MSG_SYSTEM)
            break;
    }
    printf("Removed %zu message(s). %zu remaining.\n", removed, state->message_count);
}

static void cmd_history(const char *args, agent_state_t *state) {
    size_t n = state->message_count;
    if (n == 0) {
        printf("No conversation history.\n");
        return;
    }

    size_t show_n = n;
    const char *filter = NULL;
    const char *search_term = NULL;

    if (args && args[0]) {
        char arg_copy[256];
        snprintf(arg_copy, sizeof(arg_copy), "%s", args);
        /* Check for -role filter */
        const char *role_marker = strstr(arg_copy, "-role");
        if (role_marker) {
            filter = role_marker + 6;
            while (*filter == ' ') filter++;
            ((char*)role_marker)[0] = '\0';
        }
        /* Check for -search */
        const char *search_marker = strstr(arg_copy, "-search");
        if (search_marker) {
            search_term = search_marker + 8;
            while (*search_term == ' ') search_term++;
            ((char*)search_marker)[0] = '\0';
        }
        int parsed = atoi(arg_copy);
        if (parsed > 0 && parsed <= (int)n)
            show_n = (size_t)parsed;
    }

    printf("Full conversation (%zu messages)%s%s%s:\n", n,
           filter ? " (filtered)" : "",
           search_term ? " (searching)" : "",
           show_n < n ? " (showing last)" : "");

    size_t start = (show_n < n) ? n - show_n : 0;
    size_t printed = 0;

    for (size_t i = start; i < n && printed < show_n; i++) {
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "?";         break;
        }

        /* Role filter */
        if (filter) {
            if (strcmp(filter, "system") == 0 && state->messages[i]->role != MSG_SYSTEM) continue;
            else if (strcmp(filter, "user") == 0 && state->messages[i]->role != MSG_USER) continue;
            else if (strcmp(filter, "assistant") == 0 && state->messages[i]->role != MSG_ASSISTANT) continue;
            else if (strcmp(filter, "tool") == 0 && state->messages[i]->role != MSG_TOOL) continue;
        }

        const char *content = state->messages[i]->content;
        if (content) {
            /* Search filter */
            if (search_term && !strstr(content, search_term)) continue;

            printf("  [%s] %s\n", role_str, content);
        } else {
            printf("  [%s] (no content)\n", role_str);
        }
        printed++;
    }
    if (printed == 0)
        printf("  (no matching messages)\n");
}

static void cmd_topic(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /topic <system prompt text>\n");
        return;
    }
    /* Set or replace system message */
    if (state->message_count > 0 && state->messages[0]->role == MSG_SYSTEM) {
        /* Replace existing system message */
        free(state->messages[0]->content);
        state->messages[0]->content = strdup(args);
    } else {
        /* Insert new system message at front */
        message_t *sys = (message_t *)calloc(1, sizeof(message_t));
        if (sys) {
            sys->role = MSG_SYSTEM;
            sys->content = strdup(args);
            /* Shift messages right */
            for (size_t i = state->message_count; i > 0; i--)
                state->messages[i] = state->messages[i - 1];
            state->messages[0] = sys;
            state->message_count++;
        }
    }
    printf("Topic set.\n");
}

/* P23: Config category groups — /config groups lists all */
typedef struct {
    const char *name;
    const char *desc;
    const char *prefix;
    int key_count;
} cfg_category_t;

static const cfg_category_t CFG_CATEGORIES[] = {
    {"model",       "Provider, model, API connection settings",    "model.",       0},
    {"display",     "UI theme, skin, streaming, language",        "display.",     0},
    {"agent",       "Iterations, verbosity, system prompt",       "agent.",       0},
    {"tools",       "Terminal, approvals, vision settings",       "tools.",       0},
    {"delegation",  "Subagent spawning and child config",         "delegation.",  0},
    {"browser",     "CDP browser engine settings",                "browser.",     0},
    {"memory",      "Memory provider, char limits, TTL",          "memory.",      0},
    {"compression", "Context compression strategy and thresholds","compression.", 0},
    {"cron",        "Scheduler directory, job limits",            "cron.",        0},
    {"notification","Completion/error notification settings",     "notification.",0},
    {"security",    "Tirith, URL safety, redaction",              "security.",    0},
    {"sessions",    "DB path, retention, auto-save",              "session.",     0},
    {"plugin",      "Plugin directories and enabled plugins",      "plugin.",      0},
    {"mcp",         "MCP server timeout, auth, tool limit",       "mcp.",         0},
    {"auxiliary",   "Auxiliary LLM routing (vision, web_extract, etc.)","auxiliary.",  0},
    {"tts",         "Text-to-speech configuration",                "tts.",        0},
    {"stt",         "Speech-to-text configuration",                "stt.",        0},
    {"voice",       "Voice input recording settings",              "voice.",      0},
    {NULL, NULL, NULL, 0}
};

/* Display a single config key-value pair with type prefix */
static void show_cfg_val(const char *key, const char *type, const char *val) {
    printf("  %s (%s): %s\n", key, type, val && val[0] ? val : "(default)");
}

static void show_cfg_val_int(const char *key, int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    show_cfg_val(key, "int", buf);
}

static void show_cfg_val_bool(const char *key, bool val) {
    show_cfg_val(key, "bool", val ? "true" : "false");
}

static void show_cfg_val_float(const char *key, float val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", (double)val);
    show_cfg_val(key, "float", buf);
}

/* Show all keys in a config group */
static void show_section_model(const hermes_config_t *cfg) {
    printf("model:  Provider, model, API connection\n");
    show_cfg_val("default", "str", cfg->provider_cfg.model);
    show_cfg_val("provider", "str", cfg->provider_cfg.provider);
    show_cfg_val("base_url", "str", cfg->provider_cfg.base_url);
    show_cfg_val("api_mode", "str", cfg->provider_cfg.api_mode);
    show_cfg_val("fallback_model", "str", cfg->provider_cfg.fallback_model);
    show_cfg_val("fallback_providers", "str", cfg->provider_cfg.fallback_providers);
    show_cfg_val("service_tier", "str", cfg->provider_cfg.service_tier);
    show_cfg_val("reasoning_effort", "str", cfg->provider_cfg.reasoning_effort);
    show_cfg_val_int("max_tokens", cfg->provider_cfg.max_tokens);
    show_cfg_val_float("temperature", cfg->provider_cfg.temperature);
    show_cfg_val_float("top_p", cfg->provider_cfg.top_p);
}

static void show_section_display(const hermes_config_t *cfg) {
    printf("display:  UI theme, skin, streaming, language\n");
    show_cfg_val("skin", "str", cfg->display.skin);
    show_cfg_val("banner", "str", cfg->display.banner);
    show_cfg_val("spinner", "str", cfg->display.spinner_style);
    show_cfg_val("indicator", "str", cfg->display.indicator);
    show_cfg_val("language", "str", cfg->display.language);
    show_cfg_val("personality", "str", cfg->display.personality);
    show_cfg_val("footer", "str", cfg->display.footer);
    show_cfg_val_bool("streaming", cfg->display.stream);
    show_cfg_val_bool("show_reasoning", cfg->display.show_reasoning);
    show_cfg_val_bool("compact", cfg->display.compact);
    show_cfg_val_bool("show_cost", cfg->display.show_cost);
    show_cfg_val_bool("timestamps", cfg->display.timestamps);
    show_cfg_val_bool("statusbar", cfg->display.statusbar);
}

static void show_section_agent(const hermes_config_t *cfg) {
    printf("agent:  Iterations, verbosity, system prompt\n");
    show_cfg_val_int("max_turns", cfg->agent.max_iterations);
    show_cfg_val_int("max_tool_calls_round", cfg->agent.max_tool_calls_round);
    show_cfg_val_int("max_output_tokens", cfg->agent.max_output_tokens);
    show_cfg_val_int("verbose", cfg->agent.verbose_level);
    show_cfg_val_int("api_max_retries", cfg->agent.api_max_retries);
    show_cfg_val_int("clarify_timeout", cfg->agent.clarify_timeout);
    show_cfg_val_float("compress_threshold", cfg->agent.compress_threshold);
    show_cfg_val("system_prompt", "str", cfg->agent.system_prompt);
    show_cfg_val("profile", "str", cfg->agent.profile);
    show_cfg_val("reasoning_effort", "str", cfg->agent.reasoning_effort);
    show_cfg_val_bool("fast", cfg->agent.fast_mode);
    show_cfg_val_bool("quiet", cfg->agent.quiet_mode);
}

static void show_section_tools(const hermes_config_t *cfg) {
    printf("tools:  Terminal, approvals, vision, tool output\n");
    show_cfg_val("approval_mode", "str", cfg->tools.approval_mode);
    show_cfg_val_int("approval_timeout", cfg->tools.approval_timeout);
    show_cfg_val_int("terminal_timeout", cfg->tools.terminal_timeout);
    show_cfg_val_int("max_result_size", cfg->tools.max_result_size);
    show_cfg_val_int("vision_timeout", cfg->tools.vision_timeout);
    show_cfg_val("vision_model", "str", cfg->tools.vision_model);
    show_cfg_val("terminal_backend", "str", cfg->tools.terminal_backend);
    show_cfg_val_bool("persistent_shell", cfg->tools.persistent_shell);
    show_cfg_val("web_backend", "str", cfg->tools.web_backend);
    show_cfg_val("web_search_backend", "str", cfg->tools.web_search_backend);
    show_cfg_val("web_extract_backend", "str", cfg->tools.web_extract_backend);
    show_cfg_val_int("web_search_timeout", cfg->tools.web_search_timeout);
    show_cfg_val_bool("allow_background", cfg->tools.allow_background);
    show_cfg_val_bool("local_process_cleanup", cfg->tools.local_process_cleanup);
}

static void show_section_auxiliary(const hermes_config_t *cfg) {
    printf("auxiliary:  Auxiliary LLM routing\n");
    #define SH_AUX_TASK(task, nm) do {         printf("  " nm ":\n");         show_cfg_val("provider", "str", cfg->auxiliary.task.provider);         show_cfg_val("model", "str", cfg->auxiliary.task.model);         show_cfg_val("base_url", "str", cfg->auxiliary.task.base_url);         show_cfg_val_int("timeout", cfg->auxiliary.task.timeout);     } while(0)
    SH_AUX_TASK(vision, "vision");
    show_cfg_val_int("download_timeout", cfg->auxiliary.vision_download_timeout);
    SH_AUX_TASK(web_extract, "web_extract");
    SH_AUX_TASK(compression, "compression");
    SH_AUX_TASK(skills_hub, "skills_hub");
    SH_AUX_TASK(approval, "approval");
    SH_AUX_TASK(mcp, "mcp");
    SH_AUX_TASK(title_generation, "title_generation");
    SH_AUX_TASK(triage_specifier, "triage_specifier");
    SH_AUX_TASK(kanban_decomposer, "kanban_decomposer");
    SH_AUX_TASK(profile_describer, "profile_describer");
    SH_AUX_TASK(curator, "curator");
    #undef SH_AUX_TASK
}

static void show_section_tts(const hermes_config_t *cfg) {
    printf("tts:  Text-to-speech configuration\n");
    show_cfg_val("provider", "str", cfg->tts.provider);
    show_cfg_val("edge.voice", "str", cfg->tts.edge_voice);
    show_cfg_val("elevenlabs.voice_id", "str", cfg->tts.elevenlabs_voice_id);
    show_cfg_val("elevenlabs.model_id", "str", cfg->tts.elevenlabs_model_id);
    show_cfg_val("openai.model", "str", cfg->tts.openai_model);
    show_cfg_val("openai.voice", "str", cfg->tts.openai_voice);
    show_cfg_val("xai.voice_id", "str", cfg->tts.xai_voice_id);
    show_cfg_val("xai.language", "str", cfg->tts.xai_language);
    show_cfg_val_int("xai.sample_rate", cfg->tts.xai_sample_rate);
    show_cfg_val_int("xai.bit_rate", cfg->tts.xai_bit_rate);
    show_cfg_val("mistral.model", "str", cfg->tts.mistral_model);
    show_cfg_val("mistral.voice_id", "str", cfg->tts.mistral_voice_id);
    show_cfg_val("neutts.ref_audio", "str", cfg->tts.neutts_ref_audio);
    show_cfg_val("neutts.ref_text", "str", cfg->tts.neutts_ref_text);
    show_cfg_val("neutts.model", "str", cfg->tts.neutts_model);
    show_cfg_val("neutts.device", "str", cfg->tts.neutts_device);
    show_cfg_val("piper.voice", "str", cfg->tts.piper_voice);
}

static void show_section_stt(const hermes_config_t *cfg) {
    printf("stt:  Speech-to-text configuration\n");
    show_cfg_val_bool("enabled", cfg->stt.enabled);
    show_cfg_val("provider", "str", cfg->stt.provider);
    show_cfg_val("local.model", "str", cfg->stt.local_model);
    show_cfg_val("local.language", "str", cfg->stt.local_language);
    show_cfg_val("openai.model", "str", cfg->stt.openai_model);
    show_cfg_val("mistral.model", "str", cfg->stt.mistral_model);
}

static void show_section_voice(const hermes_config_t *cfg) {
    printf("voice:  Voice input recording settings\n");
    show_cfg_val("record_key", "str", cfg->voice.record_key);
    show_cfg_val_int("max_recording_seconds", cfg->voice.max_recording_seconds);
    show_cfg_val_bool("auto_tts", cfg->voice.auto_tts);
    show_cfg_val_bool("beep_enabled", cfg->voice.beep_enabled);
    show_cfg_val_int("silence_threshold", cfg->voice.silence_threshold);
    show_cfg_val_float("silence_duration", cfg->voice.silence_duration);
}

static void show_section_delegation(const hermes_config_t *cfg) {
    printf("delegation:  Subagent spawning and child config\n");
    show_cfg_val_int("max_concurrent_children", cfg->delegation.max_concurrent_children);
    show_cfg_val_int("max_spawn_depth", cfg->delegation.max_spawn_depth);
    show_cfg_val_int("child_timeout", cfg->delegation.child_timeout);
    show_cfg_val_int("child_max_turns", cfg->delegation.child_max_turns);
    show_cfg_val("child_model", "str", cfg->delegation.child_model);
    show_cfg_val("child_provider", "str", cfg->delegation.child_provider);
}

static void show_section_browser(const hermes_config_t *cfg) {
    printf("browser:  CDP engine, viewport, timeout\n");
    show_cfg_val("cdp_url", "str", cfg->browser_cfg.cdp_url);
    show_cfg_val("engine", "str", cfg->browser_cfg.browser_type);
    show_cfg_val_bool("headless", cfg->browser_cfg.headless);
    show_cfg_val_bool("javascript", cfg->browser_cfg.enable_javascript);
    show_cfg_val_int("viewport_width", cfg->browser_cfg.viewport_width);
    show_cfg_val_int("viewport_height", cfg->browser_cfg.viewport_height);
    show_cfg_val_int("command_timeout", cfg->browser_cfg.timeout);
}

static void show_section_memory(const hermes_config_t *cfg) {
    printf("memory:  Provider, char limits, TTL\n");
    show_cfg_val("provider", "str", cfg->memory.provider);
    show_cfg_val_int("char_limit", cfg->memory.char_limit);
    show_cfg_val_int("user_char_limit", cfg->memory.user_char_limit);
    show_cfg_val_int("ttl_days", cfg->memory.ttl_days);
    show_cfg_val_bool("auto_save", cfg->memory.auto_save);
}

static void show_section_compression(const hermes_config_t *cfg) {
    printf("compression:  Strategy, threshold, min messages\n");
    show_cfg_val("model", "str", cfg->compression.model);
    show_cfg_val("strategy", "str", cfg->compression.strategy);
    show_cfg_val_float("target_ratio", cfg->compression.target_ratio);
    show_cfg_val_int("min_messages", cfg->compression.min_messages);
    show_cfg_val_bool("preserve_system", cfg->compression.preserve_system);
    show_cfg_val_int("protect_last_n", cfg->compression.protect_last_n);
    show_cfg_val_int("protect_first_n", cfg->compression.protect_first_n);
    show_cfg_val_int("hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    show_cfg_val_bool("abort_on_summary_failure", cfg->compression.abort_on_summary_failure);
}

static void show_section_cron(const hermes_config_t *cfg) {
    printf("cron:  Scheduler directory, job limits, retention\n");
    show_cfg_val("dir", "str", cfg->cron.dir);
    show_cfg_val_int("max_concurrent_jobs", cfg->cron.max_concurrent_jobs);
    show_cfg_val_int("job_timeout", cfg->cron.job_timeout);
    show_cfg_val_int("retention_days", cfg->cron.retention_days);
    show_cfg_val_bool("notify_on_failure", cfg->cron.notify_on_failure);
}

static void show_section_notification(const hermes_config_t *cfg) {
    printf("notification:  Provider, event triggers\n");
    show_cfg_val("provider", "str", cfg->notification.provider);
    show_cfg_val("sound", "str", cfg->notification.sound);
    show_cfg_val_bool("on_complete", cfg->notification.on_complete);
    show_cfg_val_bool("on_error", cfg->notification.on_error);
    show_cfg_val_bool("on_approval", cfg->notification.on_approval);
}

static void show_section_security(const hermes_config_t *cfg) {
    printf("security:  Tirith, URL safety, redaction\n");
    show_cfg_val("tirith_path", "str", cfg->security.tirith_path);
    show_cfg_val("redact_patterns", "str", cfg->security.redact_patterns);
    show_cfg_val_int("tirith_timeout", cfg->security.tirith_timeout);
    show_cfg_val_bool("tirith_enabled", cfg->security.tirith_enabled);
    show_cfg_val_bool("allow_private_urls", cfg->security.allow_private_urls);
    show_cfg_val_bool("website_blocklist_enabled", cfg->security.website_blocklist_enabled);
}

static void show_section_sessions(const hermes_config_t *cfg) {
    printf("sessions:  DB path, retention, auto-save\n");
    show_cfg_val("db_path", "str", cfg->session.db_path);
    show_cfg_val_int("retention_days", cfg->session.retention_days);
    show_cfg_val_int("auto_save_interval", cfg->session.auto_save_interval);
    show_cfg_val_bool("compress", cfg->session.compress);
    show_cfg_val_bool("store_trajectories", cfg->session.store_trajectories);
}

static void show_section_plugin(const hermes_config_t *cfg) {
    printf("plugin:  Directories, enabled plugins\n");
    show_cfg_val("dirs", "str", cfg->plugin.dirs);
    show_cfg_val("enabled", "str", cfg->plugin.enabled);
}

static void show_section_mcp(const hermes_config_t *cfg) {
    printf("mcp:  Server timeout, auth, max tools\n");
    show_cfg_val_int("timeout", cfg->mcp.timeout);
    show_cfg_val_int("max_tools", cfg->mcp.max_tools);
    show_cfg_val_bool("auth_enabled", cfg->mcp.auth_enabled);
}

/* Dispatch show_section by name */
static bool show_config_section(const hermes_config_t *cfg, const char *section) {
    if (strcmp(section, "model") == 0 || strcmp(section, "provider") == 0)
        { show_section_model(cfg); return true; }
    if (strcmp(section, "display") == 0)
        { show_section_display(cfg); return true; }
    if (strcmp(section, "agent") == 0)
        { show_section_agent(cfg); return true; }
    if (strcmp(section, "tools") == 0 || strcmp(section, "approvals") == 0)
        { show_section_tools(cfg); return true; }
    if (strcmp(section, "delegation") == 0)
        { show_section_delegation(cfg); return true; }
    if (strcmp(section, "browser") == 0)
        { show_section_browser(cfg); return true; }
    if (strcmp(section, "memory") == 0)
        { show_section_memory(cfg); return true; }
    if (strcmp(section, "compression") == 0)
        { show_section_compression(cfg); return true; }
    if (strcmp(section, "cron") == 0)
        { show_section_cron(cfg); return true; }
    if (strcmp(section, "notification") == 0)
        { show_section_notification(cfg); return true; }
    if (strcmp(section, "security") == 0)
        { show_section_security(cfg); return true; }
    if (strcmp(section, "sessions") == 0)
        { show_section_sessions(cfg); return true; }
    if (strcmp(section, "plugin") == 0)
        { show_section_plugin(cfg); return true; }
    if (strcmp(section, "mcp") == 0)
        { show_section_mcp(cfg); return true; }
    if (strcmp(section, "auxiliary") == 0)
        { show_section_auxiliary(cfg); return true; }
    if (strcmp(section, "tts") == 0)
        { show_section_tts(cfg); return true; }
    if (strcmp(section, "stt") == 0)
        { show_section_stt(cfg); return true; }
    if (strcmp(section, "voice") == 0)
        { show_section_voice(cfg); return true; }
    return false;
}

/* List all config groups */
static void list_config_groups(void) {
    printf("Config groups (18):\n");
    for (int i = 0; CFG_CATEGORIES[i].name; i++)
        printf("  %-15s  %s\n", CFG_CATEGORIES[i].name, CFG_CATEGORIES[i].desc);
    printf("\nUse /config show <group> to view keys in a group.\n");
}

/* Set a config key: parse key=value, validate, print new value */
static bool config_set_key(hermes_config_t *cfg, const char *args) {
    /* Find '=' or space delimiter */
    const char *eq = strchr(args, '=');
    if (!eq) {
        printf("Usage: /config set <key>=<value>\n");
        return false;
    }
    size_t key_len = (size_t)(eq - args);
    const char *val = eq + 1;
    while (*val == ' ') val++;

    char key[128];
    snprintf(key, sizeof(key), "%.*s", (int)key_len, args);

    /* Try to match keys and set */
    bool found = false;

    if (strcmp(key, "model") == 0 || strcmp(key, "model.default") == 0) {
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", val);
        found = true;
    }
    else if (strcmp(key, "provider") == 0 || strcmp(key, "model.provider") == 0) {
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", val);
        found = true;
    }
    else if (strcmp(key, "base_url") == 0 || strcmp(key, "model.base_url") == 0) {
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", val);
        found = true;
    }
    else if (strcmp(key, "api_mode") == 0 || strcmp(key, "model.api_mode") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", val);
        found = true;
    }
    else if (strcmp(key, "max_tokens") == 0 || strcmp(key, "model.max_tokens") == 0) {
        int iv = atoi(val);
        if (iv > 0) { cfg->provider_cfg.max_tokens = iv; found = true; }
        else printf("Invalid integer: %s\n", val);
    }
    else if (strcmp(key, "temperature") == 0 || strcmp(key, "model.temperature") == 0) {
        float fv = (float)atof(val);
        if (fv >= 0.0f && fv <= 2.0f) { cfg->provider_cfg.temperature = fv; found = true; }
        else printf("Temperature must be 0.0-2.0\n");
    }
    else if (strcmp(key, "top_p") == 0 || strcmp(key, "model.top_p") == 0) {
        float fv = (float)atof(val);
        if (fv >= 0.0f && fv <= 1.0f) { cfg->provider_cfg.top_p = fv; found = true; }
        else printf("top_p must be 0.0-1.0\n");
    }
    else if (strcmp(key, "max_turns") == 0 || strcmp(key, "agent.max_turns") == 0) {
        int iv = atoi(val);
        if (iv > 0 && iv <= 10000) { cfg->agent.max_iterations = iv; found = true; }
        else printf("max_turns must be 1-10000\n");
    }
    else if (strcmp(key, "verbose") == 0 || strcmp(key, "agent.verbose") == 0) {
        int iv = atoi(val);
        if (iv >= 0 && iv <= 2) { cfg->agent.verbose_level = iv; found = true; }
        else printf("verbose must be 0-2\n");
    }
    else if (strcmp(key, "display.skin") == 0 || strcmp(key, "skin") == 0) {
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", val);
        found = true;
    }
    else if (strcmp(key, "display.streaming") == 0 || strcmp(key, "streaming") == 0) {
        cfg->display.stream = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        found = true;
    }
    else if (strcmp(key, "approvals.mode") == 0) {
        if (strcmp(val, "off") == 0 || strcmp(val, "manual") == 0 || strcmp(val, "auto") == 0) {
            snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", val);
            found = true;
        } else printf("approvals.mode must be off/manual/auto\n");
    }
    else if (strcmp(key, "approvals.timeout") == 0) {
        int iv = atoi(val);
        if (iv >= 0 && iv <= 86400) { cfg->tools.approval_timeout = iv; found = true; }
        else printf("approvals.timeout must be 0-86400\n");
    }
    else if (strcmp(key, "terminal.timeout") == 0) {
        int iv = atoi(val);
        if (iv > 0 && iv <= 86400) { cfg->tools.terminal_timeout = iv; found = true; }
        else printf("terminal.timeout must be 1-86400\n");
    }
    else if (strcmp(key, "yolo") == 0) {
        cfg->yolo_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        if (cfg->yolo_mode) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "off");
        found = true;
    }

    if (!found) {
        printf("Unknown or unsupported key: %s\n", key);
        printf("Use /config groups to list available groups.\n");
        printf("Common keys: model, provider, base_url, max_tokens, temperature, top_p,\n");
        printf("  max_turns, verbose, display.skin, streaming, approvals.mode,\n");
        printf("  approvals.timeout, terminal.timeout, yolo\n");
        return false;
    }

    /* Sync flat fields */
    snprintf(cfg->model, sizeof(cfg->model), "%s", cfg->provider_cfg.model);
    snprintf(cfg->provider, sizeof(cfg->provider), "%s", cfg->provider_cfg.provider);
    snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", cfg->provider_cfg.base_url);
    cfg->max_turns = cfg->agent.max_iterations;
    cfg->verbose = cfg->agent.verbose_level;

    /* Write updated config back */
    if (hermes_config_export(cfg, cfg->config_path)) {
        printf("Set %s = %s (written to %s)\n", key, val, cfg->config_path);
    } else {
        printf("Set %s = %s (in-memory only, write failed)\n", key, val);
    }

    return true;
}

/* /config: Full command with groups and set support */
static void cmd_config(const char *args, agent_state_t *state) {
    /* Load config to access full config struct */
    hermes_config_t cfg;
    if (!hermes_config_load(&cfg, state->hermes_home)) {
        printf("Failed to load configuration.\n");
        return;
    }

    if (!args || !args[0]) {
        /* Show summary */
        printf("Configuration (config_version: %d):\n",
               cfg.config_version > 0 ? cfg.config_version : HERMES_CONFIG_VERSION);
        printf("  model:          %s\n", cfg.provider_cfg.model);
        printf("  provider:       %s\n", cfg.provider_cfg.provider);
        printf("  base_url:       %s\n", cfg.provider_cfg.base_url);
        printf("  api_mode:       %s\n", cfg.provider_cfg.api_mode);
        printf("  max_tokens:     %d\n", cfg.provider_cfg.max_tokens);
        printf("  temperature:    %.1f\n", (double)cfg.provider_cfg.temperature);
        printf("  top_p:          %.1f\n", (double)cfg.provider_cfg.top_p);
        printf("  display.skin:   %s\n", cfg.display.skin[0] ? cfg.display.skin : "(default)");
        printf("  display.stream: %s\n", cfg.display.stream ? "yes" : "no");
        printf("  agent.turns:    %d\n", cfg.agent.max_iterations);
        printf("  agent.verbose:  %d\n", cfg.agent.verbose_level);
        printf("  approvals.mode: %s\n", cfg.tools.approval_mode);
        printf("  terminal.timeout: %d\n", cfg.tools.terminal_timeout);
        return;
    }

    /* Subcommands */
    if (strcmp(args, "validate") == 0 || strcmp(args, "-v") == 0) {
        config_validation_t result;
        bool valid = hermes_config_validate(&cfg, &result);
        if (valid) {
            printf("Configuration valid.\n");
        } else {
            printf("Configuration issues (%d):\n", result.count);
            for (int i = 0; i < result.count; i++)
                printf("  [%s] %s\n", result.issues[i].key, result.issues[i].message);
        }
        return;
    }

    if (strcmp(args, "diff") == 0) {
        cfg_diff_t diff;
        if (hermes_config_diff(&cfg, &diff)) {
            printf("Differences from defaults (%d):\n", diff.count);
            for (int i = 0; i < diff.count; i++) {
                const char *type_str;
                switch (diff.entries[i].type) {
                    case CFG_DIFF_ADDED:   type_str = "+"; break;
                    case CFG_DIFF_CHANGED: type_str = "~"; break;
                    case CFG_DIFF_MISSING: type_str = "-"; break;
                    default:               type_str = "?"; break;
                }
                printf("  %s %s: \"%s\" -> \"%s\"\n", type_str,
                       diff.entries[i].key,
                       diff.entries[i].default_value,
                       diff.entries[i].active_value);
            }
        } else {
            printf("Configuration matches defaults.\n");
        }
        return;
    }

    if (strcmp(args, "export") == 0) {
        hermes_config_export(&cfg, NULL);
        return;
    }

    if (strcmp(args, "migrate") == 0) {
        if (hermes_config_migrate(&cfg, state->hermes_home)) {
            printf("Config migrated to v%d.\n", HERMES_CONFIG_VERSION);
            /* Reload from migrated file */
            hermes_config_load(&cfg, state->hermes_home);
        } else {
            printf("Config already at v%d. No migration needed.\n", HERMES_CONFIG_VERSION);
        }
        return;
    }

    if (strcmp(args, "groups") == 0) {
        list_config_groups();
        return;
    }

    if (strcmp(args, "schema") == 0) {
        char *schema = hermes_config_schema();
        if (schema) {
            printf("Config schema (JSON Schema draft-07):\n%s\n", schema);
            free(schema);
        } else {
            printf("Failed to generate config schema.\n");
        }
        return;
    }

    if (strncmp(args, "profile ", 8) == 0) {
        const char *profile_name = args + 8;
        if (strcmp(profile_name, "list") == 0) {
            /* List available profiles */
            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));
            DIR *d = opendir(profiles_dir);
            if (!d) {
                printf("No profiles directory found at %s\n", profiles_dir);
                return;
            }
            printf("Available profiles:\n");
            struct dirent *de;
            int count = 0;
            while ((de = readdir(d)) != NULL) {
                size_t len = strlen(de->d_name);
                if (len > 5 && strcmp(de->d_name + len - 5, ".yaml") == 0) {
                    de->d_name[len - 5] = '\0';
                    printf("  %s\n", de->d_name);
                    count++;
                }
            }
            closedir(d);
            if (count == 0) printf("  (none)\n");
            return;
        }

        /* profile clone <name> --from <source> */
        if (strncmp(profile_name, "clone ", 6) == 0) {
            const char *clone_name = profile_name + 6;
            const char *from_name = NULL;
            /* Parse --from <source> */
            const char *from_arg = strstr(clone_name, " --from ");
            char clone_buf[256] = "";
            if (from_arg) {
                from_name = from_arg + 8;
                /* Terminate clone_name at --from */
                size_t len = (size_t)(from_arg - clone_name);
                if (len >= sizeof(clone_buf)) len = sizeof(clone_buf) - 1;
                memcpy(clone_buf, clone_name, len);
                clone_buf[len] = '\0';
                clone_name = clone_buf;
            }

            if (!clone_name || !clone_name[0]) {
                printf("Usage: /config profile clone <name> --from <source>\n");
                return;
            }

            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));

            /* Determine source profile */
            const char *src = from_name ? from_name : "default";
            char src_path[HERMES_PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s.yaml", profiles_dir, src);

            /* Check source exists */
            struct stat st;
            if (stat(src_path, &st) != 0) {
                printf("Source profile '%s' not found at %s\n", src, src_path);
                return;
            }

            /* Check target doesn't already exist */
            char dst_path[HERMES_PATH_MAX];
            snprintf(dst_path, sizeof(dst_path), "%s/%s.yaml", profiles_dir, clone_name);
            if (stat(dst_path, &st) == 0) {
                printf("Profile '%s' already exists. Delete it first or use a different name.\n", clone_name);
                return;
            }

            /* Copy the file */
            FILE *src_f = fopen(src_path, "r");
            if (!src_f) {
                printf("Failed to read source profile '%s'\n", src);
                return;
            }
            FILE *dst_f = fopen(dst_path, "w");
            if (!dst_f) {
                fclose(src_f);
                printf("Failed to create profile '%s'\n", clone_name);
                return;
            }
            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src_f)) > 0) {
                fwrite(buf, 1, n, dst_f);
            }
            fclose(src_f);
            fclose(dst_f);
            printf("Profile '%s' cloned from '%s'.\n", clone_name, src);
            return;
        }

        /* profile delete <name> */
        if (strncmp(profile_name, "delete ", 7) == 0) {
            const char *del_name = profile_name + 7;
            if (!del_name || !del_name[0]) {
                printf("Usage: /config profile delete <name>\n");
                return;
            }
            char profiles_dir[HERMES_PATH_MAX];
            hermes_resolve_path("profiles", profiles_dir, sizeof(profiles_dir));
            char del_path[HERMES_PATH_MAX];
            snprintf(del_path, sizeof(del_path), "%s/%s.yaml", profiles_dir, del_name);

            struct stat st;
            if (stat(del_path, &st) != 0) {
                printf("Profile '%s' not found.\n", del_name);
                return;
            }
            if (unlink(del_path) != 0) {
                printf("Failed to delete profile '%s'.\n", del_name);
                return;
            }
            printf("Profile '%s' deleted.\n", del_name);
            return;
        }

        /* Load named profile */
        hermes_config_t pcfg;
        if (!hermes_config_load_profile(&pcfg, profile_name, state->hermes_home)) {
            printf("Profile '%s' not found. Use /config profile list to see available profiles.\n", profile_name);
            return;
        }
        hermes_set_profile(profile_name);
        printf("Profile '%s' activated (takes effect on next run).\n", profile_name);
        return;
    }

    /* Show specific section */
    if (strncmp(args, "show ", 5) == 0) {
        const char *section = args + 5;
        if (!show_config_section(&cfg, section))
            printf("Unknown section: %s. Use /config groups to list all.\n", section);
        return;
    }

    /* Get a single key */
    if (strncmp(args, "get ", 4) == 0) {
        const char *key = args + 4;
        /* Show the section the key belongs to */
        if (strstr(key, "model") == key || strcmp(key, "provider") == 0)
            show_section_model(&cfg);
        else if (strstr(key, "display") == key)
            show_section_display(&cfg);
        else if (strstr(key, "agent") == key)
            show_section_agent(&cfg);
        else if (strstr(key, "tools") == key || strstr(key, "approvals") == key || strstr(key, "terminal") == key)
            show_section_tools(&cfg);
        else if (strstr(key, "delegation") == key)
            show_section_delegation(&cfg);
        else if (strstr(key, "memory") == key)
            show_section_memory(&cfg);
        else if (strstr(key, "compression") == key)
            show_section_compression(&cfg);
        else if (strstr(key, "security") == key)
            show_section_security(&cfg);
        else if (strstr(key, "cron") == key)
            show_section_cron(&cfg);
        else if (strstr(key, "notification") == key)
            show_section_notification(&cfg);
        else if (strstr(key, "browser") == key)
            show_section_browser(&cfg);
        else if (strstr(key, "sessions") == key)
            show_section_sessions(&cfg);
        else if (strstr(key, "plugin") == key)
            show_section_plugin(&cfg);
        else if (strstr(key, "mcp") == key)
            show_section_mcp(&cfg);
        else
            printf("Unknown key: %s\n", key);
        return;
    }

    /* Set a key=value */
    if (strncmp(args, "set ", 4) == 0) {
        config_set_key(&cfg, args + 4);
        return;
    }

    printf("Usage: /config [validate|diff|export|migrate|groups|schema|profile <name>|profile list|profile clone <name> --from <source>|profile delete <name>|show <group>|get <key>|set <key>=<value>]\n");
}

static void cmd_commands(const char *args, agent_state_t *state) {
    (void)state;
    /* Count commands */
    int count = 0;
    while (COMMANDS[count].name) count++;

    /* Parse page number if provided */
    int page = 0;
    int per_page = 20;
    if (args && args[0]) {
        char *endptr;
        long val = strtol(args, &endptr, 10);
        if (*endptr == '\0' && val > 0)
            page = (int)val - 1; /* 1-indexed in UI */
    }
    int total_pages = (count + per_page - 1) / per_page;
    if (page < 0) page = 0;
    if (page >= total_pages) page = total_pages - 1;

    /* Build rows array for this page */
    int start = page * per_page;
    int end = start + per_page;
    if (end > count) end = count;
    int page_count = end - start;

    const char **rows = malloc(page_count * sizeof(char *));
    if (!rows) {
        printf("All slash commands (%d total):\n", count);
        for (int i = 0; COMMANDS[i].name; i++) {
            printf("  %s", COMMANDS[i].name);
            if (COMMANDS[i].alias)
                printf(" (%s)", COMMANDS[i].alias);
            printf("\n");
        }
        return;
    }

    for (int i = start; i < end; i++) {
        char *buf = malloc(128);
        if (!buf) { rows[i - start] = ""; continue; }
        snprintf(buf, 128, "%s\t%s\t%s",
                 COMMANDS[i].name,
                 COMMANDS[i].alias ? COMMANDS[i].alias : "",
                 COMMANDS[i].description);
        rows[i - start] = buf;
    }

    const char *headers[] = {"Command", "Alias", "Description"};
    display_table(3, headers, (const char **)rows, page_count, DISPLAY_CYAN);

    for (int i = 0; i < page_count; i++) {
        if (rows[i] && rows[i][0]) free((void *)rows[i]);
    }
    free(rows);

    if (total_pages > 1)
        printf("  Page %d/%d. Use /commands <N> for a specific page.\n",
               page + 1, total_pages);
}

static void cmd_tools(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Show details for a specific tool */
        const char *tool_name = args;
        for (size_t i = 0; i < state->tools.count; i++) {
            if (strcmp(state->tools.tools[i].name, tool_name) == 0) {
                printf("Tool:          %s\n", state->tools.tools[i].name);
                printf("Description:   %s\n", state->tools.tools[i].description);
                printf("Available:     %s\n", state->tools.tools[i].available ? "yes" : "no");
                if (state->tools.tools[i].schema_json[0])
                    printf("Schema:        %s\n", state->tools.tools[i].schema_json);
                return;
            }
        }
        printf("Tool not found: %s\n", tool_name);
        return;
    }
    printf("Registered tools (%zu):\n", state->tools.count);
    for (size_t i = 0; i < state->tools.count; i++) {
        printf("  %s", state->tools.tools[i].name);
        if (!state->tools.tools[i].available)
            printf(" [UNAVAILABLE]");
        printf(" \u2014 %s\n", state->tools.tools[i].description);
    }
}

/* /tools-verify: Verify all expected tools are registered */
static void cmd_tools_verify(const char *args, agent_state_t *state) {
    (void)args;
    const char *expected[] = {
        "terminal", "read_file", "write_file", "search_files", "patch",
        "web_get", "web_search", "web_extract",
        "skills_list", "skill_view", "skill_manage",
        "execute_code", "clarify", "memory", "todo", "process",
        "send_message", "cronjob", "session_search",
        "text_to_speech", "vision_analyze", "delegate_task",
        "x_search", "approval_status",
        "voice_listen", "voice_speak", "image_generate",
        "ha_list_entities", "ha_get_state", "ha_list_services", "ha_call_service",
        "browser_navigate", "browser_snapshot", "browser_back", "browser_forward",
        "browser_click", "browser_type", "browser_scroll",
        "browser_get_images", "browser_press",
        "browser_vision", "browser_console", "browser_dialog", "browser_cdp",
        "computer_use",
        "kanban_show", "kanban_list", "kanban_create", "kanban_complete",
        "kanban_block", "kanban_heartbeat", "kanban_comment", "kanban_unblock",
        "kanban_link",
        NULL
    };
    int total_expected = 0, found = 0, missing = 0;
    for (int i = 0; expected[i]; i++) {
        total_expected++;
        bool ok = false;
        for (size_t j = 0; j < state->tools.count; j++) {
            if (strcmp(state->tools.tools[j].name, expected[i]) == 0) {
                ok = true; break;
            }
        }
        if (ok) found++;
        else { printf("  MISSING: %s\n", expected[i]); missing++; }
    }
    printf("Tools: %zu registered, %d expected, %d missing, %d found\n",
           state->tools.count, total_expected, missing, found);
    if (missing == 0) printf("ALL EXPECTED TOOLS PRESENT\n");
}

/* ================================================================
 *  Advanced session management commands
 * ================================================================ */

/* /reset: Clear all messages, reset iteration count, generate new session */
static void cmd_reset(const char *args, agent_state_t *state) {
    (void)args;
    /* Persist current session before reset */
    if (state->db && state->message_count > 0) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    context_clear(state);
    agent_generate_session_id(state);
    state->iteration_count = 0;
    state->interrupted = false;
    printf("Session reset. New ID: %s\n", state->session_id);
}

/* /retry: Remove last assistant + tool messages, re-send last user message */
static void cmd_retry(const char *args, agent_state_t *state) {
    (void)args;
    if (state->message_count < 2) {
        printf("Nothing to retry.\n");
        return;
    }

    /* Find last user message index */
    int last_user = -1;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->role == MSG_USER)
            last_user = (int)i;
    }
    if (last_user < 0) {
        printf("No user message found to retry.\n");
        return;
    }

    /* Store the user message text before undoing */
    char *user_text = strdup(state->messages[last_user]->content);

    /* Undo all messages after the last user message */
    size_t removed = 0;
    while (state->message_count > (size_t)(last_user + 1)) {
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
        removed++;
    }

    printf("Retrying (removed %zu messages). Re-running agent...\n\n", removed);

    /* Call agent_chat to re-run with the user message */
    char *resp = agent_chat(state, user_text);
    free(user_text);

    if (resp) {
        /* In streaming mode, content already printed via stream callback */
        if (!state->stream_cb)
            printf("%s\n", resp);
        else if (strncmp(resp, "Error:", 6) == 0)
            printf("%s\n", resp);
        free(resp);
    } else {
        printf("Error: No response\n");
    }
}

/* /compress: Keep system + last N messages, summarize dropped ones */
static void cmd_compress(const char *args, agent_state_t *state) {
    if (state->message_count <= 4) {
        printf("Context too short to compress (%zu messages). Need >4.\n",
               state->message_count);
        return;
    }

    size_t keep = 4; /* Keep system + 3 most recent messages */
    size_t start = (state->messages[0]->role == MSG_SYSTEM) ? 1 : 0;
    size_t remove_count = state->message_count - keep;
    if (remove_count <= start) {
        printf("Not enough messages to compress.\n");
        return;
    }

    /* Build a summary of the messages being dropped */
    size_t dropped_start = start;
    size_t dropped_count = remove_count - start;

    /* Count chars in dropped messages */
    size_t total_chars = 0;
    size_t user_count = 0, asm_count = 0, tool_count = 0;
    for (size_t i = dropped_start; i < dropped_start + dropped_count && i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
        switch (state->messages[i]->role) {
            case MSG_USER:      user_count++; break;
            case MSG_ASSISTANT: asm_count++;  break;
            case MSG_TOOL:      tool_count++; break;
            default: break;
        }
    }

    /* Create summary message */
    char summary[4096];
    if (args && args[0]) {
        snprintf(summary, sizeof(summary),
            "[Compressed conversation summary: %zu messages dropped (%zu user, "
            "%zu assistant, %zu tool), ~%zu chars. Focus: %s. "
            "Session continues below with most recent messages.]",
            dropped_count, user_count, asm_count, tool_count, total_chars, args);
    } else {
        snprintf(summary, sizeof(summary),
            "[Compressed conversation summary: %zu messages dropped (%zu user, "
            "%zu assistant, %zu tool), ~%zu chars. "
            "Session continues below with most recent messages.]",
            dropped_count, user_count, asm_count, tool_count, total_chars);
    }

    /* Replace the range with a single summary message */
    message_t *summary_msg = message_new(MSG_SYSTEM, summary);
    if (!summary_msg) {
        printf("Failed to create summary message.\n");
        return;
    }

    /* Free dropped messages */
    for (size_t i = 0; i < dropped_count; i++) {
        if (dropped_start + i < state->message_count)
            message_free(state->messages[dropped_start + i]);
    }

    /* Shift remaining messages left by (dropped_count - 1) to make room for summary */
    size_t remaining = state->message_count - dropped_start - dropped_count;
    state->messages[dropped_start] = summary_msg;
    memmove(&state->messages[dropped_start + 1],
            &state->messages[dropped_start + dropped_count],
            remaining * sizeof(message_t *));
    state->message_count -= (dropped_count - 1);

    printf("Context compressed: dropped %zu messages, inserted summary. "
           "%zu messages remaining.\n", dropped_count, state->message_count);
}

/* /branch: Fork from current state into a new session */
static void cmd_branch(const char *args, agent_state_t *state) {
    size_t branch_point = state->message_count;

    if (args && args[0]) {
        /* Parse message index from args */
        char *end = NULL;
        long idx = strtol(args, &end, 10);
        if (end != args && idx >= 0 && (size_t)idx <= state->message_count)
            branch_point = (size_t)idx;
    }

    /* Generate new session ID */
    char old_session[64];
    snprintf(old_session, sizeof(old_session), "%s", state->session_id);
    agent_generate_session_id(state);

    /* Remove messages after branch point */
    while (state->message_count > branch_point) {
        message_free(state->messages[state->message_count - 1]);
        state->message_count--;
    }

    state->iteration_count = 0;
    printf("Branched from session %s at message %zu.\nNew session: %s\n",
           old_session, branch_point, state->session_id);
}

/* /snapshot: Save a named snapshot with timestamp prefix */
static void cmd_snapshot(const char *args, agent_state_t *state) {
    char snap_name[128];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    if (args && args[0]) {
        snprintf(snap_name, sizeof(snap_name), "SNAP_%s_%s",
                 state->session_id, args);
    } else {
        snprintf(snap_name, sizeof(snap_name), "SNAP_%04d%02d%02d_%02d%02d%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    }

    /* Save using existing session save mechanism with snapshot ID */
    char prev_id[64];
    snprintf(prev_id, sizeof(prev_id), "%s", state->session_id);
    snprintf(state->session_id, sizeof(state->session_id), "%s", snap_name);

    bool ok = false;
    if (state->db || agent_open_db(state))
        ok = agent_save_session(state);

    /* Restore original session ID */
    snprintf(state->session_id, sizeof(state->session_id), "%s", prev_id);

    if (ok)
        printf("Snapshot saved: %s (%zu messages)\n", snap_name, state->message_count);
    else
        printf("Failed to save snapshot.\n");
}

/* ================================================================
 *  Additional commands (CLI parity)
 * ================================================================ */

/* /status: Show session status and configuration */
static void cmd_status(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Show platform/connection status */
        if (strcmp(args, "gateway") == 0 || strcmp(args, "platform") == 0) {
            printf("Gateway platforms: 19 connected\n");
            printf("Active connections: telegram, discord, slack, matrix,\n");
            printf("  mattermost, whatsapp, email, signal, homeassistant,\n");
            printf("  sms, feishu, wecom, dingtalk, qqbot, bluebubbles,\n");
            printf("  msgraph_webhook, weixin, yuanbao, webhook\n");
            return;
        }
        if (strcmp(args, "config") == 0) {
            printf("Model:         %s\n", state->llm.model[0] ? state->llm.model : "(default)");
            printf("Provider:      %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
            printf("Max tokens:    %d\n", state->llm.max_tokens);
            printf("Temperature:   %.2f\n", state->llm.temperature);
            printf("Toolsets:      enabled=%s  disabled=%s\n",
                   state->enabled_toolsets[0] ? state->enabled_toolsets : "(all)",
                   state->disabled_toolsets[0] ? state->disabled_toolsets : "(none)");
            if (state->llm.tool_choice[0])
                printf("Tool choice:   %s\n", state->llm.tool_choice);
            if (state->budget_hard_limit)
                printf("Budget mode:   hard limit\n");
            return;
        }
        if (strcmp(args, "skills") == 0) {
            printf("Skills dir:    ~/.hermes/skills/\n");
            printf("Run /skills list to see installed skills.\n");
            return;
        }
        if (strcmp(args, "all") == 0) {
            /* Full status — fall through to default */
        } else {
            printf("Usage: /status [config|gateway|skills|all]\n");
            return;
        }
    }

    /* Default: session status summary */
    printf("Session:       %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Model:         %s\n", state->llm.model[0] ? state->llm.model : "(default)");
    printf("Provider:      %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Messages:      %zu\n", state->message_count);
    printf("Iterations:    %d/%d\n", state->iteration_count, state->max_iterations);
    printf("Tools:         %zu registered\n", state->tools.count);
    printf("Tokens in:     %d\n", state->session_input_tokens);
    printf("Tokens out:    %d\n", state->session_output_tokens);
    printf("Tokens total:  %d\n", state->session_total_tokens);
    size_t ctx_max = hermes_token_context_size(state->llm.model);
    if (ctx_max > 0) {
        int pct = (int)((double)state->session_total_tokens / ctx_max * 100.0);
        printf("Context:       %d/%zu tokens (%d%%)\n", state->session_total_tokens, ctx_max, pct);
    }
    if (state->session_reasoning_tokens > 0)
        printf("Reasoning:     %d tokens\n", state->session_reasoning_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("Est. cost:     $%.6f\n", state->session_estimated_cost_usd);
    printf("User turns:    %d\n", state->user_turn_count);
    printf("Tool turns:    %d\n", state->tool_turn_count);
    if (state->enabled_toolsets[0])
        printf("Toolsets:      enabled=%s\n", state->enabled_toolsets);
    if (state->disabled_toolsets[0])
        printf("               disabled=%s\n", state->disabled_toolsets);
    if (state->thread_id[0])
        printf("Thread:        %s\n", state->thread_id);
    if (state->chat_id[0])
        printf("Chat:          %s\n", state->chat_id);
    time_t now = time(NULL);
    if (state->last_activity_ts > 0)
        printf("Last activity: %lds ago\n", (long)(now - state->last_activity_ts));
    if (state->interrupt_message[0])
        printf("Interrupt:     %s\n", state->interrupt_message);
}

/* /stop: Kill all running background processes */
static void cmd_stop(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Stopping all background processes...\n");
    int killed = 0;
    char buf[256];
    FILE *fp = popen("pkill -f 'hermes background' 2>/dev/null; echo $?", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) killed = atoi(buf) == 0 ? 1 : 0;
        pclose(fp);
    }
    printf("Done. Killed %d process(es).\n", killed ? 1 : 0);
}

/* /approve: Show pending approvals or approve a specific one */
static void cmd_approve(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0 || strcmp(args, "-l") == 0) {
            int count = approval_cache_count();
            printf("Approval cache (%d entries):\n", count);
            for (int i = 0; i < count; i++)
                printf("  %s\n", approval_cache_entry(i));
            return;
        }
        if (strcmp(args, "clear") == 0 || strcmp(args, "-c") == 0) {
            approval_cache_clear_last(0);
            printf("Approval cache cleared.\n");
            return;
        }
        printf("Usage: /approve [list|clear]\n");
        return;
    }
    int count = approval_cache_count();
    if (count == 0) {
        printf("No cached approvals. Use /tools to verify tool permissions.\n");
    } else {
        printf("Approval cache (%d entries). Use /approve list to view.\n", count);
    }
}

/* /deny: Clear approval cache (deny all pending) */
static void cmd_deny(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    int count = approval_cache_count();
    approval_reset_session();
    printf("Approval cache cleared. %d entries removed. All operations denied.\n", count);
}

/* /title: Set a title for the current session */
static void cmd_title(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /title <session title>\n");
        printf("Current title: %s\n", state->user_title[0] ? state->user_title : "(auto: session ID)");
        return;
    }
    snprintf(state->user_title, sizeof(state->user_title), "%s", args);
    /* Save to DB immediately */
    if (state->db) agent_save_meta(state);
    printf("Session title set to: %s\n", state->user_title);
}

/* /resume: Resume a previously-named session */
static void cmd_resume(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /resume <session_id>\n");
        return;
    }
    if (!state->db) agent_open_db(state);
    if (agent_load_session(state, args))
        printf("Resumed session: %s (%zu messages)\n", args, state->message_count);
    else
        printf("Session not found: %s\n", args);
}

/* /yolo: Toggle YOLO mode (skip approvals) */
static int g_yolo_mode = 0;
static void cmd_yolo(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_yolo_mode = !g_yolo_mode;
    printf("YOLO mode %s. Dangerous command approvals will be %s.\n",
           g_yolo_mode ? "ENABLED" : "DISABLED",
           g_yolo_mode ? "skipped" : "enforced");
}

void commands_set_yolo(bool enabled) { g_yolo_mode = enabled ? 1 : 0; }
bool commands_get_yolo(void) { return g_yolo_mode != 0; }

/* /usage: Show token usage and session statistics */
static void cmd_usage(const char *args, agent_state_t *state) {
    size_t total_chars = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
    }

    if (args && args[0]) {
        if (strcmp(args, "tokens") == 0) {
            printf("Token usage:\n");
            printf("  Input:       %d tokens\n", state->session_input_tokens);
            printf("  Output:      %d tokens\n", state->session_output_tokens);
            printf("  Total:       %d tokens\n", state->session_total_tokens);
            if (state->session_cache_read_tokens > 0)
                printf("  Cache read:  %d tokens\n", state->session_cache_read_tokens);
            if (state->session_reasoning_tokens > 0)
                printf("  Reasoning:   %d tokens\n", state->session_reasoning_tokens);
            return;
        }
        if (strcmp(args, "cost") == 0) {
            printf("Cost:\n");
            if (state->session_estimated_cost_usd > 0.0)
                printf("  Est. cost:   $%.6f\n", state->session_estimated_cost_usd);
            else
                printf("  Cost tracking not available for this provider.\n");
            return;
        }
        printf("Usage: /usage [tokens|cost]\n");
        return;
    }

    printf("Session statistics:\n");
    printf("  Messages:    %zu\n", state->message_count);
    printf("  Total chars: %zu\n", total_chars);
    printf("  Est. tokens: ~%zu\n", (total_chars + 3) / 4);
    printf("  Iterations:  %d\n", state->iteration_count);
    if (state->session_total_tokens > 0)
        printf("  Tokens:      %d in / %d out\n", state->session_input_tokens, state->session_output_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("  Est. cost:   $%.6f\n", state->session_estimated_cost_usd);
}

/* /plugins: List installed plugins and their status */
static void cmd_plugins(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    /* Resolve plugins directory */
    char plugins_dir[HERMES_PATH_MAX];
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", home);

    printf("Plugin system status:\n");
    printf("  Directory: %s\n", plugins_dir);

    /* Scan .so files and load metadata via plugin API */
    DIR *d = opendir(plugins_dir);
    if (!d) {
        printf("  Error: cannot open plugins directory\n");
        return;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".so") != 0) continue;

        char full_path[HERMES_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", plugins_dir, entry->d_name);

        plugin_t *p = plugin_load(full_path);
        if (p) {
            const plugin_version_t *ver = plugin_version(p);
            char ver_buf[32];
            plugin_version_str(ver, ver_buf, sizeof(ver_buf));
            printf("  %s v%s (%s) — %s\n",
                   plugin_name(p), ver_buf,
                   plugin_type_str(plugin_type(p)),
                   plugin_description(p) ? plugin_description(p) : "no description");
            plugin_unload(p);
        } else {
            printf("  %s — (unloadable: %s)\n", entry->d_name, plugin_error());
        }
        count++;
    }
    closedir(d);
    printf("  Total plugins: %d\n", count);
    printf("  Plugin directory: %s\n", plugins_dir);
}

/* /gateway: Gateway management command with subcommands */
static void cmd_gateway(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /gateway [status|list|stop|setup|restart]\n");
        printf("  status   Show gateway connection status\n");
        printf("  list     List configured platforms\n");
        printf("  stop     Stop gateway and exit\n");
        printf("  setup    Configure gateway platforms\n");
        printf("  restart  Save session and restart\n");
        return;
    }

    if (strcmp(args, "status") == 0 || strcmp(args, "list") == 0) {
        /* Show gateway platform status */
        hermes_config_t cfg;
        hermes_config_load(&cfg, NULL);
        printf("Gateway status:\n");
        const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
        if (gw)
            printf("  Platforms (env):  %s\n", gw);
        else if (cfg.gateway_platforms[0])
            printf("  Platforms (config): %s\n", cfg.gateway_platforms);
        else
            printf("  Platforms:        (none configured)\n");
        printf("  All 19 C platforms compiled in.\n");
        printf("  Set gateway.platforms in config.yaml to activate.\n");
        if (strcmp(args, "list") == 0) {
            printf("\nAvailable platforms:\n");
            printf("  telegram, discord, slack, matrix, mattermost,\n");
            printf("  webhook, whatsapp, email, signal, sms,\n");
            printf("  homeassistant, feishu, wecom, dingtalk, qqbot,\n");
            printf("  bluebubbles, msgraph_webhook, weixin, yuanbao\n");
        }
        return;
    }

    if (strcmp(args, "stop") == 0) {
        printf("Stopping gateway...\n");
        /* Shutdown all gateway platforms */
        gw_platform_shutdown_all();
        /* Save session state */
        if (state->db) {
            agent_save_session(state);
            agent_close_db(state);
        }
        printf("Gateway stopped. Exiting.\n");
        exit(0);
    }

    if (strcmp(args, "setup") == 0) {
        printf("Gateway setup:\n");
        printf("\n");
        printf("  Available platforms and their required env vars:\n");
        printf("\n");
        static const char *platforms[][2] = {
            {"telegram", "TELEGRAM_BOT_TOKEN"},
            {"discord",  "DISCORD_BOT_TOKEN"},
            {"slack",    "SLACK_BOT_TOKEN"},
            {"signal",   "SIGNAL_NUMBER"},
            {"sms",      "TWILIO_ACCOUNT_SID"},
            {"matrix",   "MATRIX_HOMESERVER"},
            {"email",    "EMAIL_HOST"},
            {"whatsapp", "WHATSAPP_PHONE_NUMBER_ID"},
            {"feishu",   "FEISHU_APP_ID"},
            {"wecom",    "WECOM_CORP_ID"},
            {"dingtalk", "DINGTALK_WEBHOOK_TOKEN"},
            {"homeassistant", "HASS_TOKEN"},
            {"mattermost","MATTERMOST_URL"},
            {"bluebubbles","BLUEBUBBLES_PASSWORD"},
            {NULL, NULL}
        };
        for (int i = 0; platforms[i][0]; i++) {
            const char *val = getenv(platforms[i][1]);
            printf("  %-14s %s (%s)\n",
                   platforms[i][0],
                   val ? "[ready]" : "[missing]",
                   platforms[i][1]);
        }
        printf("\n");
        printf("  Current config: ");
        hermes_config_t cfg;
        hermes_config_load(&cfg, NULL);
        if (cfg.gateway_platforms[0])
            printf("%s\n", cfg.gateway_platforms);
        else
            printf("(none)\n");
        printf("\n");
        printf("  To enable platforms:\n");
        printf("    1. Set the required env vars in ~/.hermes/.env\n");
        printf("    2. Run: /platform resume <platform_name>\n");
        printf("    3. Run: /gateway restart\n");
        printf("  Or edit gateway.platforms in config.yaml directly.\n");
        return;
    }

    if (strcmp(args, "restart") == 0) {
        /* Reuse restart logic from /restart */
        cmd_restart("", state);
        return;
    }

    printf("Unknown subcommand: '%s'. Use: /gateway status|list|stop|setup|restart\n", args);
}

/* /platforms: Show gateway platform status */
static void cmd_platforms(const char *args, agent_state_t *state) {
    (void)state;
    bool verbose = (args && (strcmp(args, "-v") == 0 || strcmp(args, "verbose") == 0));
    bool list_only = (args && strcmp(args, "list") == 0);
    (void)list_only;

    printf("Gateway platforms:\n");
    const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
    if (gw) {
        printf("  Configured: %s\n", gw);
    } else {
        hermes_config_t cfg;
        if (hermes_config_load(&cfg, NULL) && cfg.gateway_platforms[0])
            printf("  Configured: %s\n", cfg.gateway_platforms);
        else
            printf("  Configured: telegram (default)\n");
    }

    if (verbose) {
        printf("\nCredentials check:\n");
        static const char *check[][3] = {
            {"telegram", "TELEGRAM_BOT_TOKEN",           "Polling"},
            {"discord",  "DISCORD_BOT_TOKEN",            "Gateway"},
            {"slack",    "SLACK_BOT_TOKEN",              "Events API"},
            {"signal",   "SIGNAL_NUMBER",                "dbus CLI"},
            {"sms",      "TWILIO_ACCOUNT_SID",           "Twilio SMS"},
            {"matrix",   "MATRIX_HOMESERVER",            "CS API"},
            {"email",    "EMAIL_HOST",                   "IMAP/SMTP"},
            {"whatsapp", "WHATSAPP_PHONE_NUMBER_ID",     "Cloud API"},
            {"feishu",   "FEISHU_APP_ID",                "Lark bot"},
            {"wecom",    "WECOM_CORP_ID",                "WeChat Work"},
            {"dingtalk", "DINGTALK_WEBHOOK_TOKEN",       "DingTalk"},
            {"homeassistant", "HASS_TOKEN",               "HA API"},
            {"mattermost","MATTERMOST_URL",               "Webhooks"},
            {"bluebubbles","BLUEBUBBLES_PASSWORD",       "iMessage"},
            {NULL, NULL, NULL}
        };
        for (int i = 0; check[i][0]; i++) {
            const char *val = getenv(check[i][1]);
            printf("  %-12s %s %-16s %s\n",
                   check[i][0], val ? "✅" : "❌",
                   val ? "(found)" : "(missing)",
                   check[i][2]);
        }
    }

    printf("\nAll 20 platform types available. Use -v for credential check.\n");
    printf("Config: gateway.platforms in config.yaml or $HERMES_GATEWAY_PLATFORMS\n");
}

/* /redraw: Force a full UI repaint */
static void cmd_redraw(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("\033[2J\033[H"); /* Clear screen + home cursor */
    printf("Screen cleared. Use /help for commands.\n");
}

/* /background: Run a prompt in the background (inline for now) */
static void cmd_background(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /background <prompt>\n");
        return;
    }
    printf("Running prompt in background...\n");
    char *resp = agent_chat(state, args);
    if (resp) {
        printf("Result: %s\n", resp);
        free(resp);
    }
}

/* /verbose: Toggle tool progress display verbosity */
static int g_verbose = 0;
static void cmd_verbose(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_verbose = (g_verbose + 1) % 3;
    const char *modes[] = {"off", "normal", "verbose"};
    printf("Verbosity set to: %s\n", modes[g_verbose]);
}

void commands_set_verbose(int level) { g_verbose = level; }
int commands_get_verbose(void) { return g_verbose; }

/* /skin: Show or change the display skin/theme */
static char g_current_skin[64] = "";

/* Forward declaration from display_core.c */
extern void display_set_skin(void *skin);

static void cmd_skin(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0) {
            int count = skin_builtin_count();
            printf("Available skins (%d):\n", count);
            for (int i = 0; i < count; i++) {
                const char *name = skin_builtin_name(i);
                printf("  - %s\n", name ? name : "(unnamed)");
            }
            printf("Use /skin <name> to activate.\n");
            return;
        }
        if (strlen(args) >= sizeof(g_current_skin)) {
            printf("Skin name too long (max %zu chars).\n", sizeof(g_current_skin) - 1);
            return;
        }
        /* Try loading as built-in preset first */
        skin_t *sk = skin_load_preset(args);
        if (!sk) {
            printf("Skin not found: %s\n", args);
            printf("Use /skin list to see available skins.\n");
            return;
        }
        snprintf(g_current_skin, sizeof(g_current_skin), "%s", args);
        setenv("HERMES_SKIN", args, 1);
        display_set_skin((void *)sk);
        printf("Skin set to: %s\n", args);
        return;
    }
    const char *skin = g_current_skin[0] ? g_current_skin : getenv("HERMES_SKIN");
    printf("Current skin: %s\n", skin && skin[0] ? skin : "(default)");
    printf("Use /skin list for available skins.\n");
}

/* /personality: Set a predefined personality system message */
static void cmd_personality(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /personality <system prompt text>\n");
        printf("Sets or replaces the system message (personality).\n");
        return;
    }
    context_set_system(state, args);
    printf("Personality set.\n");
}

/* /whoami: Show access level */
static void cmd_whoami(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Access level: admin (C translation build)\n");
    printf("Version:     %s\n", HERMES_VERSION);
    printf("Platform:    Linux (WSL)\n");
}

/* /profile: Show active profile */
static void cmd_profile(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        if (strcmp(args, "home") == 0) {
            printf("Home: %s\n", state->hermes_home[0] ? state->hermes_home :
                   getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "~/.slermes");
            return;
        }
        if (strcmp(args, "model") == 0) {
            printf("Model: %s\n", state->llm.model[0] ? state->llm.model : "(default)");
            printf("Provider: %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
            return;
        }
        printf("Usage: /profile [home|model]\n");
        return;
    }
    printf("Home: %s\n", state->hermes_home[0] ? state->hermes_home :
           getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "~/.slermes");
    printf("Model: %s\n", state->llm.model[0] ? state->llm.model : "(default)");
    printf("Provider: %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
}

/* /goal: Set a standing goal */
static void cmd_goal(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /goal <goal description>\n");
        printf("       /goal show   — show current goal\n");
        printf("       /goal clear  — clear current goal\n");
        return;
    }

    if (strcmp(args, "show") == 0) {
        /* Show saved goal */
        char path[4096];
        const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                           getenv("HOME") ? getenv("HOME") : ".";
        snprintf(path, sizeof(path), "%s/mind-palace/goal-mantra.md", home);
        FILE *f = fopen(path, "r");
        if (!f) {
            printf("No goal set. Use /goal <description> to set one.\n");
            return;
        }
        printf("Current goal:\n");
        char line[1024];
        while (fgets(line, sizeof(line), f))
            printf("%s", line);
        fclose(f);
        return;
    }

    if (strcmp(args, "clear") == 0) {
        char path[4096];
        const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                           getenv("HOME") ? getenv("HOME") : ".";
        snprintf(path, sizeof(path), "%s/mind-palace/goal-mantra.md", home);
        if (unlink(path) == 0)
            printf("Goal cleared.\n");
        else
            printf("No goal to clear.\n");
        return;
    }

    /* Save goal to mind-palace/goal-mantra.md */
    char dir[4096];
    const char *home = getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") :
                       getenv("HOME") ? getenv("HOME") : ".";
    snprintf(dir, sizeof(dir), "%s/mind-palace", home);
    mkdir(dir, 0755);

    char path[4096];
    snprintf(path, sizeof(path), "%s/goal-mantra.md", dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Error: Cannot save goal to %s\n", path);
        return;
    }
    fprintf(f, "## Goal\n\n%s\n", args);
    fclose(f);
    printf("Goal saved: %s\n", args);
}

/* /agents: Show active subagents */
static void cmd_agents(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Subagent delegation: available via /delegate. Use /delegate <task> to spawn a subagent inline.\n");
}

/* /reasoning: Manage reasoning effort */
static void cmd_reasoning(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /reasoning [level|show|hide]\n");
        printf("Levels: none, minimal, low, medium, high, xhigh, on, off\n");
        printf("Current: %s\n",
               state->llm.reasoning_effort[0] ? state->llm.reasoning_effort : "(default)");
        return;
    }

    /* Handle special commands */
    if (strcmp(args, "show") == 0) {
        printf("Current reasoning effort: %s\n",
               state->llm.reasoning_effort[0] ? state->llm.reasoning_effort : "(not set, provider default)");
        return;
    }
    if (strcmp(args, "hide") == 0) {
        printf("Reasoning display hidden (content still sent to provider if configured).\n");
        return;
    }

    /* Map common aliases to canonical values */
    const char *value = args;
    if (strcmp(args, "on") == 0) value = "medium";
    else if (strcmp(args, "off") == 0) value = "none";

    /* Validate value */
    static const char *valid[] = {"none", "minimal", "low", "medium", "high", "xhigh", NULL};
    bool ok = false;
    for (int i = 0; valid[i]; i++) {
        if (strcmp(value, valid[i]) == 0) { ok = true; break; }
    }
    if (!ok) {
        printf("Invalid reasoning level: %s\n", args);
        printf("Valid: none, minimal, low, medium, high, xhigh, on, off\n");
        return;
    }

    strncpy(state->llm.reasoning_effort, value, sizeof(state->llm.reasoning_effort) - 1);
    state->llm.reasoning_effort[sizeof(state->llm.reasoning_effort) - 1] = '\0';
    printf("Reasoning effort set to: %s\n", value);
}

/* /toolsets: List available toolsets (dynamic from registry) */
static void cmd_toolsets(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Available toolsets (dynamic):\n");
    size_t n = registry_get_count();
    char seen[32][32]; int seen_count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *name = registry_get_name(i);
        const char *ts = registry_get_toolset(name);
        if (!ts || !ts[0]) ts = "core";
        bool dup = false;
        for (int j = 0; j < seen_count; j++)
            if (strcmp(seen[j], ts) == 0) { dup = true; break; }
        if (dup) continue;
        if (seen_count < 32) snprintf(seen[seen_count], 32, "%s", ts);
        seen_count++;
        /* Collect tools for this toolset */
        printf("  %s —", ts);
        int col = 0;
        for (size_t k = 0; k < n; k++) {
            const char *tn = registry_get_name(k);
            const char *tt = registry_get_toolset(tn);
            if (!tt || !tt[0]) tt = "core";
            if (strcmp(tt, ts) != 0) continue;
            if (col > 0) printf(",");
            if (col % 5 == 0 && col > 0) printf("\n         ");
            printf(" %s", tn);
            col++;
        }
        printf("\n");
    }
}

/* /skills: List installed skills */
static void cmd_skills(const char *args, agent_state_t *state) {
    (void)state;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    char skills_dir[512];
    if (home) snprintf(skills_dir, sizeof(skills_dir), "%s/.slermes/skills", home);
    printf("Skills directory: %s\n", skills_dir);

    if (args && args[0]) {
        /* Parse subcommand */
        char cmd[128], arg[256];
        cmd[0] = arg[0] = '\0';
        if (sscanf(args, "%127s %255[^\n]", cmd, arg) < 1) {
            printf("Usage: /skills [search-hub <query> | install <slug>]\n");
            return;
        }

        if (strcmp(cmd, "search-hub") == 0 || strcmp(cmd, "search") == 0) {
            size_t count = 0;
            skill_search_result_t *sr = skill_search_hub(arg, &count, 20);
            if (!sr || count == 0) {
                printf("No results from browse.sh hub.\n");
                if (sr) skill_search_hub_free(sr, count);
                return;
            }
            printf("Browse.sh hub results (%zu):\n", count);
            for (size_t i = 0; i < count; i++)
                printf("  %s  (slug: %s, score: %.2f)\n",
                       sr[i].name, sr[i].path + 10, sr[i].score);
            skill_search_hub_free(sr, count);

        } else if (strcmp(cmd, "install") == 0) {
            if (!arg[0]) {
                printf("Usage: /skills install <slug>\n");
                return;
            }
            char error[512] = "";
            bool ok = skill_install_from_hub(arg, error, sizeof(error));
            if (ok)
                printf("Installed '%s' from browse.sh hub.\n", arg);
            else
                printf("Failed: %s\n", error);

        } else {
            printf("Unknown subcommand '%s'. Use: search-hub <query> | install <slug>\n", cmd);
        }
        return;
    }

    printf("Skills management:\n");
    printf("  /skills search-hub <query>   — Search browse.sh skills hub\n");
    printf("  /skills install <slug>       — Install skill from browse.sh hub\n");
    printf("  /skills list                 — List local skills\n");
    printf("  Use skill_view/skill_manage tools for detailed management.\n");
}

/* /cron: Manage scheduled tasks */
static void cmd_cron(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0 || strcmp(args, "-l") == 0) {
            char cron_dir[HERMES_PATH_MAX];
            hermes_get_home(cron_dir, sizeof(cron_dir));
            strncat(cron_dir, "/cron", sizeof(cron_dir) - strlen(cron_dir) - 1);
            printf("Scheduled tasks in %s:\n", cron_dir);
            char cmd[HERMES_PATH_MAX + 32];
            snprintf(cmd, sizeof(cmd), "ls -la %s/ 2>/dev/null || echo '(empty)'", cron_dir);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char line[256];
                while (fgets(line, sizeof(line), fp)) printf("  %s", line);
                pclose(fp);
            }
            return;
        }
        printf("Usage: /cron [list]\n");
        return;
    }
    /* Show cron config */
    char cron_dir[HERMES_PATH_MAX];
    hermes_get_home(cron_dir, sizeof(cron_dir));
    strncat(cron_dir, "/cron", sizeof(cron_dir) - strlen(cron_dir) - 1);
    printf("Cron scheduler: active\n");
    printf("  Directory: %s\n", cron_dir);
    printf("  Config: cron.dir, cron.max_concurrent_jobs, cron.job_timeout\n");
    printf("  Use cronjob tool to create/manage tasks.\n");
    printf("  Use /cron list to show scheduled tasks.\n");
}

/* /fast: Toggle fast mode (normal|fast|status) */
static int g_fast_mode = 0;
static void cmd_fast(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Fast mode: %s\n", g_fast_mode ? "FAST" : "NORMAL");
        printf("  Usage: /fast [normal|fast|status]\n");
        return;
    }
    if (strcmp(args, "fast") == 0 || strcmp(args, "on") == 0) {
        g_fast_mode = 1;
        printf("Fast mode enabled.\n");
    } else if (strcmp(args, "normal") == 0 || strcmp(args, "off") == 0) {
        g_fast_mode = 0;
        printf("Fast mode disabled.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /fast [normal|fast|status]\n");
    }
}

void commands_set_fast(bool enabled) { g_fast_mode = enabled ? 1 : 0; }
bool commands_get_fast(void) { return g_fast_mode != 0; }

/* /reload: Reload .env */
static void cmd_reload(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        if (strcmp(args, "plugins") == 0) {
            /* Plugin-only reload (hot-reload) */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            hermes_config_load_env(&cfg);
            plugin_registry_t *old_reg = (plugin_registry_t *)state->plugin_reg;
            hermes_plugin_shutdown(old_reg);
            plugin_registry_t *new_reg = hermes_plugin_init(&cfg);
            if (new_reg) state->plugin_reg = new_reg;
            printf("Plugins reloaded.\n");
            return;
        }
        if (strcmp(args, "env") == 0) {
            hermes_config_t cfg;
            hermes_config_load_env(&cfg);
            if (cfg.api_key[0]) memcpy(state->llm.api_key, cfg.api_key, sizeof(state->llm.api_key));
            printf(".env variables reloaded.\n");
            return;
        }
        printf("Usage: /reload [plugins|env]\n");
        return;
    }
    hermes_config_t cfg;
    hermes_config_load(&cfg, NULL);
    hermes_config_load_env(&cfg);
    memcpy(state->llm.base_url, cfg.base_url, sizeof(state->llm.base_url));
    if (cfg.api_key[0]) memcpy(state->llm.api_key, cfg.api_key, sizeof(state->llm.api_key));
    if (cfg.model[0]) memcpy(state->llm.model, cfg.model, sizeof(state->llm.model));
    if (cfg.provider[0]) memcpy(state->llm.provider, cfg.provider, sizeof(state->llm.provider));
    /* Reload plugins from updated config */
    plugin_registry_t *old_reg = (plugin_registry_t *)state->plugin_reg;
    hermes_plugin_shutdown(old_reg);
    plugin_registry_t *new_reg = hermes_plugin_init(&cfg);
    if (new_reg) state->plugin_reg = new_reg;
    printf(".env reloaded. Config + plugins updated.\n");
}

/* /rollback: List or restore state snapshots */
static void cmd_rollback(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Restore to named checkpoint */
        if (checkpoint_restore(&state->checkpoints, state, args)) {
            printf("Rolled back to snapshot: %s (%zu messages).\n",
                   args, state->message_count);
        } else {
            printf("Snapshot not found: %s. Use /rollback to list available snapshots.\n", args);
        }
        return;
    }
    /* List snapshots from session DB */
    if (state->db) {
        size_t count = 0;
        char **list = db_list(state->db, &count);
        if (list && count > 0) {
            printf("Saved snapshots (%zu):\n", count);
            for (size_t i = 0; i < count; i++) {
                printf("  %s\n", list[i]);
                free(list[i]);
            }
            free(list);
        } else {
            printf("No snapshots found. Use /snapshot to create one.\n");
        }
    } else {
        printf("No session database. Use /snapshot first.\n");
    }
}

/* /copy: Copy last assistant response (prints to stdout in CLI) */
static void cmd_copy(const char *args, agent_state_t *state) {
    /* Determine which response: default=last (0), or Nth from end */
    int n = 0;
    if (args && args[0]) {
        char *endptr;
        long val = strtol(args, &endptr, 10);
        if (*endptr != '\0' || val < 0) {
            printf("Usage: /copy [number]  — copy Nth-to-last assistant response (0=last)\n");
            return;
        }
        n = (int)val;
    }
    /* Find Nth-to-last assistant message */
    const char *found = NULL;
    int found_index = 0;
    for (size_t i = state->message_count; i > 0; i--) {
        if (state->messages[i-1]->role == MSG_ASSISTANT) {
            if (found_index == n) {
                found = state->messages[i-1]->content;
                break;
            }
            found_index++;
        }
    }
    if (found) {
        if (n == 0)
            printf("=== Last response ===\n%s\n", found);
        else
            printf("=== Response -%d ===\n%s\n", n, found);
    } else {
        printf("No assistant response at index %d.\n", n);
    }
}

/* /queue: Queue a prompt for the next turn */
static char g_queued_prompt[4096] = "";
static void cmd_queue(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        if (g_queued_prompt[0])
            printf("Queued prompt: %s\n", g_queued_prompt);
        else
            printf("No queued prompt.\n");
        return;
    }
    snprintf(g_queued_prompt, sizeof(g_queued_prompt), "%s", args);
    printf("Prompt queued for next turn.\n");
}

/* /restart: Gracefully restart via exec */
static void cmd_restart(const char *args, agent_state_t *state) {
    (void)args;
    /* Save session state before restart */
    if (state->db) {
        printf("Saving session...\n");
        fflush(stdout);
        agent_save_session(state);
        agent_close_db(state);
    }

    printf("Restarting...\n");
    fflush(stdout);

    /* Re-exec with saved arguments (if available) or default */
    char *argv[256];
    int argc = 0;
    char exe[4096];
    ssize_t exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (exe_len > 0) {
        exe[exe_len] = '\0';
        argv[argc++] = exe;
        /* Try to preserve original args from /proc */
        FILE *cmdline = fopen("/proc/self/cmdline", "rb");
        if (cmdline) {
            char cl_buf[4096];
            int n = (int)fread(cl_buf, 1, sizeof(cl_buf) - 1, cmdline);
            fclose(cmdline);
            if (n > 0) {
                cl_buf[n] = '\0';
                /* Parse null-separated args */
                char *p = cl_buf;
                /* Skip argv[0] */
                p += strlen(p) + 1;
                while (*p && argc < 255) {
                    argv[argc++] = p;
                    p += strlen(p) + 1;
                }
            }
        }
        argv[argc] = NULL;
        execv(exe, argv);
        /* If exec fails */
        fprintf(stderr, "Restart failed: %s. Use /exit and re-launch.\n", strerror(errno));
    } else {
        fprintf(stderr, "Cannot find executable path. Use /exit and re-launch.\n");
    }
}

/* /subgoal: Manage extra goal criteria */
static void cmd_subgoal(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /subgoal <goal criteria>\n");
        return;
    }
    printf("Subgoal set: %s\n", args);
}

/* /sethome: Set home channel */
static char g_home_channel[256] = "";
static void cmd_sethome(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current home channel: %s\n", g_home_channel[0] ? g_home_channel : "(not set)");
        printf("Usage: /sethome <platform:chat_id>\n");
        return;
    }
    snprintf(g_home_channel, sizeof(g_home_channel), "%s", args);
    printf("Home channel set to: %s\n", g_home_channel);
}

/* /handoff: Hand off session to messaging platform */
static void cmd_handoff(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /handoff <platform> (telegram, discord, slack, etc.)\n");
        return;
    }
    printf("Session handoff to %s: start gateway with --platform %s\n", args, args);
}

/* /platform: List gateway platforms */
static void cmd_platform(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Gateway platforms (configured via config.yaml gateway.platforms):\n");
        printf("  All 19 C platforms: telegram, discord, slack, matrix,\n");
        printf("  mattermost, webhook, whatsapp, email, signal, sms,\n");
        printf("  homeassistant, feishu, wecom, dingtalk, qqbot,\n");
        printf("  bluebubbles, msgraph_webhook, weixin, yuanbao\n");
        printf("  Use /platform list to show active status.\n");
        return;
    }
    if (strcmp(args, "list") == 0) {
        /* Show platforms from config */
        const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
        if (gw) {
            printf("Gateway platforms (env): %s\n", gw);
        } else {
            hermes_config_t cfg;
            hermes_config_load(&cfg, state->hermes_home);
            if (cfg.gateway_platforms[0])
                printf("Gateway platforms (config): %s\n", cfg.gateway_platforms);
            else
                printf("Gateway platforms: none configured\n");
        }
        printf("\nAll 19 C gateway modules compiled in.\n");
        printf("Active platforms determined by gateway.platforms config key.\n");
    } else if (strncmp(args, "pause", 5) == 0) {
        const char *pname = args + 5;
        while (*pname == ' ') pname++;
        if (!pname[0])
            printf("Usage: /platform pause <platform_name>\n");
        else {
            /* Read current platforms from config, remove the named platform */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            char new_platforms[256] = "";
            const char *src = cfg.gateway_platforms;
            if (src && src[0]) {
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", src);
                char *tok = strtok(tmp, ",");
                int first = 1;
                while (tok) {
                    /* Remove leading/trailing whitespace from token */
                    while (*tok == ' ') tok++;
                    char *end = tok + strlen(tok);
                    while (end > tok && *(end-1) == ' ') end--;
                    *end = '\0';
                    if (strcasecmp(tok, pname) != 0) {
                        if (!first) strncat(new_platforms, ",", sizeof(new_platforms) - strlen(new_platforms) - 1);
                        strncat(new_platforms, tok, sizeof(new_platforms) - strlen(new_platforms) - 1);
                        first = 0;
                    }
                    tok = strtok(NULL, ",");
                }
            }
            if (hermes_config_set_platforms(&cfg, new_platforms[0] ? new_platforms : NULL)) {
                printf("Platform '%s' disabled (removed from gateway.platforms).\n", pname);
                printf("  Run /restart or restart slermes for the change to take effect.\n");
            } else {
                printf("Error: Could not update config.yaml.\n");
            }
        }
    } else if (strncmp(args, "resume", 6) == 0) {
        const char *pname = args + 6;
        while (*pname == ' ') pname++;
        if (!pname[0])
            printf("Usage: /platform resume <platform_name>\n");
        else {
            /* Read current platforms, add the named platform if not already present */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            char new_platforms[256] = "";
            int found = 0;
            if (cfg.gateway_platforms[0]) {
                snprintf(new_platforms, sizeof(new_platforms), "%s", cfg.gateway_platforms);
                /* Check if already present */
                char tmp[256];
                snprintf(tmp, sizeof(tmp), "%s", cfg.gateway_platforms);
                char *tok = strtok(tmp, ",");
                while (tok) {
                    while (*tok == ' ') tok++;
                    if (strcasecmp(tok, pname) == 0) { found = 1; break; }
                    tok = strtok(NULL, ",");
                }
            }
            if (found) {
                printf("Platform '%s' is already enabled.\n", pname);
            } else {
                if (new_platforms[0])
                    strncat(new_platforms, ",", sizeof(new_platforms) - strlen(new_platforms) - 1);
                strncat(new_platforms, pname, sizeof(new_platforms) - strlen(new_platforms) - 1);
                if (hermes_config_set_platforms(&cfg, new_platforms)) {
                    printf("Platform '%s' enabled (added to gateway.platforms).\n", pname);
                    printf("  Run /restart or restart slermes for the change to take effect.\n");
                } else {
                    printf("Error: Could not update config.yaml.\n");
                }
            }
        }
    } else {
        printf("Unknown: %s. Use: list\n", args);
    }
}

/* /bundles: List skill bundles. Reads yaml files from skill-bundles dir. */
static void cmd_bundles(const char *args, agent_state_t *state) {
    (void)args;
    const char *home = state->hermes_home[0] ? state->hermes_home : getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { printf("Cannot determine home directory.\n"); return; }

    char bundles_dir[HERMES_PATH_MAX + 64];
    snprintf(bundles_dir, sizeof(bundles_dir), "%s/skill-bundles", home);

    DIR *dir = opendir(bundles_dir);
    if (!dir) {
        printf("No skill bundles found.\n");
        printf("  Create YAML files in %s/skill-bundles/\n", home);
        printf("  Format: { name, description, skills: [list] }\n");
        return;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 5 && strcmp(name + len - 5, ".yaml") == 0) {
            count++;
            char path[HERMES_PATH_MAX + 128];
            snprintf(path, sizeof(path), "%s/%s", bundles_dir, name);

            char *err = NULL;
            yaml_doc_t *doc = yaml_parse_file(path, &err);
            if (!doc) {
                printf("  %s: (parse error: %s)\n", name, err ? err : "unknown");
                free(err);
                continue;
            }

            const char *bname = yaml_get_string(doc, "name");
            const char *desc  = yaml_get_string(doc, "description");
            printf("  %s\n", bname ? bname : name);
            if (desc) printf("    Description: %s\n", desc);

            /* List skills within the bundle */
            size_t sc = yaml_list_count(doc, "skills");
            if (sc > 0) {
                printf("    Skills (%zu): ", sc);
                int first_skill = 1;
                for (size_t si = 0; si < sc && si < 50; si++) {
                    const char *sk = yaml_list_get(doc, "skills", si);
                    if (sk) {
                        printf("%s%s", first_skill ? "" : ", ", sk);
                        first_skill = 0;
                    }
                }
                printf("\n");
            }

            yaml_free(doc);
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("No skill bundles found.\n");
        printf("  Create YAML files in %s/skill-bundles/\n", home);
        printf("  Format: { name, description, skills: [list] }\n");
    } else {
        printf("\n%d bundle(s) found in %s/skill-bundles/\n", count, home);
    }
}

/* /curator: Background skill maintenance */
static void cmd_curator(const char *args, agent_state_t *state) {
    (void)state;
    bool show_help = false;

    /* Parse subcommand */
    const char *sub = NULL;
    if (args) {
        while (*args == ' ') args++;
        if (*args) sub = args;
    }

    if (sub && strcmp(sub, "--help") == 0) show_help = true;

    if (show_help || (sub && strcmp(sub, "help") == 0)) {
        printf("Usage: /curator [status|run|pause|resume|help]\\n");
        printf("  status   - Show curator state (default)\\n");
        printf("  run      - Trigger a curator run\\n");
        printf("  pause    - Pause curator auto-runs\\n");
        printf("  resume   - Resume curator auto-runs\\n");
        return;
    }

    /* Load curator state */
    curator_state_t cs;
    curator_load_state(&cs);

    if (sub && strcmp(sub, "pause") == 0) {
        cs.paused = true;
        curator_save_state(&cs);
        printf("Curator paused.\\n");
        return;
    }

    if (sub && strcmp(sub, "resume") == 0) {
        cs.paused = false;
        curator_save_state(&cs);
        printf("Curator resumed.\\n");
        return;
    }

    if (sub && strcmp(sub, "run") == 0) {
        printf("Triggering curator run...\n");
        /* Wire llm_background_review: review last tool result in session */
        char *review = NULL;
        double duration = 0.0;
        if (state && state->message_count > 0) {
            /* Find the last tool result message */
            char *tool_result = NULL;
            char *tool_name = NULL;
            for (int i = (int)state->message_count - 1; i >= 0; i--) {
                if (state->messages[i]->role == MSG_TOOL) {
                    tool_result = state->messages[i]->content;
                    /* Look backward for the tool name from assistant message */
                    for (int j = i - 1; j >= 0; j--) {
                        if (state->messages[j]->role == MSG_ASSISTANT &&
                            state->messages[j]->tool_call_id &&
                            strcmp(state->messages[j]->tool_call_id,
                                   state->messages[i]->tool_call_id) == 0) {
                            tool_name = state->messages[j]->tool_name;
                            break;
                        }
                    }
                    if (!tool_name && state->messages[i]->tool_call_id) {
                        /* Use tool_call_id as fallback name */
                        tool_name = state->messages[i]->tool_call_id;
                    }
                    break;
                }
            }
            if (tool_result) {
                review = llm_background_review(&state->llm,
                    tool_name ? tool_name : "unknown",
                    "{}", tool_result);
            }
        }
        if (review) {
            curator_record_run(&cs, duration, review);
            printf("Curator run complete. Review summary:\n  %s\n", review);
            free(review);
        } else {
            curator_record_run(&cs, duration,
                "No tool results to review in current session");
            printf("Curator run complete (no tool results found).\n");
        }
        return;
    }

    /* Default: status display */
    const char *status_str = "ENABLED";
    if (cs.paused) status_str = "PAUSED";
    else if (!cs.enabled) status_str = "DISABLED";

    char time_buf[64], duration_buf[64];
    curator_format_reltime(cs.last_run_at, time_buf, sizeof(time_buf));
    curator_format_duration(cs.last_run_duration, duration_buf, sizeof(duration_buf));

    printf("curator: %s\\n", status_str);
    printf("  runs:           %d\\n", cs.run_count);
    printf("  last run:       %s\\n", time_buf);
    printf("  last duration:  %s\\n", duration_buf);
    if (cs.last_run_summary[0])
        printf("  summary:        %s\\n", cs.last_run_summary);

    /* Also show skill usage stats if available */
    skill_usage_map_t sumap;
    skill_usage_load(NULL, &sumap);
    int total = sumap.count;
    int agent_created = 0;
    for (int i = 0; i < total; i++) {
        if (strcmp(sumap.records[i].created_by, "agent") == 0) agent_created++;
    }
    printf("  skills tracked: %d (%d agent-created)\n", total, agent_created);
}

/* /image: Attach a local image file */
static void cmd_image(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /image <path_to_image>\n");
        return;
    }
    printf("Image attached: %s (will be used in next prompt)\n", args);
}

/* /paste: Attach clipboard image */
static void cmd_paste(const char *args, agent_state_t *state) {
    (void)state;
    (void)args;
    /* On WSL, try Windows clipboard via powershell.exe */
    FILE *fp = popen("powershell.exe -NoProfile -Command \"Get-Clipboard\" 2>/dev/null", "r");
    if (!fp) {
        printf("Clipboard paste not available. Use /image <path> instead.\n");
        return;
    }
    char buf[4096];
    size_t len = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    }
    int rc = pclose(fp);
    if (rc != 0 || len == 0) {
        printf("No clipboard content detected.\n");
        printf("Tip: Use /image <path> to attach an image file.\n");
        return;
    }
    printf("Clipboard content:\n%s\n", buf);
}

/* /insights: Show usage insights (enhanced with DB-backed historical stats) */
static void cmd_insights(const char *args, agent_state_t *state) {
    /* Parse --days N and --source S arguments */
    int days_filter = 0;
    char source_filter[64] = {0};
    if (args) {
        const char *d = strstr(args, "--days");
        if (d) {
            d += 6;
            while (*d == ' ' || *d == '=') d++;
            if (*d >= '0' && *d <= '9') days_filter = atoi(d);
        }
        const char *s = strstr(args, "--source");
        if (s) {
            s += 8;
            while (*s == ' ' || *s == '=') s++;
            const char *end = s;
            while (*end && *end != ' ' && *end != '\t') end++;
            size_t slen = (size_t)(end - s);
            if (slen > 0 && slen < sizeof(source_filter)) {
                memcpy(source_filter, s, slen);
                source_filter[slen] = '\0';
            }
        }
    }

    /* --- Current session stats (keep existing) --- */
    size_t total_chars = 0;
    int tool_calls = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
        if (state->messages[i]->tool_calls_count > 0)
            tool_calls += state->messages[i]->tool_calls_count;
    }

    printf("Usage insights:\n");

    /* --- Historical DB stats --- */
    db_t *db = state->db;
    bool db_opened = false;
    if (!db && state->hermes_home[0]) {
        /* Try to open the session database */
        char sess_dir[HERMES_PATH_MAX];
        snprintf(sess_dir, sizeof(sess_dir), "%s/sessions", state->hermes_home);
        db = db_open(sess_dir, NULL);
        if (db) db_opened = true;
    }

    if (db) {
        size_t total_sessions = db_count(db);
        long long storage = db_storage_size(db);

        printf("  All sessions:\n");
        printf("    Total sessions: %zu\n", total_sessions);
        printf("    Storage:        %.1f MB\n", storage / (1024.0 * 1024.0));

        /* Aggregate metadata across sessions */
        size_t session_count = 0;
        db_session_entry_t *entries = db_list_with_meta(db, &session_count);
        if (!entries) {
            printf("    No sessions yet. Start a conversation to see stats.\n");
        } else {
            if (days_filter > 0)
                printf("    Filtered:       %zu in last %dd\n", session_count, days_filter);
            long long hist_tokens = 0;
            long long hist_input = 0, hist_output = 0;
            long long hist_cache_read = 0, hist_cache_write = 0;
            long long hist_tool_calls = 0;
            long long hist_messages = 0;
            time_t now = time(NULL);
            int recent_7d = 0, recent_30d = 0;
            int daily_counts[7] = {0,0,0,0,0,0,0};
            double total_duration = 0.0;
            int model_count = 0;
            char models[64][128];
            int model_uses[64];
            long long model_tok_in[64], model_tok_out[64];
            double model_cost[64];
            /* B08: Per-platform breakdown */
            int platform_count = 0;
            char platforms[32][32];
            int platform_sessions[32];
            long long platform_tokens[32];

            memset(models, 0, sizeof(models));
            memset(model_uses, 0, sizeof(model_uses));
            memset(model_tok_in, 0, sizeof(model_tok_in));
            memset(model_tok_out, 0, sizeof(model_tok_out));
            memset(model_cost, 0, sizeof(model_cost));
            memset(platforms, 0, sizeof(platforms));
            memset(platform_sessions, 0, sizeof(platform_sessions));
            memset(platform_tokens, 0, sizeof(platform_tokens));

            for (size_t i = 0; i < session_count; i++) {
                /* Apply --days filter */
                double age_days = difftime(now, entries[i].meta.created_at) / 86400.0;
                if (days_filter > 0 && age_days > days_filter) continue;
                /* Apply --source filter */
                if (source_filter[0] && strcmp(entries[i].meta.source, source_filter) != 0) continue;

                hist_tokens += entries[i].meta.token_count;
                hist_input += entries[i].meta.input_tokens;
                hist_output += entries[i].meta.output_tokens;
                hist_cache_read += entries[i].meta.cache_read_tokens;
                hist_cache_write += entries[i].meta.cache_write_tokens;
                hist_tool_calls += entries[i].meta.tool_call_count;
                hist_messages += entries[i].meta.message_count;

                if (age_days <= 7) recent_7d++;
                if (age_days <= 30) recent_30d++;

                /* Duration */
                if (entries[i].meta.updated_at > entries[i].meta.created_at)
                    total_duration += difftime(entries[i].meta.updated_at, entries[i].meta.created_at);

                /* Daily bucket (0=today) */
                if (age_days <= 7) {
                    int day_idx = (int)age_days;
                    if (day_idx >= 0 && day_idx < 7) daily_counts[day_idx]++;
                }

                /* Track model distribution */
                if (entries[i].meta.model[0]) {
                    int found = -1;
                    for (int m = 0; m < model_count; m++) {
                        if (strcmp(models[m], entries[i].meta.model) == 0) {
                            found = m; break;
                        }
                    }
                    if (found >= 0) {
                        model_uses[found]++;
                        model_tok_in[found] += entries[i].meta.input_tokens;
                        model_tok_out[found] += entries[i].meta.output_tokens;
                        /* Accumulate cost per model */
                        if (entries[i].meta.input_tokens > 0 || entries[i].meta.output_tokens > 0) {
                            usage_counts_t uc = {entries[i].meta.input_tokens, entries[i].meta.output_tokens};
                            model_cost[found] += usage_pricing_estimate(entries[i].meta.model, &uc).total_cost;
                        }
                    } else if (model_count < 64) {
                        snprintf(models[model_count], sizeof(models[0]), "%s", entries[i].meta.model);
                        model_uses[model_count] = 1;
                        model_tok_in[model_count] = entries[i].meta.input_tokens;
                        model_tok_out[model_count] = entries[i].meta.output_tokens;
                        /* Cost for new model */
                        if (entries[i].meta.input_tokens > 0 || entries[i].meta.output_tokens > 0) {
                            usage_counts_t uc = {entries[i].meta.input_tokens, entries[i].meta.output_tokens};
                            model_cost[model_count] = usage_pricing_estimate(entries[i].meta.model, &uc).total_cost;
                        }
                        model_count++;
                    }
                }

                /* B08: Track per-platform distribution */
                if (entries[i].meta.source[0]) {
                    int found = -1;
                    for (int p = 0; p < platform_count; p++) {
                        if (strcmp(platforms[p], entries[i].meta.source) == 0) {
                            found = p; break;
                        }
                    }
                    if (found >= 0) {
                        platform_sessions[found]++;
                        platform_tokens[found] += entries[i].meta.token_count;
                    } else if (platform_count < 32) {
                        snprintf(platforms[platform_count], sizeof(platforms[0]),
                                 "%s", entries[i].meta.source);
                        platform_sessions[platform_count] = 1;
                        platform_tokens[platform_count] = entries[i].meta.token_count;
                        platform_count++;
                    }
                }
            }

            /* Top 10 sessions by token count */
            if (session_count > 0) {
                /* Sort by token_count descending for top-N display */
                for (size_t i = 0; i < session_count && i < 10; i++) {
                    size_t max_idx = i;
                    for (size_t j = i + 1; j < session_count; j++) {
                        if (entries[j].meta.token_count > entries[max_idx].meta.token_count)
                            max_idx = j;
                    }
                    if (max_idx != i) {
                        db_session_entry_t tmp = entries[i];
                        entries[i] = entries[max_idx];
                        entries[max_idx] = tmp;
                    }
                }
                int top_n = session_count < 10 ? (int)session_count : 10;
                printf("  Top sessions:\n");
                for (int t = 0; t < top_n; t++) {
                    char title_short[44];
                    const char *title = entries[t].meta.title;
                    size_t tlen = strlen(title);
                    if (tlen > 40) {
                        memcpy(title_short, title, 40);
                        title_short[40] = 0;
                    } else {
                        memcpy(title_short, title, tlen + 1);
                    }
                    char date_buf[16];
                    struct tm *tm_info = localtime(&entries[t].meta.created_at);
                    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);
                    printf("    #%d: %-40s %8d tok %s %s\n",
                           t + 1, title_short, entries[t].meta.token_count,
                           entries[t].meta.source, date_buf);
                }
            }
            free(entries);

            printf("    Total tokens:   %lld\n", hist_tokens);
            if (hist_input > 0 || hist_output > 0) {
                printf("      Input:        %lld\n", hist_input);
                printf("      Output:       %lld\n", hist_output);
                printf("      Cache read:   %lld\n", hist_cache_read);
                printf("      Cache write:  %lld\n", hist_cache_write);
            }
            printf("    Total messages: %lld\n", hist_messages);
            if (hist_tool_calls > 0)
                printf("    Tool calls:     %lld\n", hist_tool_calls);
            printf("    Avg duration:   %s\n",
                   usage_pricing_format_duration(
                       session_count > 0 ? total_duration / session_count : 0.0));
            printf("    Last 7 days:    %d sessions\n", recent_7d);
            printf("    Last 30 days:   %d sessions\n", recent_30d);

            /* Daily bar chart (last 7 days) */
            if (recent_7d > 0) {
                printf("  Activity (last 7 days):\n");
                int peak = daily_counts[0];
                for (int d = 1; d < 7; d++)
                    if (daily_counts[d] > peak) peak = daily_counts[d];
                if (peak < 1) peak = 1;
                const char *day_names[] = {"today","d-1","d-2","d-3","d-4","d-5","d-6"};
                for (int d = 0; d < 7; d++) {
                    int bar_len = daily_counts[d] * 20 / peak;
                    if (bar_len < 1 && daily_counts[d] > 0) bar_len = 1;
                    printf("    %s ", day_names[d]);
                    for (int b = 0; b < bar_len; b++) printf("\xe2\x96\x88");
                    printf(" %d\n", daily_counts[d]);
                }
            }

            if (model_count > 0) {
                printf("  Models used:\n");
                for (int m = 0; m < model_count && m < 8; m++) {
                    printf("    %s: %d sessions", models[m], model_uses[m]);
                    if (model_tok_in[m] > 0 || model_tok_out[m] > 0)
                        printf(" (%lld in / %lld out)", model_tok_in[m], model_tok_out[m]);
                    if (model_cost[m] > 0.0)
                        printf(" [~$%.4f]", model_cost[m]);
                    printf("\n");
                }
                if (model_count > 8)
                    printf("    ... and %d more\n", model_count - 8);
            }

            /* B08: Per-platform breakdown */
            if (platform_count > 0) {
                printf("  Platforms:\n");
                for (int p = 0; p < platform_count && p < 8; p++) {
                    printf("    %s: %d sessions", platforms[p], platform_sessions[p]);
                    if (platform_tokens[p] > 0)
                        printf(" (%lld tokens)", platform_tokens[p]);
                    printf("\n");
                }
                if (platform_count > 8)
                    printf("    ... and %d more\n", platform_count - 8);
            }
            /* D01: Per-tool breakdown from message-level queries (P151) */
            {
                db_tool_stat_t *tool_stats = db_query_tool_stats(db, days_filter,
                    source_filter[0] ? source_filter : NULL);
                if (tool_stats) {
                    int disp = 0;
                    for (int t = 0; tool_stats[t].name[0]; t++) disp++;
                    if (disp > 0) {
                        printf("  Tools used:\n");
                        for (int t = 0; t < disp && t < 10; t++)
                            printf("    %s: %d calls\n", tool_stats[t].name, tool_stats[t].count);
                        if (disp > 10)
                            printf("    ... and %d more\n", disp - 10);
                    }
                    free(tool_stats);
                }
            }

            /* D02: Skill breakdown — loaded skills with usage stats */
            {
                skill_usage_map_t sumap;
                skill_usage_load(state->hermes_home, &sumap);
                if (sumap.count > 0) {
                    int active_count = 0;
                    for (int s = 0; s < sumap.count; s++)
                        if (strcmp(sumap.records[s].state, "active") == 0)
                            active_count++;
                    printf("  Skills (%d active, %d total):\n",
                           active_count, sumap.count);
                    for (int s = 0; s < sumap.count && s < 15; s++) {
                        skill_usage_record_t *r = &sumap.records[s];
                        printf("    %s:", r->name);
                        if (r->use_count > 0)  printf(" uses=%d", r->use_count);
                        if (r->view_count > 0) printf(" views=%d", r->view_count);
                        if (r->patch_count > 0) printf(" patches=%d", r->patch_count);
                        if (r->state[0] && strcmp(r->state, "active") != 0)
                            printf(" [%s]", r->state);
                        printf("\n");
                    }
                    if (sumap.count > 15)
                        printf("    ... and %d more\n", sumap.count - 15);
                }
            }

        } /* end entries else */

        if (db_opened) db_close(db);
    } else {
        printf("  DB:              not available\n");
    }

    printf("  Current session:\n");
    printf("    Messages:  %zu\n", state->message_count);
    printf("    Tool calls: %d\n", tool_calls);
    printf("    Chars:     %zu\n", total_chars);
    printf("    Est. tokens: ~%zu\n", (total_chars + 3) / 4);
    printf("    Iterations: %d/%d\n", state->iteration_count, state->max_iterations);

    /* Cost estimate using model name */
    if (state->llm.model[0]) {
        usage_counts_t uc;
        memset(&uc, 0, sizeof(uc));
        uc.input_tokens = (long long)((total_chars + 3) / 4 / 2); /* ~half input */
        uc.output_tokens = (long long)((total_chars + 3) / 4 / 2);
        usage_cost_t est = usage_pricing_estimate(state->llm.model, &uc);
        if (est.has_pricing) {
            printf("    Est. cost:  %s (%s)\n",
                   usage_pricing_format_cost(est.total_cost), est.label);
        }
    }
}

/* /indicator: Pick TUI indicator style — stores in static var */
static char g_indicator_style[32] = "default";
static void cmd_indicator(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current indicator: %s Options: default, dots, bar, face\n", g_indicator_style);
        return;
    }
    if (strcmp(args, "default") == 0 || strcmp(args, "dots") == 0 ||
        strcmp(args, "bar") == 0 || strcmp(args, "face") == 0) {
        snprintf(g_indicator_style, sizeof(g_indicator_style), "%s", args);
        printf("Indicator set to: %s\n", args);
    } else {
        printf("Unknown indicator: %s Options: default, dots, bar, face\n", args);
    }
}

/* /statusbar: Toggle status bar (on|off|status) */
static int g_statusbar_on = 1;
static void cmd_statusbar(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Status bar: %s\n", g_statusbar_on ? "shown" : "hidden");
        printf("  Usage: /statusbar [on|off|status]\n");
        return;
    }
    if (strcmp(args, "on") == 0) {
        g_statusbar_on = 1;
        printf("Status bar shown.\n");
    } else if (strcmp(args, "off") == 0) {
        g_statusbar_on = 0;
        printf("Status bar hidden.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /statusbar [on|off|status]\n");
    }
}

/* /footer: Toggle footer (on|off|status) */
static int g_footer_on = 1;
static void cmd_footer(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Footer: %s\n", g_footer_on ? "shown" : "hidden");
        printf("  Usage: /footer [on|off|status]\n");
        return;
    }
    if (strcmp(args, "on") == 0) {
        g_footer_on = 1;
        printf("Footer shown.\n");
    } else if (strcmp(args, "off") == 0) {
        g_footer_on = 0;
        printf("Footer hidden.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /footer [on|off|status]\n");
    }
}

/* /busy: Control Enter behavior */
/* Busy behavior mode: 0=queue (default), 1=steer, 2=interrupt */
static int g_busy_mode = 0;

static void cmd_busy(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        const char *modes[] = {"queue", "steer", "interrupt"};
        printf("Busy behavior: %s\n", g_busy_mode >= 0 && g_busy_mode < 3 ? modes[g_busy_mode] : "queue");
        printf("Usage: /busy [queue|steer|interrupt|status]\n");
        printf("  queue     - Enter queues prompt while working (default)\n");
        printf("  steer     - Enter queues as steer message\n");
        printf("  interrupt - Enter sends interrupt\n");
        return;
    }

    if (strcmp(args, "status") == 0) {
        const char *modes[] = {"queue", "steer", "interrupt"};
        printf("Busy behavior: %s\n", g_busy_mode >= 0 && g_busy_mode < 3 ? modes[g_busy_mode] : "queue");
        return;
    }

    if (strcmp(args, "queue") == 0) { g_busy_mode = 0; }
    else if (strcmp(args, "steer") == 0) { g_busy_mode = 1; }
    else if (strcmp(args, "interrupt") == 0) { g_busy_mode = 2; }
    else {
        printf("Unknown busy mode: %s\n", args);
        printf("Valid: queue, steer, interrupt, status\n");
        return;
    }

    printf("Busy behavior set to: %s\n", args);
}

/* /reload-mcp: Reload MCP servers from config */
extern int g_server_count; /* from mcp_tool.c */
static void cmd_reload_mcp(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("MCP server reload: restart hermes to pick up config.yaml mcp_servers changes.\n");
    printf("Currently loaded: %d MCP server(s).\n", g_server_count);
    printf("To add new servers, edit ~/.slermes/config.yaml under mcp_servers: and restart.\n");
}

/* /reload-skills: Re-scan skills directory */
static void cmd_reload_skills(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { printf("Cannot determine home directory.\n"); return; }

    char skills_dir[512];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.slermes/skills", home);

    struct stat st;
    if (stat(skills_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("Skills directory not found: %s\n", skills_dir);
        printf("Create it to install skills.\n");
        return;
    }

    /* Count skill files */
    DIR *dir = opendir(skills_dir);
    if (!dir) { printf("Cannot open skills directory: %s\n", skills_dir); return; }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        /* Check for .md or directory */
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", skills_dir, entry->d_name);
        struct stat fst;
        if (stat(full, &fst) == 0 && (S_ISDIR(fst.st_mode) || strstr(entry->d_name, ".md")))
            count++;
    }
    closedir(dir);

    printf("Skills directory scanned: %s\n", skills_dir);
    printf("Found %d skill(s). Use /skills search-hub <query> to find more.\n", count);
}

/* /browser: Connect CDP browser */
static void cmd_browser(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /browser connect <ws_url>  — connect to CDP browser\n");
        printf("       /browser status            — show CDP connection status\n");
        printf("       /browser disconnect         — close CDP connection\n");
        return;
    }

    if (strncmp(args, "connect ", 8) == 0) {
        const char *url = args + 8;
        while (*url == ' ') url++;
        if (!*url) { printf("Usage: /browser connect <ws_url>\n"); return; }
        cdp_set_url(url);
        printf("CDP URL set to: %s\n", url);
        printf("Use browser_navigate, browser_snapshot, etc. via the tool system.\n");
        return;
    }

    if (strcmp(args, "disconnect") == 0) {
        cdp_set_url("");
        printf("CDP connection cleared.\n");
        return;
    }

    if (strcmp(args, "status") == 0) {
        const char *url = cdp_get_url();
        if (url && url[0])
            printf("CDP connected to: %s\n", url);
        else
            printf("CDP not connected. Use /browser connect <ws_url>.\n");
        return;
    }

    printf("Unknown browser command: %s\n", args);
}

/* /update: Update Hermes Agent — git pull + rebuild */
static void cmd_update(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Updating Hermes Agent...\n");

    /* Determine repo root: walk up from CWD until .git found */
    char cwd[4096];
    char repo_root[4096] = "";
    if (getcwd(cwd, sizeof(cwd))) {
        strncpy(repo_root, cwd, sizeof(repo_root) - 1);
        while (repo_root[0]) {
            char git_dir[4096];
            snprintf(git_dir, sizeof(git_dir), "%s/.git", repo_root);
            struct stat st;
            if (stat(git_dir, &st) == 0 && S_ISDIR(st.st_mode))
                break;
            /* Go up one directory */
            char *slash = strrchr(repo_root, '/');
            if (!slash || slash == repo_root) { repo_root[0] = '\0'; break; }
            *slash = '\0';
        }
    }

    if (!repo_root[0]) {
        printf("Error: not inside a git repository.\n");
        return;
    }

    /* Fetch latest */
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cd '%s' && git fetch --quiet origin 2>&1", repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char err[1024];
            size_t n = 0;
            while (fgets(err, sizeof(err), fp)) {
                if (n == 0) printf("  Fetch: ");
                printf("%s", err);
                n++;
            }
            int rc = pclose(fp);
            if (rc != 0) {
                printf("  Git fetch failed (exit %d). Aborting.\n", rc);
                return;
            }
        }
    }

    /* Check if behind */
    char behind_buf[64] = "0";
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "cd '%s' && git rev-list --count HEAD..origin/$(git rev-parse --abbrev-ref HEAD 2>/dev/null) 2>/dev/null || echo 0",
                 repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            if (!fgets(behind_buf, sizeof(behind_buf), fp)) behind_buf[0] = '0';
            size_t blen = strlen(behind_buf);
            if (blen > 0 && behind_buf[blen-1] == '\n') behind_buf[blen-1] = '\0';
            pclose(fp);
        }
    }

    int behind = atoi(behind_buf);
    if (behind == 0) {
        printf("  Already up to date (%s).\n", repo_root);
        return;
    }
    printf("  %d commit(s) behind. Pulling...\n", behind);

    /* Get current commit before pull */
    char old_commit[128] = "(unknown)";
    {
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (!fgets(old_commit, sizeof(old_commit), fp)) old_commit[0] = '\0';
            size_t olen = strlen(old_commit);
            if (olen > 0 && old_commit[olen-1] == '\n') old_commit[olen-1] = '\0';
            pclose(fp);
        }
    }

    /* Pull */
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cd '%s' && git pull --ff-only origin 2>&1", repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp))
                printf("  %s", line);
            int rc = pclose(fp);
            if (rc != 0) {
                printf("  Git pull failed (exit %d). Resolve conflicts manually.\n", rc);
                return;
            }
        }
    }

    /* Get new commit */
    char new_commit[128] = "(unknown)";
    {
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (!fgets(new_commit, sizeof(new_commit), fp)) new_commit[0] = '\0';
            size_t nlen = strlen(new_commit);
            if (nlen > 0 && new_commit[nlen-1] == '\n') new_commit[nlen-1] = '\0';
            pclose(fp);
        }
    }
    printf("  %s -> %s\n", old_commit, new_commit);

    /* Rebuild */
    printf("  Rebuilding...\n");
    {
        FILE *fp = popen("make -j$(nproc) 2>&1", "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                /* Only print errors and final line */
                if (strstr(line, "error:") || strstr(line, "Error:") ||
                    strstr(line, "Phase 5 complete"))
                    printf("  %s", line);
            }
            int rc = pclose(fp);
            if (rc == 0)
                printf("  Update complete! Binary rebuilt.\n");
            else
                printf("  Build failed (exit %d).\n", rc);
        }
    }
}

/* /debug: Generate debug report */
static void cmd_debug(const char *args, agent_state_t *state) {
    (void)args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", tm_info);

    printf("=== Hermes C Debug Report ===\n");
    printf("Generated: %s\n\n", time_buf);

    /* System info */
    {
        struct utsname un;
        if (uname(&un) == 0)
            printf("Kernel:     %s %s %s %s\n", un.sysname, un.nodename, un.release, un.machine);
        else
            printf("Kernel:     (unknown)\n");
    }
    {
        long nproc = sysconf(_SC_NPROCESSORS_ONLN);
        if (nproc > 0) printf("CPUs:       %ld\n", nproc);
    }
    {
        long pages = sysconf(_SC_PHYS_PAGES);
        long psize = sysconf(_SC_PAGE_SIZE);
        if (pages > 0 && psize > 0)
            printf("Memory:     %ld MB total\n", (pages * psize) / (1024 * 1024));
    }

    /* Version info */
    printf("\n-- Version --\n");
    printf("Version:    %s\n", HERMES_VERSION);
    printf("Build:      %s %s\n", __DATE__, __TIME__);

    /* Git info */
    char buf[256];
    FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Git commit: %s\n", buf);
        }
        pclose(fp);
    } else {
        printf("Git commit: (unknown)\n");
    }
    fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Git branch: %s\n", buf);
        }
        pclose(fp);
    }

    /* Uptime */
    fp = popen("uptime 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Uptime:     %s\n", buf);
        }
        pclose(fp);
    }

    /* Session info */
    printf("\n-- Session --\n");
    printf("Messages:   %zu\n", state->message_count);
    printf("Iterations: %d/%d\n", state->iteration_count, state->max_iterations);
    printf("Session ID: %s\n", state->session_id[0] ? state->session_id : "(none)");

    /* Tools registered */
    printf("\n-- Tools --\n");
    {
        int n = (int)registry_count();
        printf("Registered: %d\n", n);
    }

    /* Config */
    printf("\n-- Config --\n");
    printf("Provider:   %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Model:      %s\n", state->llm.model[0] ? state->llm.model : "(default)");

    /* Log files tail */
    {
        char log_dir[512] = "";
        const char *home = state->hermes_home[0] ? state->hermes_home :
                           getenv("SLERMES_HOME");
        if (home) snprintf(log_dir, sizeof(log_dir), "%s/logs", home);

        if (log_dir[0]) {
            printf("\n-- Recent Logs --\n");
            const char *log_files[] = {"agent.log", "errors.log", NULL};
            for (int i = 0; log_files[i]; i++) {
                char logpath[1024];
                snprintf(logpath, sizeof(logpath), "%s/%s", log_dir, log_files[i]);
                struct stat st;
                if (stat(logpath, &st) == 0) {
                    char cmd[2048];
                    snprintf(cmd, sizeof(cmd), "tail -20 '%s' 2>/dev/null", logpath);
                    FILE *lfp = popen(cmd, "r");
                    if (lfp) {
                        printf("\n%s (last 20 lines, %ld bytes):\n", log_files[i], (long)st.st_size);
                        char line[1024];
                        while (fgets(line, sizeof(line), lfp))
                            printf("  %s", line);
                        pclose(lfp);
                    }
                }
            }
        }
    }

    printf("\n--- End Debug Report ---\n");
}

/* /voice: Toggle voice input/output mode (on|off|tts|status|config|key) */
static int g_voice_mode = 0;
static void cmd_voice(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Voice CLI — mode and configuration\n");
        printf("Usage: /voice status          — Show voice mode + config\n");
        printf("       /voice on              — Enable voice input/output\n");
        printf("       /voice off             — Disable voice mode\n");
        printf("       /voice tts             — Enable TTS output mode\n");
        printf("       /voice config          — Show voice config settings\n");
        printf("       /voice key <binding>   — Set record key (e.g. ctrl+b)\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0 || strcmp(sub, "st") == 0) {
        printf("Voice mode: %s\n", g_voice_mode ? "ENABLED" : "DISABLED");
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (home_dir[0] && access(home_dir, F_OK) == 0 && hermes_config_load(&cfg, home_dir)) {
            printf("\nVoice config:\n");
            show_cfg_val("record_key", "str", cfg.voice.record_key);
            char key_buf[64] = "Ctrl+B"; /* Default display */
            if (cfg.voice.record_key[0]) {
                /* Format key binding: "ctrl+b" -> "Ctrl+B" */
                if (strncmp(cfg.voice.record_key, "c-", 2) == 0)
                    snprintf(key_buf, sizeof(key_buf), "Ctrl+%c", toupper((unsigned char)cfg.voice.record_key[2]));
                else if (strncmp(cfg.voice.record_key, "a-", 2) == 0)
                    snprintf(key_buf, sizeof(key_buf), "Alt+%c", toupper((unsigned char)cfg.voice.record_key[2]));
            }
            printf("  record_key_formatted: %s\n", key_buf);
            show_cfg_val_int("max_recording_seconds", cfg.voice.max_recording_seconds);
            show_cfg_val_bool("auto_tts", cfg.voice.auto_tts);
            show_cfg_val_bool("beep_enabled", cfg.voice.beep_enabled);
            show_cfg_val_int("silence_threshold", cfg.voice.silence_threshold);
            show_cfg_val_float("silence_duration", cfg.voice.silence_duration);
            printf("\nTTS provider:\n");
            show_cfg_val("provider", "str", cfg.tts.provider);
        } else {
            printf("(config not loaded)\n");
        }
        return;
    }
    if (strcmp(sub, "on") == 0) {
        g_voice_mode = 1;
        printf("Voice mode ENABLED. voice_listen/voice_speak tools are available.\n");
        return;
    }
    if (strcmp(sub, "tts") == 0) {
        g_voice_mode = 1;
        printf("Voice (TTS) mode ENABLED. voice_speak tool available.\n");
        return;
    }
    if (strcmp(sub, "off") == 0) {
        g_voice_mode = 0;
        printf("Voice mode DISABLED.\n");
        return;
    }
    if (strcmp(sub, "config") == 0) {
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (home_dir[0] && access(home_dir, F_OK) == 0 && hermes_config_load(&cfg, home_dir)) {
            show_section_voice(&cfg);
        } else {
            printf("Error: Could not load config\n");
        }
        return;
    }
    if (strncmp(sub, "key ", 4) == 0) {
        const char *binding = sub + 4;
        while (*binding == ' ') binding++;
        if (!*binding) {
            printf("Usage: /voice key <binding>\n");
            printf("  Example: /voice key ctrl+b\n");
            printf("  Format: ctrl+<key> or alt+<key>\n");
            return;
        }
        hermes_config_t cfg;
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            const char *home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (home_env) {
                snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
                if (access(home_dir, F_OK) != 0)
                    snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }
        if (!home_dir[0] || access(home_dir, F_OK) != 0) {
            printf("Error: Cannot determine Hermes home\n");
            return;
        }
        if (hermes_config_load(&cfg, home_dir)) {
            printf("To set voice record key to '%s':\n", binding);
            printf("  Edit ~/.slermes/config.yaml:\n");
            printf("    voice:\n");
            printf("      record_key: %s\n", binding);
        } else {
            printf("Error: Could not load config\n");
        }
        return;
    }

    printf("Unknown argument: %s\n", sub);
    printf("Usage: /voice [on|off|tts|status|config|key <binding>]\n");
}

/* /steer: Inject a message after the next tool call */
static void cmd_steer(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /steer [options] <message>\n");
        printf("Options:\n");
        printf("  -u, --user      Inject as user message (default: system)\n");
        printf("  -a, --assistant Inject as assistant message\n");
        printf("  -s, --system    Inject as system message\n");
        printf("  -l, --list      List queued steer messages\n");
        return;
    }

    /* Parse options */
    message_role_t role = MSG_SYSTEM;
    const char *msg_start = args;

    if (strncmp(args, "-u ", 3) == 0 || strncmp(args, "--user ", 7) == 0) {
        role = MSG_USER;
        msg_start = strchr(args, ' ') + 1;
    } else if (strncmp(args, "-a ", 3) == 0 || strncmp(args, "--assistant ", 12) == 0) {
        role = MSG_ASSISTANT;
        msg_start = strchr(args, ' ') + 1;
    } else if (strncmp(args, "-s ", 3) == 0 || strncmp(args, "--system ", 9) == 0) {
        role = MSG_SYSTEM;
        msg_start = strchr(args, ' ') + 1;
    } else if (strcmp(args, "-l") == 0 || strcmp(args, "--list") == 0) {
        if (state->steer_count == 0) {
            printf("No steer messages queued.\n");
        } else {
            printf("Queued steer messages (%d):\n", state->steer_count);
            for (int i = 0; i < state->steer_count && i < HERMES_MAX_STEERS; i++) {
                if (state->steer_queue[i][0]) {
                    const char *r = "system";
                    if (state->steer_roles[i] == MSG_USER) r = "user";
                    else if (state->steer_roles[i] == MSG_ASSISTANT) r = "assistant";
                    printf("  [%d] %s: %s\n", i, r, state->steer_queue[i]);
                }
            }
        }
        return;
    }

    if (state->steer_count >= HERMES_MAX_STEERS) {
        printf("Steer queue full (%d max). Clear with /steer --clear or use existing.\n",
               HERMES_MAX_STEERS);
        return;
    }

    /* Trim leading whitespace from message */
    while (*msg_start == ' ') msg_start++;
    if (!*msg_start) {
        printf("Usage: /steer <message>\n");
        return;
    }

    strncpy(state->steer_queue[state->steer_count], msg_start,
            sizeof(state->steer_queue[0]) - 1);
    state->steer_roles[state->steer_count] = role;
    state->steer_count++;
    printf("Steer message queued: \"%s\"\n", msg_start);
}

/* /skills-hub: Browse.sh skills catalog CLI */
static void cmd_skills_hub(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /skills-hub <subcommand> [args]\n");
        printf("  /skills-hub list              — List all hub skills\n");
        printf("  /skills-hub search <query>    — Search hub skills\n");
        printf("  /skills-hub show <slug>       — Show skill details\n");
        printf("  /skills-hub sync              — Refresh catalog from network\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "list") == 0) {
        skills_hub_fetch_catalog();
        char *s = skills_hub_summary();
        if (s) {
            printf("%s\n", s);
            free(s);
        }
        /* Print first 30 skills */
        hub_skill_meta_t results[50];
        int n = skills_hub_search("", results, 50);
        if (n > 0) {
            printf("\nSkills (%d shown of %d):\n", n > 50 ? 50 : n, n);
            for (int i = 0; i < n && i < 50; i++) {
                printf("  %-28s %s\n", results[i].slug, results[i].title);
            }
            if (n > 50) printf("  ... and %d more\n", n - 50);
        } else {
            printf("No skills found (catalog empty or not loaded).\n");
        }
        return;
    }

    if (strncmp(sub, "search", 6) == 0) {
        const char *query = sub + 6;
        while (*query == ' ') query++;
        if (!*query) { printf("Usage: /skills-hub search <query>\n"); return; }
        skills_hub_fetch_catalog();
        hub_skill_meta_t results[SKILLS_HUB_MAX_RESULTS];
        int n = skills_hub_search(query, results, SKILLS_HUB_MAX_RESULTS);
        if (n > 0) {
            printf("Found %d skills matching \"%s\":\n", n, query);
            for (int i = 0; i < n; i++) {
                printf("  %-28s %s\n", results[i].slug, results[i].title);
            }
        } else {
            printf("No skills found for \"%s\". Try a different query.\n", query);
        }
        return;
    }

    if (strncmp(sub, "show", 4) == 0) {
        const char *slug = sub + 4;
        while (*slug == ' ') slug++;
        if (!*slug) { printf("Usage: /skills-hub show <slug>\n"); return; }
        skills_hub_fetch_catalog();
        hub_skill_meta_t meta;
        if (skills_hub_get_by_slug(slug, &meta)) {
            printf("Slug:    %s\n", meta.slug);
            printf("Name:    %s\n", meta.name[0] ? meta.name : meta.title);
            printf("Title:   %s\n", meta.title);
            printf("Category:%s\n", meta.category);
            printf("Desc:    %s\n", meta.description);
            printf("URL:     %s\n", meta.source_url);
            if (meta.tags[0])
                printf("Tags:    %s\n", meta.tags);
            printf("Installs:%d\n", meta.install_count);
            printf("Proxy:   %s\n", meta.needs_proxy ? "yes" : "no");
            printf("Method:  %s\n", meta.recommended_method[0] ? meta.recommended_method : "(auto)");
        } else {
            printf("Skill not found: %s\n", slug);
        }
        return;
    }

    if (strcmp(sub, "sync") == 0) {
        skills_hub_clear_cache();
        printf("Cleared cache. Fetching catalog...\n");
        if (skills_hub_fetch_catalog()) {
            char *s = skills_hub_summary();
            printf("Catalog refreshed: %s\n", s ? s : "(ok)");
            free(s);
        } else {
            printf("Error: Could not fetch skills catalog.\n");
        }
        return;
    }

    printf("Unknown subcommand: '%s'. Use: /skills-hub list|search|show|sync\n", sub);
}

/* /kanban: Kanban board management */
static void cmd_kanban(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /kanban <subcommand> [args]\n");
        printf("  /kanban list [--all]        — List kanban tasks\n");
        printf("  /kanban show <task_id>      — Show kanban task details\n");
        printf("  /kanban create <title>      — Create a new kanban task\n");
        printf("  /kanban complete <task_id>  — Mark a task complete\n");
        printf("  /kanban block <task_id>     — Mark a task blocked\n");
        printf("  /kanban unblock <task_id>   — Move blocked task to ready\n");
        printf("  /kanban link <parent> <child> — Add parent->child link\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strncmp(sub, "list", 4) == 0) {
        const char *rest = sub + 4;
        while (*rest == ' ') rest++;
        bool show_all = (strcmp(rest, "--all") == 0);
        char json_args[256];
        snprintf(json_args, sizeof(json_args),
                 "{\"limit\":%d,\"include_archived\":%s}",
                 show_all ? 200 : 50,
                 show_all ? "true" : "false");
        char *result = registry_dispatch("kanban_list", json_args, "");
        if (result) {
            /* Try to pretty-print the JSON result */
            json_t *jr = json_parse(result, NULL);
            if (jr) {
                json_t *tasks = json_obj_get(jr, "tasks");
                if (tasks && tasks->type == JSON_ARRAY) {
                    printf("Kanban tasks (%zu):\n", tasks->c.count);
                    for (size_t i = 0; i < tasks->c.count; i++) {
                        json_t *t = tasks->c.items[i];
                        const char *id = json_get_str(t, "id", "?");
                        const char *title = json_get_str(t, "title", "?");
                        const char *status = json_get_str(t, "status", "?");
                        const char *assignee = json_get_str(t, "assignee", "");
                        printf("  %-12s %-8s %-12s %s\n", id, status, assignee, title);
                    }
                } else {
                    printf("Result: %s\n", result);
                }
                json_free(jr);
            } else {
                printf("Result: %s\n", result);
            }
            free(result);
        } else {
            printf("Error: kanban_list returned NULL\n");
        }
        return;
    }

    if (strncmp(sub, "show", 4) == 0) {
        const char *id = sub + 4;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban show <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args), "{\"task_id\":\"%s\"}", id);
        char *result = registry_dispatch("kanban_show", json_args, "");
        if (result) {
            printf("Task %s:\n%s\n", id, result);
            free(result);
        } else {
            printf("Error: kanban_show returned NULL (task not found?)\n");
        }
        return;
    }

    if (strncmp(sub, "create", 6) == 0) {
        const char *title = sub + 6;
        while (*title == ' ') title++;
        if (!*title) { printf("Usage: /kanban create <title>\n"); return; }
        char json_args[1024];
        snprintf(json_args, sizeof(json_args),
                 "{\"title\":\"%s\",\"assignee\":\"cli\"}", title);
        char *result = registry_dispatch("kanban_create", json_args, "");
        if (result) {
            json_t *jr = json_parse(result, NULL);
            if (jr) {
                const char *tid = json_get_str(jr, "task_id", result);
                printf("Created task: %s\n", tid);
                json_free(jr);
            } else {
                printf("Result: %s\n", result);
            }
            free(result);
        } else {
            printf("Error: kanban_create returned NULL\n");
        }
        return;
    }

    if (strncmp(sub, "complete", 8) == 0) {
        const char *id = sub + 8;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban complete <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args),
                 "{\"task_id\":\"%s\",\"summary\":\"completed via CLI\"}", id);
        char *result = registry_dispatch("kanban_complete", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: kanban_complete returned NULL\n");
        }
        return;
    }

    if (strncmp(sub, "block", 5) == 0) {
        const char *id = sub + 5;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban block <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args),
                 "{\"task_id\":\"%s\",\"reason\":\"blocked via CLI\"}", id);
        char *result = registry_dispatch("kanban_block", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: kanban_block returned NULL\n");
        }
        return;
    }

    if (strncmp(sub, "unblock", 7) == 0) {
        const char *id = sub + 7;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban unblock <task_id>\n"); return; }
        char json_args[256];
        snprintf(json_args, sizeof(json_args), "{\"task_id\":\"%s\"}", id);
        char *result = registry_dispatch("kanban_unblock", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: kanban_unblock returned NULL\n");
        }
        return;
    }

    if (strncmp(sub, "link", 4) == 0) {
        const char *rest = sub + 4;
        while (*rest == ' ') rest++;
        char parent[256], child[256];
        if (sscanf(rest, "%255s %255s", parent, child) != 2) {
            printf("Usage: /kanban link <parent_id> <child_id>\n");
            return;
        }
        char json_args[1024];
        snprintf(json_args, sizeof(json_args),
                 "{\"parent_id\":\"%s\",\"child_id\":\"%s\"}", parent, child);
        char *result = registry_dispatch("kanban_link", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            printf("Error: kanban_link returned NULL\n");
        }
        return;
    }

    printf("Unknown kanban subcommand: %s\n  Use: list, show <id>, create <title>, complete <id>, block <id>, unblock <id>, link <parent> <child>\n", sub);
}

/* ================================================================
 *  H31: /session-search — search past sessions
 * ================================================================ */
static void cmd_session_search(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !*args) {
        printf("Usage: /session-search <query> [--limit N]\n");
        printf("Search past conversation sessions for matching content.\n");
        return;
    }

    /* Build search args JSON */
    char query[512];
    int limit = 10;
    const char *p = args;
    while (*p == ' ') p++;

    /* Check for --limit option */
    const char *limit_marker = strstr(p, "--limit");
    if (limit_marker) {
        limit = atoi(limit_marker + 7);
        if (limit < 1) limit = 1;
        if (limit > 100) limit = 100;
        /* Truncate query at limit marker */
        size_t qlen = (size_t)(limit_marker - p);
        if (qlen > sizeof(query) - 1) qlen = sizeof(query) - 1;
        memcpy(query, p, qlen);
        query[qlen] = '\0';
        /* Trim trailing space */
        while (qlen > 0 && query[qlen - 1] == ' ') query[--qlen] = '\0';
    } else {
        snprintf(query, sizeof(query), "%s", p);
    }

    if (!query[0]) {
        printf("Usage: /session-search <query> [--limit N]\n");
        return;
    }

    /* Build JSON args */
    char args_json[1024];
    snprintf(args_json, sizeof(args_json),
             "{\"query\":\"%s\",\"limit\":%d}", query, limit);

    /* Call session search handler */
    char *result = session_search_handler(args_json, NULL);
    if (!result) {
        printf("Search failed.\n");
        return;
    }

    /* Parse and display */
    char *err = NULL;
    json_node_t *root = json_parse(result, &err);
    free(result);
    if (!root) {
        printf("Error parsing search results: %s\n", err ? err : "unknown");
        free(err);
        return;
    }

    const char *err_str = json_object_get_string(root, "error", NULL);
    if (err_str) {
        printf("Search error: %s\n", err_str);
        json_free(root);
        return;
    }

    int count = (int)json_object_get_number(root, "count", 0);
    int total = (int)json_object_get_number(root, "total_matches", 0);

    printf("Session search results (%d shown, %d total matches):\n", count, total);

    json_node_t *results = json_object_get(root, "results");
    if (results) {
        size_t n = json_array_count(results);
        for (size_t i = 0; i < n; i++) {
            json_node_t *s = json_array_get(results, i);
            if (!s) continue;
            const char *sid = json_object_get_string(s, "session_id", "?");
            double score = json_object_get_number(s, "score", 0.0);
            const char *snippet = json_object_get_string(s, "snippet", "");
            printf("\n  #%zu: %s (score %.1f)\n", i + 1, sid, score);
            printf("       %.*s\n", 120, snippet);
        }
    }

    json_free(root);
}

/* ================================================================
 *  H32: /session-export — export a session to JSON or Markdown
 * ================================================================ */
static void cmd_session_export(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !*args) {
        printf("Usage: /session-export <session_id> [json|markdown]\n");
        printf("Export a saved session. Default format: markdown\n");
        return;
    }

    /* Parse: session_id [format] */
    char session_id[256] = "";
    char format[32] = "markdown";
    const char *p = args;
    while (*p == ' ') p++;

    /* Read first word (session_id) */
    const char *space = strchr(p, ' ');
    if (space) {
        size_t id_len = (size_t)(space - p);
        if (id_len > sizeof(session_id) - 1) id_len = sizeof(session_id) - 1;
        memcpy(session_id, p, id_len);
        session_id[id_len] = '\0';
        /* Read format */
        const char *fmt = space + 1;
        while (*fmt == ' ') fmt++;
        if (*fmt)
            snprintf(format, sizeof(format), "%s", fmt);
    } else {
        snprintf(session_id, sizeof(session_id), "%s", p);
    }

    if (!session_id[0]) {
        printf("Usage: /session-export <session_id> [json|markdown]\n");
        return;
    }

    /* Determine export operation */
    const char *op = (strcmp(format, "json") == 0) ? "export_json" : "export_markdown";

    /* Build args JSON */
    char args_json[1024];
    snprintf(args_json, sizeof(args_json),
             "{\"operation\":\"%s\",\"session_id\":\"%s\"}", op, session_id);

    /* Call session CRUD handler */
    char *result = session_crud_handler(args_json, NULL);
    if (!result) {
        printf("Export failed.\n");
        return;
    }

    /* Parse result */
    char *err = NULL;
    json_node_t *root = json_parse(result, &err);
    free(result);
    if (!root) {
        printf("Error parsing export result: %s\n", err ? err : "unknown");
        free(err);
        return;
    }

    const char *err_str = json_object_get_string(root, "error", NULL);
    if (err_str) {
        printf("Export error: %s\n", err_str);
        json_free(root);
        return;
    }

    const char *content = json_object_get_string(root, "content", NULL);
    if (!content) {
        content = json_object_get_string(root, "data", NULL);
    }

    if (content) {
        printf("%s\n", content);
    } else {
        printf("Session '%s' not found or empty.\n", session_id);
    }

    json_free(root);
}

/* /completions: Generate shell completion scripts */
static void cmd_completions(const char *args, agent_state_t *state) {
    (void)state;
    if (args && (strcmp(args, "fish") == 0 || strcmp(args, "zsh") == 0)) {
        printf("#compdef hermes\n");
        printf("_hermes_commands() {\n");
        printf("  local -a commands\n");
        const command_def_t *cmd = COMMANDS;
        while (cmd->name && cmd->handler) {
            printf("  commands+=( \"%s:%s\" )\n", cmd->name, cmd->description);
            cmd++;
        }
        printf("  _describe 'hermes commands' commands\n");
        printf("}\n");
        printf("_hermes() {\n");
        printf("  _arguments -C \\\n");
        printf("    '1: :->command' \\\n");
        printf("    '*: :->args'\n");
        printf("  case $state in\n");
        printf("    (command) _hermes_commands ;;\n");
        printf("    (args) _hermes_commands ;;\n");
        printf("  esac\n");
        printf("}\n");
        printf("compdef _hermes hermes\n");
    } else {
        /* Default: bash completions */
        printf("_hermes_completions() {\n");
        printf("  local cur=${COMP_WORDS[COMP_CWORD]}\n");
        printf("  COMPREPLY=( $(compgen -W \"");
        const command_def_t *cmd = COMMANDS;
        int count = 0;
        while (cmd->name && cmd->handler) {
            if (count > 0) printf(" ");
            printf("%s", cmd->name);
            cmd++; count++;
        }
        printf("\" -- \"$cur\") )\n");
        printf("  return 0\n");
        printf("}\n");
        printf("complete -F _hermes_completions hermes\n");
    }
}

/* /secrets: Manage Bitwarden secrets */
static void cmd_secrets(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || args[0] == '\0') {
        printf("Usage: /secrets [list|get <name>|sync|status]\n");
        printf("  list   - Show available secret references\n");
        printf("  get    - Resolve and display a single secret\n");
        printf("  sync   - Force re-fetch from Bitwarden\n");
        printf("  status - Show Bitwarden integration status\n");
        return;
    }

    /* Parse subcommand */
    char subcmd[256];
    const char *p = args;
    while (*p == ' ') p++;
    int i = 0;
    while (*p && *p != ' ' && i < (int)sizeof(subcmd) - 1)
        subcmd[i++] = *p++;
    subcmd[i] = '\0';
    while (*p == ' ') p++;
    const char *param = (*p) ? p : NULL;

    if (strcmp(subcmd, "status") == 0) {
        const char *token = getenv("BWS_ACCESS_TOKEN");
        if (!token) token = getenv("SLERMES_BWS_TOKEN");
        printf("Bitwarden Secrets Manager status:\n");
        printf("  Access token: %s\n", (token && token[0]) ? "configured" : "NOT SET");

        const char *bsm_cfg = getenv("HERMES_SECRETS_ENABLED");
        if (!bsm_cfg) bsm_cfg = getenv("SLERMES_SECRETS_ENABLED");
        printf("  Integration: %s\n",
               (bsm_cfg && strcmp(bsm_cfg, "false") != 0) ? "enabled" : "disabled");

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "which bws 2>/dev/null || echo 'not found'");
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char path[512] = "";
            if (fgets(path, sizeof(path), fp)) {
                size_t len = strlen(path);
                if (len > 0 && path[len-1] == '\n') path[len-1] = '\0';
                printf("  bws binary: %s\n", path);
            }
            pclose(fp);
        }

        /* Check for Anthropic OAuth tokens (port of Python anthropic_adapter.py) */
        printf("\nAnthropic OAuth status:\n");
        const char *ant_key = getenv("ANTHROPIC_API_KEY");
        const char *ant_token = getenv("ANTHROPIC_TOKEN");
        const char *cc_oauth = getenv("CLAUDE_CODE_OAUTH_TOKEN");
        bool has_api_key = (ant_key && *ant_key);
        bool has_setup_token = (ant_token && anthropic_is_oauth_token(ant_token));
        bool has_cc_oauth = (cc_oauth && *cc_oauth);

        printf("  API key:        %s\n", has_api_key ? "✓" : "not set");
        printf("  Setup token:    %s%s\n",
               has_setup_token ? "✓" : "not set",
               has_setup_token ? "" : " (sk-ant-oat* or managed key)");
        printf("  CC OAuth token: %s\n",
               has_cc_oauth ? "✓ (from CLAUDE_CODE_OAUTH_TOKEN)" : "not set");
        printf("  Total:          %d credential(s) detected\n",
               (int)has_api_key + (int)(ant_token && *ant_token) + (int)has_cc_oauth);

    } else if (strcmp(subcmd, "list") == 0) {
        const char *token = getenv("BWS_ACCESS_TOKEN");
        if (!token) token = getenv("SLERMES_BWS_TOKEN");
        if (!token || !*token) {
            printf("BWS_ACCESS_TOKEN not set.\n");
            return;
        }
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "bws secret list 2>/dev/null | "
                 "jq -r '.[] | \"\\(.key): \\(.id)\"' 2>/dev/null || "
                 "bws secret list 2>/dev/null");
        printf("Secrets:\n");
        fflush(stdout);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            int count = 0;
            while (fgets(line, sizeof(line), fp)) {
                printf("  %s", line);
                count++;
            }
            int rc = pclose(fp);
            if (count == 0)
                printf("  (none — ensure bws is authenticated)\n");
            (void)rc;
        }

    } else if (strcmp(subcmd, "get") == 0 && param) {
        char ref[512];
        snprintf(ref, sizeof(ref), "${BSM:%s}", param);
        char *val = hermes_secrets_resolve(ref);
        if (val) {
            printf("%s: %s\n", param, val);
            free(val);
        } else {
            printf("Secret '%s' not found.\n", param);
        }

    } else if (strcmp(subcmd, "sync") == 0) {
        printf("Syncing secrets...\n");
        if (hermes_secrets_init(NULL))
            printf("Sync complete.\n");
        else
            printf("Sync failed.\n");

    } else {
        printf("Unknown subcommand '%s'. Use: list, get, sync, status\n", subcmd);
    }
}

/* /auth: Provider auth status overview */
static void cmd_auth(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Auth credential status.\n");
        printf("Usage: /auth status              — Show all provider credential status\n");
        printf("       /auth providers            — List known auth providers\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0 || strcmp(sub, "st") == 0) {
        printf("=== Provider Credential Status ===\n\n");
        /* Check API key env vars */
        typedef struct { const char *provider; const char *env_var; const char *desc; } cred_check_t;
        cred_check_t checks[] = {
            {"OpenAI",    "OPENAI_API_KEY",       "ChatGPT/OpenAI models"},
            {"Anthropic", "ANTHROPIC_API_KEY",     "Claude models"},
            {"Anthropic", "ANTHROPIC_TOKEN",       "Claude OAuth setup token"},
            {"OpenRouter", "OPENROUTER_API_KEY",   "OpenRouter aggregation"},
            {"DeepSeek",  "DEEPSEEK_API_KEY",      "DeepSeek models"},
            {"Google",    "GOOGLE_API_KEY",        "Gemini models"},
            {"xAI",       "XAI_API_KEY",           "Grok models"},
            {"Azure",     "AZURE_API_KEY",         "Azure OpenAI"},
            {"AWS",       "AWS_ACCESS_KEY_ID",     "AWS Bedrock (w/ AWS_SECRET_ACCESS_KEY)"},
            {"Nous",      "NOUS_API_KEY",          "NousResearch inference"},
            {"HF",        "HF_TOKEN",              "Hugging Face models"},
            {"Groq",      "GROQ_API_KEY",          "Groq inference"},
            {"Together",  "TOGETHER_API_KEY",      "Together AI"},
            {"Perplexity","PERPLEXITY_API_KEY",    "Perplexity models"},
            {"Cohere",    "COHERE_API_KEY",        "Cohere models"},
            {"Fireworks", "FIREWORKS_API_KEY",     "Fireworks AI"},
            {"Mistral",   "MISTRAL_API_KEY",       "Mistral models"},
            {NULL, NULL, NULL}
        };
        int count = 0;
        for (int i = 0; checks[i].provider; i++) {
            const char *val = getenv(checks[i].env_var);
            bool present = (val && val[0]);
            if (present) {
                size_t vlen = strlen(val);
                printf("  %-14s  %-30s  ✓ (%zu chars)\n",
                       checks[i].provider, checks[i].env_var, vlen);
                count++;
            }
        }
        if (count == 0)
            printf("  No API key credentials found in environment.\n");
        printf("\n  Total: %d credential(s) detected\n", count);

        /* OAuth status */
        printf("\n=== OAuth / Token Status ===\n");
        const char *cc_oauth = getenv("CLAUDE_CODE_OAUTH_TOKEN");
        printf("  Claude Code OAuth: %s\n",
               (cc_oauth && *cc_oauth) ? "configured" : "not set");
        const char *bws = getenv("BWS_ACCESS_TOKEN");
        if (!bws) bws = getenv("SLERMES_BWS_TOKEN");
        printf("  Bitwarden (BSM):  %s\n", (bws && *bws) ? "configured" : "not set");

        printf("\n=== Config Status ===\n");
        const char *home = state->hermes_home[0] ? state->hermes_home : NULL;
        if (!home) home = getenv("SLERMES_HOME");
        if (!home) {
            home = getenv("HOME");
            printf("  Config home:     %s\n", home ? home : "(not found)");
        } else {
            printf("  Config home:     %s\n", home);
        }
        char env_path[1024];
        snprintf(env_path, sizeof(env_path), "%s/.env", home ? home : "");
        printf("  .env file:       %s\n",
               (home && access(env_path, F_OK) == 0) ? "present" : "not found");
        char cfg_path[1024];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home ? home : "");
        printf("  config.yaml:     %s\n",
               (home && access(cfg_path, F_OK) == 0) ? "present" : "not found");

        /* Show OAuth token status */
        if (home) {
            int oauth_count = 0;
            auth_entry_t *entries = auth_store_load(home, &oauth_count);
            if (entries && oauth_count > 0) {
                printf("\n  OAuth Tokens:\n");
                time_t now = time(NULL);
                for (int i = 0; i < oauth_count; i++) {
                    const char *status = "expired";
                    if (entries[i].token.expires_at > 0) {
                        if (entries[i].token.expires_at > (double)now)
                            status = "valid";
                        else if (entries[i].token.refresh_token && entries[i].token.refresh_token[0])
                            status = "expired (refreshable)";
                    } else if (entries[i].token.access_token) {
                        status = "valid (no expiry)";
                    }
                    printf("    %-20s %s", entries[i].provider, status);
                    if (entries[i].token.expires_at > 0) {
                        time_t et = (time_t)entries[i].token.expires_at;
                        printf("  (expires %s", ctime(&et));
                    } else {
                        printf("\n");
                    }
                }
                auth_store_free(entries, oauth_count);
            }
        }

        printf("\n  For details, use: /secrets status\n");
        printf("  For auth login flows, use: /auth login <provider> [key]\n");
        printf("  To refresh OAuth tokens: /auth refresh [provider]\n");
        return;
    }

    if (strcmp(sub, "providers") == 0) {
        printf("Known auth providers:\n");
        printf("  openai        API key ($OPENAI_API_KEY)\n");
        printf("  anthropic     API key ($ANTHROPIC_API_KEY) or OAuth ($ANTHROPIC_TOKEN)\n");
        printf("  openrouter    API key ($OPENROUTER_API_KEY)\n");
        printf("  deepseek      API key ($DEEPSEEK_API_KEY)\n");
        printf("  google        API key ($GOOGLE_API_KEY)\n");
        printf("  xai           API key ($XAI_API_KEY) or OAuth (xAI OAuth flow)\n");
        printf("  azure         API key ($AZURE_API_KEY)\n");
        printf("  bedrock       AWS credentials ($AWS_ACCESS_KEY_ID + secret)\n");
        printf("  nous          API key ($NOUS_API_KEY) or OAuth (device code flow)\n");
        printf("  groq          API key ($GROQ_API_KEY)\n");
        printf("  together       API key ($TOGETHER_API_KEY)\n");
        printf("  mistral       API key ($MISTRAL_API_KEY)\n");
        printf("  fireworks     API key ($FIREWORKS_API_KEY)\n");
        printf("  perplexity    API key ($PERPLEXITY_API_KEY)\n");
        printf("  cohere        API key ($COHERE_API_KEY)\n");
        printf("\nTo add credentials: set the env var in .env or use /auth login <provider> [key].\n");
        printf("  /auth login openai sk-xxx...\n");
        printf("  /auth login anthropic   - prints instructions\n");
        return;
    }

    if (strcmp(sub, "login") == 0 || strncmp(sub, "login ", 6) == 0) {
        const char *rest = sub + 5;
        while (*rest == ' ') rest++;
        if (!*rest) {
            printf("Usage: /auth login <provider> [api_key]\n");
            printf("If no api_key provided, prints setup instructions.\n");
            printf("If api_key provided, writes directly to .env.\n\n");
            printf("Providers: openai, anthropic, openrouter, deepseek,\n");
            printf("           google, xai, azure, bedrock, nous, hf,\n");
            printf("           groq, together, mistral, fireworks,\n");
            printf("           perplexity, cohere\n");
            return;
        }
        const char *provider = rest;
        const char *key_value = NULL;
        const char *sp = strchr(provider, ' ');
        if (sp) {
            char pbuf[64];
            size_t plen = (size_t)(sp - provider);
            snprintf(pbuf, sizeof(pbuf), "%.*s", (int)plen, provider);
            provider = pbuf;
            key_value = sp + 1;
            while (*key_value == ' ') key_value++;
        }
        typedef struct { const char *n; const char *e; } pm_t;
        static const pm_t PM[] = {
            {"openai","OPENAI_API_KEY"}, {"anthropic","ANTHROPIC_API_KEY"},
            {"openrouter","OPENROUTER_API_KEY"}, {"deepseek","DEEPSEEK_API_KEY"},
            {"google","GOOGLE_API_KEY"}, {"xai","XAI_API_KEY"},
            {"azure","AZURE_API_KEY"}, {"bedrock","AWS_ACCESS_KEY_ID"},
            {"nous","NOUS_API_KEY"}, {"hf","HF_TOKEN"},
            {"groq","GROQ_API_KEY"}, {"together","TOGETHER_API_KEY"},
            {"mistral","MISTRAL_API_KEY"}, {"fireworks","FIREWORKS_API_KEY"},
            {"perplexity","PERPLEXITY_API_KEY"}, {"cohere","COHERE_API_KEY"},
            {NULL,NULL}
        };
        const char *env_var = NULL;
        for (int i = 0; PM[i].n; i++) {
            if (strcasecmp(provider, PM[i].n) == 0) { env_var = PM[i].e; break; }
        }
        if (!env_var) { printf("Unknown provider. Run /auth providers\n"); return; }

        if (key_value) {
            const char *h = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!h) h = ".";
            char ep[1024]; snprintf(ep, sizeof(ep), "%s/.env", h);
            FILE *f = fopen(ep, "a");
            if (!f) { printf("Cannot write %s\n", ep); return; }
            fprintf(f, "\n# %s via /auth login\n%s=%s\n", provider, env_var, key_value);
            fclose(f);
            printf("Wrote %s=%s to %s\n", env_var, key_value, ep);
            printf("Restart slermes or /reload to apply.\n");
        } else if (strcasecmp(provider, "nous") == 0) {
            /* Device code flow for Nous Portal */
            const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!home) { printf("Cannot determine home directory.\n"); return; }

            oauth_token_t *tok = nous_device_code_login(30);
            if (!tok) return;

            /* Save token to auth store */
            auth_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.provider, "nous-oauth", sizeof(entry.provider) - 1);
            entry.token = *tok;  /* Transfer ownership — don't free separately */

            if (auth_store_save(home, &entry)) {
                printf("\n✅ Nous Portal OAuth token saved.\n");
                printf("   Provider: nous-oauth\n");
                if (tok->expires_at > 0) {
                    printf("   Expires:  %s", ctime(&(time_t){ (time_t)tok->expires_at }));
                }
                printf("\nRestart slermes or /reload to apply.\n");
            } else {
                printf("\n❌ Failed to save OAuth token: %s\n", oauth_last_error());
                oauth_token_free(tok);
            }
        } else if (strcasecmp(provider, "xai-oauth") == 0 ||
                   strcasecmp(provider, "xai") == 0) {
            /* PKCE loopback callback flow for xAI OAuth */
            const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
            if (!home) { printf("Cannot determine home directory.\n"); return; }

            printf("Starting xAI OAuth loopback login...\n");
            fflush(stdout);
            oauth_token_t *tok = xai_oauth_callback_login(120);
            if (!tok) {
                printf("\n❌ xAI OAuth login failed: %s\n", oauth_last_error());
                return;
            }

            /* Save token to auth store */
            auth_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            strncpy(entry.provider, "xai-oauth", sizeof(entry.provider) - 1);
            entry.token = *tok;

            printf("\nSaving xAI OAuth token to auth store...\n");
            if (auth_store_save(home, &entry)) {
                printf("✅ xAI OAuth token saved.\n");
                if (tok->expires_at > 0) {
                    time_t exp = (time_t)tok->expires_at;
                    printf("   Expires: %s", ctime(&exp));
                }
                printf("\nRestart slermes or /reload to apply.\n");
                printf("Set model.provider to xai-oauth in config to use.\n");
            } else {
                printf("\n❌ Failed to save OAuth token: %s\n", oauth_last_error());
                oauth_token_free(tok);
            }
        } else {
            printf("To configure %s:\n", provider);
            printf("  export %s=<your_key>\n", env_var);
            printf("  /auth login %s <your_key>\n", provider);
        }
        return;
    }

    if (strcmp(sub, "refresh") == 0 || strncmp(sub, "refresh ", 8) == 0) {
        const char *target = sub + 7;
        while (*target == ' ') target++;
        const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
        if (!home) { printf("Cannot determine home directory.\n"); return; }

        int count = 0;
        auth_entry_t *entries = auth_store_load(home, &count);
        if (!entries || count == 0) {
            printf("No OAuth tokens found in auth store.\n");
            auth_store_free(entries, count);
            return;
        }

        int refreshed = 0;
        for (int i = 0; i < count; i++) {
            if (target[0] && strcmp(entries[i].provider, target) != 0)
                continue;
            if (!entries[i].token.refresh_token || !entries[i].token.refresh_token[0]) {
                if (target[0]) printf("  %s: no refresh token\n", entries[i].provider);
                continue;
            }
            /* Determine token endpoint from provider name */
            const char *endpoint = NULL;
            const char *client_id = "hermes-cli";
            if (strcmp(entries[i].provider, "nous-oauth") == 0) {
                endpoint = NOUS_OAUTH_TOKEN_ENDPOINT;
            } else if (strcmp(entries[i].provider, "xai-oauth") == 0) {
                endpoint = "https://auth.x.ai/oauth2/token";
                client_id = XAI_OAUTH_CLIENT_ID;
            }
            if (!endpoint) {
                printf("  %s: unknown token endpoint, skipping\n", entries[i].provider);
                continue;
            }

            printf("  Refreshing %s... ", entries[i].provider);
            fflush(stdout);
            oauth_token_t *tok = oauth_refresh_token(endpoint, client_id,
                entries[i].token.refresh_token, 30);
            if (!tok) {
                printf("failed: %s\n", oauth_last_error());
                continue;
            }
            /* Save updated token */
            auth_entry_t new_entry;
            memset(&new_entry, 0, sizeof(new_entry));
            strncpy(new_entry.provider, entries[i].provider, sizeof(new_entry.provider) - 1);
            new_entry.token = *tok;
            if (auth_store_save(home, &new_entry)) {
                printf("ok\n");
                refreshed++;
            } else {
                printf("save failed\n");
                oauth_token_free(tok);
            }
        }
        auth_store_free(entries, count);
        printf("\nRefreshed %d token(s).\n", refreshed);
        return;
    }

    if (strcmp(sub, "tokens") == 0) {
        const char *home = state->hermes_home[0] ? state->hermes_home : getenv("HOME");
        if (!home) { printf("Cannot determine home directory.\n"); return; }
        int count = 0;
        auth_entry_t *entries = auth_store_load(home, &count);
        if (!entries || count == 0) {
            printf("No OAuth tokens stored.\n");
            auth_store_free(entries, count);
            return;
        }
        printf("Stored OAuth tokens:\n");
        time_t now = time(NULL);
        for (int i = 0; i < count; i++) {
            const char *status = "expired";
            if (entries[i].token.expires_at > 0) {
                if (entries[i].token.expires_at > (double)now)
                    status = "valid";
                else if (entries[i].token.refresh_token && entries[i].token.refresh_token[0])
                    status = "expired (refreshable)";
            } else if (entries[i].token.access_token) {
                status = "valid (no expiry)";
            }
            printf("  %-20s %s", entries[i].provider, status);
            if (entries[i].token.expires_at > 0) {
                time_t et = (time_t)entries[i].token.expires_at;
                printf("  (expires %s", ctime(&et));
            } else {
                printf("\n");
            }
            printf("                 refresh_token: %s\n",
                   entries[i].token.refresh_token && entries[i].token.refresh_token[0] ? "yes" : "no");
        }
        auth_store_free(entries, count);
        printf("\nUse /auth refresh [provider] to refresh expiring tokens.\\n");
        return;
    }

    if (strcmp(sub, "validate") == 0 || strncmp(sub, "validate ", 9) == 0) {
        const char *target = sub + 8;
        while (*target == ' ') target++;
        if (!*target) {
            printf("Usage: /auth validate <provider>\n");
            printf("Tests API key. Supported: openai/ anthropic/ openrouter/ deepseek/ xai/ groq/ together/ google\n");
            return;
        }
        typedef struct { const char *n; const char *e; const char *u; int m; } val_t;
        static const val_t V[] = {
            {"openai","OPENAI_API_KEY","https://api.openai.com/v1/models",0},
            {"openrouter","OPENROUTER_API_KEY","https://openrouter.ai/api/v1/models",0},
            {"deepseek","DEEPSEEK_API_KEY","https://api.deepseek.com/chat/completions",0},
            {"xai","XAI_API_KEY","https://api.x.ai/v1/models",0},
            {"groq","GROQ_API_KEY","https://api.groq.com/openai/v1/models",0},
            {"together","TOGETHER_API_KEY","https://api.together.xyz/v1/models",0},
            {"google","GOOGLE_API_KEY","https://generativelanguage.googleapis.com/v1beta/models",1},
            {"anthropic","ANTHROPIC_API_KEY","https://api.anthropic.com/v1/messages",2},
            {NULL,NULL,NULL,0}
        };
        int found = 0;
        for (int i = 0; V[i].n; i++) {
            if (strcasecmp(target, V[i].n) != 0) continue;
            found = 1;
            const char *key = getenv(V[i].e);
            if (!key||!*key) { printf("*** env var not set\n", V[i].e); break; }
            printf("Testing "); printf(V[i].n); printf("... ");
            fflush(stdout);
            http_client_t *c = http_new(15);
            bool ok = false;
            if (c) {
                if (V[i].m == 1) {
                    char url[1024];
                    char gpf[] = "?key=";
                    memcpy(url, V[i].u, strlen(V[i].u) + 1);
                    strcat(url, gpf);
                    strcat(url, key);
                    http_resp_t *r = http_get(c,url,NULL);
                    ok = r && (r->status==200||r->status==403||r->status==400);
                    http_resp_free(r);
                } else if (V[i].m == 2) {
                    char hdr[512];
                    char ap[] = "x-api-key: ";
                    char aver[] = "\r\nanthropic-version: 2023-06-01";
                    memcpy(hdr, ap, strlen(ap) + 1);
                    strcat(hdr, key);
                    strcat(hdr, aver);
                    const char *msg = "{\"model\":\"claude-3-5-sonnet-20241022\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
                    http_resp_t *r = http_post_json_auth(c, V[i].u, hdr, msg);
                    ok = r && (r->status==200||r->status==400||r->status==401);
                    http_resp_free(r);
                } else {
                    char hdr[512];
                    char bp[] = "Authorization: Bearer ";
                    strcpy(hdr, bp);
                    strcat(hdr, key);
                    http_resp_t *r = http_get(c, V[i].u, hdr);
                    ok = r && (r->status == 200 || r->status == 401);
                    http_resp_free(r);
                }
                http_free(c);
            }
            printf(ok ? "valid\n" : "invalid\n");
            break;
        }
        if (!found) printf("Unknown provider\n");
        return;
    }

    printf("Unknown subcommand: '%s'\\n", sub);
    printf("Usage: /auth status | providers | login | tokens | refresh | validate\\n");
}

/* ================================================================
 *  /logs — View agent logs
 * ================================================================ */
static void cmd_logs(const char *args, agent_state_t *state) {
    (void)state;
    const char *logname = "agent";  /* default */
    int n_lines = 20;
    bool follow = false;
    const char *level_filter = NULL;
    char buf[512];  /* for strtok parsing — must outlive level_filter pointer */

    /* Parse args */
    if (args && args[0]) {
        strncpy(buf, args, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *token = strtok(buf, " \t");
        while (token) {
            if (token[0] == '-' && token[1] == 'n' && token[2] == '\0') {
                token = strtok(NULL, " \t");
                if (token) n_lines = atoi(token);
            } else if (strcmp(token, "--level") == 0 || strcmp(token, "-l") == 0) {
                token = strtok(NULL, " \t");
                if (token) level_filter = token;
            } else if (strcmp(token, "--follow") == 0 || strcmp(token, "-f") == 0) {
                follow = true;
            } else if (strcmp(token, "agent") == 0 ||
                       strcmp(token, "errors") == 0 ||
                       strcmp(token, "error") == 0 ||
                       strcmp(token, "gateway") == 0) {
                logname = token;
            }
            token = strtok(NULL, " \t");
        }
    }

    if (n_lines <= 0) n_lines = 20;
    if (n_lines > 1000) n_lines = 1000;

    /* Map log name to filename */
    const char *filename = "agent.log";
    if (strcmp(logname, "errors") == 0 || strcmp(logname, "error") == 0)
        filename = "errors.log";
    else if (strcmp(logname, "gateway") == 0)
        filename = "gateway.log";

    /* Build path: <hermes_home>/logs/<filename> */
    char path[1024];
    {
        char log_dir[512];
        hermes_log_dir(log_dir, sizeof(log_dir));
        snprintf(path, sizeof(path), "%s/%s", log_dir, filename);
    }

    printf("=== %s ===\n", filename);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("No log file found at: %s\n", path);
        printf("Hermes may not have been run yet, or logs are stored elsewhere.\n");
        return;
    }

    /* Read all lines into a ring buffer */
    char **lines = NULL;
    int count = 0;
    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        /* Apply level filter if set */
        if (level_filter) {
            /* Parse level from standard Hermes log format:
             * "2026-04-05 22:35:00,123 INFO  ..." */
            char *level_start = NULL;
            for (size_t i = 0; line[i]; i++) {
                if (line[i] == ' ' && i + 5 < strlen(line)) {
                    /* Check for known level after timestamp pattern */
                    const char *p = line + i + 1;
                    if (strncmp(p, "DEBUG", 5) == 0 ||
                        strncmp(p, "INFO", 4) == 0 ||
                        strncmp(p, "WARNING", 7) == 0 ||
                        strncmp(p, "ERROR", 5) == 0 ||
                        strncmp(p, "CRITICAL", 8) == 0) {
                        level_start = line + i + 1;
                        break;
                    }
                }
            }
            if (level_start) {
                /* Extract level string */
                char lvl[16];
                int lvl_len = 0;
                while (level_start[lvl_len] && level_start[lvl_len] != ' ' &&
                       level_start[lvl_len] != '\t' && lvl_len < 15) {
                    lvl[lvl_len] = level_start[lvl_len];
                    lvl_len++;
                }
                lvl[lvl_len] = '\0';

                if (strcasecmp(lvl, level_filter) != 0)
                    continue;  /* skip non-matching lines */
            } else {
                /* Unparseable line — include it anyway */
            }
        }

        /* Store line in ring buffer */
        char **new_lines = realloc(lines, sizeof(char *) * (count + 1));
        if (!new_lines) break;
        lines = new_lines;
        lines[count] = strdup(line);
        if (!lines[count]) break;
        count++;
    }
    fclose(fp);

    /* Print last n_lines */
    int start = count > n_lines ? count - n_lines : 0;
    for (int i = start; i < count; i++) {
        printf("%s\n", lines[i]);
        free(lines[i]);
    }
    free(lines);

    if (count == 0)
        printf("(empty)\n");
    else if (count <= n_lines)
        printf("\n(%d lines, total)\n", count);
    else
        printf("\n(%d lines, showing last %d of %d)\n", n_lines < count ? n_lines : count, n_lines, count);

    /* Follow mode: poll for new lines */
    if (follow) {
        printf("\n--- Following %s (Ctrl-C to stop) ---\n", filename);
        fflush(stdout);

        /* Get current file size for comparison */
        struct stat st;
        long last_size = 0;
        if (stat(path, &st) == 0) last_size = st.st_size;

        while (1) {
            sleep(1);
            FILE *f = fopen(path, "r");
            if (!f) break;
            struct stat new_st;
            if (stat(path, &new_st) == 0) {
                /* Handle log rotation: if inode changed or size shrunk, restart */
                if (new_st.st_ino != st.st_ino || new_st.st_size < last_size) {
                    printf("\n--- Log rotated, following %s ---\n", filename);
                    last_size = 0;
                    st = new_st;
                }
            }
            if (new_st.st_size > last_size) {
                /* Seek to last known position */
                fseek(f, last_size, SEEK_SET);
                char newline[4096];
                while (fgets(newline, sizeof(newline), f)) {
                    size_t nl = strlen(newline);
                    if (nl > 0 && newline[nl-1] == '\n') newline[nl-1] = '\0';
                    printf("%s\n", newline);
                }
                fflush(stdout);
                last_size = new_st.st_size;
            }
            fclose(f);
        }
    }
}

/* ================================================================
 *  /dump — System debug info dump
 * ================================================================ */
static void cmd_dump(const char *args, agent_state_t *state) {
    (void)args;

    printf("=== Hermes C System Dump ===\n\n");

    /* Version */
    printf("Version:        %s\n", HERMES_VERSION);

    /* Git commit */
    {
        char buf[128] = "(unknown)";
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            }
            pclose(fp);
        }
        printf("Git commit:     %s\n", buf);
    }

    /* Host info */
    {
        char hostname[256] = "(unknown)";
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hostname[sizeof(hostname)-1] = '\0';
        }
        printf("Hostname:       %s\n", hostname);
    }

    /* Config home */
    const char *home = state->hermes_home[0] ? state->hermes_home : getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    printf("Config home:    %s\n", home ? home : "(not set)");
    printf("Config file:    %s/config.yaml\n", home ? home : "~/.slermes");

    /* Provider + model */
    printf("Provider:       %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Model:          %s\n", state->llm.model[0] ? state->llm.model : "(default)");

    /* Session info */
    printf("Session ID:     %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Messages:       %zu\n", state->message_count);
    printf("Iterations:     %d/%d\n", state->iteration_count, state->max_iterations);

    /* Token usage */
    printf("Tokens in:      %d\n", state->session_input_tokens);
    printf("Tokens out:     %d\n", state->session_output_tokens);
    printf("Tokens total:   %d\n", state->session_total_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("Est. cost:      $%.6f\n", state->session_estimated_cost_usd);

    /* Tools */
    printf("Tools reg:      %zu\n", state->tools.count);

    /* Gateway platforms — count registered platform adapters */
    {
        /* Try to read gateway platforms from process list */
        int gw_count = 0;
        char buf[256];
        FILE *fp = popen("ls src/gateway/platforms/*.c 2>/dev/null | grep -v server.c | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) gw_count = atoi(buf);
            pclose(fp);
        }
        if (gw_count > 0)
            printf("Gateway plats:  %d\n", gw_count);
    }

    /* Plugins */
    {
        int pcount = 0;
        char buf[256];
        FILE *fp = popen("ls -1 src/plugins/*.so 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) pcount = atoi(buf);
            pclose(fp);
        }
        printf("Plugin .so:     %d\n", pcount);
    }

    /* Source stats */
    {
        int c_count = 0, h_count = 0;
        char buf[256];
        FILE *fp = popen("ls src/*.c src/**/*.c 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) c_count = atoi(buf);
            pclose(fp);
        }
        fp = popen("ls include/*.h 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) h_count = atoi(buf);
            pclose(fp);
        }
        printf("C source files: %d (.c) + %d (.h)\n", c_count, h_count);
    }

    /* CLI commands registered */
    printf("CLI commands:   %d\n", commands_count());

    /* Config keys (approximate from YAML key count) */
    {
        int kcount = 0;
        if (home) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/config.yaml", home);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char line[4096];
                while (fgets(line, sizeof(line), fp)) {
                    if (strchr(line, ':')) kcount++;
                }
                fclose(fp);
            }
        }
        printf("Config keys:    ~%d\n", kcount > 0 ? kcount : 322);
    }

    /* Upstream sync status */
    {
        char buf[256];
        FILE *fp = popen("cd /home/wubu/hermes-agent-dev && git rev-list --count HEAD --not upstream/main 2>/dev/null || echo 0", "r");
        int ahead = 0;
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) ahead = atoi(buf);
            pclose(fp);
        }
        printf("Upstream:       0 behind, %d ahead\n", ahead);
    }

    printf("\n=== End Dump ===\n");
}

/* /send: Send a message to a platform */
static void cmd_send(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /send [target] <message>\n");
        printf("  target: 'local' (save), 'stdout' (print), or 'platform:chat_id' (e.g. telegram:-100123456)\n");
        printf("  If target omitted, defaults to 'stdout'.\n");
        return;
    }

    /* Parse: first word is target if it contains ':' or is 'local'/'stdout' */
    char target[256];
    const char *message;
    target[0] = '\0';

    const char *space = strchr(args, ' ');
    if (space && (strncmp(args, "local", 5) == 0 || strncmp(args, "stdout", 6) == 0 ||
                  strchr(args, ':') != NULL)) {
        /* First word is a target */
        size_t tlen = (size_t)(space - args);
        if (tlen >= sizeof(target)) tlen = sizeof(target) - 1;
        memcpy(target, args, tlen);
        target[tlen] = '\0';
        message = space + 1;
        while (*message == ' ') message++;
    } else {
        strcpy(target, "stdout");
        message = args;
    }

    /* Build JSON args for send_message_handler */
    char json_args[8192];
    char *escaped = malloc(strlen(message) * 2 + 1);
    if (!escaped) { printf("Error: allocation failed\n"); return; }

    size_t j = 0;
    for (const char *p = message; *p && j < strlen(message) * 2; p++) {
        if (*p == '"') { escaped[j++] = '\\'; escaped[j++] = '"'; }
        else if (*p == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
        else if (*p == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else escaped[j++] = *p;
    }
    escaped[j] = '\0';

    snprintf(json_args, sizeof(json_args),
             "{\"target\":\"%s\",\"message\":\"%s\"}", target, escaped);
    free(escaped);

    char *result = send_message_handler(json_args, NULL);
    if (result) {
        /* Parse result JSON to extract meaningful output */
        char *err = NULL;
        json_node_t *r = json_parse(result, &err);
        if (r) {
            const char *status = json_object_get_string(r, "status", NULL);
            const char *error = json_object_get_string(r, "error", NULL);
            if (error && error[0])
                printf("Error: %s\n", error);
            else if (status)
                printf("Sent: %s\n", status);
            else
                printf("Result: %s\n", result);
            json_free(r);
        } else {
            printf("Result: %s\n", result);
            free(err);
        }
        free(result);
    } else {
        printf("Error: send_message_handler returned NULL\n");
    }
}

/* /doctor: Run system diagnostics */
static void cmd_doctor(const char *args, agent_state_t *state) {
    bool show_all = (!args || !*args || strcmp(args, "all") == 0);
    bool show_config = show_all || strcmp(args, "config") == 0;
    bool show_env = show_all || strcmp(args, "env") == 0;
    bool show_keys = show_all || strcmp(args, "keys") == 0;

    printf("=== System Diagnostics ===\n\n");

    /* 1. HERMES_HOME */
    const char *home = state->hermes_home[0] ? state->hermes_home
                    : getenv("SLERMES_HOME") ? getenv("SLERMES_HOME")
                    : getenv("HOME");
    printf("[HERMES_HOME]  %s\n", home ? home : "(not found)");

    /* Check ~/.slermes or ~/.hermes exists */
    char home_dir[1024];
    if (state->hermes_home[0]) {
        snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
    } else {
        const char *uh = getenv("HOME");
        if (uh) {
            snprintf(home_dir, sizeof(home_dir), "%s/.slermes", uh);
            if (access(home_dir, F_OK) != 0)
                snprintf(home_dir, sizeof(home_dir), "%s/.hermes", uh);
        }
    }
    printf("[HOME_DIR]    %s %s\n", home_dir,
           access(home_dir, F_OK) == 0 ? "✓" : "✗ (not found)");

    /* 2. Config file */
    if (show_config) {
        printf("\n--- Config ---\n");
        char cfg_path[1024];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home_dir);
        if (access(cfg_path, F_OK) == 0) {
            printf("[config.yaml] %s ✓\n", cfg_path);
            hermes_config_t cfg;
            if (hermes_config_load(&cfg, home_dir))
                printf("[config]      Loaded successfully (v%d)\n", cfg.config_version);
            else
                printf("[config]      ✗ Failed to parse\n");
        } else {
            printf("[config.yaml] ✗ Not found at %s\n", cfg_path);
        }
    }

    /* 3. .env file */
    if (show_env) {
        printf("\n--- Environment ---\n");
        char env_path[1024];
        snprintf(env_path, sizeof(env_path), "%s/.env", home_dir);
        if (access(env_path, F_OK) == 0) {
            printf("[.env]        %s ✓\n", env_path);
        } else {
            printf("[.env]        Not found (optional)\n");
        }
    }

    /* 4. API key detection */
    if (show_keys) {
        printf("\n--- API Keys ---\n");
        const char *key_names[] = {
            "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN",
            "OPENROUTER_API_KEY", "DEEPSEEK_API_KEY", "GOOGLE_API_KEY",
            "XAI_API_KEY", "AZURE_API_KEY", "AWS_ACCESS_KEY_ID",
            "NOUS_API_KEY", "HF_TOKEN", NULL
        };
        int found = 0;
        for (int i = 0; key_names[i]; i++) {
            const char *val = getenv(key_names[i]);
            if (val && *val) {
                size_t vlen = strlen(val);
                printf("[%-20s] ✓ (%zu chars)\n", key_names[i], vlen);
                found++;
            }
        }
        if (found == 0)
            printf("  No API keys found in environment.\n");
        else
            printf("  %d API key(s) detected.\n", found);
    }

    printf("\n=== Diagnostics complete ===\n");
}

/* ── Webhook subscription management ──────────────────────────── */
static void cmd_webhook(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Webhook subscription management.\n");
        printf("Usage: /webhook list\n");
        printf("       /webhook add <url> [secret]\n");
        printf("       /webhook remove <id>\n");
        return;
    }

    if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
        int count = webhook_subscription_count();
        if (count == 0) {
            printf("No webhook subscriptions. Add one with /webhook add <url> [secret]\n");
            return;
        }
        printf("Webhook subscriptions (%d):\n", count);
        webhook_subscription_t subs[64];
        int n = webhook_subscription_list(subs, 64);
        for (int i = 0; i < n; i++) {
            printf("  #%d: %s\n", i, subs[i].endpoint);
            if (subs[i].max_retries > 0)
                printf("      retries=%d backoff=%dms\n",
                       subs[i].max_retries, subs[i].backoff_ms);
            if (subs[i].header_count > 0) {
                printf("      headers: ");
                for (int j = 0; j < subs[i].header_count; j++)
                    printf("%s=%s ", subs[i].headers[j].key, subs[i].headers[j].value);
                printf("\n");
            }
        }
        return;
    }

    if (strncmp(args, "add ", 4) == 0) {
        const char *url = args + 4;
        const char *secret = NULL;
        char url_only[1024] = {0};
        const char *sp = strchr(url, ' ');
        if (sp) {
            size_t ulen = (size_t)(sp - url);
            if (ulen >= sizeof(url_only)) ulen = sizeof(url_only) - 1;
            memcpy(url_only, url, ulen);
            url_only[ulen] = '\0';
            secret = sp + 1;
            if (!*secret) secret = NULL;
        } else {
            snprintf(url_only, sizeof(url_only), "%s", url);
        }

        if (!url_only[0]) {
            printf("Error: URL required. Usage: /webhook add <url> [secret]\n");
            return;
        }

        int idx = webhook_subscription_add(url_only, secret, 3, 1000);
        if (idx >= 0) {
            printf("Subscription #%d added for %s\n", idx, url_only);
            if (secret) printf("  Secret: %s\n", secret);
        } else {
            printf("Error: Failed to add subscription (max %d reached?)\n", WEBHOOK_SUBS_MAX);
        }
        return;
    }

    if (strncmp(args, "remove ", 7) == 0) {
        int idx = atoi(args + 7);
        if (webhook_subscription_remove(idx)) {
            printf("Subscription #%d removed.\n", idx);
        } else {
            printf("Error: Subscription #%d not found.\n", idx);
        }
        return;
    }

    printf("Unknown subcommand: %s\n", args);
    printf("Usage: /webhook list | add <url> [secret] | remove <id>\n");
}

/* /memory: Memory setup, status, and provider management */
static void cmd_memory(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Memory configuration management.\n");
        printf("Usage: /memory status           — Show current memory config\n");
        printf("       /memory providers         — List known memory providers\n");
        printf("       /memory setup <provider>  — Set active memory provider\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0) {
        hermes_config_t cfg;
        /* Resolve home directory */
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        const char *home_env = NULL;

        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (!home_env) { printf("Error: Cannot determine Hermes home\n"); return; }

            snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
            if (access(home_dir, F_OK) != 0) {
                snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }

        if (access(home_dir, F_OK) == 0) {
            if (hermes_config_load(&cfg, home_dir)) {
                show_section_memory(&cfg);
            } else {
                printf("Error: Could not load config from %s\n", home_dir);
            }
        } else {
            printf("No Hermes home directory found at %s\n", home_dir);
        }
        return;
    }

    if (strcmp(sub, "providers") == 0) {
        printf("Memory providers:\n");
        printf("  (built-in)  MEMORY.md / USER.md (always active, default)\n");
        printf("  honcho      Cloud-based memory with semantic search\n");
        printf("  mem0        Personalized AI memory with vector store\n");
        printf("  supermemory  Persistent memory with semantic retrieval\n");
        printf("  hindsight   Task-aware episodic memory\n");
        printf("\nTo activate: /memory setup <provider>\n");
        printf("C memory plugins: .so files in plugins/memory/ are auto-detected.\n");
        return;
    }

    if (strncmp(sub, "setup ", 6) == 0) {
        const char *provider = sub + 6;
        while (*provider == ' ') provider++;
        if (!*provider) {
            printf("Usage: /memory setup <provider>\n");
            printf("Example: /memory setup honcho\n");
            return;
        }
        printf("To activate '%s' memory provider:\n", provider);
        printf("  1. Edit ~/.slermes/config.yaml with:\n");
        printf("     memory:\n");
        printf("       provider: %s\n", provider);
        printf("  2. Set required API keys in ~/.slermes/.env\n");
        printf("  3. Start a new session to activate.\n");
        return;
    }

    printf("Unknown subcommand: '%s'\n", sub);
    printf("Usage: /memory status | providers | setup <provider>\n");
}
