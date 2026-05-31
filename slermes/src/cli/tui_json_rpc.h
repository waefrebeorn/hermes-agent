#ifndef TUI_JSON_RPC_H
#define TUI_JSON_RPC_H

/*
 * tui_json_rpc.h — JSON-RPC 2.0 gateway server for TUI backend.
 *
 * Replaces ad-hoc strstr/sscanf parsing with a proper dispatch table,
 * JSON parameter extraction, request ID tracking, and standard error
 * responses.
 *
 * Usage:
 *   tui_rpc_register("print", handler_print, "Display info message");
 *   const char *response = tui_rpc_dispatch(request_json);
 *   // response is NULL for notifications, allocated string for requests
 *   if (response) { write(fd, response, strlen(response)); free(response); }
 */

#include <stdbool.h>

/* ── JSON-RPC 2.0 error codes ── */
#define JSON_RPC_PARSE_ERROR     -32700
#define JSON_RPC_INVALID_REQUEST -32600
#define JSON_RPC_METHOD_NOT_FOUND -32601
#define JSON_RPC_INVALID_PARAMS  -32602
#define JSON_RPC_INTERNAL_ERROR  -32603

/* ── Handler function signature ──
 * Handler receives parsed JSON params node and a scratch buffer (4096 bytes).
 * Should return:
 *   NULL  → notification (no response expected)
 *   msg   → result JSON string (caller must free, e.g. "\"ok\"" or "42" or "{}")
 */
typedef const char *(*tui_rpc_handler_t)(const void *params, char *scratch, size_t scratch_sz);

/* ── Registered method entry ── */
typedef struct {
    const char        *method;     /* JSON-RPC method name */
    tui_rpc_handler_t  handler;    /* Handler function */
    const char        *desc;       /* Human-readable description */
} tui_rpc_method_t;

/* ── Library initialization ── */
/* Register built-in methods; call once at TUI startup */
void tui_rpc_init(void);

/* ── Method registration ── */
/* Register a new RPC method handler (not thread-safe; call at init time) */
void tui_rpc_register(const char *method, tui_rpc_handler_t handler, const char *desc);

/* ── Dispatch ── */
/* Parse and dispatch a single JSON-RPC message.
 * For notifications (no "id" field): returns NULL, no action needed.
 * For requests: returns allocated result JSON string (caller must free).
 * On error: returns allocated error JSON string (caller must free).
 * Pass RPC_FIFO_PATH for unidirectional mode (no response expected) as fifo_dir
 *   to auto-send responses back via out-FIFO. Pass NULL for manual handling.
 */
const char *tui_rpc_dispatch(const char *request_json,
                             char *out_response, size_t out_sz);

/* ── Response builders ── */
/* Build a JSON-RPC 2.0 success response into scratch buffer */
void tui_rpc_build_result(char *buf, size_t sz, int id, const char *result_json);

/* Build a JSON-RPC 2.0 error response into scratch buffer */
void tui_rpc_build_error(char *buf, size_t sz, int id, int code,
                         const char *message, const void *data);

/* ── Parameter extraction (uses hermes_json / libjson) ── */
/* Extract string param from JSON params object. Returns default on missing. */
const char *tui_rpc_param_string(const void *params, const char *key,
                                  const char *default_val);

/* Extract integer param from JSON params object. Returns default on missing. */
int tui_rpc_param_int(const void *params, const char *key, int default_val);

/* Extract boolean param. Returns default on missing. */
bool tui_rpc_param_bool(const void *params, const char *key, bool default_val);

/* Extract double param. */
double tui_rpc_param_double(const void *params, const char *key, double default_val);

/* ── Query registered methods ── */
/* Returns NULL-terminated array of all registered methods (for help/list). */
const tui_rpc_method_t **tui_rpc_get_all(void);

#endif /* TUI_JSON_RPC_H */
