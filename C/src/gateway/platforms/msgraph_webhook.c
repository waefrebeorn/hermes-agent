/*
 * msgraph_webhook.c — Microsoft Graph webhook gateway for Hermes C.
 * Raw socket HTTP server for incoming change notifications.
 * Handles subscription validation (GET) and change notification (POST).
 * MIT License — WuBu Slermes Project
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 *  Limits
 * ================================================================ */

#define MAX_SEEN_RECEIPTS 5000
#define RECEIPT_TTL 3600 /* 1 hour — discard old receipts periodically */
#define MAX_BODY 65536
#define DEFAULT_PORT 8646
#define DEFAULT_WEBHOOK_PATH "/msgraph/webhook"
#define DEFAULT_HEALTH_PATH "/health"

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    char *webhook_path;
    char *health_path;
    int port;
    int accepted_count;
    int duplicate_count;
} msgraph_state_t;

static msgraph_state_t g_msgraph;

/* Simple string set for dedup (ring buffer) */
typedef struct {
    char **entries;
    size_t count;
    size_t capacity;
    time_t *timestamps;
} str_set_t;

static str_set_t g_seen;

static void seen_init(void) {
    g_seen.capacity = MAX_SEEN_RECEIPTS;
    g_seen.entries = calloc(g_seen.capacity, sizeof(char *));
    g_seen.timestamps = calloc(g_seen.capacity, sizeof(time_t));
    g_seen.count = 0;
}

static void seen_destroy(void) {
    for (size_t i = 0; i < g_seen.count; i++)
        free(g_seen.entries[i]);
    free(g_seen.entries);
    free(g_seen.timestamps);
}

static bool seen_contains(const char *key) {
    for (size_t i = 0; i < g_seen.count; i++) {
        if (g_seen.entries[i] && strcmp(g_seen.entries[i], key) == 0)
            return true;
    }
    return false;
}

static void seen_add(const char *key) {
    if (g_seen.count >= g_seen.capacity) {
        /* Evict oldest */
        free(g_seen.entries[0]);
        memmove(g_seen.entries, g_seen.entries + 1,
                (g_seen.count - 1) * sizeof(char *));
        memmove(g_seen.timestamps, g_seen.timestamps + 1,
                (g_seen.count - 1) * sizeof(time_t));
        g_seen.count--;
    }
    g_seen.entries[g_seen.count] = strdup(key);
    g_seen.timestamps[g_seen.count] = time(NULL);
    g_seen.count++;
}

/* ================================================================
 *  Helpers
 * ================================================================ */

/* ================================================================
 *  HTTP response builder
 * ================================================================ */

/* No CIDR filter or clientState verification — everything allowed */
#define source_ip_allowed(remote) true
#define client_state_verified(body) true

static char *build_http_response(int status, const char *status_text,
                                  const char *content_type,
                                  const char *body) {
    int blen = body ? (int)strlen(body) : 0;
    int total = 256 + blen + 1;
    char *resp = malloc(total);
    if (!resp) return NULL;

    int n = snprintf(resp, total,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n"
        "%s",
        status, status_text,
        content_type,
        blen,
        body ? body : "");
    if (n < 0) { free(resp); return NULL; }
    return resp;
}

/* ================================================================
 *  HTTP helpers
 * ================================================================ */

static const char *extract_http_body(const char *buf) {
    const char *body = strstr(buf, "\r\n\r\n");
    return body ? body + 4 : NULL;
}

static int parse_http_request_line(const char *buf, char *method, size_t msize,
                                    char *path, size_t psize, char *query, size_t qsize) {
    while (*buf == ' ' || *buf == '\t' || *buf == '\r' || *buf == '\n') buf++;
    if (!*buf) return -1;

    size_t i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < msize - 1)
        method[i++] = *buf++;
    method[i] = '\0';
    if (*buf != ' ') return -1;
    buf++;

    /* Read path + query */
    i = 0;
    while (*buf && *buf != ' ' && *buf != '\r' && i < psize - 1) {
        if (*buf == '?') {
            path[i] = '\0';
            buf++;
            /* Copy query string */
            size_t qi = 0;
            while (*buf && *buf != ' ' && *buf != '\r' && qi < qsize - 1)
                query[qi++] = *buf++;
            query[qi] = '\0';
            return 0;
        }
        path[i++] = *buf++;
    }
    path[i] = '\0';
    query[0] = '\0';
    return 0;
}

/* ================================================================
 *  Request handler
 * ================================================================ */

static void handle_request(int client_fd) {
    char buf[MAX_BODY];
    int n = (int)read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(client_fd); return; }
    buf[n] = '\0';

    char method[16], path[512], query[1024];
    if (parse_http_request_line(buf, method, sizeof(method),
                                 path, sizeof(path), query, sizeof(query)) < 0) {
        char *r = build_http_response(400, "Bad Request", "text/plain", "Bad Request");
        if (r) { write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        char *r = build_http_response(204, "No Content", "text/plain", "");
        if (r) { write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* GET /health */
    if (strcmp(method, "GET") == 0 && strcmp(path, g_msgraph.health_path) == 0) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"status\":\"ok\",\"platform\":\"msgraph_webhook\","
            "\"webhook_path\":\"%s\",\"accepted\":%d,\"duplicates\":%d}",
            g_msgraph.webhook_path, g_msgraph.accepted_count, g_msgraph.duplicate_count);
        char *r = build_http_response(200, "OK", "application/json", json);
        if (r) { write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* GET {webhook_path} — subscription validation */
    if (strcmp(method, "GET") == 0 && strcmp(path, g_msgraph.webhook_path) == 0) {
        /* Extract validationToken from query string */
        const char *vt = NULL;
        if (query[0]) {
            const char *tok = strstr(query, "validationToken=");
            if (tok) {
                tok += 15;
                const char *end = strchr(tok, '&');
                size_t len = end ? (size_t)(end - tok) : strlen(tok);
                char *vt_buf = malloc(len + 1);
                if (vt_buf) {
                    memcpy(vt_buf, tok, len);
                    vt_buf[len] = '\0';
                    vt = vt_buf;
                }
            }
        }

        if (vt) {
            char *r = build_http_response(200, "OK", "text/plain", vt);
            if (r) { write(client_fd, r, strlen(r)); free(r); }
        } else {
            char *r = build_http_response(400, "Bad Request", "text/plain",
                                           "validationToken required");
            if (r) { write(client_fd, r, strlen(r)); free(r); }
        }
        close(client_fd);
        if (vt) free((void *)vt);
        return;
    }

    /* POST {webhook_path} — notification */
    if (strcmp(method, "POST") == 0 && strcmp(path, g_msgraph.webhook_path) == 0) {
        const char *body = extract_http_body(buf);
        if (!body) {
            char *r = build_http_response(400, "Bad Request", "application/json",
                                           "{\"error\":\"No body\"}");
            if (r) { write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        /* Parse JSON body */
        json_t *root = json_parse(body, NULL);
        if (!root) {
            char *r = build_http_response(400, "Bad Request", "application/json",
                                           "{\"error\":\"Invalid JSON\"}");
            if (r) { write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        /* Verify client state */
        if (!client_state_verified(body)) {
            json_free(root);
            char *r = build_http_response(403, "Forbidden", "application/json",
                                           "{\"error\":\"Invalid clientState\"}");
            if (r) { write(client_fd, r, strlen(r)); free(r); }
            close(client_fd);
            return;
        }

        /* Extract notifications from the "value" array */
        json_t *value = json_obj_get(root, "value");
        int accepted = 0, duplicates = 0, rejected = 0;

        if (value && value->type == JSON_ARRAY) {
            size_t n = json_len(value);
            for (size_t i = 0; i < n; i++) {
                json_t *notif = json_get(value, i);
                if (!notif || notif->type != JSON_OBJECT) {
                    rejected++;
                    continue;
                }

                /* Build receipt key from notification id */
                const char *notif_id = json_get_str(notif, "id", "");
                char receipt_key[512];
                snprintf(receipt_key, sizeof(receipt_key), "id:%s", notif_id);

                /* Dedup */
                if (seen_contains(receipt_key)) {
                    duplicates++;
                    continue;
                }
                seen_add(receipt_key);

                accepted++;

                /* Format the notification text */
                const char *resource = json_get_str(notif, "resource", "");
                const char *change_type = json_get_str(notif, "changeType", "");
                const char *sub_id = json_get_str(notif, "subscriptionId", "");

                /* Build prompt for the agent */
                char *notif_json = json_serialize(notif);
                char prompt[8192];
                if (notif_json) {
                    size_t nl = strlen(notif_json);
                    if (nl > 4000) nl = 4000;
                    snprintf(prompt, sizeof(prompt),
                        "Microsoft Graph change notification:\n\n"
                        "```json\n%.*s\n```",
                        (int)nl, notif_json);
                    free(notif_json);
                } else {
                    snprintf(prompt, sizeof(prompt),
                        "Microsoft Graph notification: resource=%s change_type=%s",
                        resource, change_type);
                }

                printf("[gateway:msgraph] Notification: resource=%s type=%s sub=%s\n",
                       resource, change_type, sub_id);

                /* Forward to agent */
                extern gateway_state_t g_gw;
                char *agent_resp = agent_chat(&g_gw.agent, prompt);
                if (agent_resp) {
                    printf("[gateway:msgraph] Agent response: %.200s\n", agent_resp);
                    free(agent_resp);
                }
            }
        }

        json_free(root);

        g_msgraph.accepted_count += accepted;
        g_msgraph.duplicate_count += duplicates;

        /* Choose response status */
        int resp_status = 202;
        const char *resp_body = "{\"status\":\"accepted\"}";
        if (accepted == 0 && duplicates == 0 && rejected > 0)
            resp_status = 400;
        if (accepted == 0 && duplicates == 0 && rejected == 0)
            resp_status = 400;

        char *r = build_http_response(resp_status, resp_status == 202 ? "Accepted" : "Bad Request",
                                       "application/json", resp_body);
        if (r) { write(client_fd, r, strlen(r)); free(r); }
        close(client_fd);
        return;
    }

    /* 404 */
    char *r = build_http_response(404, "Not Found", "application/json",
                                   "{\"error\":\"Not Found\"}");
    if (r) { write(client_fd, r, strlen(r)); free(r); }
    close(client_fd);
}

/* ================================================================
 *  Main server loop
 * ================================================================ */

static void msgraph_server_run(void) {
    int port = g_msgraph.port;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("[msgraph_webhook] socket"); return; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[msgraph_webhook] bind");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        perror("[msgraph_webhook] listen");
        close(server_fd);
        return;
    }

    printf("[msgraph_webhook] Listening on port %d%s\n", port, g_msgraph.webhook_path);

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    extern gateway_state_t g_gw;

    seen_init();

    while (g_gw.running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd >= 0) {
            handle_request(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (g_gw.running) {
                perror("[msgraph_webhook] accept");
                sleep(1);
            }
        } else {
            usleep(100000);
        }
    }

    seen_destroy();
    close(server_fd);
    printf("[msgraph_webhook] Server stopped\n");
}

/* ================================================================
 *  Public API
 * ================================================================ */

void msgraph_webhook_init(const char *webhook_path, const char *health_path, int port) {
    memset(&g_msgraph, 0, sizeof(g_msgraph));
    g_msgraph.webhook_path = strdup(webhook_path ? webhook_path : DEFAULT_WEBHOOK_PATH);
    g_msgraph.health_path = strdup(health_path ? health_path : DEFAULT_HEALTH_PATH);
    g_msgraph.port = port > 0 ? port : DEFAULT_PORT;
}

void msgraph_webhook_run(void) {
    msgraph_server_run();
}

void msgraph_webhook_stop(void) {
    free(g_msgraph.webhook_path);
    free(g_msgraph.health_path);
}
