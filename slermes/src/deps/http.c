/*
 * http.c — HTTP client for Hermes C.
 * Uses OpenSSL + POSIX sockets. No libcurl.
 */

#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

/* OpenSSL */
#include <openssl/ssl.h>
#include <openssl/err.h>

/* ================================================================
 *  Internal structures
 * ================================================================ */

/* Forward declarations for internal helpers */
static void *xmalloc(size_t sz);
static char *xstrdup(const char *s);

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "http: OOM\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

struct http_client_t {
    int      timeout_sec;
    SSL_CTX *ssl_ctx;
    bool     ssl_initialized;
};

/* ================================================================
 *  URL parsing
 * ================================================================ */

typedef struct {
    char  scheme[16];
    char  host[256];
    int   port;
    char  path[4096];
} parsed_url_t;

static bool parse_url(const char *url, parsed_url_t *purl) {
    memset(purl, 0, sizeof(*purl));
    purl->port = 0;

    const char *p = url;
    const char *colon = strstr(p, "://");
    if (!colon || colon == p) return false;

    size_t scheme_len = (size_t)(colon - p);
    if (scheme_len >= sizeof(purl->scheme)) return false;
    memcpy(purl->scheme, p, scheme_len);
    purl->scheme[scheme_len] = '\0';

    p = colon + 3;
    /* Find end of host */
    const char *host_end = strchr(p, '/');
    if (!host_end) host_end = p + strlen(p);

    size_t host_len = (size_t)(host_end - p);
    if (host_len >= sizeof(purl->host)) return false;
    memcpy(purl->host, p, host_len);
    purl->host[host_len] = '\0';

    /* Check for port in host */
    char *port_colon = strchr(purl->host, ':');
    if (port_colon) {
        *port_colon = '\0';
        purl->port = atoi(port_colon + 1);
    }

    if (purl->port == 0) {
        if (strcmp(purl->scheme, "https") == 0) purl->port = 443;
        else purl->port = 80;
    }

    /* Path */
    if (*host_end == '/')
        snprintf(purl->path, sizeof(purl->path), "%s", host_end);
    else
        snprintf(purl->path, sizeof(purl->path), "/");

    return true;
}

/* ================================================================
 *  Socket I/O
 * ================================================================ */

static int socket_connect(const char *host, int port, int timeout_sec) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0) return -1;

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        /* Non-blocking connect with timeout */
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc < 0 && errno != EINPROGRESS) { close(fd); fd = -1; continue; }

        if (rc == 0) {
            /* Connected immediately */
            fcntl(fd, F_SETFL, flags);
            break;
        }

        /* Wait for connection */
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv;
        tv.tv_sec = timeout_sec > 0 ? timeout_sec : 30;
        tv.tv_usec = 0;

        rc = select(fd + 1, NULL, &wset, NULL, &tv);
        if (rc <= 0) { close(fd); fd = -1; continue; }

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) { close(fd); fd = -1; continue; }

        fcntl(fd, F_SETFL, flags);
        break;
    }

    freeaddrinfo(res);
    return fd;
}

static bool socket_send_all(int fd, const char *data, size_t len, int timeout_sec) {
    while (len > 0) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv;
        tv.tv_sec = timeout_sec > 0 ? timeout_sec : 30;
        tv.tv_usec = 0;

        int rc = select(fd + 1, NULL, &wset, NULL, &tv);
        if (rc <= 0) return false;

        ssize_t n = write(fd, data, len);
        if (n <= 0) return false;
        data += n;
        len -= (size_t)n;
    }
    return true;
}

/* ================================================================
 *  SSL wrapper
 * ================================================================ */

static SSL *ssl_connect_wrap(SSL_CTX *ctx, int fd, const char *hostname) {
    SSL *ssl = SSL_new(ctx);
    if (!ssl) return NULL;
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, hostname);

    int rc = SSL_connect(ssl);
    if (rc != 1) {
        SSL_free(ssl);
        return NULL;
    }
    return ssl;
}

static bool ssl_send_all(SSL *ssl, const char *data, size_t len) {
    while (len > 0) {
        int n = SSL_write(ssl, data, (int)(len > 65536 ? 65536 : len));
        if (n <= 0) return false;
        data += n;
        len -= (size_t)n;
    }
    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

http_client_t *http_client_new(int timeout_sec) {
    http_client_t *c = (http_client_t *)xmalloc(sizeof(http_client_t));
    c->timeout_sec = timeout_sec > 0 ? timeout_sec : 30;
    c->ssl_ctx = NULL;
    c->ssl_initialized = false;
    return c;
}

void http_client_free(http_client_t *client) {
    if (!client) return;
    if (client->ssl_ctx) SSL_CTX_free(client->ssl_ctx);
    free(client);
}

/* ================================================================
 *  Internal request
 * ================================================================ */

static http_response_t *do_request(http_client_t *client,
                                    http_method_t method,
                                    const char *url,
                                    const char *extra_headers,
                                    const char *body,
                                    size_t body_len)
{
    parsed_url_t purl;
    if (!parse_url(url, &purl)) {
        http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
        r->status_code = -1;
        r->headers = xstrdup("Failed to parse URL");
        r->body = NULL;
        r->body_len = 0;
        return r;
    }

    bool use_ssl = strcmp(purl.scheme, "https") == 0;

    /* Connect socket */
    int fd = socket_connect(purl.host, purl.port, client->timeout_sec);
    if (fd < 0) {
        http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
        r->status_code = -1;
        r->headers = xstrdup("Connection failed");
        r->body = NULL;
        r->body_len = 0;
        return r;
    }

    SSL *ssl = NULL;
    if (use_ssl) {
        if (!client->ssl_initialized) {
            SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            SSL_library_init();
#else
            OPENSSL_init_ssl(0, NULL);
#endif
            client->ssl_ctx = SSL_CTX_new(TLS_client_method());
            if (client->ssl_ctx)
                SSL_CTX_set_default_verify_paths(client->ssl_ctx);
            client->ssl_initialized = true;
        }
        ssl = ssl_connect_wrap(client->ssl_ctx, fd, purl.host);
        if (!ssl) {
            close(fd);
            http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
            r->status_code = -1;
            r->headers = xstrdup("SSL handshake failed");
            r->body = NULL;
            r->body_len = 0;
            return r;
        }
    }

    /* Build HTTP request */
    char method_str[16];
    switch (method) {
        case HTTP_GET:    strcpy(method_str, "GET");    break;
        case HTTP_POST:   strcpy(method_str, "POST");   break;
        case HTTP_PUT:    strcpy(method_str, "PUT");    break;
        case HTTP_DELETE: strcpy(method_str, "DELETE"); break;
        default:          strcpy(method_str, "GET");    break;
    }

    char request[16384];
    int req_len = snprintf(request, sizeof(request),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        method_str, purl.path, purl.host);

    if (extra_headers)
        req_len += snprintf(request + req_len, sizeof(request) - (size_t)req_len,
                            "%s\r\n", extra_headers);

    if (body && body_len > 0)
        req_len += snprintf(request + req_len, sizeof(request) - (size_t)req_len,
                            "Content-Length: %zu\r\n", body_len);

    req_len += snprintf(request + req_len, sizeof(request) - (size_t)req_len, "\r\n");

    if ((size_t)req_len >= sizeof(request)) {
        if (ssl) SSL_shutdown(ssl);
        close(fd);
        http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
        r->status_code = -1;
        r->headers = xstrdup("Request too large");
        r->body = NULL;
        r->body_len = 0;
        return r;
    }

    /* Send request */
    bool ok;
    if (ssl)
        ok = ssl_send_all(ssl, request, (size_t)req_len);
    else
        ok = socket_send_all(fd, request, (size_t)req_len, client->timeout_sec);

    if (!ok) {
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
        close(fd);
        http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
        r->status_code = -1;
        r->headers = xstrdup("Failed to send request");
        r->body = NULL;
        r->body_len = 0;
        return r;
    }

    /* Send body */
    if (body && body_len > 0) {
        if (ssl)
            ok = ssl_send_all(ssl, body, body_len);
        else
            ok = socket_send_all(fd, body, body_len, client->timeout_sec);
        if (!ok) {
            if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
            close(fd);
            http_response_t *r = (http_response_t *)xmalloc(sizeof(http_response_t));
            r->status_code = -1;
            r->headers = xstrdup("Failed to send body");
            r->body = NULL;
            r->body_len = 0;
            return r;
        }
    }

    /* Read response */
    char resp_buf[65536];
    size_t total = 0;
    int n;
    do {
        if (ssl)
            n = SSL_read(ssl, resp_buf + total, (int)(sizeof(resp_buf) - total - 1));
        else
            n = (int)read(fd, resp_buf + total, sizeof(resp_buf) - total - 1);

        if (n > 0) total += (size_t)n;
    } while (n > 0 && total < sizeof(resp_buf) - 1);

    resp_buf[total] = '\0';

    if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
    close(fd);

    /* Parse response */
    http_response_t *resp = (http_response_t *)xmalloc(sizeof(http_response_t));
    resp->headers = NULL;
    resp->body = NULL;
    resp->body_len = 0;

    /* Find header/body boundary */
    char *header_end = strstr(resp_buf, "\r\n\r\n");
    if (!header_end) {
        resp->status_code = -1;
        resp->headers = xstrdup("Malformed response");
        return resp;
    }

    *header_end = '\0';

    /* Parse status line to get code */
    const char *status_line = resp_buf;
    while (*status_line && *status_line != ' ') status_line++;
    while (*status_line == ' ') status_line++;
    resp->status_code = atoi(status_line);

    resp->headers = xstrdup(resp_buf);
    resp->body = xstrdup(header_end + 4);
    resp->body_len = strlen(resp->body);

    return resp;
}

http_response_t *http_request_json(http_client_t *client,
                                    http_method_t method,
                                    const char *url,
                                    const char *json_body)
{
    const char *extra = "Content-Type: application/json\r\n"
                        "Accept: application/json";
    return do_request(client, method, url, extra,
                      json_body, json_body ? strlen(json_body) : 0);
}

http_response_t *http_request(http_client_t *client,
                              http_method_t method,
                              const char *url,
                              const char *headers,
                              const char *body,
                              size_t body_len)
{
    return do_request(client, method, url, headers, body, body_len);
}

void http_response_free(http_response_t *resp) {
    if (!resp) return;
    free(resp->headers);
    free(resp->body);
    free(resp);
}

char *http_url_encode(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *out = (char *)xmalloc(len * 3 + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == ' ') { out[j++] = '+'; }
        else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = (char)c;
        } else {
            snprintf(out + j, 4, "%%%02X", c);
            j += 3;
        }
    }
    out[j] = '\0';
    return out;
}
