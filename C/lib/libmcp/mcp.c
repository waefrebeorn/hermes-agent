/*
 * mcp.c — MCP (Model Context Protocol) client library.
 *
 * JSON-RPC 2.0 message types, MCP protocol methods, stdio transport.
 * P56-P58: Core client library, stdio transport, tool registration.
 */

#include "mcp.h"
#include "../libjson/json.h"
#include "../libhttp/http.h"
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

/* SSE transport state (P62) */
typedef struct {
    char    url[1024];
    char    post_url[1024];    /* POST endpoint for sending requests */
    char    headers[2048];
    void   *http_client;       /* http_t * for SSE stream */
    void   *http_post_client;  /* http_t * for POST requests */
    int     sse_fd;            /* not used with stream callback */
    char    event_buf[65536];  /* SSE event accumulator */
    size_t  event_len;
    char    current_event[64]; /* current SSE event type */
    bool    streaming;         /* SSE stream active */
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

    /* P70: Workspace roots (server-to-client capability) */
    mcp_root_t roots[MAX_MCP_ROOTS];
    int        root_count;

    /* C01-C03: Resource subscriptions */
    char        subscriptions[MAX_MCP_ROOTS][512]; /* subscribed resource URIs */
    int         subscription_count;
    void      (*on_resource_change)(const char *server_name, const char *resource_uri, void *userdata);
    void       *on_resource_change_data;
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

/* Write JSON-RPC message to server stdin (stdio) or POST to SSE endpoint */
static bool transport_send(mcp_server_t *srv, const char *msg) {
    if (srv->transport_type == MCP_TRANSPORT_STDIO) {
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

    if (srv->transport_type == MCP_TRANSPORT_SSE) {
        if (!srv->sse.http_post_client) {
            snprintf(srv->last_error, sizeof(srv->last_error), "SSE not connected");
            return false;
        }
        /* Send via HTTP POST to server URL */
        http_resp_t *resp = http_post_json(
            (http_t *)srv->sse.http_post_client,
            srv->sse.post_url,
            msg);

        if (!resp) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "SSE POST failed");
            return false;
        }

        bool ok = (resp->status >= 200 && resp->status < 300);
        http_resp_free(resp);
        return ok;
    }

    snprintf(srv->last_error, sizeof(srv->last_error), "No transport");
    return false;
}

/* Read a single JSON-RPC line from server (stdio) or from response buffer (SSE).
 * For stdio: reads \n-delimited lines from pipe with select().
 * For SSE: sends request via POST, reads response from POST body. */
static json_t *transport_read_response(mcp_server_t *srv, const char *request_id,
                                        int timeout_ms) {
    if (srv->transport_type == MCP_TRANSPORT_STDIO) {
        /* Read \n-delimited lines until we find matching id */
        int max_reads = 100;
        while (max_reads-- > 0) {
            /* Accumulate until we hit \n */
            while (1) {
                char *nl = (char *)memchr(srv->read_buf, '\n', srv->read_len);
                if (nl) {
                    size_t line_len = (size_t)(nl - srv->read_buf);
                    srv->read_buf[line_len] = '\0';

                    char *jerr = NULL;
                    json_t *result = json_parse(srv->read_buf, &jerr);
                    if (jerr) { free(jerr); }
                    if (jerr) { /* not JSON, skip */ }

                    /* Shift buffer */
                    size_t remaining = srv->read_len - line_len - 1;
                    if (remaining > 0)
                        memmove(srv->read_buf, nl + 1, remaining);
                    srv->read_len = remaining;

                    if (result) {
                        const char *rid = json_get_str(result, "id", "");
                        if (rid && strcmp(rid, request_id) == 0)
                            return result;
                        json_free(result);
                    }
                    continue;
                }

                /* Need more data */
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
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "No matching response for id %s", request_id);
        return NULL;
    }

    if (srv->transport_type == MCP_TRANSPORT_SSE) {
        /* For SSE, the POST response IS the response.
         * This is a simplification — real SSE uses streaming. */
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "SSE read via send_request with POST");
        return NULL;
    }

    return NULL;
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

    if (!transport_send(srv, msg)) {
        free(msg);
        return NULL;
    }
    free(msg);

    /* Read response matching our request id */
    return transport_read_response(srv, id, timeout_ms);
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

/* P70: Set workspace roots for a server */
void mcp_server_set_roots(mcp_server_t *srv, const mcp_root_t *roots, int count) {
    if (!srv || !roots) return;
    int n = count < MAX_MCP_ROOTS ? count : MAX_MCP_ROOTS;
    for (int i = 0; i < n; i++) {
        snprintf(srv->roots[i].uri, sizeof(srv->roots[i].uri), "%s", roots[i].uri);
        snprintf(srv->roots[i].name, sizeof(srv->roots[i].name), "%s", roots[i].name);
    }
    srv->root_count = n;
}

/* C08: Add a root dynamically */
bool mcp_server_add_root(mcp_server_t *srv, const char *uri, const char *name) {
    if (!srv || !uri || srv->root_count >= MAX_MCP_ROOTS) return false;
    snprintf(srv->roots[srv->root_count].uri, sizeof(srv->roots[0].uri), "%s", uri);
    if (name)
        snprintf(srv->roots[srv->root_count].name, sizeof(srv->roots[0].name), "%s", name);
    else
        srv->roots[srv->root_count].name[0] = '\0';
    srv->root_count++;
    return true;
}

/* C09: Remove a root by URI */
bool mcp_server_remove_root(mcp_server_t *srv, const char *uri) {
    if (!srv || !uri) return false;
    for (int i = 0; i < srv->root_count; i++) {
        if (strcmp(srv->roots[i].uri, uri) == 0) {
            /* Shift remaining roots */
            for (int j = i; j < srv->root_count - 1; j++)
                srv->roots[j] = srv->roots[j + 1];
            srv->root_count--;
            return true;
        }
    }
    return false;
}

/* C10: Get root count */
int mcp_server_root_count(mcp_server_t *srv) {
    return srv ? srv->root_count : 0;
}

/* C10: Get root at index */
const mcp_root_t *mcp_server_get_root(mcp_server_t *srv, int index) {
    if (!srv || index < 0 || index >= srv->root_count) return NULL;
    return &srv->roots[index];
}

/* Handle a roots/list request from a connected MCP server.
 * Builds a JSON-RPC response with the configured root URIs. */
char *mcp_server_handle_roots_request(mcp_server_t *srv) {
    if (!srv) return NULL;

    json_t *result = json_object();
    json_t *roots_arr = json_array();

    for (int i = 0; i < srv->root_count; i++) {
        json_t *r = json_object();
        json_set(r, "uri", json_string(srv->roots[i].uri));
        if (srv->roots[i].name[0])
            json_set(r, "name", json_string(srv->roots[i].name));
        json_append(roots_arr, r);
    }

    json_set(result, "roots", roots_arr);
    char *s = json_serialize(result);
    json_free(result);
    return s;
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

        /* Client capabilities — include roots if configured (P70) */
        json_t *caps = json_object();
        if (srv->root_count > 0) {
            json_t *roots_cap = json_object();
            json_set(roots_cap, "listChanged", json_bool(false));
            json_set(caps, "roots", roots_cap);
        }
        json_set(params, "capabilities", caps);
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
            transport_send(srv, notif);
            free(notif);
        }

        srv->initialized = true;
        srv->status = MCP_STATUS_CONNECTED;
        srv->reconnect_delay_ms = 1000; /* reset backoff */
        return true;
    }

    if (srv->transport_type == MCP_TRANSPORT_SSE) {
        if (!srv->sse.url[0]) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "No URL configured for SSE transport");
            srv->status = MCP_STATUS_FAILED;
            return false;
        }

        /* Open HTTP client for SSE stream */
        srv->sse.http_client = http_new(srv->connect_timeout);
        if (!srv->sse.http_client) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "Failed to create HTTP client");
            srv->status = MCP_STATUS_FAILED;
            return false;
        }

        /* Send initialize via POST to SSE endpoint first.
         * SSE transport: client sends requests via POST, receives via SSE. */
        bool init_ok = false;

        /* Make initial POST to establish session */
        json_t *init_params = json_object();
        json_t *client_info = json_object();
        json_set(client_info, "name", json_string("hermes-c"));
        json_set(client_info, "version", json_string("0.14.1"));
        json_set(init_params, "protocolVersion", json_string(MCP_PROTOCOL_VERSION));

        /* Client capabilities — include roots if configured (P70) */
        {
            json_t *sse_caps = json_object();
            if (srv->root_count > 0) {
                json_t *roots_cap = json_object();
                json_set(roots_cap, "listChanged", json_bool(false));
                json_set(sse_caps, "roots", roots_cap);
            }
            json_set(init_params, "capabilities", sse_caps);
        }
        json_set(init_params, "clientInfo", client_info);

        char *init_req = build_request("init-1", "initialize", init_params);
        if (init_req) {
            /* Send initialize via POST */
            http_resp_t *init_resp = http_post_json(
                (http_t *)srv->sse.http_client,
                srv->sse.url,
                init_req);
            free(init_req);

            if (init_resp && init_resp->status == 200 && init_resp->body) {
                char *jerr = NULL;
                json_t *resp = json_parse(init_resp->body, &jerr);
                if (resp) {
                    json_t *result = json_obj_get(resp, "result");
                    if (result) {
                        /* Extract capabilities */
                        json_t *server_caps = json_obj_get(result, "capabilities");
                        if (server_caps) {
                            srv->caps.supports_tools     = json_obj_get(server_caps, "tools") != NULL;
                            srv->caps.supports_resources = json_obj_get(server_caps, "resources") != NULL;
                            srv->caps.supports_prompts   = json_obj_get(server_caps, "prompts") != NULL;
                            srv->caps.supports_logging   = json_obj_get(server_caps, "logging") != NULL;
                            srv->caps.supports_sampling  = json_obj_get(server_caps, "sampling") != NULL;
                        }
                        init_ok = true;
                    }
                    json_free(resp);
                }
                if (jerr) free(jerr);
            }
            if (init_resp) http_resp_free(init_resp);
        }

        if (!init_ok) {
            snprintf(srv->last_error, sizeof(srv->last_error),
                     "SSE initialize failed");
            http_free((http_t *)srv->sse.http_client);
            srv->sse.http_client = NULL;
            srv->status = MCP_STATUS_FAILED;
            return false;
        }

        /* Create separate HTTP client for POST requests */
        srv->sse.http_post_client = http_new_with_retry(srv->tool_timeout, 3, 1000);
        if (!srv->sse.http_post_client) {
            http_free((http_t *)srv->sse.http_client);
            srv->sse.http_client = NULL;
            srv->status = MCP_STATUS_FAILED;
            return false;
        }

        /* Send initialized notification via POST */
        char *notif = build_notification("notifications/initialized", NULL);
        if (notif) {
            http_post_json((http_t *)srv->sse.http_post_client,
                           srv->sse.url, notif);
            free(notif);
        }

        /* Copy URL as POST URL */
        snprintf(srv->sse.post_url, sizeof(srv->sse.post_url), "%s", srv->sse.url);
        srv->sse.streaming = true;

        srv->initialized = true;
        srv->status = MCP_STATUS_CONNECTED;
        srv->reconnect_delay_ms = 1000;
        return true;
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
            transport_send(srv, msg);
            free(msg);
        }
        stdio_close(srv);
    }

    if (srv->transport_type == MCP_TRANSPORT_SSE) {
        if (srv->sse.http_client) {
            http_free((http_t *)srv->sse.http_client);
            srv->sse.http_client = NULL;
        }
        if (srv->sse.http_post_client) {
            http_free((http_t *)srv->sse.http_post_client);
            srv->sse.http_post_client = NULL;
        }
        srv->sse.streaming = false;
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
 *  P67: Resource protocol methods
 * ================================================================ */

int mcp_server_list_resources(mcp_server_t *srv, mcp_resource_t **resources_out) {
    if (!srv || !srv->initialized || !resources_out) return -1;

    if (!srv->caps.supports_resources) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Server does not support resources");
        return -1;
    }

    json_t *resp = send_request(srv, "resources/list", NULL,
                                 srv->tool_timeout * 1000);
    if (!resp) return -1;

    json_t *result = json_obj_get(resp, "result");
    if (!result) {
        json_free(resp);
        return -1;
    }

    json_t *resources_arr = json_obj_get(result, "resources");
    if (!resources_arr) {
        json_free(resp);
        return 0;
    }

    size_t count = json_len(resources_arr);
    if (count == 0) {
        json_free(resp);
        *resources_out = NULL;
        return 0;
    }

    mcp_resource_t *resources = (mcp_resource_t *)calloc(count, sizeof(mcp_resource_t));
    if (!resources) { json_free(resp); return -1; }

    for (size_t i = 0; i < count; i++) {
        json_t *r = json_get(resources_arr, i);
        if (!r) continue;

        const char *uri = json_get_str(r, "uri", "");
        if (uri) snprintf(resources[i].uri, sizeof(resources[i].uri), "%s", uri);

        const char *name = json_get_str(r, "name", "");
        if (name) snprintf(resources[i].name, sizeof(resources[i].name), "%s", name);

        const char *desc = json_get_str(r, "description", "");
        if (desc) snprintf(resources[i].description, sizeof(resources[i].description), "%s", desc);

        const char *mime = json_get_str(r, "mimeType", "");
        if (mime) snprintf(resources[i].mime_type, sizeof(resources[i].mime_type), "%s", mime);
    }

    json_free(resp);
    *resources_out = resources;
    return (int)count;
}

mcp_resource_content_t *mcp_server_read_resource(mcp_server_t *srv,
                                                   const char *resource_uri) {
    if (!srv || !srv->initialized || !resource_uri) return NULL;

    json_t *params = json_object();
    json_set(params, "uri", json_string(resource_uri));

    json_t *resp = send_request(srv, "resources/read", params,
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

    json_t *contents = json_obj_get(result, "contents");
    mcp_resource_content_t *content = NULL;

    if (contents && json_len(contents) > 0) {
        json_t *first = json_get(contents, 0);
        if (first) {
            content = (mcp_resource_content_t *)calloc(1, sizeof(mcp_resource_content_t));
            if (content) {
                const char *uri = json_get_str(first, "uri", "");
                if (uri) snprintf(content->uri, sizeof(content->uri), "%s", uri);

                const char *mime = json_get_str(first, "mimeType", "");
                if (mime) snprintf(content->mime_type, sizeof(content->mime_type), "%s", mime);

                const char *text = json_get_str(first, "text", NULL);
                if (text) {
                    content->text = strdup(text);
                    content->text_len = strlen(text);
                    content->is_text = true;
                } else {
                    /* Check for binary blob */
                    const char *blob_b64 = json_get_str(first, "blob", NULL);
                    if (blob_b64) {
                        content->blob = (unsigned char *)strdup(blob_b64);
                        content->blob_len = strlen(blob_b64);
                        content->is_text = false;
                    }
                }
            }
        }
    }

    if (!content) {
        /* Return the raw result as text fallback */
        content = (mcp_resource_content_t *)calloc(1, sizeof(mcp_resource_content_t));
        if (content) {
            char *result_str = json_serialize(result);
            if (result_str) {
                content->text = result_str;
                content->text_len = strlen(result_str);
                content->is_text = true;
            }
        }
    }

    json_free(resp);
    json_free(params);
    return content;
}

void mcp_resource_list_free(mcp_resource_t *resources, int count) {
    (void)count;
    free(resources);
}

void mcp_resource_content_free(mcp_resource_content_t *content) {
    if (!content) return;
    free(content->text);
    free(content->blob);
    free(content);
}

/* C01: Subscribe to resource changes — sends resources/subscribe */
bool mcp_server_subscribe_resource(mcp_server_t *srv, const char *resource_uri) {
    if (!srv || !srv->initialized || !resource_uri) return false;

    /* Check if already subscribed */
    if (mcp_server_is_subscribed(srv, resource_uri)) return true;

    if (srv->subscription_count >= MAX_MCP_ROOTS) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Max subscriptions reached");
        return false;
    }

    /* Send resources/subscribe request */
    json_t *params = json_object();
    json_set(params, "uri", json_string(resource_uri));

    json_t *resp = send_request(srv, "resources/subscribe", params,
                                 srv->tool_timeout * 1000);
    json_free(params);

    if (!resp) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Subscribe request failed");
        return false;
    }

    json_t *result = json_obj_get(resp, "result");
    bool ok = (result != NULL);
    json_free(resp);

    if (ok) {
        snprintf(srv->subscriptions[srv->subscription_count],
                 sizeof(srv->subscriptions[0]), "%s", resource_uri);
        srv->subscription_count++;
    }
    return ok;
}

/* C02: Unsubscribe from resource changes */
bool mcp_server_unsubscribe_resource(mcp_server_t *srv, const char *resource_uri) {
    if (!srv || !resource_uri) return false;

    for (int i = 0; i < srv->subscription_count; i++) {
        if (strcmp(srv->subscriptions[i], resource_uri) == 0) {
            /* Send resources/unsubscribe */
            json_t *params = json_object();
            json_set(params, "uri", json_string(resource_uri));
            json_t *resp = send_request(srv, "resources/unsubscribe", params,
                                         srv->tool_timeout * 1000);
            json_free(params);
            json_free(resp); /* don't need the result */

            /* Remove from local list */
            for (int j = i; j < srv->subscription_count - 1; j++)
                snprintf(srv->subscriptions[j], sizeof(srv->subscriptions[0]),
                         "%s", srv->subscriptions[j + 1]);
            srv->subscription_count--;
            return true;
        }
    }
    return false; /* not found */
}

/* C03 helper: Check if resource is subscribed */
bool mcp_server_is_subscribed(mcp_server_t *srv, const char *resource_uri) {
    if (!srv || !resource_uri) return false;
    for (int i = 0; i < srv->subscription_count; i++) {
        if (strcmp(srv->subscriptions[i], resource_uri) == 0)
            return true;
    }
    return false;
}

/* C03: Set callback for resource change notifications */
void mcp_server_set_resource_callback(mcp_server_t *srv,
    void (*callback)(const char *server_name, const char *resource_uri, void *userdata),
    void *userdata) {
    if (!srv) return;
    srv->on_resource_change = callback;
    srv->on_resource_change_data = userdata;
}

/* C03: Handle incoming notifications — dispatches resource change events */
bool mcp_server_handle_notification(mcp_server_t *srv, const char *method,
                                     const char *params_json) {
    if (!srv || !method) return false;

    if (strcmp(method, "notifications/resources/list_changed") == 0 ||
        strcmp(method, "notifications/resources/updated") == 0) {
        /* Resource change notification */
        const char *uri = NULL;
        if (params_json && params_json[0]) {
            json_t *params = json_parse(params_json, NULL);
            if (params) {
                uri = json_get_str(params, "uri", NULL);
                json_free(params);
            }
        }

        if (srv->on_resource_change) {
            srv->on_resource_change(srv->name, uri ? uri : "*",
                                     srv->on_resource_change_data);
        }
        return true;
    }

    return false; /* unknown notification */
}

/* ================================================================
 *  P69: Prompt protocol methods
 * ================================================================ */

int mcp_server_list_prompts(mcp_server_t *srv, mcp_prompt_t **prompts_out) {
    if (!srv || !srv->initialized || !prompts_out) return -1;

    if (!srv->caps.supports_prompts) {
        snprintf(srv->last_error, sizeof(srv->last_error),
                 "Server does not support prompts");
        return -1;
    }

    json_t *resp = send_request(srv, "prompts/list", NULL,
                                 srv->tool_timeout * 1000);
    if (!resp) return -1;

    json_t *result = json_obj_get(resp, "result");
    if (!result) {
        json_free(resp);
        return -1;
    }

    json_t *prompts_arr = json_obj_get(result, "prompts");
    if (!prompts_arr) {
        json_free(resp);
        return 0;
    }

    size_t count = json_len(prompts_arr);
    if (count == 0) {
        json_free(resp);
        *prompts_out = NULL;
        return 0;
    }

    mcp_prompt_t *prompts = (mcp_prompt_t *)calloc(count, sizeof(mcp_prompt_t));
    if (!prompts) { json_free(resp); return -1; }

    for (size_t i = 0; i < count; i++) {
        json_t *p = json_get(prompts_arr, i);
        if (!p) continue;

        const char *name = json_get_str(p, "name", "");
        if (name) snprintf(prompts[i].name, sizeof(prompts[i].name), "%s", name);

        const char *desc = json_get_str(p, "description", "");
        if (desc) snprintf(prompts[i].description, sizeof(prompts[i].description), "%s", desc);

        json_t *args_schema = json_obj_get(p, "arguments");
        if (args_schema) {
            char *schema_str = json_serialize(args_schema);
            if (schema_str) {
                snprintf(prompts[i].arguments_schema,
                         sizeof(prompts[i].arguments_schema), "%s", schema_str);
                free(schema_str);
            }
        }
    }

    json_free(resp);
    *prompts_out = prompts;
    return (int)count;
}

char *mcp_server_get_prompt(mcp_server_t *srv, const char *prompt_name,
                              const char *args_json) {
    if (!srv || !srv->initialized || !prompt_name) return NULL;

    json_t *params = json_object();
    json_set(params, "name", json_string(prompt_name));

    if (args_json && args_json[0]) {
        char *err = NULL;
        json_t *args = json_parse(args_json, &err);
        if (args) {
            json_set(params, "arguments", args);
        } else {
            free(err);
        }
    }

    json_t *resp = send_request(srv, "prompts/get", params,
                                 srv->tool_timeout * 1000);
    json_free(params);
    if (!resp) return NULL;

    json_t *result = json_obj_get(resp, "result");
    char *out = NULL;
    if (result) {
        out = json_serialize(result);
    }
    json_free(resp);
    return out;
}

void mcp_prompt_list_free(mcp_prompt_t *prompts, int count) {
    (void)count;
    free(prompts);
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
    if (!srv || !srv->initialized) return false;
    if (srv->transport_type == MCP_TRANSPORT_STDIO)
        return srv->stdio.pid > 0;
    if (srv->transport_type == MCP_TRANSPORT_SSE)
        return srv->sse.http_client != NULL;
    return false;
}
