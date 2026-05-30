/**
 * fal_common.h — Shared FAL.ai HTTP utilities.
 *
 * Port of Python tools/fal_common.py concepts:
 *   - Shared API key resolution
 *   - JSON-escaping helper for prompt strings
 *   - FAL HTTP POST with auth
 *   - Error response builder
 *
 * Both image_gen.c and video_gen.c use this instead of duplicating
 * the key lookup + HTTP client creation + response-checking pattern.
 */
#ifndef FAL_COMMON_H
#define FAL_COMMON_H

#include "hermes_json.h"
#include "hermes_http.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── API key ─────────────────────────────────────────────────── */

/**
 * Resolve the FAL API key. Tries, in order:
 *   1. getenv("FAL_API_KEY")
 *   2. getenv("SLERMES_FAL_KEY")
 *
 * Returns NULL if neither is set (caller should produce an error).
 */
const char *fal_get_api_key(void);

/* ── JSON escaping ───────────────────────────────────────────── */

/**
 * Escape a string for embedding in a JSON string literal.
 * Handles ", \, \n, \t. Writes at most max-1 chars + NUL.
 * Returns the number of chars written (excluding NUL).
 */
size_t fal_escape_json(const char *input, char *output, size_t max);

/* ── HTTP POST with FAL auth ─────────────────────────────────── */

/**
 * POST JSON body to a FAL.ai REST endpoint.
 * Uses fal_get_api_key() internally — returns NULL on missing key.
 * Returns NULL on connection failure; caller must check resp->status.
 */
http_resp_t *fal_post_json(const char *url, const char *body,
                           int timeout_sec);

/* ── Error response builders ─────────────────────────────────── */

/**
 * Build a JSON error response string:
 *   {"success":false,"error":"<formatted message>"}
 * Caller must free the returned string.
 */
char *fal_error_response(const char *fmt, ...);

/**
 * Build a JSON error response from an HTTP response:
 *   {"success":false,"error":"FAL API returned HTTP <status>: <body snippet>"}
 * Caller must free the returned string.
 */
char *fal_error_from_http(const http_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* FAL_COMMON_H */
