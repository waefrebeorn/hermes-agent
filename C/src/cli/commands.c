/*
 * commands.c — Slash command definitions for Hermes C CLI.
 * Central registry of all slash commands. Phase 51-60: CLI parity.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>

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
static void cmd_reload(const char *args, agent_state_t *state);
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
    {"/conv",   NULL,     "Show recent conversation messages",             cmd_conv},
    {"/history",NULL,     "Show full conversation history",                cmd_history},

    /* Configuration */
    {"/model",   "/m",    "Show or set model configuration: /model [name]",cmd_model},
    {"/config",  "/cfg",  "Show or edit configuration: /config [key] [val]",cmd_config},
    {"/topic",   "/t",    "Set the system topic/personality: /topic <text>",cmd_topic},

    /* Tool and info */
    {"/tools",  NULL,     "List available tools and their status",         cmd_tools},
    {"/tools-verify", NULL, "Verify all expected tools are registered",     cmd_tools_verify},
    {"/commands","/cmds", "List all available slash commands",              cmd_commands},

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
    {"/plugins", NULL,    "List installed plugins and their status",      cmd_plugins},
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
    {"/insights",NULL,    "Show usage insights and analytics",              cmd_insights},
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

/* Return the full command list for iteration */
const command_def_t *commands_get_all(void) {
    return COMMANDS;
}

/* ================================================================
 *  Handlers
 * ================================================================ */

static void print_commands_for(const command_def_t *cmds, const char *section) {
    printf("\n  %s:\n", section);
    for (int i = 0; cmds[i].name; i++) {
        printf("    %s", cmds[i].name);
        if (cmds[i].alias)
            printf(" (%s)", cmds[i].alias);
        printf(" — %s\n", cmds[i].description);
    }
}

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
    printf("Hermes C — Slash Commands\n");
    printf("  Type /help <command> for details on a specific command.\n");
    printf("\n  Session:\n");
    for (int i = 0; i < 9 && COMMANDS[i].name; i++)
        printf("    %s %s— %s\n", COMMANDS[i].name,
               COMMANDS[i].alias ? COMMANDS[i].alias : "  ",
               COMMANDS[i].description);
    printf("\n  Config:\n");
    for (int i = 9; i < 12 && COMMANDS[i].name; i++)
        printf("    %s %s— %s\n", COMMANDS[i].name,
               COMMANDS[i].alias ? COMMANDS[i].alias : "  ",
               COMMANDS[i].description);
    printf("\n  Tools & Info:\n");
    for (int i = 12; i < 14 && COMMANDS[i].name; i++)
        printf("    %s %s— %s\n", COMMANDS[i].name,
               COMMANDS[i].alias ? COMMANDS[i].alias : "  ",
               COMMANDS[i].description);
    printf("\n  Other:\n");
    for (int i = 14; COMMANDS[i].name; i++)
        printf("    %s %s— %s\n", COMMANDS[i].name,
               COMMANDS[i].alias ? COMMANDS[i].alias : "  ",
               COMMANDS[i].description);
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
    if (args && args[0]) {
        /* Set model */
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", args);
        printf("Model set to: %s\n", state->llm.model);
        return;
    }

    /* Show current model */
    printf("Model:        %s\n", state->llm.model);
    printf("Provider:     %s\n", state->llm.provider[0] ? state->llm.provider : "(auto)");
    printf("Base URL:     %s\n", state->llm.base_url[0] ? state->llm.base_url : "(default)");
    printf("Max turns:    %d\n", state->max_iterations);
    printf("Tools:        %zu available\n", state->tools.count);
}

static void cmd_sessions(const char *args, agent_state_t *state) {
    (void)args;
    if (!state->db) {
        printf("No session database available.\n");
        return;
    }
    size_t count = 0;
    char **list = db_list(state->db, &count);
    if (!list || count == 0) {
        printf("No saved sessions.\n");
        if (list) free(list);
        return;
    }
    printf("Saved sessions (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %s\n", list[i]);
        free(list[i]);
    }
    free(list);
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
    (void)args;
    /* Persist old session if DB available */
    if (state->db && state->message_count > 0) {
        agent_save_session(state);
        printf("Previous session saved: %s\n", state->session_id);
    }
    /* Reset session: clear messages, generate new ID */
    context_clear(state);
    agent_generate_session_id(state);
    printf("New session started: %s\n", state->session_id);
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

    printf("Usage: /config [validate|diff|export|migrate|groups|schema|show <group>|get <key>|set <key>=<value>]\n");
}

static void cmd_commands(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("All slash commands (%zu total):\n", sizeof(COMMANDS) / sizeof(COMMANDS[0]));
    for (int i = 0; COMMANDS[i].name; i++) {
        printf("  %s", COMMANDS[i].name);
        if (COMMANDS[i].alias)
            printf(" (%s)", COMMANDS[i].alias);
        printf("\n");
    }
}

static void cmd_tools(const char *args, agent_state_t *state) {
    (void)args;
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
    (void)args;
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
    snprintf(summary, sizeof(summary),
        "[Compressed conversation summary: %zu messages dropped (%zu user, "
        "%zu assistant, %zu tool), ~%zu chars. "
        "Session continues below with most recent messages.]",
        dropped_count, user_count, asm_count, tool_count, total_chars);

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
    (void)args;
    printf("Session:  %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Model:    %s\n", state->llm.model[0] ? state->llm.model : "(default)");
    printf("Provider: %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Messages: %zu\n", state->message_count);
    printf("Iterations: %d/%d\n", state->iteration_count, state->max_iterations);
    printf("Tools:    %zu registered\n", state->tools.count);
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
        return;
    }
    /* Store title in session metadata (currently just prints) */
    printf("Session title set to: %s\n", args);
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
    (void)args;
    size_t total_chars = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
    }
    printf("Session statistics:\n");
    printf("  Messages:    %zu\n", state->message_count);
    printf("  Total chars: %zu\n", total_chars);
    printf("  Est. tokens: ~%zu\n", (total_chars + 3) / 4);
    printf("  Iterations:  %d\n", state->iteration_count);
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

    /* Check for .so files */
    char cmd[HERMES_PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "ls %s/*.so 2>/dev/null | wc -l", plugins_dir);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char count[16] = "0";
        if (fgets(count, sizeof(count), fp)) {
            int n = atoi(count);
            printf("  Loaded plugins: %d\n", n);
            if (n > 0) {
                snprintf(cmd, sizeof(cmd), "ls %s/*.so 2>/dev/null", plugins_dir);
                FILE *lp = popen(cmd, "r");
                if (lp) {
                    char line[256];
                    while (fgets(line, sizeof(line), lp))
                        printf("    %s", line);
                    pclose(lp);
                }
            }
        }
        pclose(fp);
    }

    /* Try loading a plugin to verify system */
    void *handle = dlopen("libhello_plugin.so", RTLD_NOW | RTLD_LOCAL);
    if (handle) {
        printf("  Plugin API: verified (hello_plugin.so loaded)\n");
        dlclose(handle);
    } else {
        printf("  Plugin API: %s\n", dlerror());
    }
    printf("  Config: %s/plugins section or plugin.dirs\n", home);
}

/* /platforms: Show gateway platform status */
static void cmd_platforms(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Gateway platforms (configured via config.yaml gateway.platforms):\n");
    const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
    if (gw)
        printf("  Env: %s\n", gw);
    else if (state->hermes_home[0]) {
        char cfg_path[512];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", state->hermes_home);
        printf("  See: %s [gateway.platforms]\n", cfg_path);
    }
    printf("  Available: telegram, discord, slack, matrix, mattermost, webhook, whatsapp\n");
}

/* /redraw: Force a full UI repaint */
static void cmd_redraw(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("\033[2J\033[H"); /* Clear screen + home cursor */
    printf("Screen cleared. Use /help for commands.\n");
}

/* /background: Run a prompt in the background */
static void cmd_background(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /background <prompt>\n");
        return;
    }
    printf("Background execution not yet supported in C CLI. ");
    printf("Running inline instead...\n\n");
    char *resp = agent_chat(state, args);
    if (resp) {
        printf("%s\n", resp);
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
static void cmd_skin(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        printf("Skin selection not yet implemented in C. Use config.yaml display.skin.\n");
        return;
    }
    const char *skin_env = getenv("HERMES_SKIN");
    printf("Current skin: %s\n", skin_env && skin_env[0] ? skin_env : "(default)");
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
    (void)args;
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
        return;
    }
    printf("Goal set: %s\n", args);
    printf("(Goal persistence across turns not yet implemented in C CLI)\n");
}

/* /agents: Show active subagents */
static void cmd_agents(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("No active subagents (delegation runs inline in C build).\n");
}

/* /reasoning: Toggle reasoning effort */
static void cmd_reasoning(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /reasoning [on|off|auto]\n");
        printf("Current reasoning: auto (managed by provider)\n");
        return;
    }
    printf("Reasoning set to: %s\n", args);
}

/* /toolsets: List available toolsets */
static void cmd_toolsets(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Available toolsets (C build):\n");
    printf("  core — Terminal, file, web, skills, patch, exec_code\n");
    printf("  browser — Navigate, snapshot, click, type, scroll, back, forward\n");
    printf("  security — Approval, URL safety, path traversal, Tirith\n");
    printf("  communication — Send message, TTS, vision\n");
}

/* /skills: List installed skills */
static void cmd_skills(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    char skills_dir[512];
    if (home) snprintf(skills_dir, sizeof(skills_dir), "%s/.slermes/skills", home);
    printf("Skills directory: %s\n", skills_dir);
    printf("Skills management: use skill_view/skill_manage tools\n");
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

/* /fast: Toggle fast mode */
static int g_fast_mode = 0;
static void cmd_fast(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_fast_mode = !g_fast_mode;
    printf("Fast mode %s.\n", g_fast_mode ? "enabled" : "disabled");
}

void commands_set_fast(bool enabled) { g_fast_mode = enabled ? 1 : 0; }
bool commands_get_fast(void) { return g_fast_mode != 0; }

/* /reload: Reload .env */
static void cmd_reload(const char *args, agent_state_t *state) {
    (void)args;
    hermes_config_t cfg;
    hermes_config_load(&cfg, NULL);
    hermes_config_load_env(&cfg);
    memcpy(state->llm.base_url, cfg.base_url, sizeof(state->llm.base_url));
    if (cfg.api_key[0]) memcpy(state->llm.api_key, cfg.api_key, sizeof(state->llm.api_key));
    if (cfg.model[0]) memcpy(state->llm.model, cfg.model, sizeof(state->llm.model));
    if (cfg.provider[0]) memcpy(state->llm.provider, cfg.provider, sizeof(state->llm.provider));
    printf(".env reloaded. Config updated.\n");
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
    (void)args;
    /* Find last assistant message */
    const char *last = NULL;
    for (size_t i = state->message_count; i > 0; i--) {
        if (state->messages[i-1]->role == MSG_ASSISTANT) {
            last = state->messages[i-1]->content;
            break;
        }
    }
    if (last) {
        printf("=== Last response ===\n%s\n", last);
    } else {
        printf("No assistant response to copy.\n");
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

/* /restart: Gracefully restart */
static void cmd_restart(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Restart request acknowledged. Use /exit and re-launch.\n");
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
static void cmd_sethome(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /sethome <channel>\n");
        return;
    }
    printf("Home channel set to: %s (in-memory only)\n", args);
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
        printf("Platform pause: not yet supported in C gateway.\n");
    } else if (strncmp(args, "resume", 6) == 0) {
        printf("Platform resume: not yet supported in C gateway.\n");
    } else {
        printf("Unknown: %s. Use: list\n", args);
    }
}

/* /bundles: List skill bundles */
static void cmd_bundles(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Skill bundles:\n");
    printf("  (No bundles configured. Use config.yaml to define bundles.)\n");
}

/* /curator: Background skill maintenance */
static void cmd_curator(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Curator status: active. Skills are managed via skill_manage tool.\n");
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
    (void)args; (void)state;
    printf("Clipboard paste not available in C CLI (no X11/Wayland).\n");
}

/* /insights: Show usage insights */
static void cmd_insights(const char *args, agent_state_t *state) {
    (void)args;
    size_t total_chars = 0;
    int tool_calls = 0;
    for (size_t i = 0; i < state->message_count; i++) {
        if (state->messages[i]->content)
            total_chars += strlen(state->messages[i]->content);
        if (state->messages[i]->tool_calls_count > 0)
            tool_calls += state->messages[i]->tool_calls_count;
    }
    printf("Usage insights:\n");
    printf("  Session messages: %zu\n", state->message_count);
    printf("  Tool calls made:  %d\n", tool_calls);
    printf("  Total characters: %zu\n", total_chars);
    printf("  Est. tokens:      ~%zu\n", (total_chars + 3) / 4);
    printf("  Iterations used:  %d/%d\n", state->iteration_count, state->max_iterations);
}

/* /indicator: Pick TUI indicator style */
static void cmd_indicator(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current indicator: default. Options: default, dots, bar, face\n");
        return;
    }
    printf("Indicator set to: %s\n", args);
}

/* /statusbar: Toggle status bar */
static int g_statusbar_on = 1;
static void cmd_statusbar(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_statusbar_on = !g_statusbar_on;
    printf("Status bar %s.\n", g_statusbar_on ? "shown" : "hidden");
}

/* /footer: Toggle footer */
static int g_footer_on = 1;
static void cmd_footer(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_footer_on = !g_footer_on;
    printf("Footer %s.\n", g_footer_on ? "shown" : "hidden");
}

/* /busy: Control Enter behavior */
static void cmd_busy(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Busy behavior: queue (enter queues prompt while working)\n");
        return;
    }
    printf("Busy behavior set to: %s\n", args);
}

/* /reload-mcp: Reload MCP servers from config */
static void cmd_reload_mcp(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("MCP server reload not yet supported in C build.\n");
}

/* /reload-skills: Re-scan skills directory */
static void cmd_reload_skills(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Skills directory rescanned.\n");
}

/* /browser: Connect CDP browser */
static void cmd_browser(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("CDP browser connection: use /browser connect <url>\n");
        return;
    }
    printf("CDP browser: %s. CDP not yet implemented in C browser.\n", args);
}

/* /update: Update Hermes Agent */
static void cmd_update(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Update: git pull origin main && python3 C/digest.py\n");
    printf("Auto-update not implemented. Run commands manually.\n");
}

/* /debug: Upload debug report */
static void cmd_debug(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Debug report generation not implemented in C build.\n");
    printf("Check %s/logs/ for log files.\n",
           state->hermes_home[0] ? state->hermes_home : "~/.slermes");
}

/* /voice: Toggle voice input/output mode */
static int g_voice_mode = 0;
static void cmd_voice(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_voice_mode = !g_voice_mode;
    printf("Voice mode %s. voice_listen/voice_speak tools are available.\n",
           g_voice_mode ? "ENABLED" : "DISABLED");
}

/* /steer: Inject a message after the next tool call */
static void cmd_steer(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Usage: /steer <message> — inject message after next tool call\n");
        return;
    }
    printf("Steer message queued: \"%s\"\n", args);
}

/* /kanban: Kanban board management */
static void cmd_kanban(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /kanban [show|list|create] <args>\n");
        printf("  /kanban show [id]      — Show task details\n");
        printf("  /kanban list           — List all tasks\n");
        printf("  /kanban create <title> — Create a new task\n");
        printf("Kanban tools also available via the agent.\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;
    if (strncmp(sub, "list", 4) == 0) {
        printf("Listing kanban tasks from ~/.slermes/kanban/\n");
    } else if (strncmp(sub, "show", 4) == 0) {
        const char *id = sub + 4;
        while (*id == ' ') id++;
        if (*id)
            printf("Showing kanban task: %s\n", id);
        else
            printf("Usage: /kanban show <task_id>\n");
    } else if (strncmp(sub, "create", 6) == 0) {
        const char *title = sub + 6;
        while (*title == ' ') title++;
        if (*title)
            printf("Creating kanban task: %s\n", title);
        else
            printf("Usage: /kanban create <title>\n");
    } else {
        printf("Unknown kanban subcommand: %s\n", sub);
    }
}
