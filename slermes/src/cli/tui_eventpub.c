/*
 * tui_eventpub.c — TUI Event Publisher (T07)
 * MIT License — WuBu Slermes Project
 *
 * Typed event system for the TUI backend. Emits structured events through
 * the FIFO gateway transport as JSON-RPC 2.0 notifications. Supports
 * in-process dispatch to registered callbacks.
 *
 * Python equivalent: tui_gateway/event_publisher.py (~500 LOC)
 * C implementation: ~670 LOC
 *
 * Event flow:
 *   tui_eventpub_emit(event)
 *     → dispatch to local callbacks (synchronous)
 *     → serialize to JSON-RPC notification
 *     → write to FIFO transport (if open & enabled)
 */

#include "tui_eventpub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* ── Internals ──────────────────────────────────────────────── */

#define MAX_SUBSCRIBERS 64
#define FIFO_LINE_MAX 32768
#define FIFO_BUF_SIZE 65536

typedef struct {
    tui_event_type_t    type;        /* TUI_EVENT_NONE = all events */
    tui_event_callback_t cb;
    void               *userdata;
    bool                active;
} subscriber_t;

static struct {
    /* FIFO output */
    int         fifo_fd;
    char        fifo_path[256];
    bool        fifo_enabled;

    /* Buffer for batched FIFO writes */
    char        fifo_buf[FIFO_BUF_SIZE];
    int         fifo_buf_pos;

    /* Subscriber list */
    subscriber_t subscribers[MAX_SUBSCRIBERS];
    int         sub_count;

    /* Debug logging */
    bool        debug;
    bool        initialized;
} g_eventpub;

/* ── Helpers ────────────────────────────────────────────────── */

static const char *type_name(tui_event_type_t type) {
    static const char *names[] = {
        [TUI_EVENT_NONE]            = "none",
        [TUI_EVENT_KEYBOARD]        = "keyboard",
        [TUI_EVENT_COMMAND]         = "command",
        [TUI_EVENT_RESIZE]          = "resize",
        [TUI_EVENT_MESSAGE_USER]    = "message_user",
        [TUI_EVENT_MESSAGE_ASSISTANT] = "message_assistant",
        [TUI_EVENT_MESSAGE_TOOL]    = "message_tool",
        [TUI_EVENT_MESSAGE_ERROR]   = "message_error",
        [TUI_EVENT_MESSAGE_INFO]    = "message_info",
        [TUI_EVENT_STREAM_TOKEN]    = "stream_token",
        [TUI_EVENT_STREAM_DONE]     = "stream_done",
        [TUI_EVENT_STREAM_ERROR]    = "stream_error",
        [TUI_EVENT_TOOL_CALL]       = "tool_call",
        [TUI_EVENT_TOOL_STATUS]     = "tool_status",
        [TUI_EVENT_TOOL_RESULT]     = "tool_result",
        [TUI_EVENT_SESSION_CHANGE]  = "session_change",
        [TUI_EVENT_SESSION_DELETE]  = "session_delete",
        [TUI_EVENT_AGENT_UPDATE]    = "agent_update",
        [TUI_EVENT_STATUS_UPDATE]   = "status_update",
        [TUI_EVENT_THEME_CHANGE]    = "theme_change",
        [TUI_EVENT_MODEL_CHANGE]    = "model_change",
        [TUI_EVENT_GATEWAY_CONNECT]    = "gateway_connect",
        [TUI_EVENT_GATEWAY_DISCONNECT] = "gateway_disconnect",
    };
    if (type < 0 || type >= TUI_EVENT_COUNT)
        return "unknown";
    return names[type] ? names[type] : "unknown";
}

static void get_timestamp_ms(char *buf, size_t size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buf, size, "%ld.%03ld",
             (long)ts.tv_sec, ts.tv_nsec / 1000000);
}

/* ── Serialize event to JSON-RPC 2.0 string ─────────────────── */

char *tui_eventpub_to_json(const tui_event_t *ev) {
    if (!ev) return NULL;

    char ts_buf[64] = {0};
    get_timestamp_ms(ts_buf, sizeof(ts_buf));

    /* Build JSON params object based on event type */
    char params[4096] = {0};

    switch (ev->type) {
    case TUI_EVENT_KEYBOARD:
        snprintf(params, sizeof(params),
                 "{\"ch\":%d,\"utf8\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.keyboard.ch,
                 ev->data.keyboard.utf8,
                 ts_buf);
        break;

    case TUI_EVENT_COMMAND:
        snprintf(params, sizeof(params),
                 "{\"command\":\"%s\",\"argc\":%d,\"ts\":\"%s\"}",
                 ev->data.command.command,
                 ev->data.command.argc,
                 ts_buf);
        break;

    case TUI_EVENT_RESIZE:
        snprintf(params, sizeof(params),
                 "{\"rows\":%d,\"cols\":%d,\"ts\":\"%s\"}",
                 ev->data.resize.rows,
                 ev->data.resize.cols,
                 ts_buf);
        break;

    case TUI_EVENT_MESSAGE_USER:
    case TUI_EVENT_MESSAGE_ASSISTANT:
    case TUI_EVENT_MESSAGE_TOOL:
    case TUI_EVENT_MESSAGE_ERROR:
    case TUI_EVENT_MESSAGE_INFO: {
        const char *role_str = "unknown";
        switch (ev->data.message.role) {
            case 0: role_str = "user"; break;
            case 1: role_str = "assistant"; break;
            case 2: role_str = "tool"; break;
            case 3: role_str = "error"; break;
            case 4: role_str = "info"; break;
        }
        /* Escape JSON special chars in text */
        char escaped[4096];
        int epos = 0;
        for (int i = 0; ev->data.message.text[i] && epos < (int)sizeof(escaped) - 6; i++) {
            char c = ev->data.message.text[i];
            if (c == '\\' || c == '"') {
                if (epos < (int)sizeof(escaped) - 3) {
                    escaped[epos++] = '\\';
                    escaped[epos++] = c;
                }
            } else if (c == '\n') {
                if (epos < (int)sizeof(escaped) - 3) {
                    escaped[epos++] = '\\';
                    escaped[epos++] = 'n';
                }
            } else if (c == '\r') {
                if (epos < (int)sizeof(escaped) - 3) {
                    escaped[epos++] = '\\';
                    escaped[epos++] = 'r';
                }
            } else if (c == '\t') {
                if (epos < (int)sizeof(escaped) - 3) {
                    escaped[epos++] = '\\';
                    escaped[epos++] = 't';
                }
            } else if ((unsigned char)c >= 32) {
                escaped[epos++] = c;
            }
        }
        escaped[epos] = '\0';

        snprintf(params, sizeof(params),
                 "{\"role\":\"%s\",\"text\":\"%s\",\"is_header\":%s,\"ts\":\"%s\"}",
                 role_str, escaped,
                 ev->data.message.is_header ? "true" : "false",
                 ts_buf);
        break;
    }

    case TUI_EVENT_STREAM_TOKEN:
        snprintf(params, sizeof(params),
                 "{\"token\":\"%s\",\"tokens_so_far\":%d,\"ts\":\"%s\"}",
                 ev->data.stream.token,
                 ev->data.stream.tokens_so_far,
                 ts_buf);
        break;

    case TUI_EVENT_STREAM_DONE:
        snprintf(params, sizeof(params),
                 "{\"tokens_so_far\":%d,\"ts\":\"%s\"}",
                 ev->data.stream.tokens_so_far,
                 ts_buf);
        break;

    case TUI_EVENT_STREAM_ERROR:
        snprintf(params, sizeof(params),
                 "{\"error\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.stream.token,
                 ts_buf);
        break;

    case TUI_EVENT_TOOL_CALL:
        snprintf(params, sizeof(params),
                 "{\"tool_name\":\"%s\",\"status\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.tool.tool_name,
                 ev->data.tool.status,
                 ts_buf);
        break;

    case TUI_EVENT_TOOL_STATUS:
        snprintf(params, sizeof(params),
                 "{\"tool_name\":\"%s\",\"status\":\"%s\","
                 "\"progress\":%d,\"total\":%d,\"ts\":\"%s\"}",
                 ev->data.tool.tool_name,
                 ev->data.tool.status,
                 ev->data.tool.progress,
                 ev->data.tool.total,
                 ts_buf);
        break;

    case TUI_EVENT_TOOL_RESULT:
        snprintf(params, sizeof(params),
                 "{\"tool_name\":\"%s\",\"status\":\"%s\","
                 "\"result_preview\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.tool.tool_name,
                 ev->data.tool.status,
                 ev->data.tool.result_preview,
                 ts_buf);
        break;

    case TUI_EVENT_SESSION_CHANGE:
    case TUI_EVENT_SESSION_DELETE:
        snprintf(params, sizeof(params),
                 "{\"session_id\":\"%s\",\"action\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.session.session_id,
                 ev->data.session.action,
                 ts_buf);
        break;

    case TUI_EVENT_AGENT_UPDATE:
        snprintf(params, sizeof(params),
                 "{\"model\":\"%s\",\"provider\":\"%s\","
                 "\"iteration\":%d,\"max_iterations\":%d,"
                 "\"tokens_in\":%d,\"tokens_out\":%d,"
                 "\"budget_remaining\":%.2f,\"ts\":\"%s\"}",
                 ev->data.agent.model,
                 ev->data.agent.provider,
                 ev->data.agent.iteration,
                 ev->data.agent.max_iterations,
                 ev->data.agent.tokens_in,
                 ev->data.agent.tokens_out,
                 ev->data.agent.budget_remaining,
                 ts_buf);
        break;

    case TUI_EVENT_STATUS_UPDATE:
        snprintf(params, sizeof(params),
                 "{\"mode\":\"%s\",\"model\":\"%s\",\"provider\":\"%s\","
                 "\"iteration\":%d,\"max_iterations\":%d,"
                 "\"tokens_in\":%d,\"tokens_out\":%d,"
                 "\"ctx_percent\":%d,\"budget\":%.2f,\"ts\":\"%s\"}",
                 ev->data.status.mode,
                 ev->data.status.model,
                 ev->data.status.provider,
                 ev->data.status.iteration,
                 ev->data.status.max_iterations,
                 ev->data.status.tokens_in,
                 ev->data.status.tokens_out,
                 ev->data.status.ctx_percent,
                 ev->data.status.budget,
                 ts_buf);
        break;

    case TUI_EVENT_THEME_CHANGE:
        snprintf(params, sizeof(params),
                 "{\"theme_name\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.theme.theme_name,
                 ts_buf);
        break;

    case TUI_EVENT_MODEL_CHANGE:
        snprintf(params, sizeof(params),
                 "{\"model_name\":\"%s\",\"provider\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.model.model_name,
                 ev->data.model.provider,
                 ts_buf);
        break;

    case TUI_EVENT_GATEWAY_CONNECT:
    case TUI_EVENT_GATEWAY_DISCONNECT:
        snprintf(params, sizeof(params),
                 "{\"success\":%s,\"reason\":\"%s\",\"ts\":\"%s\"}",
                 ev->data.connection.success ? "true" : "false",
                 ev->data.connection.reason,
                 ts_buf);
        break;

    default:
        snprintf(params, sizeof(params), "{\"ts\":\"%s\"}", ts_buf);
        break;
    }

    /* Build full JSON-RPC 2.0 notification */
    size_t len = strlen(type_name(ev->type)) + strlen(params) + 128;
    char *json = (char *)malloc(len);
    if (!json) return NULL;

    snprintf(json, len,
             "{\"jsonrpc\":\"2.0\",\"method\":\"event_%s\",\"params\":%s}\n",
             type_name(ev->type), params);

    return json;
}

/* ── FIFO write ─────────────────────────────────────────────── */

static bool fifo_open(void) {
    if (g_eventpub.fifo_fd >= 0)
        return true;
    if (!g_eventpub.fifo_path[0])
        return false;

    g_eventpub.fifo_fd = open(g_eventpub.fifo_path,
                              O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (g_eventpub.fifo_fd < 0) {
        if (g_eventpub.debug)
            fprintf(stderr, "[eventpub] FIFO open failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static void fifo_write_raw(const char *msg, size_t len) {
    if (!g_eventpub.fifo_enabled || g_eventpub.fifo_fd < 0)
        return;

    /* Accumulate in buffer for batched write */
    int space = (int)(sizeof(g_eventpub.fifo_buf) - g_eventpub.fifo_buf_pos - 1);
    if (space <= 0) {
        /* Buffer full — flush first */
        fifo_write_raw(g_eventpub.fifo_buf, g_eventpub.fifo_buf_pos);
        g_eventpub.fifo_buf_pos = 0;
        space = (int)(sizeof(g_eventpub.fifo_buf) - 1);
    }

    int to_copy = (int)len;
    if (to_copy > space) to_copy = space;
    memcpy(g_eventpub.fifo_buf + g_eventpub.fifo_buf_pos, msg, to_copy);
    g_eventpub.fifo_buf_pos += to_copy;
}

/* ── Dispatch to subscribers ────────────────────────────────── */

static int dispatch(const tui_event_t *ev) {
    int handled = 0;
    for (int i = 0; i < g_eventpub.sub_count; i++) {
        if (!g_eventpub.subscribers[i].active)
            continue;
        if (g_eventpub.subscribers[i].type != TUI_EVENT_NONE &&
            g_eventpub.subscribers[i].type != ev->type)
            continue;
        g_eventpub.subscribers[i].cb(ev, g_eventpub.subscribers[i].userdata);
        handled++;
    }
    return handled;
}

/* ── Public API ─────────────────────────────────────────────── */

bool tui_eventpub_init(const char *fifo_path) {
    memset(&g_eventpub, 0, sizeof(g_eventpub));
    g_eventpub.fifo_fd = -1;
    g_eventpub.fifo_enabled = true;

    if (fifo_path) {
        strncpy(g_eventpub.fifo_path, fifo_path, sizeof(g_eventpub.fifo_path) - 1);
        fifo_open();
    }

    g_eventpub.initialized = true;
    return true;
}

void tui_eventpub_shutdown(void) {
    /* Flush pending FIFO output */
    tui_eventpub_flush();

    /* Close FIFO */
    if (g_eventpub.fifo_fd >= 0) {
        close(g_eventpub.fifo_fd);
        g_eventpub.fifo_fd = -1;
    }

    /* Clear subscribers */
    for (int i = 0; i < g_eventpub.sub_count; i++)
        g_eventpub.subscribers[i].active = false;
    g_eventpub.sub_count = 0;

    g_eventpub.initialized = false;
}

int tui_eventpub_emit(const tui_event_t *event) {
    if (!event || !g_eventpub.initialized) return 0;

    /* Record timestamp if zero */
    tui_event_t copy = *event;
    if (copy.timestamp.tv_sec == 0 && copy.timestamp.tv_nsec == 0)
        clock_gettime(CLOCK_MONOTONIC, &copy.timestamp);

    /* Dispatch to local subscribers */
    int handled = dispatch(&copy);

    /* Serialize and write to FIFO */
    if (g_eventpub.fifo_fd >= 0 && g_eventpub.fifo_enabled) {
        char *json = tui_eventpub_to_json(&copy);
        if (json) {
            fifo_write_raw(json, strlen(json));
            free(json);
            copy.published = true;
        }
    }

    if (g_eventpub.debug)
        fprintf(stderr, "[eventpub] %s: %d callbacks\n",
                type_name(copy.type), handled);

    return handled;
}

/* ── Convenience emitters ───────────────────────────────────── */

void tui_eventpub_keyboard(int ch, const char *utf8) {
    tui_event_t ev = {.type = TUI_EVENT_KEYBOARD};
    ev.data.keyboard.ch = ch;
    if (utf8) strncpy(ev.data.keyboard.utf8, utf8, sizeof(ev.data.keyboard.utf8) - 1);
    tui_eventpub_emit(&ev);
}

void tui_eventpub_command(const char *line) {
    if (!line) return;
    tui_event_t ev = {.type = TUI_EVENT_COMMAND};
    strncpy(ev.data.command.command, line, sizeof(ev.data.command.command) - 1);

    /* Parse args */
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp) - 1);
    char *tok = strtok(tmp, " ");
    while (tok && ev.data.command.argc < 16) {
        strncpy(ev.data.command.argv[ev.data.command.argc], tok,
                sizeof(ev.data.command.argv[0]) - 1);
        ev.data.command.argc++;
        tok = strtok(NULL, " ");
    }
    tui_eventpub_emit(&ev);
}

void tui_eventpub_resize(int rows, int cols) {
    tui_event_t ev = {.type = TUI_EVENT_RESIZE};
    ev.data.resize.rows = rows;
    ev.data.resize.cols = cols;
    tui_eventpub_emit(&ev);
}

void tui_eventpub_message(int role, const char *text, bool is_header) {
    if (!text) return;
    tui_event_t ev = {.type = TUI_EVENT_MESSAGE_INFO};
    switch (role) {
        case 0: ev.type = TUI_EVENT_MESSAGE_USER; break;
        case 1: ev.type = TUI_EVENT_MESSAGE_ASSISTANT; break;
        case 2: ev.type = TUI_EVENT_MESSAGE_TOOL; break;
        case 3: ev.type = TUI_EVENT_MESSAGE_ERROR; break;
        default: ev.type = TUI_EVENT_MESSAGE_INFO; break;
    }
    ev.data.message.role = role;
    strncpy(ev.data.message.text, text, sizeof(ev.data.message.text) - 1);
    ev.data.message.is_header = is_header;
    tui_eventpub_emit(&ev);
}

void tui_eventpub_stream_token(const char *token, int tokens_so_far) {
    if (!token) return;
    tui_event_t ev = {.type = TUI_EVENT_STREAM_TOKEN};
    strncpy(ev.data.stream.token, token, sizeof(ev.data.stream.token) - 1);
    ev.data.stream.tokens_so_far = tokens_so_far;
    tui_eventpub_emit(&ev);
}

void tui_eventpub_stream_done(void) {
    tui_event_t ev = {.type = TUI_EVENT_STREAM_DONE};
    tui_eventpub_emit(&ev);
}

void tui_eventpub_tool(const char *name, const char *status,
                       int progress, int total, const char *result_preview) {
    if (!name || !status) return;

    tui_event_type_t type;
    if (strcmp(status, "running") == 0 || strcmp(status, "started") == 0)
        type = TUI_EVENT_TOOL_CALL;
    else if (strcmp(status, "done") == 0 || strcmp(status, "completed") == 0)
        type = TUI_EVENT_TOOL_RESULT;
    else
        type = TUI_EVENT_TOOL_STATUS;

    tui_event_t ev = {.type = type};
    strncpy(ev.data.tool.tool_name, name, sizeof(ev.data.tool.tool_name) - 1);
    strncpy(ev.data.tool.status, status, sizeof(ev.data.tool.status) - 1);
    ev.data.tool.progress = progress;
    ev.data.tool.total = total;
    if (result_preview)
        strncpy(ev.data.tool.result_preview, result_preview,
                sizeof(ev.data.tool.result_preview) - 1);
    tui_eventpub_emit(&ev);
}

void tui_eventpub_session(const char *session_id, const char *action) {
    if (!session_id || !action) return;
    tui_event_t ev = {.type = TUI_EVENT_SESSION_CHANGE};
    if (strcmp(action, "deleted") == 0)
        ev.type = TUI_EVENT_SESSION_DELETE;
    strncpy(ev.data.session.session_id, session_id,
            sizeof(ev.data.session.session_id) - 1);
    strncpy(ev.data.session.action, action,
            sizeof(ev.data.session.action) - 1);
    tui_eventpub_emit(&ev);
}

void tui_eventpub_agent(const char *model, const char *provider,
                        int iteration, int max_iters,
                        int tokens_in, int tokens_out, double budget) {
    tui_event_t ev = {.type = TUI_EVENT_AGENT_UPDATE};
    if (model)
        strncpy(ev.data.agent.model, model, sizeof(ev.data.agent.model) - 1);
    if (provider)
        strncpy(ev.data.agent.provider, provider, sizeof(ev.data.agent.provider) - 1);
    ev.data.agent.iteration = iteration;
    ev.data.agent.max_iterations = max_iters;
    ev.data.agent.tokens_in = tokens_in;
    ev.data.agent.tokens_out = tokens_out;
    ev.data.agent.budget_remaining = budget;
    tui_eventpub_emit(&ev);
}

void tui_eventpub_status(const char *mode, const char *model,
                         const char *provider,
                         int iteration, int max_iters,
                         int tokens_in, int tokens_out,
                         int ctx_pct, double budget) {
    tui_event_t ev = {.type = TUI_EVENT_STATUS_UPDATE};
    if (mode)
        strncpy(ev.data.status.mode, mode, sizeof(ev.data.status.mode) - 1);
    if (model)
        strncpy(ev.data.status.model, model, sizeof(ev.data.status.model) - 1);
    if (provider)
        strncpy(ev.data.status.provider, provider, sizeof(ev.data.status.provider) - 1);
    ev.data.status.iteration = iteration;
    ev.data.status.max_iterations = max_iters;
    ev.data.status.tokens_in = tokens_in;
    ev.data.status.tokens_out = tokens_out;
    ev.data.status.ctx_percent = ctx_pct;
    ev.data.status.budget = budget;
    tui_eventpub_emit(&ev);
}

void tui_eventpub_theme(const char *theme_name) {
    if (!theme_name) return;
    tui_event_t ev = {.type = TUI_EVENT_THEME_CHANGE};
    strncpy(ev.data.theme.theme_name, theme_name,
            sizeof(ev.data.theme.theme_name) - 1);
    tui_eventpub_emit(&ev);
}

void tui_eventpub_model(const char *model_name, const char *provider) {
    if (!model_name) return;
    tui_event_t ev = {.type = TUI_EVENT_MODEL_CHANGE};
    strncpy(ev.data.model.model_name, model_name,
            sizeof(ev.data.model.model_name) - 1);
    if (provider)
        strncpy(ev.data.model.provider, provider,
                sizeof(ev.data.model.provider) - 1);
    tui_eventpub_emit(&ev);
}

void tui_eventpub_connection(bool success, const char *reason) {
    tui_event_t ev = {
        .type = success ? TUI_EVENT_GATEWAY_CONNECT : TUI_EVENT_GATEWAY_DISCONNECT
    };
    ev.data.connection.success = success;
    if (reason)
        strncpy(ev.data.connection.reason, reason,
                sizeof(ev.data.connection.reason) - 1);
    tui_eventpub_emit(&ev);
}

/* ── Subscription management ────────────────────────────────── */

int tui_eventpub_subscribe(tui_event_type_t type,
                           tui_event_callback_t cb, void *userdata) {
    if (!cb || g_eventpub.sub_count >= MAX_SUBSCRIBERS) return 0;

    int id = g_eventpub.sub_count + 1;
    g_eventpub.subscribers[g_eventpub.sub_count].type = type;
    g_eventpub.subscribers[g_eventpub.sub_count].cb = cb;
    g_eventpub.subscribers[g_eventpub.sub_count].userdata = userdata;
    g_eventpub.subscribers[g_eventpub.sub_count].active = true;
    g_eventpub.sub_count++;
    return id;
}

void tui_eventpub_unsubscribe(int sub_id) {
    if (sub_id < 1 || sub_id > g_eventpub.sub_count) return;
    g_eventpub.subscribers[sub_id - 1].active = false;
}

/* ── FIFO management ────────────────────────────────────────── */

void tui_eventpub_flush(void) {
    if (g_eventpub.fifo_buf_pos <= 0) return;

    if (g_eventpub.fifo_fd < 0 && !fifo_open())
        return;

    int written = 0;
    while (written < g_eventpub.fifo_buf_pos) {
        int n = (int)write(g_eventpub.fifo_fd,
                           g_eventpub.fifo_buf + written,
                           g_eventpub.fifo_buf_pos - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (g_eventpub.debug)
                fprintf(stderr, "[eventpub] FIFO write error: %s\n", strerror(errno));
            break;
        }
        written += n;
    }

    /* Shift remaining unwritten data */
    if (written > 0) {
        int remaining = g_eventpub.fifo_buf_pos - written;
        if (remaining > 0)
            memmove(g_eventpub.fifo_buf, g_eventpub.fifo_buf + written, remaining);
        g_eventpub.fifo_buf_pos = remaining;
    }
}

void tui_eventpub_set_fifo_enabled(bool enabled) {
    g_eventpub.fifo_enabled = enabled;
}

bool tui_eventpub_fifo_is_open(void) {
    return g_eventpub.fifo_fd >= 0;
}

bool tui_eventpub_fifo_reconnect(const char *fifo_path) {
    if (g_eventpub.fifo_fd >= 0) {
        close(g_eventpub.fifo_fd);
        g_eventpub.fifo_fd = -1;
    }
    if (fifo_path)
        strncpy(g_eventpub.fifo_path, fifo_path,
                sizeof(g_eventpub.fifo_path) - 1);
    return fifo_open();
}

const char *tui_eventpub_type_name(tui_event_type_t type) {
    return type_name(type);
}

void tui_eventpub_set_debug(bool debug) {
    g_eventpub.debug = debug;
}
