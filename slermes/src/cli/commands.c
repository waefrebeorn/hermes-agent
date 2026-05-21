/*
 * commands.c — Slash command definitions for Hermes C CLI.
 * Central registry of all slash commands. Phase 51-60: CLI parity.
 */

#include "slermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Command definition — full struct (forward-declared in hermes.h) */
struct command_def_t {
    const char *name;
    const char *alias;
    const char *description;
    void (*handler)(const char *args, agent_state_t *state);
};

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
    {"/commands","/cmds", "List all available slash commands",             cmd_commands},

    /* Help and exit */
    {"/help",    "/h",    "Show help for commands: /help [command]",       cmd_help},
    {"/exit",    "/quit", "Exit the program",                              cmd_exit},
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
    /* Partial match: check if input matches the start of any command */
    size_t inlen = strlen(input);
    for (int i = 0; COMMANDS[i].name; i++) {
        if (strncmp(input, COMMANDS[i].name, inlen) == 0)
            return &COMMANDS[i];
    }
    return NULL;
}

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
    context_clear(state);
    printf("Context cleared.\n");
}

static void cmd_model(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Set model */
        snprintf(state->llm.model, sizeof(state->llm.model), "%s", args);
        printf("Model set to: %s\n", state->llm.model);
        return;
    }
    printf("Model:    %s\n", state->llm.model);
    printf("Provider: %s\n", state->llm.provider);
    printf("Base URL: %s\n", state->llm.base_url);
    printf("Max turns: %d\n", state->max_iterations);
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
}

static void cmd_conv(const char *args, agent_state_t *state) {
    (void)args;
    size_t n = state->message_count;
    size_t start = (n > 10) ? n - 10 : 0;
    printf("Recent conversation (%zu-%zu of %zu):\n", start + 1, n, n);
    for (size_t i = start; i < n; i++) {
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
        } else {
            printf("  [%s] (no content)\n", role_str);
        }
    }
}

static void cmd_new(const char *args, agent_state_t *state) {
    (void)args;
    /* Reset session: clear messages, generate new ID */
    context_clear(state);
    agent_generate_session_id(state);
    printf("New session started: %s\n", state->session_id);
}

static void cmd_undo(const char *args, agent_state_t *state) {
    (void)args;
    /* Remove last assistant message (and any tool messages after it) */
    if (state->message_count == 0) {
        printf("No messages to undo.\n");
        return;
    }
    /* Remove from the end backwards until we find a user or system message */
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
    (void)args;
    size_t n = state->message_count;
    if (n == 0) {
        printf("No conversation history.\n");
        return;
    }
    printf("Full conversation (%zu messages):\n", n);
    for (size_t i = 0; i < n; i++) {
        const char *role_str;
        switch (state->messages[i]->role) {
            case MSG_SYSTEM:    role_str = "system";    break;
            case MSG_USER:      role_str = "user";      break;
            case MSG_ASSISTANT: role_str = "assistant"; break;
            case MSG_TOOL:      role_str = "tool";      break;
            default:            role_str = "?";         break;
        }
        const char *content = state->messages[i]->content;
        if (content) {
            char preview[101];
            size_t clen = strlen(content);
            if (clen > 100) {
                memcpy(preview, content, 97);
                preview[97] = '.'; preview[98] = '.'; preview[99] = '.';
                preview[100] = '\0';
            } else {
                memcpy(preview, content, clen + 1);
            }
            printf("  [%d] %s: %s\n", (int)i, role_str, preview);
        } else {
            printf("  [%d] %s: (no content)\n", (int)i, role_str);
        }
    }
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

static void cmd_config(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current configuration:\n");
        printf("  model:      %s\n", state->llm.model);
        printf("  provider:   %s\n", state->llm.provider);
        printf("  base_url:   %s\n", state->llm.base_url);
        printf("  max_turns:  %d\n", state->max_iterations);
        printf("  messages:   %zu\n", state->message_count);
        return;
    }
    printf("Config editing not yet implemented. Use /model to set model.\n");
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
        printf(" — %s\n", state->tools.tools[i].description);
    }
}
