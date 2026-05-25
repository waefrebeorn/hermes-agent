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

/* zlib for gzip/deflate decompression */
#include <zlib.h>

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
    int      max_redirects;    /* 0 = disabled, default 5 */
    bool     decompress;       /* auto-decompress gzip/deflate */
    SSL_CTX *ssl_ctx;
    bool     ssl_init;
    char     proxy[512];
    /* Cookie jar */
    bool     cookies_enabled;
    http_cookie_t cookies[HTTP_COOKIE_MAX];
    int      cookie_count;
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
    c->proxy[0] = '\0';
    c->max_redirects = 5;  /* default: follow up to 5 redirects */
    c->decompress = false; /* default: return raw response */

    /* Auto-detect proxy from environment (HTTPS_PROXY > HTTP_PROXY > ALL_PROXY) */
    const char *env_keys[] = {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY",
                              "http_proxy", "ALL_PROXY", "all_proxy", NULL};
    for (int i = 0; env_keys[i]; i++) {
        const char *val = getenv(env_keys[i]);
        if (val && val[0]) {
            snprintf(c->proxy, sizeof(c->proxy), "%s", val);
            break;
        }
    }

    /* Check NO_PROXY — if it's "*", disable proxy entirely */
    const char *no_proxy = getenv("NO_PROXY");
    if (!no_proxy || !no_proxy[0])
        no_proxy = getenv("no_proxy");
    if (no_proxy && no_proxy[0] && strcmp(no_proxy, "*") == 0)
        c->proxy[0] = '\0';

    return c;
}

void http_free(http_t *h) {
    if (!h) return;
    if (h->ssl_ctx) SSL_CTX_free(h->ssl_ctx);
    free(h);
}

/* Set HTTP proxy for all subsequent requests.
 * proxy_url format: "http://host:port" or "host:port".
 * Clears proxy when proxy_url is NULL or empty. */
void http_client_set_proxy(http_t *h, const char *proxy_url) {
    if (!h) return;
    if (proxy_url && proxy_url[0])
        snprintf(h->proxy, sizeof(h->proxy), "%s", proxy_url);
    else
        h->proxy[0] = '\0';
}

/* Forward declaration for decompression function defined below */
static char *http_decompress_body(const char *compressed, size_t compressed_len,
                                   size_t *decompressed_len);

/* ================================================================
 *  Core request logic
 * ================================================================ */

static http_resp_t *do_request(http_t *h, http_method_t method,
                                const char *url,
                                const char *extra_headers,
                                const char *body, size_t body_len)
{
    int attempt = 0;
    int redirect_count = 0;
    char redirect_url[4096];
    (void)redirect_url; /* used below for redirect following */
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

        /* Resolve connect target — proxy may override */
        const char *connect_host = purl.host;
        int connect_port = purl.port;
        bool use_proxy = h->proxy[0] != '\0';

        /* NO_PROXY bypass — check if target host is excluded */
        if (use_proxy) {
            const char *np = getenv("NO_PROXY");
            if (!np || !np[0]) np = getenv("no_proxy");
            if (np && np[0]) {
                /* Check for exact or suffix match with leading dot */
                char np_buf[1024];
                snprintf(np_buf, sizeof(np_buf), "%s", np);
                char *token = np_buf;
                while (token && *token) {
                    /* Skip leading spaces */
                    while (*token == ' ') token++;
                    char *comma = strchr(token, ',');
                    if (comma) *comma = '\0';
                    /* Trim trailing spaces */
                    char *end = token + strlen(token) - 1;
                    while (end > token && *end == ' ') *end-- = '\0';
                    if (token[0]) {
                        if (token[0] == '.' && strstr(purl.host, token) != NULL) {
                            /* Suffix match: .example.com matches any.example.com */
                            use_proxy = false;
                            break;
                        } else if (strcasecmp(purl.host, token) == 0) {
                            use_proxy = false;
                            break;
                        }
                    }
                    token = comma ? comma + 1 : NULL;
                }
            }
        }

        if (use_proxy) {
            /* Parse proxy URL to get proxy host:port */
            parsed_url_t proxy_url;
            memset(&proxy_url, 0, sizeof(proxy_url));
            char proxy_url_buf[512];
            snprintf(proxy_url_buf, sizeof(proxy_url_buf), "%s", h->proxy);
            /* Prepend http:// if missing */
            if (strncmp(h->proxy, "http://", 7) != 0 &&
                strncmp(h->proxy, "https://", 8) != 0) {
                char with_scheme[520];
                snprintf(with_scheme, sizeof(with_scheme), "http://%s", h->proxy);
                parse_url(with_scheme, &proxy_url);
            } else {
                parse_url(proxy_url_buf, &proxy_url);
            }
            if (proxy_url.host[0]) {
                connect_host = proxy_url.host;
                connect_port = proxy_url.port > 0 ? proxy_url.port :
                               (strcmp(proxy_url.scheme, "https") == 0 ? 443 : 8080);
            }
        }

        int fd = socket_connect(connect_host, connect_port, h->timeout_sec);
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
            /* For HTTPS through proxy: send CONNECT tunnel request first */
            if (use_proxy) {
                char connect_req[1024];
                snprintf(connect_req, sizeof(connect_req),
                         "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
                         purl.host, purl.port, purl.host, purl.port);
                socket_send_all(fd, connect_req, strlen(connect_req),
                                h->timeout_sec);

                /* Read CONNECT response */
                size_t resp_len = 0;
                char *resp = read_all(fd, NULL, &resp_len, h->timeout_sec);
                if (!resp || strstr(resp, "200") == NULL) {
                    free(resp);
                    close(fd);
                    if (attempt <= h->max_retries) {
                        sleep_ms(h->backoff_ms * attempt);
                        continue;
                    }
                    http_resp_t *r = (http_resp_t *)xmalloc(sizeof(http_resp_t));
                    if (!r) return NULL;
                    r->status = -1;
                    r->headers = xstrdup("Proxy CONNECT failed");
                    r->body = NULL; r->body_len = 0;
                    return r;
                }
                free(resp);
            }

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

        /* Append Cookie header from cookie jar if enabled */
        if (h->cookies_enabled && h->cookie_count > 0) {
            char *cookie_hdr = http_cookie_build_header(h, url);
            if (cookie_hdr) {
                REQ_APPEND("Cookie: %s\r\n", cookie_hdr);
                free(cookie_hdr);
            }
        }

        /* Auto-add Accept-Encoding for decompression */
        if (h->decompress)
            REQ_APPEND("Accept-Encoding: gzip, deflate\r\n");

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

        /* Parse Set-Cookie headers from response */
        if (h->cookies_enabled) {
            const char *sc = resp->headers;
            while ((sc = strstr(sc, "Set-Cookie:")) != NULL) {
                sc += 11;
                while (*sc == ' ') sc++;
                const char *nl = strstr(sc, "\r\n");
                size_t sc_len = nl ? (size_t)(nl - sc) : strlen(sc);
                char *sc_val = (char *)malloc(sc_len + 1);
                if (sc_val) {
                    memcpy(sc_val, sc, sc_len);
                    sc_val[sc_len] = '\0';
                    http_cookie_parse_set_cookie(h, sc_val);
                    free(sc_val);
                }
                if (!nl) break;
                sc = nl + 2;
            }
        }

        /* Handle chunked transfer encoding */
        if (strstr(resp->headers, "Transfer-Encoding: chunked") ||
            strstr(resp->headers, "transfer-encoding: chunked")) {
            char *dechunked = NULL;
            size_t dechunked_len = 0;
            const char *src = resp->body;
            while (*src) {
                long chunk_sz = strtol(src, (char**)&src, 16);
                if (chunk_sz == 0) break;
                while (*src == '\r' || *src == '\n') src++;
                char *newbuf = realloc(dechunked, dechunked_len + (size_t)chunk_sz + 1);
                if (!newbuf) { free(dechunked); dechunked = NULL; break; }
                dechunked = newbuf;
                memcpy(dechunked + dechunked_len, src, (size_t)chunk_sz);
                dechunked_len += (size_t)chunk_sz;
                src += chunk_sz;
                while (*src == '\r' || *src == '\n') src++;
            }
            if (dechunked) {
                dechunked[dechunked_len] = '\0';
                free(resp->body);
                resp->body = dechunked;
                resp->body_len = dechunked_len;
            }
        }
        free(resp_buf);

        /* L07: Auto-decompress gzip/deflate body if enabled */
        if (h->decompress && resp->body && resp->body_len > 0) {
            const char *ce = strstr(resp->headers, "Content-Encoding:");
            if (!ce) ce = strstr(resp->headers, "content-encoding:");
            if (ce) {
                bool is_gzip = strstr(ce, "gzip") != NULL;
                bool is_deflate = strstr(ce, "deflate") != NULL;
                if (is_gzip || is_deflate) {
                    size_t dec_len = 0;
                    char *dec = http_decompress_body(resp->body, resp->body_len, &dec_len);
                    if (dec) {
                        free(resp->body);
                        resp->body = dec;
                        resp->body_len = dec_len;
                    }
                }
            }
        }

        /* Retry on server errors (5xx) if configured */
        if (resp->status >= 500 && attempt <= h->max_retries) {
            http_resp_free(resp);
            sleep_ms(h->backoff_ms * attempt);
            continue;
        }

        /* L06: Redirect following — handle 3xx responses */
        if (h->max_redirects > 0 &&
            (resp->status == 301 || resp->status == 302 ||
             resp->status == 303 || resp->status == 307 || resp->status == 308) &&
            redirect_count < h->max_redirects) {
            /* Extract Location header from response headers */
            const char *loc = strstr(resp->headers, "Location:");
            if (!loc) loc = strstr(resp->headers, "location:");
            if (loc) {
                loc += 9; /* skip "Location:" */
                while (*loc == ' ') loc++;
                /* Copy to newline or end */
                size_t loc_len = 0;
                const char *nl = strstr(loc, "\r\n");
                if (nl) loc_len = (size_t)(nl - loc);
                else loc_len = strlen(loc);
                if (loc_len > 0 && loc_len < sizeof(redirect_url) - 1) {
                    memcpy(redirect_url, loc, loc_len);
                    redirect_url[loc_len] = '\0';
                    /* Resolve relative URL against original */
                    if (redirect_url[0] == '/') {
                        /* Relative path — prepend scheme + host from original */
                        parsed_url_t p;
                        if (parse_url(url, &p)) {
                            char base[4096];
                            snprintf(base, sizeof(base), "%s://%s", p.scheme, p.host);
                            if (p.port > 0 && p.port != 80 && p.port != 443)
                                snprintf(base + strlen(base), sizeof(base) - strlen(base),
                                         ":%d", p.port);
                            /* Build full URL: base + relative_path */
                            char tmp[4096];
                            snprintf(tmp, sizeof(tmp), "%s%s", base, redirect_url);
                            memcpy(redirect_url, tmp, sizeof(redirect_url));
                        }
                    }
                    redirect_count++;
                    http_resp_free(resp);
                    /* resp_buf already freed above */
                    url = redirect_url;
                    continue;
                }
            }
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

/* ================================================================
 *  SSE Streaming
 * ================================================================ */

/* Read one line (up to newline) from socket/SSL into buf.
 * Returns number of chars read (incl newline), 0 on closed, -1 on error. */
static int read_line_buffered(int fd, SSL *ssl, char *buf, size_t cap,
                               char *rbuf, size_t *rlen, size_t rcap, int timeout_sec) {
    size_t out = 0;
    while (out + 1 < cap) {
        if (*rlen == 0) {
            /* Refill read buffer */
            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            struct timeval tv;
            tv.tv_sec = timeout_sec > 0 ? timeout_sec : 30;
            tv.tv_usec = 0;
            int rc = select(fd + 1, &rset, NULL, NULL, &tv);
            if (rc <= 0) return -1;
            int n = ssl ? SSL_read(ssl, rbuf, (int)rcap) : (int)read(fd, rbuf, rcap);
            if (n <= 0) return (out > 0) ? (int)out : (n == 0 ? 0 : -1);
            *rlen = (size_t)n;
        }
        /* Copy from rbuf to out until newline or out of data */
        size_t i = 0;
        while (i < *rlen && out + 1 < cap) {
            buf[out++] = rbuf[i];
            if (rbuf[i] == '\n') {
                i++;
                *rlen -= i;
                memmove(rbuf, rbuf + i, *rlen);
                buf[out] = '\0';
                return (int)out;
            }
            i++;
        }
        *rlen -= i;
        memmove(rbuf, rbuf + i, *rlen);
    }
    buf[out] = '\0';
    return (int)out;
}

int http_stream_request(http_t *h, http_method_t method,
                        const char *url,
                        const char *extra_headers,
                        const char *body, size_t body_len,
                        http_stream_cb callback, void *userdata) {
    parsed_url_t purl;
    memset(&purl, 0, sizeof(purl));
    if (!parse_url(url, &purl)) return -1;

    bool use_ssl = strcmp(purl.scheme, "https") == 0;

    int fd = socket_connect(purl.host, purl.port, h->timeout_sec);
    if (fd < 0) return -1;

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
            if (h->ssl_ctx) SSL_CTX_set_default_verify_paths(h->ssl_ctx);
            h->ssl_init = true;
        }
        ssl = ssl_connect_wrap(h->ssl_ctx, fd, purl.host);
        if (!ssl) { close(fd); return -1; }
    }

    /* Build request */
    char method_str[16] = "GET";
    switch (method) {
        case HTTP_GET:    strcpy(method_str, "GET"); break;
        case HTTP_POST:   strcpy(method_str, "POST"); break;
        case HTTP_PUT:    strcpy(method_str, "PUT"); break;
        case HTTP_DELETE: strcpy(method_str, "DELETE"); break;
    }

    size_t req_cap = 4096;
    char *req = (char *)malloc(req_cap);
    if (!req) { if (ssl) SSL_free(ssl); close(fd); return -1; }
    size_t req_len = 0;
    int ss;

    /* Build request string manually (avoid REQ_APPEND redefinition) */
    {
        /* extra_headers typically: "Authorization: Bearer ***\r\nContent-Type: application/json"
         * The format below: add \r\n after extra_headers to terminate its last header,
         * then Content-Length as another header, then \r\n blank-line = header terminator. */
        char hdr[8192];
        int n;
        if (body && body_len > 0) {
            n = snprintf(hdr, sizeof(hdr),
                "%s %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Accept: text/event-stream\r\n"
                "Connection: close\r\n"
                "%s\r\n"
                "Content-Length: %zu\r\n"
                "\r\n",
                method_str, purl.path, purl.host,
                extra_headers ? extra_headers : "",
                body_len);
        } else {
            n = snprintf(hdr, sizeof(hdr),
                "%s %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Accept: text/event-stream\r\n"
                "Connection: close\r\n"
                "%s\r\n"
                "\r\n",
                method_str, purl.path, purl.host,
                extra_headers ? extra_headers : "");
        }
        if (n < 0 || (size_t)n >= sizeof(hdr)) {
            free(req); if (ssl) SSL_free(ssl); close(fd); return -1;
        }
        size_t hdr_len = (size_t)n;

        /* Allocate: headers + body */
        size_t req_total = hdr_len;
        if (body && body_len > 0) req_total += body_len + 1;
        if (req_total > req_cap) {
            char *nr = (char *)realloc(req, req_total);
            if (!nr) { free(req); if (ssl) SSL_free(ssl); close(fd); return -1; }
            req = nr;
            req_cap = req_total;
        }
        memcpy(req + req_len, hdr, hdr_len);
        req_len += hdr_len;

        if (body && body_len > 0) {
            memcpy(req + req_len, body, body_len);
            req_len += body_len;
        }
        req[req_len] = '\0';
    }

    /* Send */
    bool ok;
    if (ssl) ok = SSL_write(ssl, req, (int)req_len) > 0;
    else ok = socket_send_all(fd, req, req_len, h->timeout_sec);
    free(req);
    if (!ok) { if (ssl) SSL_free(ssl); close(fd); return -1; }

    /* Send body */
    if (body && body_len > 0) {
        if (ssl) ok = SSL_write(ssl, body, (int)body_len) > 0;
        else ok = socket_send_all(fd, body, body_len, h->timeout_sec);
        if (!ok) { if (ssl) SSL_free(ssl); close(fd); return -1; }
    }

    /* Read response header first */
    size_t rcap = 4096;
    char *rbuf = (char *)malloc(rcap);
    size_t rlen = 0;
    if (!rbuf) { if (ssl) SSL_free(ssl); close(fd); return -1; }

    /* Read until blank line */
    while (1) {
        char line[4096];
        ss = read_line_buffered(fd, ssl, line, sizeof(line), rbuf, &rlen, rcap, h->timeout_sec);
        if (ss <= 0) { free(rbuf); if (ssl) SSL_free(ssl); close(fd); return -1; }
        if (line[0] == '\r' || line[0] == '\n') break; /* end of headers */
    }

    /* Read SSE stream */
    int result = 0;
    while (1) {
        char line[8192];
        ss = read_line_buffered(fd, ssl, line, sizeof(line), rbuf, &rlen, rcap, h->timeout_sec);
        if (ss <= 0) break; /* EOF or error */

        /* Skip empty lines */
        if (line[0] == '\r' || line[0] == '\n') continue;

        /* Check for "[DONE]" */
        if (strncmp(line, "data: [DONE]", 12) == 0) break;

        /* Extract data: content */
        if (strncmp(line, "data: ", 6) == 0) {
            char *data = line + 6;
            size_t dlen = strlen(data);
            /* Strip trailing \r */
            while (dlen > 0 && (data[dlen-1] == '\r' || data[dlen-1] == '\n')) dlen--;
            data[dlen] = '\0';
            if (dlen > 0 && callback) {
                if (callback(data, dlen, userdata) != 0) {
                    result = -2; /* aborted by callback */
                    break;
                }
            }
        }
        /* Ignore other SSE fields (event:, id:, retry:) */
    }

    free(rbuf);
    if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
    close(fd);
    return result;
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

/* ================================================================
 *  Cookie jar
 * ================================================================ */

void http_client_enable_cookies(http_t *h, bool enable) {
    if (!h) return;
    h->cookies_enabled = enable;
    if (!enable) http_client_clear_cookies(h);
}

void http_client_clear_cookies(http_t *h) {
    if (!h) return;
    h->cookie_count = 0;
    memset(h->cookies, 0, sizeof(h->cookies));
}

/* Parse a single Set-Cookie header value: name=value[; attr...] */
void http_cookie_parse_set_cookie(http_t *h, const char *header_value) {
    if (!h || !header_value || !h->cookies_enabled) return;

    /* Extract name=value (first token before ;) */
    const char *semi = strchr(header_value, ';');
    size_t nv_len = semi ? (size_t)(semi - header_value) : strlen(header_value);

    const char *eq = strchr(header_value, '=');
    if (!eq || (size_t)(eq - header_value) >= nv_len) return;

    size_t name_len = (size_t)(eq - header_value);
    size_t val_len = nv_len - name_len - 1;

    if (name_len == 0 || name_len >= HTTP_COOKIE_NAME_LEN) return;
    if (val_len >= HTTP_COOKIE_VALUE_LEN) return;

    if (h->cookie_count >= HTTP_COOKIE_MAX) return;
    http_cookie_t *c = &h->cookies[h->cookie_count];

    memcpy(c->name, header_value, name_len);
    c->name[name_len] = '\0';
    memcpy(c->value, eq + 1, val_len);
    c->value[val_len] = '\0';

    /* Expand cookie value escape sequences like \x22 */
    size_t wi = 0;
    for (size_t ri = 0; ri < val_len; ri++) {
        if (c->value[ri] == '\\' && ri + 1 < val_len) {
            if (c->value[ri + 1] == '"') continue; /* skip \" */
        }
        c->value[wi++] = c->value[ri];
    }
    c->value[wi] = '\0';

    /* Default domain and path */
    strncpy(c->domain, "*", sizeof(c->domain) - 1);
    strncpy(c->path, "/", sizeof(c->path) - 1);
    c->expires = 0;
    c->secure_only = false;
    c->http_only = false;

    h->cookie_count++;
}

/* Build Cookie header value for a given URL. Returns malloc'd string or NULL if none. */
char *http_cookie_build_header(http_t *h, const char *url) {
    if (!h || !url || !h->cookies_enabled || h->cookie_count == 0) return NULL;

    parsed_url_t purl;
    memset(&purl, 0, sizeof(purl));
    if (!parse_url(url, &purl)) return NULL;

    size_t total = 0;
    char buf[4096] = {0};

    for (int i = 0; i < h->cookie_count; i++) {
        http_cookie_t *c = &h->cookies[i];
        bool domain_match = (strcmp(c->domain, "*") == 0)
            || strstr(purl.host, c->domain) != NULL;
        bool path_match = strncmp(purl.path, c->path, strlen(c->path)) == 0;
        bool not_expired = (c->expires == 0) || (difftime(c->expires, time(NULL)) > 0);
        bool secure_ok = !c->secure_only || strcmp(purl.scheme, "https") == 0;

        if (domain_match && path_match && not_expired && secure_ok) {
            size_t remaining = sizeof(buf) - total - 1;
            int n = snprintf(buf + total, remaining, "%s%s=%s",
                             total > 0 ? "; " : "", c->name, c->value);
            if (n > 0 && (size_t)n < remaining) total += (size_t)n;
        }
    }

    if (total == 0) return NULL;
    return strdup(buf);
}

/* ================================================================
 *  Gzip/Deflate decompression
 * ================================================================ */

/**
 * Decompress gzip or deflate data using zlib.
 * Returns malloc'd buffer on success, NULL on error.
 * Caller must free() the result.
 */
static char *http_decompress_body(const char *compressed, size_t compressed_len,
                                   size_t *decompressed_len) {
    if (!compressed || compressed_len == 0) return NULL;

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    /* Auto-detect gzip, zlib, or raw deflate format */
    int ret = inflateInit2(&strm, 15 + 32);
    if (ret != Z_OK) return NULL;

    strm.next_in = (unsigned char *)compressed;
    strm.avail_in = (uInt)compressed_len;

    size_t out_cap = compressed_len * 4 + 1024;
    char *out = (char *)malloc(out_cap);
    if (!out) { inflateEnd(&strm); return NULL; }
    size_t out_len = 0;

    do {
        if (out_len + 32768 >= out_cap) {
            out_cap *= 2;
            char *new_out = (char *)realloc(out, out_cap);
            if (!new_out) { free(out); inflateEnd(&strm); return NULL; }
            out = new_out;
        }
        strm.next_out = (unsigned char *)(out + out_len);
        strm.avail_out = (uInt)(out_cap - out_len);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            free(out);
            inflateEnd(&strm);
            return NULL;
        }
        out_len = strm.total_out;
    } while (ret != Z_STREAM_END && strm.avail_in > 0);

    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        free(out);
        return NULL;
    }

    out[out_len] = '\0';
    if (decompressed_len) *decompressed_len = out_len;
    return out;
}

/* ================================================================
 *  Public setters
 * ================================================================ */

void http_client_set_max_redirects(http_t *h, int max_redirects) {
    if (!h) return;
    h->max_redirects = max_redirects > 0 ? max_redirects : 0;
}

void http_client_set_decompress(http_t *h, bool enable) {
    if (!h) return;
    h->decompress = enable;
}
