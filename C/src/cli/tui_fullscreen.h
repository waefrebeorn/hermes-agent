#ifndef TUI_FULLSCREEN_H
#define TUI_FULLSCREEN_H

/*
 * tui_fullscreen.h — Full ncurses TUI for Hermes C (P189-P200).
 * MIT License — WuBu Hermes Project
 *
 * 12 phases:
 *   P189: Layout — split panes (message history | input | tool feed | status bar)
 *   P190: Input — multi-line, emoji picker, slash autocomplete
 *   P191: Message display — role colors, syntax highlight, markdown
 *   P192: Streaming — token streaming, real-time token counter
 *   P193: Tool feed — live tool call status, progress bar, result preview
 *   P194: Status bar — model/provider, tokens, iterations, budget
 *   P195: Session browser — list, search, preview, load/delete/export
 *   P196: Config editor — interactive key browser, set/get/explain
 *   P197: Image viewer — sixel/kitty inline display, zoom/pan
 *   P198: Theme engine — skin files, color schemes, fonts
 *   P199: Gateway — JSON-RPC backend, split TUI and agent processes
 *   P200: Mobile mode — responsive layout, touch input, compact status
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Core API === */

/* Initialize and run the full-screen TUI. Returns 0 on success. */
int tui_fullscreen_run(agent_state_t *state);

/* === Display API (called from agent/gateway) === */

/* Print a message to the output pane (role-colored) */
void tui_fullscreen_print(const char *fmt, ...);

/* Print an error message (red) */
void tui_fullscreen_error(const char *fmt, ...);

/* Print a warning message (yellow) */
void tui_fullscreen_warn(const char *fmt, ...);

/* Stream a token to the output pane */
void tui_fullscreen_stream_token(const char *token);

/* Signal end of streaming */
void tui_fullscreen_stream_done(void);

/* Update tool feed with current tool call status */
void tui_fullscreen_tool_status(const char *tool_name, const char *status,
                                 int progress, int total);

/* Update status bar with agent runtime info */
void tui_fullscreen_status_update(const char *model, const char *provider,
                                    int iteration, int max_iterations,
                                    int tokens_in, int tokens_out,
                                    double budget_remaining);

/* === Session Browser === */
void tui_fullscreen_session_browse(void);

/* === Config Editor === */
void tui_fullscreen_config_edit(void);

/* === Theme === */
void tui_fullscreen_theme_reload(const char *skin_name);

#ifdef __cplusplus
}
#endif

#endif /* TUI_FULLSCREEN_H */
