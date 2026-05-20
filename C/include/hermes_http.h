#ifndef HERMES_HTTP_H
#define HERMES_HTTP_H

/*
 * hermes_http.h — Compatibility shim: maps old hermes HTTP API → libhttp.
 *
 * Old API used http_client_t / http_response_t / http_client_new() etc.
 * New libhttp uses http_t / http_resp_t / http_new() etc.
 * This header provides backward-compatible typedefs + wrappers.
 */

#include "../lib/libhttp/http.h"

/* Type aliases */
typedef http_t       http_client_t;
typedef http_resp_t  http_response_t;

/* Construction wrappers */
static inline http_client_t *http_client_new(int timeout_sec) {
    return http_new(timeout_sec);
}

static inline void http_client_free(http_client_t *client) {
    http_free(client);
}

static inline void http_response_free(http_response_t *resp) {
    http_resp_free(resp);
}

/* Request with JSON body (old name, always POST in practice) */
static inline http_response_t *http_request_json(http_client_t *client,
                                                  http_method_t method,
                                                  const char *url,
                                                  const char *json_body)
{
    (void)method; /* Old API always used POST with this function */
    return http_post_json(client, url, json_body);
}

#endif /* HERMES_HTTP_H */
