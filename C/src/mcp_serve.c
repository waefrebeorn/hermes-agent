/*
 * mcp_serve.c — MCP serve: expose Hermes tools as an MCP server.
 *
 * C11-C13: Runs an HTTP server that serves the MCP JSON-RPC protocol,
 * allowing external clients (Claude Desktop, VS Code, etc.) to discover
 * and call Hermes C tools.
 *
 * Protocol:
 *   POST /mcp  {"jsonrpc":"2.0","id":"1","method":"tools/list","params":{}}
 *   POST /mcp  {"jsonrpc":"2.0","id":"2","method":"tools/call","params":{"name":"file_read","arguments":{...}}}
 *   GET  /health  — simple health check
 *
 * Lifecycle: mcp_serve_start() spawns thread, mcp_serve_stop() joins.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

/* ================================================================
 *  Configuration
 * ================================================================ */

#define MCP_SERVE_PORT        9100   /* Default port */
#define MCP_SERVE_BACKLOG     10
#define MCP_SERVE_BUF_SIZE    131072
#define MCP_SERVE_MAX_CONN    32

/* ================================================================
 *  State
 * ================================================================ */

static int          g_serve_port = MCP_SERVE_PORT;
static bool         g_serve_running = false;
static pthread_t    g_serve_thread;
static int          g_server_fd = -1;

/* ================================================================
 *  C13: Lifecycle
 * ================================================================ */

void mcp_serve_set_port(int port) {
    if (port > 0 && port < 65536) g_serve_port = port;
}

bool mcp_serve_is_running(void) {
    return g_serve_running;
}

/* ================================================================
 *  C11: Tool discovery — serialize registered tools as MCP tool list
 * ================================================================ */

static json_t *mcp_serve_tools_list(void) {
    tool_registry_t *reg = registry_get();
    if (!reg) {
        json_t *err = json_object();
        json_set(err, "code", json_number(-32603));
        json_set(err, "message", json_string("Registry not initialized"));
        return err;
    }

    json_t *tools = json_array();
    for (size_t i = 0; i < reg->count; i++) {
        json_t *tool = json_object();
        json_set(tool, "name", json_string(reg->tools[i].name));
        json_set(tool, "description", json_string(reg->tools[i].description));
        json_set(tool, "inputSchema", json_string(reg->tools[i].schema_json));
        json_array_append(tools, tool);
    }

    json_t *result = json_object();
    json_set(result, "tools", tools);
    return result;
}

/* ================================================================
 *  C12: Tool call dispatch
 * ================================================================ */

static json_t *mcp_serve_tool_call(const char *tool_name, const char *args_json) {
    if (!tool_name || !tool_name[0]) {
        json_t *err = json_object();
        json_set(err, "code", json_number(-32602));
        json_set(err, "message", json_string("Tool name is required"));
        return err;
    }

    char *result_str = registry_dispatch(tool_name, args_json ? args_json : "{}", "");
    if (!result_str) {
        json_t *err = json_object();
        json_set(err, "code", json_number(-32603));
        json_set(err, "message", json_string("Tool dispatch returned NULL"));
        return err;
    }

    json_t *result = json_parse(result_str, NULL);
    free(result_str);

    if (!result) {
        json_t *err = json_object();
        json_set(err, "code", json_number(-32603));
        json_set(err, "message", json_string("Tool result is not valid JSON"));
        return err;
    }

    return result;
}

/* ================================================================
 *  MCP JSON-RPC dispatcher
 * ================================================================ */

static char *mcp_serve_handle_request(const char *body) {
    if (!body || !body[0]) {
        json_t *err = json_object();
        json_set(err, "code", json_number(-32700));
        json_set(err, "message", json_string("Parse error: empty body"));
        json_t *resp = json_object();
        json_set(resp, "jsonrpc", json_string("2.0"));
        json_set(resp, "id", json_null());
        json_set(resp, "error", err);
        char *s = json_serialize(resp);
        json_free(resp);
        return s;
    }

    char *jerr = NULL;
    json_t *req = json_parse(body, &jerr);
    if (jerr) {
        free(jerr);
        json_t *err = json_object();
        json_set(err, "code", json_number(-32700));
        json_set(err, "message", json_string("Parse error: invalid JSON"));
        json_t *resp = json_object();
        json_set(resp, "jsonrpc", json_string("2.0"));
        json_set(resp, "id", json_null());
        json_set(resp, "error", err);
        char *s = json_serialize(resp);
        json_free(resp);
        return s;
    }

    const char *method = json_get_str(req, "method", "");
    json_t *id_node = json_obj_get(req, "id");
    const char *id_str = NULL;
    char id_buf[64];
    if (id_node) {
        char *serialized = json_serialize(id_node);
        if (serialized) {
            snprintf(id_buf, sizeof(id_buf), "%s", serialized);
            id_str = id_buf;
            free(serialized);
        }
    }
    if (!id_str) id_str = "null";

    json_t *params = json_obj_get(req, "params");
    char *params_str = params ? json_serialize(params) : NULL;

    json_t *result = NULL;
    bool is_error = false;

    if (strcmp(method, "initialize") == 0) {
        /* Return server capabilities */
        json_t *caps = json_object();
        json_set(caps, "tools", json_object()); /* {} = supported */
        json_t *server_info = json_object();
        json_set(server_info, "name", json_string("hermes-c-mcp"));
        json_set(server_info, "version", json_string("0.1.0"));
        result = json_object();
        json_set(result, "protocolVersion", json_string("2025-03-26"));
        json_set(result, "capabilities", caps);
        json_set(result, "serverInfo", server_info);
    } else if (strcmp(method, "tools/list") == 0) {
        result = mcp_serve_tools_list();
    } else if (strcmp(method, "tools/call") == 0) {
        const char *tool_name = params ? json_get_str(params, "name", "") : "";
        json_t *args = params ? json_obj_get(params, "arguments") : NULL;
        char *args_json = args ? json_serialize(args) : NULL;
        result = mcp_serve_tool_call(tool_name, args_json ? args_json : "{}");
        if (args_json) free(args_json);
        /* Check if result is an error (has "code" field) */
        if (result && json_obj_get(result, "code")) {
            is_error = true;
        }
    } else if (strcmp(method, "ping") == 0) {
        result = json_object(); /* empty result */
    } else if (strcmp(method, "notifications/initialized") == 0) {
        result = json_object(); /* no response expected, but be polite */
    } else {
        result = json_object();
        json_set(result, "code", json_number(-32601));
        json_set(result, "message", json_string("Method not found"));
        is_error = true;
    }

    json_t *resp = json_object();
    json_set(resp, "jsonrpc", json_string("2.0"));

    /* Set id — string or proper JSON value */
    if (id_node) {
        json_set(resp, "id", json_copy(id_node));
    } else {
        json_set(resp, "id", json_null());
    }

    if (is_error) {
        json_set(resp, "error", result);
    } else {
        json_set(resp, "result", result);
    }

    char *resp_str = json_serialize(resp);
    json_free(resp);
    json_free(req);
    if (params_str) free(params_str);
    return resp_str;
}

/* ================================================================
 *  HTTP helpers
 * ================================================================ */

/* Send HTTP response */
static void send_http_response(int client_fd, int status, const char *status_text,
                                const char *content_type, const char *body) {
    char resp[65536];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, status_text,
        content_type,
        strlen(body),
        body);
    if (n > 0) {
        ssize_t written = write(client_fd, resp, (size_t)n);
        (void)written;
    }
}

/* ================================================================
 *  Request handler
 * ================================================================ */

static void handle_mcp_request(int client_fd) {
    char buf[MCP_SERVE_BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    /* Parse HTTP method and path */
    char method[16], path[1024];
    if (sscanf(buf, "%15s %1023s", method, path) < 2) {
        send_http_response(client_fd, 400, "Bad Request",
                           "text/plain", "Bad Request");
        close(client_fd);
        return;
    }

    /* Handle OPTIONS (CORS preflight) */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client_fd, 204, "No Content",
                           "text/plain", "");
        close(client_fd);
        return;
    }

    /* Handle GET /health */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        send_http_response(client_fd, 200, "OK",
                           "application/json",
                           "{\"status\":\"ok\",\"server\":\"hermes-c-mcp\"}");
        close(client_fd);
        return;
    }

    /* Handle POST /mcp or POST / */
    if (strcmp(method, "POST") == 0 &&
        (strcmp(path, "/mcp") == 0 || strcmp(path, "/") == 0)) {
        /* Read body */
        const char *body_start = strstr(buf, "\r\n\r\n");
        if (!body_start) {
            send_http_response(client_fd, 400, "Bad Request",
                               "text/plain", "Malformed HTTP request");
            close(client_fd);
            return;
        }
        body_start += 4;
        size_t body_size = (size_t)(buf + n - body_start);

        char *body = (char *)malloc(body_size + 1);
        if (!body) {
            send_http_response(client_fd, 500, "Internal Server Error",
                               "text/plain", "OOM");
            close(client_fd);
            return;
        }
        memcpy(body, body_start, body_size);
        body[body_size] = '\0';

        /* Handle MCP JSON-RPC */
        char *resp_body = mcp_serve_handle_request(body);
        send_http_response(client_fd, 200, "OK",
                           "application/json",
                           resp_body ? resp_body : "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}");
        free(body);
        free(resp_body);
        close(client_fd);
        return;
    }

    /* 404 for everything else */
    send_http_response(client_fd, 404, "Not Found",
                       "text/plain", "Not Found");
    close(client_fd);
}

/* ================================================================
 *  C13: Server lifecycle — run loop (runs in thread)
 * ================================================================ */

static void *mcp_serve_run(void *arg) {
    (void)arg;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("[mcp-serve] socket");
        g_serve_running = false;
        return NULL;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)g_serve_port);

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[mcp-serve] bind");
        close(g_server_fd);
        g_server_fd = -1;
        g_serve_running = false;
        return NULL;
    }

    if (listen(g_server_fd, MCP_SERVE_BACKLOG) < 0) {
        perror("[mcp-serve] listen");
        close(g_server_fd);
        g_server_fd = -1;
        g_serve_running = false;
        return NULL;
    }

    printf("[mcp-serve] MCP HTTP server listening on port %d\n", g_serve_port);
    printf("[mcp-serve] Endpoint: POST http://localhost:%d/mcp\n", g_serve_port);

    /* Non-blocking so we can check g_serve_running */
    int flags = fcntl(g_server_fd, F_GETFL, 0);
    fcntl(g_server_fd, F_SETFL, flags | O_NONBLOCK);

    while (g_serve_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd,
                               (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd >= 0) {
            handle_mcp_request(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (g_serve_running) {
                perror("[mcp-serve] accept");
                sleep(1);
            }
        } else {
            usleep(100000); /* 100ms — avoid busy-wait */
        }
    }

    close(g_server_fd);
    g_server_fd = -1;
    printf("[mcp-serve] Server stopped\n");
    return NULL;
}

/* C13: Start the MCP serve server (non-blocking). Returns true on success. */
bool mcp_serve_start(void) {
    if (g_serve_running) return true;

    g_serve_running = true;
    if (pthread_create(&g_serve_thread, NULL, mcp_serve_run, NULL) != 0) {
        perror("[mcp-serve] pthread_create");
        g_serve_running = false;
        return false;
    }
    pthread_detach(g_serve_thread);

    /* Brief sleep so the server can start listening */
    usleep(200000); /* 200ms */
    return true;
}

/* C13: Stop the MCP serve server. */
void mcp_serve_stop(void) {
    if (!g_serve_running) return;
    g_serve_running = false;

    /* Wake up accept() by connecting briefly */
    int tmp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tmp_fd >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((uint16_t)g_serve_port);
        connect(tmp_fd, (struct sockaddr*)&addr, sizeof(addr));
        close(tmp_fd);
    }

    usleep(300000); /* 300ms for thread to exit */
    printf("[mcp-serve] Stopped\n");
}
