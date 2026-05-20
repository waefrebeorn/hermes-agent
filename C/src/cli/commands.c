/*
 * commands.c — Slash command definitions for Hermes C CLI.
 * Central registry of all slash commands.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Command definition */
typedef struct {
    const char *name;
    const char *alias;
    const char *description;
    void (*handler)(const char *args, agent_state_t *state);
} command_def_t;

/* Forward declarations of handlers */
static void cmd_help(const char *args, agent_state_t *state);
static void cmd_exit(const char *args, agent_state_t *state);
static void cmd_clear(const char *args, agent_state_t *state);
static void cmd_model(const char *args, agent_state_t *state);

/* Registry */
static const command_def_t COMMANDS[] = {
    {"/help",  "/h",    "Show this help message",                  cmd_help},
    {"/exit",  "/quit", "Exit the program",                        cmd_exit},
    {"/clear", "/c",    "Clear conversation context",               cmd_clear},
    {"/model", "/m",    "Show current model configuration",         cmd_model},
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

/* ================================================================
 *  Handlers
 * ================================================================ */

static void cmd_help(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Commands:\n");
    for (int i = 0; COMMANDS[i].name; i++) {
        printf("  %s", COMMANDS[i].name);
        if (COMMANDS[i].alias)
            printf(" (%s)", COMMANDS[i].alias);
        printf(" — %s\n", COMMANDS[i].description);
    }
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
    (void)args;
    printf("Model: %s\n", state->llm.model);
    printf("Provider: %s\n", state->llm.provider);
    printf("Base URL: %s\n", state->llm.base_url);
    printf("Max turns: %d\n", state->max_iterations);
}
