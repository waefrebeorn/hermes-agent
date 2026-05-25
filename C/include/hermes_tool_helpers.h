#ifndef HERMES_TOOL_HELPERS_H
#define HERMES_TOOL_HELPERS_H

/*
 * hermes_tool_helpers.h — Standardized HTTP/API helpers for tool implementations.
 *
 * P51: Tool backend helpers. Provides a single-shot API call pattern with
 * auth, retry, error handling, and JSON response parsing.
 *
 * Usage:
 *   tool_api_result_t *r = tool_api_call("https://api.example.com/data",
 *                                         getenv("EXAMPLE_API_KEY"),
 *                                         "GET", NULL);
 *   if (r->error) { printf("Error: %s\n", r->error); tool_api_result_free(r); return; }
 *   const char *val = json_get_str(r->json, "key", "default");
 *   tool_api_result_free(r);
 */

#include <stdbool.h>
#include "../lib/libjson/json.h"
#include "../lib/libhttp/http.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Types === */

typedef struct {
    int      status;       /* HTTP status code, 0 if connection failed */
    char    *body;         /* Raw response body (malloc'd), NULL on error */
    char    *error;        /* Error message (malloc'd), NULL on success */
    json_t  *json;         /* Parsed JSON body, NULL if not JSON or on error */
    size_t   body_len;
} tool_api_result_t;

/* === Main API === */

/* Make an API call with Bearer token auth.
 * url: full URL including scheme
 * api_key: Bearer token (pass NULL if no auth needed)
 * method: "GET", "POST", "PUT", "DELETE" (NULL defaults to GET)
 * json_body: JSON body string (pass NULL for GET/DELETE or bodyless requests)
 * timeout_sec: request timeout (0 = default 30s)
 * max_retries: automatic retry count (0 = no retry, 3 recommended for transient errors)
 * Returns malloc'd result. Caller must free with tool_api_result_free(). */
tool_api_result_t *tool_api_call(const char *url,
                                  const char *api_key,
                                  const char *method,
                                  const char *json_body,
                                  int timeout_sec,
                                  int max_retries);

/* Make an API call with custom headers string.
 * headers: raw header lines separated by \n, e.g. "Authorization: Bearer ***\nX-Custom: val"
 * Other params same as tool_api_call(). */
tool_api_result_t *tool_api_call_with_headers(const char *url,
                                               const char *headers,
                                               const char *method,
                                               const char *json_body,
                                               int timeout_sec,
                                               int max_retries);

/* === Convenience wrappers === */

/* GET request, returns parsed JSON result */
tool_api_result_t *tool_api_get_json(const char *url,
                                      const char *api_key,
                                      int timeout_sec);

/* POST JSON body, returns parsed JSON result */
tool_api_result_t *tool_api_post_json(const char *url,
                                       const char *api_key,
                                       const char *json_body,
                                       int timeout_sec);

/* === Response handling === */

/* Free API result (frees body, error, and json recursively) */
void tool_api_result_free(tool_api_result_t *result);

/* Check if HTTP status indicates success (200-299) */
static inline bool tool_api_success(const tool_api_result_t *r) {
    return r && !r->error && r->status >= 200 && r->status < 300;
}

/* Get error string: returns error message or HTTP status text */
const char *tool_api_error_str(const tool_api_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOL_HELPERS_H */
