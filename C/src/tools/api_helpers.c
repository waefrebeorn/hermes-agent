/*
 * api_helpers.c — P51: Tool backend helpers for Hermes C.
 *
 * Standardized HTTP/API call pattern with auth, retry, error handling,
 * and JSON response parsing. Intended as the foundation for all
 * tool port implementations (discord, feishu, video, yuanbao, etc.).
 */

#include "hermes_tool_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Internal helpers
 * ================================================================ */

/* Map method string to http_method_t */
static http_method_t parse_method(const char *method) {
    if (!method) return HTTP_GET;
    if (strcasecmp(method, "POST") == 0)   return HTTP_POST;
    if (strcasecmp(method, "PUT") == 0)    return HTTP_PUT;
    if (strcasecmp(method, "DELETE") == 0) return HTTP_DELETE;
    return HTTP_GET; /* default */
}

/* Build bearer auth header from api_key */
static char *build_auth_header(const char *api_key) {
    if (!api_key || !api_key[0]) return NULL;
    size_t len = strlen(api_key) + 32;
    char *hdr = (char *)malloc(len);
    if (!hdr) return NULL;
    snprintf(hdr, len, "Authorization: Bearer %s", api_key);
    return hdr;
}

/* ================================================================
 *  Main API
 * ================================================================ */

tool_api_result_t *tool_api_call(const char *url,
                                  const char *api_key,
                                  const char *method,
                                  const char *json_body,
                                  int timeout_sec,
                                  int max_retries) {
    char *headers = NULL;
    if (api_key && api_key[0]) {
        headers = build_auth_header(api_key);
    }
    tool_api_result_t *result = tool_api_call_with_headers(
        url, headers, method, json_body, timeout_sec, max_retries);
    if (headers) free(headers);
    return result;
}

tool_api_result_t *tool_api_call_with_headers(const char *url,
                                               const char *headers,
                                               const char *method,
                                               const char *json_body,
                                               int timeout_sec,
                                               int max_retries) {
    tool_api_result_t *result = (tool_api_result_t *)calloc(1, sizeof(tool_api_result_t));
    if (!result) return NULL;

    if (!url || !url[0]) {
        result->error = strdup("No URL provided");
        return result;
    }

    /* Set default timeout if not specified */
    if (timeout_sec <= 0) timeout_sec = 30;

    /* Create HTTP client with retry */
    http_t *h;
    if (max_retries > 0) {
        h = http_new_with_retry(timeout_sec, max_retries, 1000);
    } else {
        h = http_new(timeout_sec);
    }
    if (!h) {
        result->error = strdup("Failed to create HTTP client");
        return result;
    }

    http_method_t m = parse_method(method);
    http_resp_t *resp = NULL;

    if (m == HTTP_GET || (m == HTTP_DELETE && !json_body)) {
        resp = http_request(h, m, url, headers, NULL, 0);
    } else if (json_body) {
        resp = http_request(h, m, url, headers, json_body, strlen(json_body));
    } else {
        resp = http_request(h, m, url, headers, NULL, 0);
    }

    if (!resp) {
        result->error = strdup("HTTP request failed (no response)");
        http_free(h);
        return result;
    }

    result->status = resp->status;

    if (resp->status < 0) {
        /* Connection/SSL error */
        result->error = (char *)malloc(128);
        if (result->error)
            snprintf(result->error, 128, "HTTP connection error (status=%d)", resp->status);
    } else if (resp->body && resp->body_len > 0) {
        /* Store body */
        result->body = (char *)malloc(resp->body_len + 1);
        if (result->body) {
            memcpy(result->body, resp->body, resp->body_len);
            result->body[resp->body_len] = '\0';
            result->body_len = resp->body_len;
        }

        /* Try to parse as JSON */
        char *json_err = NULL;
        result->json = json_parse(resp->body, &json_err);
        if (json_err) {
            free(json_err);
            /* Not JSON — that's OK, body is still available */
        }
    }

    /* If status >= 400, create error message */
    if (resp->status >= 400) {
        if (!result->error) {
            result->error = (char *)malloc(256);
            if (result->error) {
                if (result->json) {
                    /* Try to extract error message from JSON response */
                    const char *err_msg = json_get_str(result->json, "error.message", NULL);
                    if (!err_msg) err_msg = json_get_str(result->json, "error", NULL);
                    if (!err_msg) err_msg = json_get_str(result->json, "message", NULL);
                    if (err_msg)
                        snprintf(result->error, 256, "HTTP %d: %s", resp->status, err_msg);
                    else
                        snprintf(result->error, 256, "HTTP %d", resp->status);
                } else {
                    snprintf(result->error, 256, "HTTP %d", resp->status);
                }
            }
        }
    }

    http_resp_free(resp);
    http_free(h);
    return result;
}

/* ================================================================
 *  Convenience wrappers
 * ================================================================ */

tool_api_result_t *tool_api_get_json(const char *url,
                                      const char *api_key,
                                      int timeout_sec) {
    return tool_api_call(url, api_key, "GET", NULL, timeout_sec, 3);
}

tool_api_result_t *tool_api_post_json(const char *url,
                                       const char *api_key,
                                       const char *json_body,
                                       int timeout_sec) {
    return tool_api_call(url, api_key, "POST", json_body, timeout_sec, 3);
}

/* ================================================================
 *  Response handling
 * ================================================================ */

void tool_api_result_free(tool_api_result_t *result) {
    if (!result) return;
    if (result->body) free(result->body);
    if (result->error) free(result->error);
    if (result->json) json_free(result->json);
    free(result);
}

const char *tool_api_error_str(const tool_api_result_t *r) {
    if (!r) return "NULL result";
    if (r->error) return r->error;
    if (r->status >= 400) {
        /* Return a static string — caller can check status directly */
        return "HTTP error";
    }
    return NULL; /* no error */
}
