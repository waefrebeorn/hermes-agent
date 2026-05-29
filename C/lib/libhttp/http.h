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

/* Get raw response headers captured during streaming request (diagnostics) */
const char *http_get_resp_headers(http_t *h);

/* === SSE stream (persistent connection) === */
/* Handle for persistent SSE stream — one connection, multiple events. */
typedef struct http_sse_t http_sse_t;

/* Open persistent GET to SSE endpoint. Reads response headers.
 * Returns handle on success, NULL on error (check http_sse_last_error).
 * Call http_sse_free() to close. */
http_sse_t *http_sse_start(http_t *h, const char *url, const char *extra_headers);

/* Read one complete SSE event. Returns event type string pointer (no free).
 * Stores event data in buf (up to cap bytes). timeout_ms: 0=block.
 * Returns NULL on EOF, error, or timeout. */
const char *http_sse_read_event(http_sse_t *sse, char *buf, size_t cap, int timeout_ms);

/* Close SSE stream and free handle. */
void http_sse_free(http_sse_t *sse);

/* Get last error message from SSE stream (thread-safe). Returns pointer to
 * internal buffer — do not free. Empty string if no error. */
const char *http_sse_last_error(http_sse_t *sse);

/* === URL utilities === */
/* URL-encode a string. Caller free()s result. */
char *http_url_encode(const char *str);

/* === Proxy support === */
/* Set HTTP proxy (CONNECT tunnel for HTTPS). Empty/NULL to clear. */
void http_client_set_proxy(http_t *h, const char *proxy_url);

/* === Connection pool for keep-alive === */
/* Enable connection pooling. max_connections: 0 = disable, 1-16. idle_timeout_sec: 0 = never expire. */
void http_client_set_pool(http_t *h, int max_connections, int idle_timeout_sec);

/* Get pool stats: active, idle, max. Returns total connection count. */
int http_pool_stats(http_t *h, int *active, int *idle, int *max);

/* === Multipart form data (RFC 2046) === */
/* Builder for multipart/form-data bodies. Create, add fields/files, finalize. */
typedef struct http_multipart_form_t http_multipart_form_t;

/* Create a multipart form builder. Returns NULL on OOM. */
http_multipart_form_t *http_multipart_form_new(void);

/* Add a text field (name=value). Both params copied internally. */
bool http_multipart_add_field(http_multipart_form_t *f,
                              const char *name, const char *value);

/* Add a file field. Content is copied internally.
 * If content_type is NULL, defaults to "application/octet-stream".
 * If filename is NULL, no filename is included in the disposition header. */
bool http_multipart_add_file(http_multipart_form_t *f,
                             const char *name, const char *filename,
                             const char *content, size_t content_len,
                             const char *content_type);

/* Finalize the form. Returns the complete multipart body (malloc'd).
 * Sets *out_len and *out_boundary (malloc'd boundary string).
 * After finalize, you can still http_multipart_form_free() the builder.
 * Returns NULL on error. */
char *http_multipart_form_finalize(http_multipart_form_t *f,
                                   size_t *out_len,
                                   char **out_boundary);

/* Free the form builder. Safe to call after finalize. */
void http_multipart_form_free(http_multipart_form_t *f);

/* Convenience: POST with multipart body.
 * Builds Content-Type with boundary automatically.
 * Returns response like http_request(). */
http_resp_t *http_post_multipart(http_t *h, const char *url,
                                  const char *extra_headers,
                                  http_multipart_form_t *form);

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

/* === Retry-After header parsing === */
/* Parse a Retry-After header value and return the delay in seconds.
 * Supports both integer seconds and HTTP-date format.
 * Returns -1 on parse failure or NULL/empty input. */
double http_parse_retry_after(const char *header_value);

#ifdef __cplusplus
}
#endif

#endif /* LIBHTTP_H */
