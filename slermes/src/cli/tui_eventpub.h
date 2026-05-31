#ifndef TUI_EVENTPUB_H
#define TUI_EVENTPUB_H

/*
 * tui_eventpub.h — TUI Event Publisher (T07)
 * MIT License — WuBu Slermes Project
 *
 * Typed event system for the TUI backend. Emits structured events through
 * the FIFO gateway transport as JSON-RPC 2.0 notifications. Supports
 * in-process dispatch and FIFO serialization for remote clients.
 *
 * Python equivalent: tui_gateway/event_publisher.py
 */

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Event types ────────────────────────────────────────────── */

typedef enum {
    TUI_EVENT_NONE = 0,

    /* Input events */
    TUI_EVENT_KEYBOARD,       /* Raw keyboard input */
    TUI_EVENT_COMMAND,        /* Slash command issued */

    /* Display events */
    TUI_EVENT_RESIZE,         /* Terminal resize */
    TUI_EVENT_MESSAGE_USER,       /* User message added to history */
    TUI_EVENT_MESSAGE_ASSISTANT,  /* Assistant message added */
    TUI_EVENT_MESSAGE_TOOL,       /* Tool output message */
    TUI_EVENT_MESSAGE_ERROR,      /* Error message */
    TUI_EVENT_MESSAGE_INFO,       /* Info/status message */

    /* Streaming events */
    TUI_EVENT_STREAM_TOKEN,   /* Single token received */
    TUI_EVENT_STREAM_DONE,    /* Stream finished */
    TUI_EVENT_STREAM_ERROR,   /* Stream error */

    /* Tool events */
    TUI_EVENT_TOOL_CALL,      /* Tool call started */
    TUI_EVENT_TOOL_STATUS,    /* Tool status update (progress) */
    TUI_EVENT_TOOL_RESULT,    /* Tool result received */

    /* Session events */
    TUI_EVENT_SESSION_CHANGE, /* Session switched/created/loaded */
    TUI_EVENT_SESSION_DELETE, /* Session deleted */

    /* Agent / runtime events */
    TUI_EVENT_AGENT_UPDATE,   /* Agent state change (model, iter, budget) */
    TUI_EVENT_STATUS_UPDATE,  /* Status bar update */
    TUI_EVENT_THEME_CHANGE,   /* Theme/skin changed */
    TUI_EVENT_MODEL_CHANGE,   /* Model changed */

    /* Connection events */
    TUI_EVENT_GATEWAY_CONNECT,   /* Gateway connected */
    TUI_EVENT_GATEWAY_DISCONNECT,/* Gateway disconnected */

    TUI_EVENT_COUNT
} tui_event_type_t;

/* ── Event payload ──────────────────────────────────────────── */

/* Keyboard event */
typedef struct {
    int ch;                    /* Raw keycode */
    char utf8[8];              /* UTF-8 representation if printable */
} tui_keyboard_event_t;

/* Command event */
typedef struct {
    char command[256];         /* Full command line */
    char argv[16][64];         /* Parsed arguments (up to 16) */
    int  argc;                 /* Argument count */
} tui_command_event_t;

/* Resize event */
typedef struct {
    int rows, cols;            /* New terminal dimensions */
} tui_resize_event_t;

/* Message event */
typedef struct {
    int  role;                 /* MSG_ROLE_USER/ASSISTANT/TOOL/ERROR/INFO */
    char text[4096];           /* Message text (truncated) */
    bool is_header;            /* Header/separator message */
} tui_message_event_t;

/* Stream token event */
typedef struct {
    char token[512];           /* Token text */
    int  tokens_so_far;        /* Cumulative token count */
} tui_stream_event_t;

/* Tool event */
typedef struct {
    char tool_name[128];       /* Tool name */
    char status[32];           /* "running" / "done" / "error" / "cancelled" */
    int  progress;             /* Current progress (0-100) */
    int  total;                /* Total steps */
    char result_preview[512];  /* Truncated result text */
} tui_tool_event_t;

/* Session event */
typedef struct {
    char session_id[128];      /* Session identifier */
    char action[32];           /* "created" / "loaded" / "saved" / "deleted" */
} tui_session_event_t;

/* Agent update event */
typedef struct {
    char model[256];           /* Current model name */
    char provider[64];         /* Current provider */
    int  iteration;            /* Current iteration */
    int  max_iterations;       /* Max iterations */
    int  tokens_in;            /* Input token count */
    int  tokens_out;           /* Output token count */
    double budget_remaining;   /* Budget remaining (or -1) */
} tui_agent_event_t;

/* Status bar update event */
typedef struct {
    char mode[32];             /* "idle" / "streaming" / "thinking" / "busy" */
    char model[64];            /* Current model */
    char provider[64];         /* Current provider */
    int  iteration;            /* Iteration count */
    int  max_iterations;       /* Max iterations */
    int  tokens_in;
    int  tokens_out;
    int  ctx_percent;          /* Context usage percentage */
    double budget;
} tui_status_event_t;

/* Theme event */
typedef struct {
    char theme_name[64];       /* Theme/skin name */
} tui_theme_event_t;

/* Model change event */
typedef struct {
    char model_name[256];      /* New model name */
    char provider[64];         /* New provider */
} tui_model_event_t;

/* Connection event */
typedef struct {
    bool success;              /* Whether connect/disconnect was clean */
    char reason[128];          /* Reason or error message */
} tui_connection_event_t;

/* ── Unified event structure ────────────────────────────────── */

typedef struct {
    tui_event_type_t type;
    struct timespec timestamp;   /* Monotonic timestamp */
    bool            published;   /* Whether already sent via FIFO */

    union {
        tui_keyboard_event_t    keyboard;
        tui_command_event_t     command;
        tui_resize_event_t      resize;
        tui_message_event_t     message;
        tui_stream_event_t      stream;
        tui_tool_event_t        tool;
        tui_session_event_t     session;
        tui_agent_event_t       agent;
        tui_status_event_t      status;
        tui_theme_event_t       theme;
        tui_model_event_t       model;
        tui_connection_event_t  connection;
    } data;
} tui_event_t;

/* ── Event callback ─────────────────────────────────────────── */

/* Callback receives a const event pointer. Must not free it. */
typedef void (*tui_event_callback_t)(const tui_event_t *ev, void *userdata);

/* ── API ────────────────────────────────────────────────────── */

/*
 * Initialize the event publisher system.
 * Must be called once before any other eventpub functions.
 * fifo_path: path to FIFO for outbound events (NULL = no FIFO).
 * Returns true on success.
 */
bool tui_eventpub_init(const char *fifo_path);

/*
 * Shutdown the event publisher: flush remaining events, close FIFO.
 */
void tui_eventpub_shutdown(void);

/*
 * Emit an event. The event is:
 *   1. Dispatched synchronously to all registered callbacks.
 *   2. Serialized as JSON-RPC 2.0 and written to the FIFO (if open).
 *
 * Returns the number of callbacks that handled the event.
 */
int tui_eventpub_emit(const tui_event_t *event);

/*
 * Convenience: emit a keyboard event.
 */
void tui_eventpub_keyboard(int ch, const char *utf8);

/*
 * Convenience: emit a command event.
 */
void tui_eventpub_command(const char *line);

/*
 * Convenience: emit a resize event.
 */
void tui_eventpub_resize(int rows, int cols);

/*
 * Convenience: emit a message event by role.
 */
void tui_eventpub_message(int role, const char *text, bool is_header);

/*
 * Convenience: emit a stream token event.
 */
void tui_eventpub_stream_token(const char *token, int tokens_so_far);

/*
 * Convenience: emit stream done event.
 */
void tui_eventpub_stream_done(void);

/*
 * Convenience: emit tool call/status/result event.
 */
void tui_eventpub_tool(const char *name, const char *status,
                       int progress, int total, const char *result_preview);

/*
 * Convenience: emit session change event.
 */
void tui_eventpub_session(const char *session_id, const char *action);

/*
 * Convenience: emit agent update event.
 */
void tui_eventpub_agent(const char *model, const char *provider,
                        int iteration, int max_iters,
                        int tokens_in, int tokens_out, double budget);

/*
 * Convenience: emit status update event.
 */
void tui_eventpub_status(const char *mode, const char *model,
                         const char *provider,
                         int iteration, int max_iters,
                         int tokens_in, int tokens_out,
                         int ctx_pct, double budget);

/*
 * Convenience: emit theme change event.
 */
void tui_eventpub_theme(const char *theme_name);

/*
 * Convenience: emit model change event.
 */
void tui_eventpub_model(const char *model_name, const char *provider);

/*
 * Convenience: emit connection event.
 */
void tui_eventpub_connection(bool success, const char *reason);

/*
 * Subscribe a callback to all events (or a specific type).
 * type: TUI_EVENT_NONE = all events, otherwise filter to one type.
 * userdata: opaque pointer passed to callback.
 * Returns subscription ID (>0) or 0 on failure.
 */
int tui_eventpub_subscribe(tui_event_type_t type,
                           tui_event_callback_t cb, void *userdata);

/*
 * Unsubscribe a callback by subscription ID.
 */
void tui_eventpub_unsubscribe(int sub_id);

/*
 * Flush any pending events to FIFO.
 */
void tui_eventpub_flush(void);

/*
 * Serialize an event to JSON-RPC 2.0 notification string.
 * Returns a malloc'd string (caller must free), or NULL on error.
 */
char *tui_eventpub_to_json(const tui_event_t *event);

/*
 * Enable/disable FIFO output at runtime.
 */
void tui_eventpub_set_fifo_enabled(bool enabled);

/*
 * Check if FIFO is open and writable.
 */
bool tui_eventpub_fifo_is_open(void);

/*
 * Reconnect FIFO (e.g. after gateway restart).
 */
bool tui_eventpub_fifo_reconnect(const char *fifo_path);

/*
 * Get the event type name as a string (debug/display).
 */
const char *tui_eventpub_type_name(tui_event_type_t type);

/*
 * Enable/disable verbose debug logging.
 */
void tui_eventpub_set_debug(bool debug);

#ifdef __cplusplus
}
#endif

#endif /* TUI_EVENTPUB_H */
