#ifndef LIBFAL_COMMON_H
#define LIBFAL_COMMON_H

/*
 * libfal_common.h — Shared FAL.ai SDK plumbing for Hermes C.
 * Port of Python tools/fal_common.py.
 *
 * Holds stateless atoms that every FAL-backed tool needs:
 *   - fal_queue_url_normalize: URL format normalization
 *   - fal_extract_http_status: extract HTTP status code from error objects
 *   - fal_managed_client_t: simplified managed client for FAL queue endpoints
 *
 * The actual FAL HTTP calls are in plugins/plugin_image_gen.c (curl-based)
 * and src/tools/image_gen.c.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Helpers
 * ================================================================ */

/*
 * Normalize a FAL queue URL format string.
 * Strips whitespace, removes trailing '/', returns the result with "/".
 * Returns malloc'd string, caller free()s. NULL on error.
 */
char *fal_queue_url_normalize(const char *queue_run_origin);

/*
 * Extract an HTTP status code from an error message or response object.
 * Parses strings like "HTTP 401: ..." or "{..., \"status_code\": 429, ...}".
 * Returns status code, or 0 if none found.
 */
int fal_extract_http_status(const char *error_message);

/* ================================================================
 *  Managed FAL client (simplified — replaces _ManagedFalSyncClient)
 * ================================================================ */

/* Result handle for a submitted FAL queue request */
typedef struct {
    char request_id[128];   /* request_id from FAL */
    char response_url[512]; /* response_url for polling results */
    char status_url[512];   /* status_url for checking status */
    char cancel_url[512];   /* cancel_url for cancelling request */
    int  http_status;       /* HTTP status of the submit response */
    bool success;           /* true if submission succeeded */
    char error[512];        /* error message if !success */
} fal_request_handle_t;

/*
 * Submit a request to a FAL queue endpoint.
 *
 * api_key: FAL API key for authorization.
 * queue_origin: FAL queue origin (e.g. "https://queue.fal.run").
 * application: Application path (e.g. "fal-ai/flux/dev").
 * arguments_json: JSON string with application arguments.
 * path: Optional sub-path (empty string for none).
 * timeout_sec: HTTP request timeout in seconds.
 *
 * Returns a malloc'd fal_request_handle_t. Caller free()s with fal_handle_free().
 */
fal_request_handle_t *fal_submit_request(const char *api_key,
                                          const char *queue_origin,
                                          const char *application,
                                          const char *arguments_json,
                                          const char *path,
                                          int timeout_sec);

/* Free a request handle */
void fal_handle_free(fal_request_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* LIBFAL_COMMON_H */
