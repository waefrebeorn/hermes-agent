/**
 * fal_common.c — Shared FAL.ai HTTP utilities.
 */
#include "fal_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ── API key ─────────────────────────────────────────────────── */

const char *fal_get_api_key(void) {
    const char *key = getenv("FAL_API_KEY");
    if (key && *key) return key;
    key = getenv("SLERMES_FAL_KEY");
    if (key && *key) return key;
    return NULL;
}

/* ── JSON escaping ───────────────────────────────────────────── */

size_t fal_escape_json(const char *input, char *output, size_t max) {
    if (!input || !output || max == 0) return 0;
    size_t ei = 0;
    for (const char *sp = input; *sp && ei < max - 4; sp++) {
        if (*sp == '"' || *sp == '\\') {
            if (ei + 2 >= max - 1) break;
            output[ei++] = '\\';
            output[ei++] = *sp;
        } else if (*sp == '\n') {
            if (ei + 2 >= max - 1) break;
            output[ei++] = '\\';
            output[ei++] = 'n';
        } else if (*sp == '\t') {
            if (ei + 2 >= max - 1) break;
            output[ei++] = '\\';
            output[ei++] = 't';
        } else {
            output[ei++] = *sp;
        }
    }
    output[ei] = '\0';
    return ei;
}

/* ── HTTP POST with FAL auth ─────────────────────────────────── */

http_resp_t *fal_post_json(const char *url, const char *body,
                           int timeout_sec) {
    const char *api_key = fal_get_api_key();
    if (!api_key) return NULL;

    http_t *h = http_new(timeout_sec);
    if (!h) return NULL;

    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr),
             "Authorization: Key %s\r\n", api_key);

    http_resp_t *resp = http_post_json_auth(h, url, body, auth_hdr);
    http_free(h);
    return resp;
}

/* ── Error response builders ─────────────────────────────────── */

char *fal_error_response(const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    json_t *root = json_object();
    json_set(root, "success", json_bool(false));
    json_set(root, "error", json_string(msg));
    char *out = json_serialize(root);
    json_free(root);
    return out ? out : strdup("{\"success\":false,\"error\":\"OOM\"}");
}

char *fal_error_from_http(const http_resp_t *resp) {
    json_t *root = json_object();
    json_set(root, "success", json_bool(false));
    char msg[1024];
    int status = resp ? resp->status : 0;
    const char *body = (resp && resp->body) ? resp->body : "unknown";
    snprintf(msg, sizeof(msg),
             "FAL API returned HTTP %d: %.900s", status, body);
    json_set(root, "error", json_string(msg));
    char *out = json_serialize(root);
    json_free(root);
    return out ? out : strdup("{\"success\":false,\"error\":\"OOM\"}");
}
