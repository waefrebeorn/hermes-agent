/*
 * xai_http.c — Shared helpers for direct xAI HTTP integrations.
 *
 * Port of Python tools/xai_http.py.
 *
 * Provides simple environment-based credential resolution for xAI
 * HTTP endpoints (images, TTS, STT, web search, etc.).
 */

#include "xai_http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool xai_has_credentials(void)
{
    const char *key = getenv("HERMES_XAI_API_KEY");
    return key && key[0] != '\0';
}

bool xai_get_api_key(char out_key[XAI_API_KEY_MAX])
{
    if (!out_key) return false;

    const char *key = getenv("HERMES_XAI_API_KEY");
    if (!key || key[0] == '\0') {
        out_key[0] = '\0';
        return false;
    }

    snprintf(out_key, XAI_API_KEY_MAX, "%s", key);
    return true;
}

void xai_get_base_url(char out_url[XAI_BASE_URL_MAX])
{
    if (!out_url) return;

    const char *url = getenv("HERMES_XAI_BASE_URL");
    if (url && url[0] != '\0') {
        snprintf(out_url, XAI_BASE_URL_MAX, "%s", url);
    } else {
        snprintf(out_url, XAI_BASE_URL_MAX, "%s", XAI_DEFAULT_BASE_URL);
    }
}

const char *xai_user_agent(void)
{
    return XAI_USER_AGENT;
}
