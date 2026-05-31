/*
 * tui_entry.c — TUI Entry/Startup Module (T05)
 * MIT License — WuBu Slermes Project
 *
 * Wraps the TUI lifecycle: pre-flight checks, ncurses initialization,
 * signal handling, main loop, and graceful shutdown.
 *
 * Python equivalent: tui_gateway/entry.py
 */

#include "tui_entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>

/* Forward declaration for agent_state_t (avoids pulling in hermes.h) */
struct agent_state;
typedef struct agent_state agent_state_t;

#include "tui_fullscreen.h"
#include "tui_eventpub.h"

/* ── Internals ──────────────────────────────────────────────── */

static volatile sig_atomic_t g_stop_requested = 0;
static tui_exit_reason_t g_exit_reason = TUI_EXIT_NORMAL;
static bool g_in_entry_run = false;

/* Signal handler for SIGTERM — request graceful stop */
static void sigterm_handler(int sig) {
    (void)sig;
    g_stop_requested = 1;
}

/* ── Pre-flight checks ─────────────────────────────────────── */

bool tui_entry_check_terminal(void) {
    /* Check if stdout is a terminal */
    if (!isatty(STDOUT_FILENO))
        return false;

    /* Check TERM is set */
    const char *term = getenv("TERM");
    if (!term || !term[0])
        return false;

    /* Check for dumb/smart terminals that don't support ncurses */
    if (strcmp(term, "dumb") == 0 || strcmp(term, "emacs") == 0)
        return false;

    return true;
}

static tui_start_result_t run_preflight(void) {
    /* Check TERM */
    if (!tui_entry_check_terminal())
        return TUI_START_NO_TERM;

    return TUI_START_OK;
}

/* ── Error messages ─────────────────────────────────────────── */

const char *tui_entry_result_message(tui_start_result_t result) {
    switch (result) {
        case TUI_START_OK:         return "TUI started successfully";
        case TUI_START_NO_COLOR:   return "Terminal does not support color — use CLI mode (slermes) instead of slermes-tui";
        case TUI_START_TOO_SMALL:  return "Terminal too small — need at least 40x8";
        case TUI_START_NO_TERM:    return "Not a terminal or TERM=dumb — run in a proper terminal or use CLI mode";
        case TUI_START_INIT_FAILED: return "ncurses initialization failed";
        case TUI_START_NO_STATE:   return "NULL agent state — internal error";
        case TUI_START_ERRNO:      return "System error during TUI startup";
    }
    return "Unknown startup result";
}

const char *tui_entry_exit_name(tui_exit_reason_t reason) {
    switch (reason) {
        case TUI_EXIT_USER_QUIT:  return "user_quit";
        case TUI_EXIT_SIGINT:     return "sigint";
        case TUI_EXIT_SIGTERM:    return "sigterm";
        case TUI_EXIT_ERROR:      return "error";
        case TUI_EXIT_NORMAL:     return "normal";
    }
    return "unknown";
}

/* ── Core entry point ───────────────────────────────────────── */

tui_start_result_t tui_entry_run(void *agent_state,
                                  const tui_entry_config_t *config,
                                  tui_exit_reason_t *exit_reason) {
    /* Use default config if none provided */
    tui_entry_config_t cfg = TUI_ENTRY_DEFAULT_CONFIG;
    if (config)
        cfg = *config;

    g_in_entry_run = true;
    g_stop_requested = 0;
    g_exit_reason = TUI_EXIT_NORMAL;

    /* Step 1: Pre-flight checks */
    tui_start_result_t preflight = run_preflight();
    if (preflight != TUI_START_OK)
        return preflight;

    /* Step 2: Set up SIGTERM handler (SIGINT and SIGWINCH handled by TUI) */
    struct sigaction old_sa;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, &old_sa);

    /* Step 3: Run the TUI */
    if (!agent_state) {
        sigaction(SIGTERM, &old_sa, NULL);
        return TUI_START_NO_STATE;
    }

    /* tui_fullscreen_run handles init + loop + cleanup internally */
    int result = tui_fullscreen_run((agent_state_t *)agent_state);

    /* Step 4: Determine exit reason */
    if (g_stop_requested)
        g_exit_reason = TUI_EXIT_SIGTERM;
    else if (result != 0)
        g_exit_reason = TUI_EXIT_ERROR;
    else {
        /* Determine from TUI state if user quit */
        /* (tui.running is static to tui_fullscreen.c, can't access here) */
        g_exit_reason = TUI_EXIT_NORMAL;
    }

    /* Restore previous SIGTERM handler */
    sigaction(SIGTERM, &old_sa, NULL);

    if (exit_reason)
        *exit_reason = g_exit_reason;

    /* Emit shutdown event */
    if (cfg.enable_events) {
        tui_eventpub_connection(false, tui_entry_exit_name(g_exit_reason));
        tui_eventpub_flush();
    }

    g_in_entry_run = false;
    return TUI_START_OK;
}

void tui_entry_request_stop(void) {
    g_stop_requested = 1;
    /* Also set the TUI's running flag — signal-safe */
    /* The TUI handles this via tui.running in the main loop */
}

bool tui_entry_stop_requested(void) {
    return g_stop_requested != 0;
}

tui_exit_reason_t tui_entry_get_exit_reason(void) {
    return g_exit_reason;
}
