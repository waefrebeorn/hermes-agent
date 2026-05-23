/*
 * fal_common.c — Shared FAL.ai SDK plumbing for Hermes C.
 * Port of Python tools/fal_common.py.
 *
 * MIT License — WuBu Hermes Project
 */

#include "fal_common.h"
#include "json.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  URL normalizer
 * ================================================================ */

char *fal_queue_url_normalize(const char *queue_run_origin) {
    if (!queue_run_origin) return NULL;

    /* Strip whitespace */
    const char *start = queue_run_origin;
    while (*start && isspace((unsigned char)*start)) start++;
    if (!*start) return NULL;

    const char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    size_t len = (size_t)(end - start + 1);

    /* Remove trailing slash */
    if (start[len - 1] == '/') len--;

    if (len == 0) return NULL;

    char *result = (char *)malloc(len + 2);  /* +1 for "/", +1 for \0 */
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '/';
    result[len + 1] = '\0';
    return result;
}

/* ================================================================
 *  HTTP status extractor
 * ================================================================ */

int fal_extract_http_status(const char *error_message) {
    if (!error_message || !*error_message) return 0;

    /* Try extracting from "HTTP NNN:" pattern */
    const char *http_marker = strstr(error_message, "HTTP ");
    if (http_marker) {
        http_marker += 5;  /* past "HTTP " */
        while (*http_marker == ' ') http_marker++;
        if (isdigit((unsigned char)*http_marker)) {
            int status = atoi(http_marker);
            if (status >= 100 && status <= 599)
                return status;
        }
    }

    /* Try JSON blob: "\"status_code\": NNN" */
    const char *sc_key = strstr(error_message, "\"status_code\"");
    if (sc_key) {
        const char *colon = strchr(sc_key, ':');
        if (colon) {
            colon++;
            while (*colon == ' ') colon++;
            if (isdigit((unsigned char)*colon)) {
                int status = atoi(colon);
                if (status >= 100 && status <= 599)
                    return status;
            }
        }
    }

    /* Try JSON blob: "\"status\": NNN" */
    const char *st_key = strstr(error_message, "\"status\"");
    if (st_key) {
        const char *colon = strchr(st_key, ':');
        if (colon) {
            colon++;
            while (*colon == ' ') colon++;
            if (isdigit((unsigned char)*colon)) {
                int status = atoi(colon);
                if (status >= 100 && status <= 599)
                    return status;
            }
        }
    }

    return 0;
}

/* ================================================================
 *  Managed FAL client
 * ================================================================ */

fal_request_handle_t *fal_submit_request(const char *api_key,
                                          const char *queue_origin,
                                          const char *application,
                                          const char *arguments_json,
                                          const char *path,
                                          int timeout_sec) {
    fal_request_handle_t *h = (fal_request_handle_t *)calloc(1, sizeof(*h));
    if (!h) return NULL;

    if (!api_key || !*api_key || !queue_origin || !*queue_origin ||
        !application || !*application) {
        snprintf(h->error, sizeof(h->error), "api_key, queue_origin, and application are required");
        return h;
    }

    /* Normalize the queue URL */
    char *normalized_origin = fal_queue_url_normalize(queue_origin);
    if (!normalized_origin) {
        snprintf(h->error, sizeof(h->error), "Invalid queue origin: %s", queue_origin);
        return h;
    }

    /* Build URL: <origin><application>[/<path>] */
    char url[2048];
    if (path && *path) {
        snprintf(url, sizeof(url), "%s%s/%s", normalized_origin, application, path);
    } else {
        snprintf(url, sizeof(url), "%s%s", normalized_origin, application);
    }
    free(normalized_origin);

    /* Build request headers */
    char headers[1024];
    snprintf(headers, sizeof(headers),
             "Authorization: Key %s\r\n"
             "Content-Type: application/json",
             api_key);

    /* Use parsed args or empty JSON object */
    const char *body = arguments_json ? arguments_json : "{}";

    /* Make HTTP POST */
    int t = timeout_sec > 0 ? timeout_sec : 120;
    http_t *http = http_new(t);
    if (!http) {
        snprintf(h->error, sizeof(h->error), "Failed to create HTTP client");
        return h;
    }

    http_resp_t *resp = http_request(http, HTTP_POST, url, headers, body, strlen(body));
    if (!resp) {
        snprintf(h->error, sizeof(h->error), "HTTP request failed");
        http_free(http);
        return h;
    }

    h->http_status = resp->status;

    if (resp->status != 200) {
        snprintf(h->error, sizeof(h->error), "FAL API error (HTTP %d): %s",
                 resp->status, resp->body ? resp->body : "(no body)");
        http_resp_free(resp);
        http_free(http);
        return h;
    }

    /* Parse JSON response */
    json_t *j = json_parse(resp->body, NULL);
    if (!j) {
        snprintf(h->error, sizeof(h->error), "Failed to parse FAL response JSON");
        http_resp_free(resp);
        http_free(http);
        return h;
    }

    const char *req_id = json_get_str(j, "request_id", NULL);
    const char *resp_url = json_get_str(j, "response_url", NULL);
    const char *stat_url = json_get_str(j, "status_url", NULL);
    const char *cancel_url = json_get_str(j, "cancel_url", NULL);

    if (!req_id) {
        snprintf(h->error, sizeof(h->error), "FAL response missing request_id");
        json_free(j);
        http_resp_free(resp);
        http_free(http);
        return h;
    }

    h->success = true;
    snprintf(h->request_id, sizeof(h->request_id), "%s", req_id);
    if (resp_url) snprintf(h->response_url, sizeof(h->response_url), "%s", resp_url);
    if (stat_url) snprintf(h->status_url, sizeof(h->status_url), "%s", stat_url);
    if (cancel_url) snprintf(h->cancel_url, sizeof(h->cancel_url), "%s", cancel_url);

    json_free(j);
    http_resp_free(resp);
    http_free(http);
    return h;
}

void fal_handle_free(fal_request_handle_t *h) {
    free(h);
}
