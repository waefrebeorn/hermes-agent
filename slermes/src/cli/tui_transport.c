/*
 * tui_transport.c — TUI transport layer abstraction.
 *
 * Provides reliable message transport between TUI frontend and agent
 * process. Encapsulates FIFO/socket connect, message framing, buffering,
 * reconnection, and state change notifications.
 *
 * MIT License — WuBu Hermes Project
 */

#define _GNU_SOURCE  /* for O_NONBLOCK, mkfifo, ppoll */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>

#include "tui_transport.h"

/* ── Internal helpers ── */

static void set_state(tui_transport_t *t, tui_transport_state_t new_state) {
    tui_transport_state_t old = t->state;
    t->state = new_state;
    if (t->on_state_change && old != new_state) {
        t->on_state_change(old, new_state, t->callback_data);
    }
}

/* Close fd if valid, set to -1 */
static void close_fd(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static bool is_valid_path(const char *path) {
    return path && *path && strlen(path) < 256;
}

/* ── API: Init ── */
bool tui_transport_init(tui_transport_t *t,
                         const char *path,
                         tui_transport_type_t type) {
    if (!t) return false;
    memset(t, 0, sizeof(*t));

    t->type = type;
    t->read_fd = -1;
    t->write_fd = -1;
    t->state = TUI_TRANSPORT_IDLE;
    t->max_retries = TUI_TRANSPORT_DEFAULT_RETRIES;
    t->retry_delay_ms = TUI_TRANSPORT_DEFAULT_RETRY_MS;
    t->on_state_change = NULL;
    t->callback_data = NULL;

    /* Copy path */
    if (path && *path) {
        t->path = strdup(path);
        if (!t->path) return false;
        t->owned_path = true;
    } else {
        t->path = NULL;
    }

    /* Allocate read buffer */
    t->buf_size = TUI_TRANSPORT_DEFAULT_BUF_SIZE;
    t->buf = (char *)malloc(t->buf_size);
    if (!t->buf) {
        free(t->path);
        t->path = NULL;
        return false;
    }
    t->read_pos = 0;
    t->msg_start = 0;

    return true;
}

/* ── API: Configure retry ── */
void tui_transport_set_retry(tui_transport_t *t,
                              int max_retries, int delay_ms) {
    if (!t) return;
    t->max_retries = max_retries;
    t->retry_delay_ms = delay_ms;
}

/* ── API: Register callback ── */
void tui_transport_set_callback(tui_transport_t *t,
                                 tui_transport_callback_t cb,
                                 void *user_data) {
    if (!t) return;
    t->on_state_change = cb;
    t->callback_data = user_data;
}

/* ── API: Connect (FIFO) ── */
static bool transport_connect_fifo(tui_transport_t *t) {
    if (!is_valid_path(t->path)) {
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Remove old FIFO if exists */
    unlink(t->path);

    /* Create new FIFO */
    if (mkfifo(t->path, 0600) != 0) {
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Open read fd (use O_RDWR so FIFO stays alive without a writer) */
    t->read_fd = open(t->path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (t->read_fd < 0) {
        unlink(t->path);
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Open write FIFO path (with .out suffix).
     * Use O_RDWR so open succeeds without a reader on the other end.
     */
    char write_path[512];
    int ret = snprintf(write_path, sizeof(write_path), "%s.out", t->path);
    if (ret < 0 || (size_t)ret >= sizeof(write_path)) {
        close_fd(&t->read_fd);
        unlink(t->path);
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Create .out FIFO for responses */
    unlink(write_path);
    if (mkfifo(write_path, 0600) != 0) {
        close_fd(&t->read_fd);
        unlink(t->path);
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Open write end O_RDWR so it works without a reader */
    t->write_fd = open(write_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (t->write_fd < 0) {
        t->write_fd = open(write_path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (t->write_fd < 0) {
        close_fd(&t->read_fd);
        unlink(t->path);
        unlink(write_path);
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }

    /* Reset buffer */
    t->read_pos = 0;
    t->msg_start = 0;

    set_state(t, TUI_TRANSPORT_CONNECTED);
    return true;
}

bool tui_transport_connect(tui_transport_t *t) {
    if (!t) return false;

    set_state(t, TUI_TRANSPORT_CONNECTING);

    switch (t->type) {
    case TUI_TRANSPORT_FIFO:
        return transport_connect_fifo(t);
    case TUI_TRANSPORT_PIPE:
        /* Pipe transport not yet implemented */
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    case TUI_TRANSPORT_SOCKET:
        /* Socket transport not yet implemented */
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    default:
        set_state(t, TUI_TRANSPORT_ERROR);
        return false;
    }
}

/* ── API: Reconnect ── */
bool tui_transport_reconnect(tui_transport_t *t) {
    if (!t) return false;

    /* Close old connections */
    close_fd(&t->read_fd);
    close_fd(&t->write_fd);
    t->read_pos = 0;
    t->msg_start = 0;

    /* Retry loop */
    for (int attempt = 0; attempt < t->max_retries; attempt++) {
        if (attempt > 0) {
            usleep(t->retry_delay_ms * 1000);
        }
        if (tui_transport_connect(t)) {
            return true;
        }
    }

    set_state(t, TUI_TRANSPORT_ERROR);
    return false;
}

/* ── API: Poll for incoming data ── */
int tui_transport_poll(tui_transport_t *t) {
    if (!t || t->read_fd < 0) return -1;
    if (t->state != TUI_TRANSPORT_CONNECTED) return -1;

    /* Try non-blocking read to fill buffer */
    int space = (int)(t->buf_size - t->read_pos - 1);
    if (space <= 0) {
        /* Buffer full — try to compact */
        if (t->msg_start > 0) {
            size_t remaining = t->read_pos - t->msg_start;
            if (remaining > 0) {
                memmove(t->buf, t->buf + t->msg_start, remaining);
            }
            t->read_pos = remaining;
            t->msg_start = 0;
            space = (int)(t->buf_size - t->read_pos - 1);
        }
        if (space <= 0) {
            /* Truly full — return what we have */
            return 0;
        }
    }

    int n = (int)read(t->read_fd, t->buf + t->read_pos, space);
    if (n > 0) {
        t->read_pos += n;
        t->buf[t->read_pos] = '\0';
        /* Count bytes available in current/next message */
        return t->read_pos - t->msg_start;
    }

    if (n == 0) {
        /* Writer closed FIFO — EOF */
        set_state(t, TUI_TRANSPORT_DISCONNECTED);
        return -1;
    }

    /* n < 0 */
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0; /* No data available */
    }

    /* Real error */
    set_state(t, TUI_TRANSPORT_ERROR);
    return -1;
}

/* ── API: Receive a message ── */
int tui_transport_recv(tui_transport_t *t, char *buf, size_t sz, int timeout_ms) {
    if (!t || !buf || sz == 0) return -1;

    if (t->state != TUI_TRANSPORT_CONNECTED) {
        return -1;
    }

    /* Check if we have a complete message (newline-delimited) */
    if (t->msg_start < t->read_pos) {
        char *nl = (char *)memchr(t->buf + t->msg_start, '\n',
                                   t->read_pos - t->msg_start);
        if (nl) {
            size_t len = nl - (t->buf + t->msg_start);
            size_t to_copy = len < sz ? len : sz - 1;
            memcpy(buf, t->buf + t->msg_start, to_copy);
            buf[to_copy] = '\0';
            t->msg_start = (size_t)(nl - t->buf) + 1;
            return (int)to_copy;
        }
    }

    /* Need more data. If timeout is 0 (non-blocking), return now. */
    if (timeout_ms == 0) {
        return 0;
    }

    /* Poll for more data */
    struct pollfd pfd;
    pfd.fd = t->read_fd;
    pfd.events = POLLIN;
    int poll_timeout = (timeout_ms < 0) ? -1 : timeout_ms;

    int ret = poll(&pfd, 1, poll_timeout);

    if (ret > 0 && (pfd.revents & POLLIN)) {
        /* Read more data */
        int avail = tui_transport_poll(t);
        if (avail > 0) {
            /* Try again to find a newline */
            char *nl = (char *)memchr(t->buf + t->msg_start, '\n',
                                       t->read_pos - t->msg_start);
            if (nl) {
                size_t len = nl - (t->buf + t->msg_start);
                size_t to_copy = len < sz ? len : sz - 1;
                memcpy(buf, t->buf + t->msg_start, to_copy);
                buf[to_copy] = '\0';
                t->msg_start = (size_t)(nl - t->buf) + 1;
                return (int)to_copy;
            }
        }
        return avail; /* Partial data, still no complete message */
    }

    if (ret == 0) {
        return 0; /* Timeout, no data */
    }

    /* poll error */
    return -1;
}

/* ── API: Send message ── */
bool tui_transport_send(tui_transport_t *t, const char *buf, size_t len) {
    if (!t || !buf) return false;
    if (t->write_fd < 0) return false;

    ssize_t written = write(t->write_fd, buf, len);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Try blocking write as fallback */
            int flags = fcntl(t->write_fd, F_GETFL);
            if (flags >= 0) {
                fcntl(t->write_fd, F_SETFL, flags & ~O_NONBLOCK);
                written = write(t->write_fd, buf, len);
                fcntl(t->write_fd, F_SETFL, flags);
            }
        }
        if (written < 0) {
            return false;
        }
    }

    return (size_t)written == len;
}

/* ── API: Send formatted string ── */
bool tui_transport_sendf(tui_transport_t *t, const char *fmt, ...) {
    if (!t || !fmt) return false;

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= sizeof(buf)) return false;
    return tui_transport_send(t, buf, (size_t)n);
}

/* ── API: Check connected ── */
bool tui_transport_is_connected(const tui_transport_t *t) {
    return t && t->state == TUI_TRANSPORT_CONNECTED;
}

/* ── API: Get state ── */
tui_transport_state_t tui_transport_state(const tui_transport_t *t) {
    return t ? t->state : TUI_TRANSPORT_ERROR;
}

/* ── API: State name ── */
const char *tui_transport_state_name(tui_transport_state_t state) {
    switch (state) {
    case TUI_TRANSPORT_IDLE:         return "idle";
    case TUI_TRANSPORT_CONNECTING:   return "connecting";
    case TUI_TRANSPORT_CONNECTED:    return "connected";
    case TUI_TRANSPORT_DISCONNECTED: return "disconnected";
    case TUI_TRANSPORT_ERROR:        return "error";
    default:                         return "unknown";
    }
}

/* ── API: Shutdown ── */
void tui_transport_shutdown(tui_transport_t *t) {
    if (!t) return;

    close_fd(&t->read_fd);
    close_fd(&t->write_fd);

    /* Clean up FIFO/socket paths */
    if (t->owned_path && t->path) {
        unlink(t->path);
        char write_path[512];
        snprintf(write_path, sizeof(write_path), "%s.out", t->path);
        unlink(write_path);
    }

    free(t->buf);
    t->buf = NULL;
    t->buf_size = 0;
    t->read_pos = 0;
    t->msg_start = 0;

    free(t->path);
    t->path = NULL;

    set_state(t, TUI_TRANSPORT_IDLE);
}

/* ── API: Send JSON-RPC notification ── */
bool tui_transport_send_rpc(tui_transport_t *t,
                             const char *method, const char *params_json) {
    if (!t || !method) return false;

    char msg[4096];
    int n = snprintf(msg, sizeof(msg),
                     "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}\n",
                     method, params_json ? params_json : "{}");
    if (n < 0 || (size_t)n >= sizeof(msg)) return false;

    return tui_transport_send(t, msg, (size_t)n);
}
