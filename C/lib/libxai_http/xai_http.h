#ifndef HERMES_XAI_HTTP_H
#define HERMES_XAI_HTTP_H

/*
 * xai_http.h — Shared helpers for direct xAI HTTP integrations.
 *
 * Port of Python tools/xai_http.py.
 *
 * Provides credential resolution for direct xAI HTTP endpoints
 * (images, TTS, STT, etc.) outside the main provider framework.
 *
 * Resolution order:
 *   1. HERMES_XAI_API_KEY env var
 *   2. HERMES_XAI_BASE_URL env var (default: https://api.x.ai/v1)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define XAI_DEFAULT_BASE_URL "https://api.x.ai/v1"
#define XAI_USER_AGENT       "Hermes-Agent/0.14.1-wubu"
#define XAI_API_KEY_MAX      256
#define XAI_BASE_URL_MAX     256

/**
 * Check if xAI credentials are available.
 * Returns true if HERMES_XAI_API_KEY env var is set and non-empty.
 */
bool xai_has_credentials(void);

/**
 * Resolve xAI API key from environment.
 * @param out_key  Buffer for API key (XAI_API_KEY_MAX bytes).
 * @return true if key was resolved, false if unavailable.
 */
bool xai_get_api_key(char out_key[XAI_API_KEY_MAX]);

/**
 * Resolve xAI base URL from environment.
 * @param out_url  Buffer for base URL (XAI_BASE_URL_MAX bytes).
 *                 Defaults to XAI_DEFAULT_BASE_URL if env var not set.
 */
void xai_get_base_url(char out_url[XAI_BASE_URL_MAX]);

/**
 * Return the Hermes xAI User-Agent string.
 * Returns a pointer to a static string — no allocation, no free needed.
 */
const char *xai_user_agent(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_XAI_HTTP_H */
