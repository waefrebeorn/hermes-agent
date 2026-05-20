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

/* === URL utilities === */
/* URL-encode a string. Caller free()s result. */
char *http_url_encode(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* LIBHTTP_H */
