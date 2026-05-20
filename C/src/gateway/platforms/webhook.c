/*
 * webhook.c — HTTP API gateway for Hermes C.
 * Accepts POST /webhook with JSON {"text":"...", "chat_id":"..."}
 * Returns JSON {"status":"ok", "response":"..."}
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

/* ================================================================
 *  Safe write wrapper (discards return value to suppress warnings)
 * ================================================================ */

static inline void safe_write(int fd, const void *buf, size_t count) {
    ssize_t r = write(fd, buf, count);
    (void)r;
}

/* ================================================================
 *  HTTP response builder
 * ================================================================ */

static char *build_http_response(int status, const char *status_text,
                                  const char *content_type,
                                  const char *body) {
    int blen = strlen(body);
    /* header: ~256 bytes + body */
    int total = 256 + blen + 1;
    char *resp = malloc(total);
    if (!resp) return NULL;

    int n = snprintf(resp, total,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n"
        "%s",
        status, status_text,
        content_type,
        blen,
        body);
    if (n < 0) { free(resp); return NULL; }
    return resp;
}

/* ================================================================
 *  JSON string escaper
 * ================================================================ */

static char *json_escape_str(const char *s) {
    if (!s) return strdup("null");
    size_t cap = strlen(s) * 2 + 3; /* worst case: all chars escaped */
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    out[j++] = '"';
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  out[j++] = '\\'; out[j++] = '"';  break;
            case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
            case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
            case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
            case '\t': out[j++] = '\\'; out[j++] = 't';  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    /* control char — skip or encode as \\u00xx */
                    j += snprintf(out + j, cap - j, "\\u%04x", (unsigned char)*p);
                } else {
                    out[j++] = *p;
                }
                break;
        }
        if (j >= cap - 8) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
    }
    out[j++] = '"';
    out[j] = '\0';
    return out;
}

/* ================================================================
 *  HTTP request body extractor
 * ================================================================
 * Returns pointer to body within buf (after \r\n\r\n), or NULL.
 */

static const char *extract_http_body(const char *buf) {
    const char *body = strstr(buf, "\r\n\r\n");
    return body ? body + 4 : NULL;
}

/* ================================================================
 *  Extract HTTP method + path
 * ================================================================
 * Parses "POST /webhook HTTP/1.1" from buf.
 * Returns -1 on error.
 */

static int parse_http_request_line(const char *buf, char *method, size_t msize,
                                    char *path, size_t psize) {
    /* Skip leading whitespace */
    while (*buf == ' ' || *buf == '\t' || *buf == '\r' || *buf == '\n') buf++;
    if (!*buf) return -1;

    /* Read method up to space */
    size_t i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < msize - 1) {
        method[i++] = *buf++;
    }
    method[i] = '\0';
    if (*buf != ' ') return -1;
    buf++; /* skip space */

    /* Read path up to space */
    i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < psize - 1) {
        path[i++] = *buf++;
    }
    path[i] = '\0';
    return 0;
}

/* ================================================================
 *  Handle a single webhook request
 * ================================================================ */

static void handle_webhook_request(int client_fd) {
    char buf[65536];
    int n = (int)read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    char method[16], path[512];
    if (parse_http_request_line(buf, method, sizeof(method),
                                 path, sizeof(path)) < 0) {
        char *r = build_http_response(400, "Bad Request",
                                       "text/plain", "Bad Request");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        char *r = build_http_response(204, "No Content",
                                       "text/plain", "");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* GET /health — simple health check */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/health") == 0) {
        char *r = build_http_response(200, "OK",
                                       "application/json",
                                       "{\"status\":\"ok\",\"service\":\"hermes-webhook\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* Only POST /webhook */
    if (strcmp(method, "POST") != 0 || strcmp(path, "/webhook") != 0) {
        /* Check for WhatsApp webhook callback on GET /whatsapp-webhook */
        if (strcmp(method, "GET") == 0 && strcmp(path, "/whatsapp-webhook") == 0) {
            const char *qs = strchr(buf, '?');
            if (qs) qs++; else qs = "";
            const char *challenge = whatsapp_verify_webhook(qs);
            if (challenge) {
                char *r = build_http_response(200, "OK",
                                               "text/plain", challenge);
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            } else {
                char *r = build_http_response(403, "Forbidden",
                                               "text/plain", "Verification failed");
                if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            }
            close(client_fd);
            return;
        }

        /* Check for WhatsApp POST callback */
        if (strcmp(method, "POST") == 0 && strcmp(path, "/whatsapp-webhook") == 0) {
            const char *body = extract_http_body(buf);
            if (body) {
                json_node_t *updates = whatsapp_parse_webhook(body);
                if (updates && json_len(updates) > 0) {
                    size_t n = json_len(updates);
                    for (size_t i = 0; i < n; i++) {
                        json_node_t *update = json_get(updates, i);
                        const char *from = whatsapp_get_chat_id(update);
                        const char *text = whatsapp_get_text(update);
                        if (from && text) {
                            printf("[gateway:whatsapp] Message from %s: %s\n",
                                   from, text);
                            char *resp = agent_chat(&g_gw.agent, text);
                            if (resp) {
                                whatsapp_send_message(g_gw.http, from, resp);
                                free(resp);
                            }
                        }
                    }
                    json_free(updates);
                }
            }
            char *r = build_http_response(200, "OK",
                                           "application/json",
                                           "{\"status\":\"ok\"}");
            if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        char *r = build_http_response(404, "Not Found",
                                       "application/json",
                                       "{\"error\":\"Not Found. Use POST /webhook\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    const char *body = extract_http_body(buf);
    if (!body) {
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"No request body\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    json_node_t *root = json_parse(body, NULL);
    if (!root) {
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"Invalid JSON\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    const char *text = json_get_str(root, "text", "");
    const char *chat_id = json_get_str(root, "chat_id", "");

    if (!*text) {
        json_free(root);
        char *r = build_http_response(400, "Bad Request",
                                       "application/json",
                                       "{\"error\":\"Missing 'text' field\"}");
        if (r) { safe_write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    printf("[gateway:webhook] POST /webhook from '%s': %s\n",
           *chat_id ? chat_id : "(anonymous)", text);

    /* Get agent state via gateway extern — defined in server.c */
    extern gateway_state_t g_gw;
    char *agent_resp = agent_chat(&g_gw.agent, text);

    /* Build JSON response */
    char *escaped_resp = json_escape_str(agent_resp ? agent_resp : "");
    char *escaped_chat = json_escape_str(chat_id);

    char json_buf[65536];
    snprintf(json_buf, sizeof(json_buf),
        "{\"status\":\"ok\",\"response\":%s,\"chat_id\":%s}",
        escaped_resp, escaped_chat);

    char *http_resp = build_http_response(200, "OK",
                                           "application/json", json_buf);
    if (http_resp) {
        safe_write(client_fd, http_resp, strlen(http_resp));
        free(http_resp);
    }

    free(escaped_resp);
    free(escaped_chat);
    json_free(root);
    free(agent_resp);
    close(client_fd);
}

/* ================================================================
 *  Main webhook server loop
 * ================================================================
 * Creates TCP server socket, accepts connections, handles requests.
 * Blocks until g_gw.running is set to false.
 */

void webhook_server_run(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[webhook] socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[webhook] bind");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        perror("[webhook] listen");
        close(server_fd);
        return;
    }

    printf("[webhook] HTTP API server listening on port %d\n", port);

    /* Make server socket non-blocking so we can check g_gw.running */
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    extern gateway_state_t g_gw;

    while (g_gw.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd >= 0) {
            handle_webhook_request(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            /* Real error, not just no connection waiting */
            if (g_gw.running) {
                perror("[webhook] accept");
                sleep(1);
            }
        } else {
            /* No pending connection — sleep briefly to avoid busy-wait */
            usleep(100000); /* 100ms */
        }
    }

    close(server_fd);
    printf("[webhook] Server stopped\n");
}
