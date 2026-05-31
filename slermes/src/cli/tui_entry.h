#ifndef TUI_ENTRY_H
#define TUI_ENTRY_H

/*
 * tui_entry.h — TUI Entry/Startup Module (T05)
 * MIT License — WuBu Slermes Project
 *
 * Wraps the TUI lifecycle: pre-flight checks, ncurses initialization,
 * signal handling, main loop, and graceful shutdown.
 *
 * Python equivalent: tui_gateway/entry.py
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Startup result ─────────────────────────────────────────── */

typedef enum {
    TUI_START_OK = 0,           /* TUI started successfully */
    TUI_START_NO_COLOR,         /* Terminal doesn't support color */
    TUI_START_TOO_SMALL,        /* Terminal too small */
    TUI_START_NO_TERM,          /* Not a terminal */
    TUI_START_INIT_FAILED,      /* ncurses initscr failed */
    TUI_START_NO_STATE,         /* NULL agent state */
    TUI_START_ERRNO,            /* System error */
} tui_start_result_t;

/* ── Shutdown reason ────────────────────────────────────────── */

typedef enum {
    TUI_EXIT_USER_QUIT,         /* User typed /quit or Ctrl+C at prompt */
    TUI_EXIT_SIGINT,            /* Interrupted by SIGINT during streaming */
    TUI_EXIT_SIGTERM,           /* Terminated by SIGTERM */
    TUI_EXIT_ERROR,             /* Fatal error during run */
    TUI_EXIT_NORMAL,            /* Normal end of tui_entry_run() */
} tui_exit_reason_t;

/* ── Lifecycle callbacks ────────────────────────────────────── */

/* Called after successful init, before running main loop.
 * Return false to abort startup. */
typedef bool (*tui_entry_post_init_cb_t)(void *userdata);

/* Called at start of shutdown sequence. */
typedef void (*tui_entry_pre_shutdown_cb_t)(void *userdata);

/* ── Config ─────────────────────────────────────────────────── */

typedef struct {
    /* Minimum terminal dimensions */
    int min_rows;
    int min_cols;

    /* Callbacks */
    tui_entry_post_init_cb_t   post_init;
    tui_entry_pre_shutdown_cb_t pre_shutdown;
    void *userdata;

    /* Whether to emit events to tui_eventpub */
    bool enable_events;

    /* Welcome message to show (NULL = default) */
    const char *welcome_message;
} tui_entry_config_t;

/* ── Default config ─────────────────────────────────────────── */

#define TUI_ENTRY_DEFAULT_CONFIG { \
    .min_rows = 8, \
    .min_cols = 40, \
    .post_init = NULL, \
    .pre_shutdown = NULL, \
    .userdata = NULL, \
    .enable_events = true, \
    .welcome_message = NULL, \
}

/* ── API ────────────────────────────────────────────────────── */

/*
 * Run a pre-flight check of the terminal.
 * Checks: isatty, TERM env var, color support.
 * Returns true if terminal appears capable.
 */
bool tui_entry_check_terminal(void);

/*
 * Get a human-readable message for a startup result code.
 * Returns a static string (do not free).
 */
const char *tui_entry_result_message(tui_start_result_t result);

/*
 * Get a human-readable name for an exit reason.
 * Returns a static string (do not free).
 */
const char *tui_entry_exit_name(tui_exit_reason_t reason);

/*
 * Initialize and run the TUI. This is the top-level entry point:
 *   1. Runs pre-flight checks
 *   2. Calls tui_fullscreen_init (ncurses, themes, subsystems)
 *   3. Runs post-init callback
 *   4. Enters main event loop
 *   5. On exit, runs pre-shutdown callback + cleanup
 *   6. Returns exit reason code
 *
 * agent_state: the agent state to use (must be non-NULL).
 * config: entry configuration (NULL = defaults).
 * exit_reason: [out] populated with the exit reason.
 *
 * Returns TUI_START_OK on successful run, or an error code if
 * startup failed before entering the loop.
 */
tui_start_result_t tui_entry_run(void *agent_state,
                                  const tui_entry_config_t *config,
                                  tui_exit_reason_t *exit_reason);

/*
 * Request a graceful shutdown of the TUI (sets running=false).
 * Safe to call from signal handlers (sets an atomic flag).
 */
void tui_entry_request_stop(void);

/*
 * Check if a stop has been requested.
 */
bool tui_entry_stop_requested(void);

/*
 * Get the current exit reason (only valid after tui_entry_run returns).
 */
tui_exit_reason_t tui_entry_get_exit_reason(void);

#ifdef __cplusplus
}
#endif

#endif /* TUI_ENTRY_H */
