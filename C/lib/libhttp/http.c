/*
 * http.c — Standalone HTTP/HTTPS client with retry support.
 * OpenSSL + POSIX sockets. Dynamic response buffers.
 * MIT License — WuBu Hermes Project
 */

#include "http.h"
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
 *  Internal helpers
 * ================================================================ */

static void *xmalloc(size_t sz) {
    void *p = malloc(sz ? sz : 1);
    if (!p) { fprintf(stderr, "http: OOM\n"); return NULL; }
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

/* ================================================================
 *  Internal types
 * ================================================================ */

struct http_t {
    int      timeout_sec;
    int      max_retries;
    int      backoff_ms;
    SSL_CTX *ssl_ctx;
    bool     ssl_init;
};

typedef struct {
    char  scheme[16];
    char  host[256];
    int   port;
    char  path[4096];
} parsed_url_t;

/* ================================================================
 *  URL parsing
 * ================================================================ */

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
    const char *host_end = strchr(p, '/');
    if (!host_end) host_end = p + strlen(p);

    size_t host_len = (size_t)(host_end - p);
    if (host_len >= sizeof(purl->host)) return false;
    memcpy(purl->host, p, host_len);
    purl->host[host_len] = '\0';

    char *port_colon = strchr(purl->host, ':');
    if (port_colon) {
        *port_colon = '\0';
        purl->port = atoi(port_colon + 1);
    }

    if (purl->port == 0) {
        purl->port = (strcmp(purl->scheme, "https") == 0) ? 443 : 80;
    }

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
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0) return -1;

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc < 0 && errno != EINPROGRESS) { close(fd); fd = -1; continue; }

        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);
            break;
        }

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
 *  SSL
 * ================================================================ */

static SSL *ssl_connect_wrap(SSL_CTX *ctx, int fd, const char *hostname) {
    SSL *ssl = SSL_new(ctx);
    if (!ssl) return NULL;
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, hostname);
    if (SSL_connect(ssl) != 1) { SSL_free(ssl); return NULL; }
    return ssl;
}

/* ================================================================
 *  Dynamic buffer reader
 * ================================================================ */

/* Read all data from socket/SSL into a dynamically grown buffer */
static char *read_all(int fd, SSL *ssl, size_t *out_len, int timeout_sec) {
    size_t cap = 8192, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    while (1) {
        if (len + 4096 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        int n;
        if (ssl) {
            n = SSL_read(ssl, buf + len, (int)(cap - len - 1));
        } else {
            /* Use select to check readability */
            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            struct timeval tv;
            tv.tv_sec = timeout_sec > 0 ? timeout_sec : 30;
            tv.tv_usec = 0;
            int rc = select(fd + 1, &rset, NULL, NULL, &tv);
            if (rc <= 0) break;
            n = (int)read(fd, buf + len, cap - len - 1);
        }
        if (n > 0) {
            len += (size_t)n;
        } else {
            if (n == 0 || (!ssl && errno != EAGAIN) || (ssl && SSL_get_error(ssl, n) != SSL_ERROR_WANT_READ))
                break;
        }
    }

    buf[len] = '\0';
    if (out_len) *out_len = len;
    /* Trim to fit */
    char *trim = (char *)realloc(buf, len + 1);
    return trim ? trim : buf;
}

/* ================================================================
 *  Retry helper
 * ================================================================ */

static void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ================================================================
 *  Public API
 * ================================================================ */

http_t *http_new(int timeout_sec) {
    return http_new_with_retry(timeout_sec, 0, 0);
}

http_t *http_new_with_retry(int timeout_sec, int max_retries, int backoff_ms) {
    http_t *c = (http_t *)xmalloc(sizeof(http_t));
    if (!c) return NULL;
    c->timeout_sec = timeout_sec > 0 ? timeout_sec : 30;
    c->max_retries = max_retries > 0 ? max_retries : 0;
    c->backoff_ms = backoff_ms > 0 ? backoff_ms : 1000;
    c->ssl_ctx = NULL;
    c->ssl_init = false;
    return c;
}

void http_free(http_t *h) {
    if (!h) return;
    if (h->ssl_ctx) SSL_CTX_free(h->ssl_ctx);
    free(h);
}

/* ================================================================
 *  Internal request (with retry)
 * ================================================================ */

static http_resp_t *do_request(http_t *h, http_method_t method,
                                const char *url,
                                const char *extra_headers,
                                const char *body, size_t body_len)
{
    int attempt = 0;
    while (1) {
        attempt++;

        parsed_url_t purl;
        memset(&purl, 0, sizeof(purl));
        if (!parse_url(url, &purl)) {
            http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
            if (!r) return NULL;
            r->status = -1;
            r->headers = xstrdup("Failed to parse URL");
            r->body = NULL; r->body_len = 0;
            return r;
        }

        bool use_ssl = strcmp(purl.scheme, "https") == 0;

        int fd = socket_connect(purl.host, purl.port, h->timeout_sec);
        if (fd < 0) {
            if (attempt <= h->max_retries) {
                sleep_ms(h->backoff_ms * attempt);
                continue;
            }
            http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
            if (!r) return NULL;
            r->status = -1;
            r->headers = xstrdup("Connection failed");
            r->body = NULL; r->body_len = 0;
            return r;
        }

        SSL *ssl = NULL;
        if (use_ssl) {
            if (!h->ssl_init) {
                SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
                SSL_library_init();
#else
                OPENSSL_init_ssl(0, NULL);
#endif
                h->ssl_ctx = SSL_CTX_new(TLS_client_method());
                if (h->ssl_ctx)
                    SSL_CTX_set_default_verify_paths(h->ssl_ctx);
                h->ssl_init = true;
            }
            ssl = ssl_connect_wrap(h->ssl_ctx, fd, purl.host);
            if (!ssl) {
                close(fd);
                if (attempt <= h->max_retries) {
                    sleep_ms(h->backoff_ms * attempt);
                    continue;
                }
                http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
                if (!r) return NULL;
                r->status = -1;
                r->headers = xstrdup("SSL handshake failed");
                r->body = NULL; r->body_len = 0;
                return r;
            }
        }

        /* Build request — dynamically */
        char method_str[16] = "GET";
        switch (method) {
            case HTTP_GET:    strcpy(method_str, "GET"); break;
            case HTTP_POST:   strcpy(method_str, "POST"); break;
            case HTTP_PUT:    strcpy(method_str, "PUT"); break;
            case HTTP_DELETE: strcpy(method_str, "DELETE"); break;
        }

        /* Estimate initial buffer size */
        size_t req_cap = 4096;
        char *req = (char *)malloc(req_cap);
        if (!req) { if (ssl) SSL_free(ssl); close(fd); goto err_oom; }
        size_t req_len = 0;

        /* Helper: append to dynamic buffer */
#define REQ_APPEND(...) do { \
            int needed = snprintf(NULL, 0, __VA_ARGS__); \
            if (needed < 0) { free(req); if (ssl) SSL_free(ssl); close(fd); goto err_oom; } \
            if (req_len + (size_t)needed + 1 >= req_cap) { \
                req_cap = (req_len + (size_t)needed + 1) * 2; \
                char *nr = (char *)realloc(req, req_cap); \
                if (!nr) { free(req); if (ssl) SSL_free(ssl); close(fd); goto err_oom; } \
                req = nr; \
            } \
            req_len += (size_t)snprintf(req + req_len, req_cap - req_len, __VA_ARGS__); \
        } while(0)

        REQ_APPEND("%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n",
                   method_str, purl.path, purl.host);

        if (extra_headers)
            REQ_APPEND("%s\r\n", extra_headers);

        if (body && body_len > 0)
            REQ_APPEND("Content-Length: %zu\r\n", body_len);

        REQ_APPEND("\r\n");

        /* Send headers */
        bool ok;
        if (ssl) ok = SSL_write(ssl, req, (int)req_len) > 0;
        else ok = socket_send_all(fd, req, req_len, h->timeout_sec);
        free(req);

        if (!ok) {
            if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
            close(fd);
            if (attempt <= h->max_retries) {
                sleep_ms(h->backoff_ms * attempt);
                continue;
            }
            http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
            if (!r) return NULL;
            r->status = -1;
            r->headers = xstrdup("Failed to send request");
            r->body = NULL; r->body_len = 0;
            return r;
        }

        /* Send body */
        if (body && body_len > 0) {
            if (ssl) ok = SSL_write(ssl, body, (int)body_len) > 0;
            else ok = socket_send_all(fd, body, body_len, h->timeout_sec);
            if (!ok) {
                if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
                close(fd);
                if (attempt <= h->max_retries) {
                    sleep_ms(h->backoff_ms * attempt);
                    continue;
                }
                http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
                if (!r) return NULL;
                r->status = -1;
                r->headers = xstrdup("Failed to send body");
                r->body = NULL; r->body_len = 0;
                return r;
            }
        }

        /* Read response */
        size_t total = 0;
        char *resp_buf = read_all(fd, ssl, &total, h->timeout_sec);

        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
        close(fd);

        if (!resp_buf) {
            if (attempt <= h->max_retries) {
                sleep_ms(h->backoff_ms * attempt);
                continue;
            }
            http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
            if (!r) return NULL;
            r->status = -1;
            r->headers = xstrdup("Failed to read response");
            r->body = NULL; r->body_len = 0;
            return r;
        }

        /* Build response */
        http_resp_t *resp = (http_resp_t *)xmalloc(sizeof(http_resp_t));
        if (!resp) { free(resp_buf); return NULL; }
        resp->headers = NULL;
        resp->body = NULL;
        resp->body_len = 0;

        /* Parse header/body boundary */
        char *header_end = strstr(resp_buf, "\r\n\r\n");
        if (!header_end) {
            resp->status = -1;
            resp->headers = xstrdup("Malformed response");
            resp->body = resp_buf;
            resp->body_len = total;
            return resp;
        }

        *header_end = '\0';

        /* Parse status code */
        const char *sl = resp_buf;
        while (*sl && *sl != ' ') sl++;
        while (*sl == ' ') sl++;
        resp->status = atoi(sl);

        resp->headers = xstrdup(resp_buf);
        resp->body = xstrdup(header_end + 4);
        resp->body_len = strlen(resp->body);
        free(resp_buf);

        /* Retry on server errors (5xx) if configured */
        if (resp->status >= 500 && attempt <= h->max_retries) {
            http_resp_free(resp);
            sleep_ms(h->backoff_ms * attempt);
            continue;
        }

        return resp;
    }

err_oom: {
    http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
    if (!r) return NULL;
    r->status = -1;
    r->headers = xstrdup("Out of memory");
    r->body = NULL; r->body_len = 0;
    return r;
}
}

/* ================================================================
 *  Public request wrappers
 * ================================================================ */

http_resp_t *http_request(http_t *h, http_method_t method,
                           const char *url,
                           const char *extra_headers,
                           const char *body, size_t body_len)
{
    return do_request(h, method, url, extra_headers, body, body_len);
}

http_resp_t *http_get(http_t *h, const char *url, const char *headers) {
    return do_request(h, HTTP_GET, url, headers, NULL, 0);
}

http_resp_t *http_post_json(http_t *h, const char *url,
                             const char *json_body)
{
    const char *headers = "Content-Type: application/json\r\n"
                          "Accept: application/json";
    return do_request(h, HTTP_POST, url, headers,
                      json_body, json_body ? strlen(json_body) : 0);
}

http_resp_t *http_post_json_auth(http_t *h, const char *url,
                                  const char *json_body,
                                  const char *auth_header)
{
    /* Build combined headers */
    size_t hdr_len = strlen("Content-Type: application/json\r\nAccept: application/json\r\n")
                   + (auth_header ? strlen(auth_header) + 2 : 0) + 1;
    char *headers = (char *)malloc(hdr_len);
    if (!headers) {
        http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
        if (!r) return NULL;
        r->status = -1;
        r->headers = xstrdup("OOM");
        r->body = NULL; r->body_len = 0;
        return r;
    }
    snprintf(headers, hdr_len,
             "Content-Type: application/json\r\nAccept: application/json\r\n%s",
             auth_header ? auth_header : "");

    http_resp_t *resp = do_request(h, HTTP_POST, url, headers,
                                    json_body, json_body ? strlen(json_body) : 0);
    free(headers);
    return resp;
}

void http_resp_free(http_resp_t *resp) {
    if (!resp) return;
    free(resp->headers);
    free(resp->body);
    free(resp);
}

char *http_url_encode(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == ' ') { out[j++] = '+'; }
        else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out[j++] = (char)c;
        else {
            snprintf(out + j, 4, "%%%02X", c);
            j += 3;
        }
    }
    out[j] = '\0';
    return out;
}
