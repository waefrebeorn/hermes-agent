#ifndef LIBHTTP_H
#define LIBHTTP_H

/*
 * libhttp.h — Standalone HTTP/HTTPS client for C.
 * Uses OpenSSL + POSIX sockets. Zero other deps.
 * Replaces Python's httpx, requests, urllib3 + tenacity (retry).
 *
 * MIT License — WuBu Hermes Project
 *
 * Features:
 *   - HTTP/1.1 with SSL/TLS
 *   - Dynamic response buffers (no fixed limits)
 *   - Configurable retry with backoff (replaces tenacity)
 *   - Connection timeout
 *   - GET, POST, PUT, DELETE
 *   - Custom headers, JSON helper
 *   - URL encoding
 *
 * Usage:
 *   http_t *h = http_new(10);  // 10s timeout
 *   http_resp_t *r = http_get(h, "https://api.example.com/data",
 *                              "Authorization: Bearer sk-...");
 *   printf("%d %s\n", r->status, r->body);
 *   http_resp_free(r);
 *   http_free(h);
 */

#include <stddef.h>
#include <stdbool.h>
#include <time.h>  /* for time_t in cookie expiry */

#ifdef __cplusplus
extern "C" {
#endif

/* === Methods === */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
} http_method_t;

/* === Response === */
typedef struct {
    int     status;       /* HTTP status code, -1 on error */
    char   *headers;      /* raw response headers */
    char   *body;         /* response body (null-terminated) */
    size_t  body_len;
} http_resp_t;

/* === Client handle === */
typedef struct http_t http_t;

/* === Construction === */
/* Create client. timeout_sec: 0 = default 30s. retry_count: 0 = no retry. */
http_t *http_new(int timeout_sec);
http_t *http_new_with_retry(int timeout_sec, int max_retries, int backoff_ms);

/* Free client */
void http_free(http_t *h);

/* === Requests === */
/* Raw request */
http_resp_t *http_request(http_t *h, http_method_t method,
                          const char *url,
                          const char *extra_headers,
                          const char *body, size_t body_len);

/* Convenience: GET */
http_resp_t *http_get(http_t *h, const char *url, const char *headers);

/* Convenience: POST with JSON body */
http_resp_t *http_post_json(http_t *h, const char *url,
                            const char *json_body);

/* Convenience: POST with JSON body and auth header */
http_resp_t *http_post_json_auth(http_t *h, const char *url,
                                 const char *json_body,
                                 const char *auth_header);

/* === Response handling === */
void http_resp_free(http_resp_t *resp);

/* === Streaming (SSE) === */
/* Callback receives each data chunk. Return non-zero to abort. */
typedef int (*http_stream_cb)(const char *chunk, size_t len, void *userdata);

/* Send request and stream SSE response line-by-line.
 * Each "data: {...}" line is extracted and passed to callback.
 * Returns 0 on success, -1 on error. */
int http_stream_request(http_t *h, http_method_t method,
                        const char *url,
                        const char *extra_headers,
                        const char *body, size_t body_len,
                        http_stream_cb callback, void *userdata);

/* === URL utilities === */
/* URL-encode a string. Caller free()s result. */
char *http_url_encode(const char *str);

/* === Proxy support === */
/* Set HTTP proxy (CONNECT tunnel for HTTPS). Empty/NULL to clear. */
void http_client_set_proxy(http_t *h, const char *proxy_url);

/* === Cookie jar support === */
#define HTTP_COOKIE_MAX 64
#define HTTP_COOKIE_NAME_LEN 128
#define HTTP_COOKIE_VALUE_LEN 512
#define HTTP_COOKIE_DOMAIN_LEN 256
#define HTTP_COOKIE_PATH_LEN 256

typedef struct {
    char name[HTTP_COOKIE_NAME_LEN];
    char value[HTTP_COOKIE_VALUE_LEN];
    char domain[HTTP_COOKIE_DOMAIN_LEN];
    char path[HTTP_COOKIE_PATH_LEN];
    time_t expires;
    bool secure_only;
    bool http_only;
} http_cookie_t;

void http_client_enable_cookies(http_t *h, bool enable);
void http_client_clear_cookies(http_t *h);

/* === Redirect following === */
/* Set max redirects to follow (0 = disable, default 5). Call before requests. */
void http_client_set_max_redirects(http_t *h, int max_redirects);

/* === Content decompression (gzip/deflate) === */
/* Enable automatic gzip/deflate decompression. Default: disabled. */
void http_client_set_decompress(http_t *h, bool enable);

/* Internal http_request hooks */
void http_cookie_parse_set_cookie(http_t *h, const char *header_value);
char *http_cookie_build_header(http_t *h, const char *url);

#ifdef __cplusplus
}
#endif

#endif /* LIBHTTP_H */
