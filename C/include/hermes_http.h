#ifndef HERMES_HTTP_H
#define HERMES_HTTP_H

/*
 * hermes_http.h — HTTP client for Hermes C.
 * Uses OpenSSL + POSIX sockets. No libcurl dependency.
 * Supports: GET, POST, streaming, headers, timeouts.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP methods */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
} http_method_t;

/* Response structure */
typedef struct {
    int     status_code;
    char   *headers;       /* raw response headers */
    char   *body;          /* response body */
    size_t  body_len;
} http_response_t;

/* HTTP client handle (reusable connection pool) */
typedef struct http_client_t http_client_t;

/* Create HTTP client. timeout_sec = 0 means default (30s). */
http_client_t *http_client_new(int timeout_sec);

/* Free client */
void http_client_free(http_client_t *client);

/* Perform request with JSON body. JSON Content-Type set automatically. */
http_response_t *http_request_json(http_client_t *client,
                                   http_method_t method,
                                   const char *url,
                                   const char *json_body);

/* Perform request with raw body and custom content-type. */
http_response_t *http_request(http_client_t *client,
                              http_method_t method,
                              const char *url,
                              const char *headers,
                              const char *body,
                              size_t body_len);

/* Free response */
void http_response_free(http_response_t *resp);

/* URL encode a string. Caller must free result. */
char *http_url_encode(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_HTTP_H */
