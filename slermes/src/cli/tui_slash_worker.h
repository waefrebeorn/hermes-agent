#ifndef TUI_SLASH_WORKER_H
#define TUI_SLASH_WORKER_H

/*
 * tui_slash_worker.h — TUI Slash Command Worker (T06)
 * MIT License — WuBu Slermes Project
 *
 * Dispatch-table-based slash command handler for the ncurses TUI.
 * Replaces the flat if/else chain in tui_process_input with a
 * registered command system supporting argument parsing, categories,
 * and programmatic registration.
 *
 * Python equivalent: tui_gateway/slash_worker.py
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Command category ──────────────────────────────────────── */

typedef enum {
    SLASH_CAT_TUI,       /* TUI display/layout commands (/help, /clear, /mobile) */
    SLASH_CAT_AGENT,     /* Agent runtime commands (/model, /tokens, /budget) */
    SLASH_CAT_SESSION,   /* Session management (/save, /load, /export, /delete) */
    SLASH_CAT_MODAL,     /* Modal overlay commands (/sessions, /config, /gateway) */
    SLASH_CAT_META,      /* Meta commands (/quit, /exit, /undo, /reset) */
    SLASH_CAT_SKILL,     /* Skill-managed commands */
} slash_category_t;

/* ── Command handler signature ──────────────────────────────── */

/* A handler receives the full line, the argc/argv parsed arguments,
 * and an opaque TUI state pointer (agent_state_t).
 * Returns true if the command was handled, false if unknown. */
typedef bool (*slash_handler_t)(const char *line, int argc, char **argv,
                                 void *tui_state);

/* ── Registered command entry ───────────────────────────────── */

typedef struct {
    const char *cmd;           /* "/help", "/model", etc. */
    const char *desc;          /* One-line description */
    const char *args;          /* Arg hints: "<model>" or "[name]" or "" */
    slash_category_t category; /* Category for grouping */
    slash_handler_t handler;   /* Callback function */
    void *userdata;            /* Opaque data passed to handler */
} slash_cmd_entry_t;

/* ── API ────────────────────────────────────────────────────── */

/*
 * Initialize the slash worker system.
 * Must be called once before registration/dispatch.
 * tui_state: opaque pointer (agent_state_t) passed to all handlers.
 * Returns true on success.
 */
bool tui_slash_init(void *tui_state);

/*
 * Shutdown the slash worker: clear registrations, free resources.
 */
void tui_slash_shutdown(void);

/*
 * Register a slash command.
 * cmd: "/command" format, must be unique (asserts if duplicate).
 * Returns true on success.
 */
bool tui_slash_register(const slash_cmd_entry_t *entry);

/*
 * Register a batch of commands from an array (NULL-terminated).
 * Returns the number registered.
 */
int tui_slash_register_batch(const slash_cmd_entry_t entries[]);

/*
 * Dispatch a line. If line[0] == '/', tries to find and invoke a handler.
 * Returns true if handled (including unknown commands displayed as error).
 * This is the main entry point — replaces the if/else chain.
 */
bool tui_slash_dispatch(const char *line);

/*
 * Find a registered command by name.
 * Returns the entry, or NULL if not found.
 */
const slash_cmd_entry_t *tui_slash_find(const char *cmd_name);

/*
 * Get the full registered command table (NULL-terminated).
 * Used by help display and autocomplete.
 */
const slash_cmd_entry_t **tui_slash_get_all(void);

/*
 * Get the count of registered commands.
 */
int tui_slash_count(void);

/*
 * Generate help text for a specific category.
 * Returns a malloc'd string, caller must free.
 */
char *tui_slash_help_for_category(slash_category_t cat);

/*
 * Unregister a command by name.
 */
void tui_slash_unregister(const char *cmd);

/*
 * Parse a line into argc/argv.
 * max_args: max number of argv slots (must be >= 1).
 * allocates argv slots with strdup'd strings — caller frees with tui_slash_free_args().
 */
int tui_slash_parse(const char *line, int max_args, char **argv);

/*
 * Free argv allocated by tui_slash_parse().
 */
void tui_slash_free_args(int argc, char **argv);

/*
 * Get category name as a string (for help display).
 */
const char *tui_slash_category_name(slash_category_t cat);

#ifdef __cplusplus
}
#endif

#endif /* TUI_SLASH_WORKER_H */
