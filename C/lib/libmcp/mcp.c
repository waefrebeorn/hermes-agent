/*
 * mcp.c — MCP (Model Context Protocol) client library.
 *
 * JSON-RPC 2.0 message types, MCP protocol methods, stdio transport.
 * P56-P58: Core client library, stdio transport, tool registration.
 */

#include "mcp.h"
#include "../libjson/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

/* environ is not always declared in POSIX headers on Linux */
extern char **environ;

/* ================================================================
 *  Internal types
 * ================================================================ */

/* Pending request tracking */
typedef struct {
    char        id[64];
    json_t     *response;   /* stored response JSON */
    bool        done;
} pending_req_t;

#define MAX_PENDING 32

/* Stdio transport state */
typedef struct {
    pid_t   pid;
    int     stdin_fd;   /* write to server stdin */
    int     stdout_fd;  /* read from server stdout */
    int     stderr_fd;  /* capture stderr for diagnostics */
} mcp_stdio_t;

/* SSE transport state (P62+) */
typedef struct {
    char    url[1024];
    char    headers[2048];
    void   *http_client;  /* http_t * */
    int     sse_fd;       /* SSE stream fd */
} mcp_sse_t;

/* Server instance */
struct mcp_server {
    char    name[64];
    char    last_error[512];

    /* Transport */
    mcp_transport_type_t transport_type;
    mcp_stdio_t stdio;
    mcp_sse_t   sse;

    /* Stdio config */
    char    command[256];
    char  **args;           /* NULL-terminated */
    char  **env;            /* NULL-terminated */

    /* Timeouts */
    int     tool_timeout;      /* per-tool-call timeout */
    int     connect_timeout;   /* connection timeout */

    /* Protocol state */
    bool    initialized;
    mcp_capabilities_t caps;

    /* Pending request tracking */
    pending_req_t pending[MAX_PENDING];
    int           pending_count;

    /* Message buffer for reading responses */
    char    read_buf[65536];
    size_t  read_len;

    /* P61: Server lifecycle */
    mcp_server_status_t status;
    int     max_retries;         /* max reconnect attempts */
    int     reconnect_count;     /* total reconnection attempts */
    int     reconnect_delay_ms;  /* current backoff delay */
};

/* ================================================================
 *  JSON-RPC message helpers
 * ================================================================ */

/* Build a JSON-RPC request: {"jsonrpc":"2.0","id":"X","method":"Y","params":Z} */
static char *build_request(const char *id, const char *method, json_t *params) {
    json_t *req = json_object();
    json_set(req, "jsonrpc", json_string(MCP_JSONRPC_VERSION));
    json_set(req, "id", json_string(id));
    json_set(req, "method", json_string(method));
    if (params)
        json_set(req, "params", params);
    else
        json_set(req, "params", json_object());
    char *s = json_serialize(req);
    json_free(req);
    return s;
}

/* Build a JSON-RPC notification (no id): {"jsonrpc":"2.0","method":"Y","params":Z} */
static char *build_notification(const char *method, json_t *params) {
    json_t *req = json_object();
    json_set(req, "jsonrpc", json_string(MCP_JSONRPC_VERSION));
    json_set(req, "method", json_string(method));
    if (params)
        json_set(req, "params", params);
    else
        json_set(req, "params", json_object());
    char *s = json_serialize(req);
    json_free(req);
    return s;
}

/* ================================================================
 *  Stdio transport
 * ================================================================ */

static bool stdio_spawn(mcp_server_t *srv) {
    /* Create pipes for stdin/stdout/stderr */
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "pipe() failed: %s", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "fork() failed: %s", strerror(errno));
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return false;
    }

    if (pid == 0) {
        /* Child: MCP server process */
        close(stdin_pipe[1]);   /* Close write end of child's stdin */
        close(stdout_pipe[0]);  /* Close read end of child's stdout */
        close(stderr_pipe[0]);  /* Close read end of child's stderr */

        dup2(stdin_pipe[0], STDIN_FILENO);   /* Read from parent's write */
        dup2(stdout_pipe[1], STDOUT_FILENO); /* Write to parent's read */
        dup2(stderr_pipe[1], STDERR_FILENO); /* Write to parent's error read */

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Execute MCP server */
        execve(srv->command, srv->args, srv->env ? srv->env : environ);
        /* If execve fails */
        _exit(127);
    }

    /* Parent */
    close(stdin_pipe[0]);   /* Close read end of parent's stdin pipe */
    close(stdout_pipe[1]);  /* Close write end of parent's stdout pipe */
    close(stderr_pipe[1]);  /* Close write end of parent's stderr pipe */

    srv->stdio.pid = pid;
    srv->stdio.stdin_fd = stdin_pipe[1];   /* Write to server here */
    srv->stdio.stdout_fd = stdout_pipe[0]; /* Read from server here */
    srv->stdio.stderr_fd = stderr_pipe[0]; /* Read stderr here */

    return true;
}

static void stdio_close(mcp_server_t *srv) {
    if (srv->stdio.stdin_fd > 0) {
        close(srv->stdio.stdin_fd);
        srv->stdio.stdin_fd = 0;
    }
    if (srv->stdio.stdout_fd > 0) {
        close(srv->stdio.stdout_fd);
        srv->stdio.stdout_fd = 0;
    }
    if (srv->stdio.stderr_fd > 0) {
        close(srv->stdio.stderr_fd);
        srv->stdio.stderr_fd = 0;
    }

    /* Kill child process */
    if (srv->stdio.pid > 0) {
        kill(srv->stdio.pid, SIGTERM);
        usleep(100000); /* 100ms */
        kill(srv->stdio.pid, SIGKILL);
        waitpid(srv->stdio.pid, NULL, WNOHANG);
        srv->stdio.pid = 0;
    }
}

/* Write JSON-RPC message to server stdin */
static bool stdio_send(mcp_server_t *srv, const char *msg) {
    if (srv->stdio.stdin_fd <= 0) {
        snprintf(srv->last_error, sizeof(srv->last_error), "Not connected");
        return false;
    }

    /* MCP messages are newline-delimited JSON */
    size_t len = strlen(msg);
    char *full = (char *)malloc(len + 2);
    if (!full) return false;
    memcpy(full, msg, len);
    full[len] = '\n';
    full[len + 1] = '\0';

    ssize_t written = write(srv->stdio.stdin_fd, full, len + 1);
    free(full);
    return written >= (ssize_t)(len + 1);
}

/* Read a single JSON-RPC line from server stdout.
 * Returns parsed json_t* or NULL on timeout/error.
 * Uses simple \n-delimited reading. */
static json_t *stdio_read_line(mcp_server_t *srv, int timeout_ms) {
    /* Accumulate until we hit \n */
    while (1) {
        /* Check if we have a complete line in buffer */
        char *nl = (char *)memchr(srv->read_buf, '\n', srv->read_len);
        if (nl) {
            size_t line_len = (size_t)(nl - srv->read_buf);
            srv->read_buf[line_len] = '\0';

            /* Parse line as JSON */
            char *err = NULL;
            json_t *result = json_parse(srv->read_buf, &err);
            if (err) {
                free(err);
                /* Not JSON — might be stderr output, skip */
            }

            /* Shift buffer past the line */
            size_t remaining = srv->read_len - line_len - 1;
            if (remaining > 0)
                memmove(srv->read_buf, nl + 1, remaining);
            srv->read_len = remaining;

            if (result) return result;
            continue; /* Try next line */
        }

        /* Need more data — use poll/select with timeout */
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(srv->stdio.stdout_fd, &fds);

        int ret = select(srv->stdio.stdout_fd + 1, &fds, NULL, NULL,
                         timeout_ms > 0 ? &tv : NULL);
        if (ret <= 0) {
            if (ret == 0)
                snprintf(srv->last_error, sizeof(srv->last_error),
                         "Read timeout (%dms)", timeout_ms);
            else
                snprintf(srv->last_error, sizeof(srv->last_error),
                         "select() error: %s", strerror(errno));
            return NULL;
        }

        ssize_t n = read(srv->stdio.stdout_fd,
                         srv->read_buf + srv->read_len,
                         sizeof(srv->read_buf) - srv->read_len - 1);
        if (n <= 0) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "Server closed connection");
            return NULL;
        }
        srv->read_len += (size_t)n;
        srv->read_buf[srv->read_len] = '\0';
    }
}

/* ================================================================
 *  JSON-RPC request/response matching
 * ================================================================ */

/* Send request and wait for matching response */
static json_t *send_request(mcp_server_t *srv, const char *method,
                             json_t *params, int timeout_ms) {
    static int g_req_id = 0;
    char id[32];
    snprintf(id, sizeof(id), "mcp-%d", ++g_req_id);

    char *msg = build_request(id, method, params);
    if (!msg) return NULL;

    if (!stdio_send(srv, msg)) {
        free(msg);
        return NULL;
    }
    free(msg);

    /* Read responses until we find matching id */
    int max_reads = 100; /* safety limit */
    while (max_reads-- > 0) {
        json_t *resp = stdio_read_line(srv, timeout_ms);
        if (!resp) return NULL;

        const char *resp_id = json_get_str(resp, "id", "");
        if (resp_id && strcmp(resp_id, id) == 0) {
            /* This is our response */
            json_t *error_obj = json_obj_get(resp, "error");
            if (error_obj) {
                const char *err_msg = json_get_str(error_obj, "message", "Unknown error");
                snprintf(srv->last_error, sizeof(srv->last_error),
                         "MCP error: %s", err_msg);
                json_free(resp);
                return NULL;
            }
            return resp; /* Caller must json_free */
        }

        /* Not our response — might be a notification or other request's response.
         * For now just skip. In production, would route to correct pending slot. */
        json_free(resp);
    }

    snprintf(srv->last_error, sizeof(srv->last_error),
             "No matching response for %s", method);
    return NULL;
}

/* ================================================================
 *  Server lifecycle
 * ================================================================ */

mcp_server_t *mcp_server_new(const char *name) {
    mcp_server_t *srv = (mcp_server_t *)calloc(1, sizeof(mcp_server_t));
    if (!srv) return NULL;
    if (name) snprintf(srv->name, sizeof(srv->name), "%s", name);
    srv->tool_timeout = 120;
    srv->connect_timeout = 60;
    srv->transport_type = MCP_TRANSPORT_NONE;
    srv->status = MCP_STATUS_DISCONNECTED;
    srv->max_retries = 3;       /* default: 3 reconnect attempts */
    srv->reconnect_delay_ms = 1000; /* start with 1s backoff */
    return srv;
}

void mcp_server_set_stdio(mcp_server_t *srv, const char *command, char **args) {
    if (!srv) return;
    srv->transport_type = MCP_TRANSPORT_STDIO;
    if (command) snprintf(srv->command, sizeof(srv->command), "%s", command);
    srv->args = args;
}

void mcp_server_set_sse(mcp_server_t *srv, const char *url) {
    if (!srv) return;
    srv->transport_type = MCP_TRANSPORT_SSE;
    if (url) snprintf(srv->sse.url, sizeof(srv->sse.url), "%s", url);
}

void mcp_server_set_env(mcp_server_t *srv, char **env) {
    if (!srv) return;
    srv->env = env;
}

void mcp_server_set_timeout(mcp_server_t *srv, int tool_timeout_sec) {
    if (srv) srv->tool_timeout = tool_timeout_sec;
}

void mcp_server_set_connect_timeout(mcp_server_t *srv, int connect_timeout_sec) {
    if (srv) srv->connect_timeout = connect_timeout_sec;
}

void mcp_server_set_max_retries(mcp_server_t *srv, int max_retries) {
    if (srv) srv->max_retries = max_retries;
}

void mcp_server_set_headers(mcp_server_t *srv, const char *headers) {
    if (srv && headers) snprintf(srv->sse.headers, sizeof(srv->sse.headers), "%s", headers);
}

bool mcp_server_connect(mcp_server_t *srv) {
    if (!srv) return false;

    srv->status = MCP_STATUS_CONNECTING;

    /* Validate transport config */
    if (srv->transport_type == MCP_TRANSPORT_STDIO) {
        if (!srv->command[0]) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "No command configured for stdio transport");
            return false;
        }
        if (!stdio_spawn(srv)) return false;

        /* Give server a moment to start, then send initialize */
        usleep(200000); /* 200ms */

        /* Send initialize request */
        json_t *params = json_object();
        json_t *client_info = json_object();
        json_set(client_info, "name", json_string("hermes-c"));
        json_set(client_info, "version", json_string("0.14.1"));
        json_set(params, "protocolVersion", json_string(MCP_PROTOCOL_VERSION));
        json_set(params, "capabilities", json_object());
        json_set(params, "clientInfo", client_info);

        json_t *resp = send_request(srv, "initialize", params,
                                     srv->connect_timeout * 1000);
        if (!resp) {
            stdio_close(srv);
            return false;
        }

        /* Extract server capabilities from response */
        json_t *server_caps = json_obj_get(json_obj_get(resp, "result"), "capabilities");
        if (server_caps) {
            srv->caps.supports_tools     = json_obj_get(server_caps, "tools") != NULL;
            srv->caps.supports_resources = json_obj_get(server_caps, "resources") != NULL;
            srv->caps.supports_prompts   = json_obj_get(server_caps, "prompts") != NULL;
            srv->caps.supports_logging   = json_obj_get(server_caps, "logging") != NULL;
            srv->caps.supports_sampling  = json_obj_get(server_caps, "sampling") != NULL;
        }
        json_free(resp);

        /* Send initialized notification */
        char *notif = build_notification("notifications/initialized", NULL);
        if (notif) {
            stdio_send(srv, notif);
            free(notif);
        }

        srv->initialized = true;
        srv->status = MCP_STATUS_CONNECTED;
        srv->reconnect_delay_ms = 1000; /* reset backoff */
        return true;
    }

    if (srv->transport_type == MCP_TRANSPORT_SSE) {
        /* SSE transport not yet implemented (P62) */
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "SSE transport not yet implemented");
        srv->status = MCP_STATUS_FAILED;
        return false;
    }

    snprintf(srv->last_error, sizeof(srv->last_error),
             "No transport configured");
    srv->status = MCP_STATUS_FAILED;
    return false;
}

void mcp_server_disconnect(mcp_server_t *srv) {
    if (!srv) return;

    if (srv->transport_type == MCP_TRANSPORT_STDIO) {
        /* Send shutdown notification */
        char *msg = build_notification("exit", NULL);
        if (msg) {
            stdio_send(srv, msg);
            free(msg);
        }
        stdio_close(srv);
    }

    srv->initialized = false;
    srv->read_len = 0;
    srv->status = MCP_STATUS_DISCONNECTED;
}

void mcp_server_free(mcp_server_t *srv) {
    if (!srv) return;
    mcp_server_disconnect(srv);
    if (srv->args) { /* args are owned by caller, don't free */ }
    if (srv->env)  { /* env is owned by caller */ }
    free(srv);
}

/* ================================================================
 *  MCP Protocol methods
 * ================================================================ */

bool mcp_server_ping(mcp_server_t *srv) {
    if (!srv || !srv->initialized) return false;

    json_t *resp = send_request(srv, "ping", NULL, 5000);
    if (!resp) return false;
    json_free(resp);
    return true;
}

int mcp_server_list_tools(mcp_server_t *srv, mcp_tool_t **tools_out) {
    if (!srv || !srv->initialized || !tools_out) return -1;

    if (!srv->caps.supports_tools) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Server does not support tools");
        return -1;
    }

    json_t *resp = send_request(srv, "tools/list", NULL,
                                 srv->tool_timeout * 1000);
    if (!resp) return -1;

    json_t *result = json_obj_get(resp, "result");
    if (!result) {
        json_free(resp);
        return -1;
    }

    json_t *tools_arr = json_obj_get(result, "tools");
    if (!tools_arr) {
        json_free(resp);
        return 0; /* No tools, not an error */
    }

    size_t count = json_len(tools_arr);
    if (count == 0) {
        json_free(resp);
        *tools_out = NULL;
        return 0;
    }

    mcp_tool_t *tools = (mcp_tool_t *)calloc(count, sizeof(mcp_tool_t));
    if (!tools) { json_free(resp); return -1; }

    for (size_t i = 0; i < count; i++) {
        json_t *t = json_get(tools_arr, i);
        if (!t) continue;

        const char *name = json_get_str(t, "name", "");
        if (name) snprintf(tools[i].name, sizeof(tools[i].name), "%s", name);

        const char *desc = json_get_str(t, "description", "");
        if (desc) snprintf(tools[i].description, sizeof(tools[i].description), "%s", desc);

        /* Store input schema as JSON string */
        json_t *schema = json_obj_get(t, "inputSchema");
        if (schema) {
            char *schema_str = json_serialize(schema);
            if (schema_str) {
                snprintf(tools[i].input_schema, sizeof(tools[i].input_schema),
                         "%s", schema_str);
                free(schema_str);
            }

            /* Parse required params */
            json_t *props = json_obj_get(schema, "properties");
            if (props) {
                /* Just count — detailed param parsing is done on demand */
                tools[i].param_count = (int)json_len(props);
            }
        }
    }

    json_free(resp);
    *tools_out = tools;
    return (int)count;
}

char *mcp_server_call_tool(mcp_server_t *srv, const char *tool_name,
                            const char *args_json) {
    if (!srv || !srv->initialized || !tool_name) return NULL;

    json_t *params = json_object();
    json_set(params, "name", json_string(tool_name));

    /* Parse arguments JSON if provided */
    if (args_json && args_json[0]) {
        char *err = NULL;
        json_t *args = json_parse(args_json, &err);
        if (args) {
            json_set(params, "arguments", args);
        } else {
            free(err);
            json_set(params, "arguments", json_object());
        }
    } else {
        json_set(params, "arguments", json_object());
    }

    json_t *resp = send_request(srv, "tools/call", params,
                                 srv->tool_timeout * 1000);
    if (!resp) {
        json_free(params);
        return NULL;
    }

    json_t *result = json_obj_get(resp, "result");
    if (!result) {
        json_free(resp);
        json_free(params);
        return NULL;
    }

    /* Extract content from result */
    json_t *content = json_obj_get(result, "content");
    char *response_str = NULL;
    if (content && json_len(content) > 0) {
        json_t *first = json_get(content, 0);
        if (first) {
            const char *text = json_get_str(first, "text", "");
            response_str = strdup(text);
        }
    }

    if (!response_str) {
        /* Return full result as JSON */
        response_str = json_serialize(result);
    }

    json_free(resp);
    json_free(params);
    return response_str;
}

/* ================================================================
 *  Tool list cleanup
 * ================================================================ */

void mcp_tool_list_free(mcp_tool_t *tools, int count) {
    (void)count;
    free(tools);
}

/* ================================================================
 *  P61: Server lifecycle
 * ================================================================ */

mcp_server_status_t mcp_server_status(mcp_server_t *srv) {
    return srv ? srv->status : MCP_STATUS_FAILED;
}

int mcp_server_reconnect_count(mcp_server_t *srv) {
    return srv ? srv->reconnect_count : 0;
}

bool mcp_server_health_check(mcp_server_t *srv) {
    if (!srv) return false;

    /* Try ping */
    if (mcp_server_ping(srv))
        return true;

    /* Ping failed — server may be dead. Try reconnect with backoff. */
    return mcp_server_reconnect(srv);
}

bool mcp_server_reconnect(mcp_server_t *srv) {
    if (!srv) return false;

    /* Check retry limit */
    if (srv->max_retries >= 0 && srv->reconnect_count >= srv->max_retries) {
        srv->status = MCP_STATUS_FAILED;
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Max reconnects (%d) reached", srv->max_retries);
        return false;
    }

    srv->reconnect_count++;
    srv->status = MCP_STATUS_RECONNECTING;

    /* Calculate backoff: 1s, 2s, 4s, 8s, ... capped at 60s */
    int delay = srv->reconnect_delay_ms;
    if (delay < 60000)
        srv->reconnect_delay_ms *= 2; /* exponential backoff */

    fprintf(stderr, "MCP: Reconnecting '%s' (attempt %d, delay %dms)\n",
            srv->name, srv->reconnect_count, delay);

    usleep(delay * 1000);

    /* Disconnect and reconnect */
    mcp_server_disconnect(srv);
    return mcp_server_connect(srv);
}

/* ================================================================
 *  Accessors
 * ================================================================ */

const char *mcp_server_last_error(mcp_server_t *srv) {
    return srv ? srv->last_error : "NULL server";
}

mcp_capabilities_t mcp_server_capabilities(mcp_server_t *srv) {
    mcp_capabilities_t empty = {0};
    return srv ? srv->caps : empty;
}

const char *mcp_server_name(mcp_server_t *srv) {
    return srv ? srv->name : "";
}

bool mcp_server_is_connected(mcp_server_t *srv) {
    return srv && srv->initialized &&
           (srv->transport_type == MCP_TRANSPORT_STDIO ? srv->stdio.pid > 0 : false);
}
