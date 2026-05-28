/*
 * web_search_registry.c — Web Search Provider Registry for Hermes C.
 * Port of Python agent/web_search_registry.py (262 lines).
 *
 * Thread-safe provider registry for web search/extract/crawl backends.
 * Supports capability-based resolution with config overrides and legacy
 * preference fallback.
 */
#include "web_search_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>

/* ================================================================
 *  Registry state
 * ================================================================ */

static web_search_provider_t g_providers[WEB_MAX_PROVIDERS];
static int g_provider_count = 0;
static pthread_mutex_t g_registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* Configured backend overrides (set by config loader via web_search_set_configured) */
static char g_configured_search[64] = "";
static char g_configured_extract[64] = "";
static char g_configured_crawl[64] = "";
static char g_configured_backend[64] = "";

/* Legacy preference order (matches Python _LEGACY_PREFERENCE) */
static const char *g_legacy_preference[] = {
    "firecrawl", "parallel", "tavily", "exa",
    "searxng", "brave-free", "ddgs", NULL
};

/* ================================================================
 *  Registration
 * ================================================================ */

bool web_search_register_provider(const web_search_provider_t *provider) {
    if (!provider || !provider->name[0])
        return false;

    pthread_mutex_lock(&g_registry_lock);

    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i].name, provider->name) == 0) {
            g_providers[i] = *provider;
            pthread_mutex_unlock(&g_registry_lock);
            return true;
        }
    }

    if (g_provider_count >= WEB_MAX_PROVIDERS) {
        pthread_mutex_unlock(&g_registry_lock);
        return false;
    }

    g_providers[g_provider_count++] = *provider;
    pthread_mutex_unlock(&g_registry_lock);
    return true;
}

int web_search_provider_count(void) {
    pthread_mutex_lock(&g_registry_lock);
    int count = g_provider_count;
    pthread_mutex_unlock(&g_registry_lock);
    return count;
}

const web_search_provider_t *web_search_get_provider_by_index(int idx) {
    pthread_mutex_lock(&g_registry_lock);
    const web_search_provider_t *p = NULL;
    if (idx >= 0 && idx < g_provider_count)
        p = &g_providers[idx];
    pthread_mutex_unlock(&g_registry_lock);
    return p;
}

const web_search_provider_t *web_search_get_provider(const char *name) {
    if (!name || !name[0]) return NULL;

    pthread_mutex_lock(&g_registry_lock);
    const web_search_provider_t *found = NULL;
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i].name, name) == 0) {
            found = &g_providers[i];
            break;
        }
    }
    pthread_mutex_unlock(&g_registry_lock);
    return found;
}

/* ================================================================
 *  Configured backend overrides
 * ================================================================ */

void web_search_set_configured(const char *capability, const char *backend_name) {
    if (!capability) return;

    char *target = NULL;
    if (strcmp(capability, "search") == 0)
        target = g_configured_search;
    else if (strcmp(capability, "extract") == 0)
        target = g_configured_extract;
    else if (strcmp(capability, "crawl") == 0)
        target = g_configured_crawl;
    else if (strcmp(capability, "backend") == 0)
        target = g_configured_backend;
    else
        return;

    if (backend_name && backend_name[0])
        snprintf(target, 64, "%s", backend_name);
    else
        target[0] = '\0';
}

/* ================================================================
 *  Active provider resolution
 * ================================================================ */

/* Map capability string to bitmask */
static unsigned int capability_mask(const char *cap) {
    if (strcmp(cap, "search") == 0)  return WEB_CAP_SEARCH;
    if (strcmp(cap, "extract") == 0) return WEB_CAP_EXTRACT;
    if (strcmp(cap, "crawl") == 0)   return WEB_CAP_CRAWL;
    return 0;
}

/* Check if provider supports the capability */
static bool supports_capability(const web_search_provider_t *p,
                                 unsigned int cap_mask) {
    return (p->capabilities & cap_mask) != 0;
}

/* Safe is_available wrapper — NULL = assume available */
static bool is_available_safe(const web_search_provider_t *p) {
    if (!p->is_available) return true;
    return p->is_available();
}

/* Build an env var key: WEB_{cap}_BACKEND */
static void make_env_key(const char *cap, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "WEB_%s_BACKEND", cap);
    for (size_t i = 0; buf[i]; i++)
        buf[i] = toupper((unsigned char)buf[i]);
}

/* Resolve the configured name for a capability */
static const char *get_configured_name(const char *capability) {
    /* 1. Per-capability override (set via web_search_set_configured) */
    if (strcmp(capability, "search") == 0 && g_configured_search[0])
        return g_configured_search;
    if (strcmp(capability, "extract") == 0 && g_configured_extract[0])
        return g_configured_extract;
    if (strcmp(capability, "crawl") == 0 && g_configured_crawl[0])
        return g_configured_crawl;

    /* 2. Shared backend override */
    if (g_configured_backend[0])
        return g_configured_backend;

    /* 3. Env var: WEB_{CAP}_BACKEND */
    char env_key[64];
    make_env_key(capability, env_key, sizeof(env_key));
    const char *env = getenv(env_key);
    if (env && env[0]) return env;

    /* 4. Env var: WEB_BACKEND */
    const char *web_env = getenv("WEB_BACKEND");
    if (web_env && web_env[0]) return web_env;

    return NULL;
}

const web_search_provider_t *web_search_get_active(const char *capability) {
    unsigned int cap_mask = capability_mask(capability);
    if (cap_mask == 0 || !capability) return NULL;

    const char *configured = get_configured_name(capability);

    pthread_mutex_lock(&g_registry_lock);

    int count = g_provider_count;
    web_search_provider_t snapshot[WEB_MAX_PROVIDERS];
    memcpy(snapshot, g_providers, count * sizeof(web_search_provider_t));
    pthread_mutex_unlock(&g_registry_lock);

    /* 1. Explicit config wins — return even if unavailable */
    if (configured && configured[0]) {
        for (int i = 0; i < count; i++) {
            if (strcmp(snapshot[i].name, configured) == 0
                && supports_capability(&snapshot[i], cap_mask)) {
                return &g_providers[i];
            }
        }
    }

    /* 2. Single eligible + available provider */
    int eligible_count = 0;
    int eligible_idx = -1;
    for (int i = 0; i < count; i++) {
        if (supports_capability(&snapshot[i], cap_mask)
            && is_available_safe(&snapshot[i])) {
            eligible_count++;
            eligible_idx = i;
        }
    }
    if (eligible_count == 1) {
        return &g_providers[eligible_idx];
    }

    /* 3. Legacy preference walk */
    for (int l = 0; g_legacy_preference[l]; l++) {
        for (int i = 0; i < count; i++) {
            if (strcmp(snapshot[i].name, g_legacy_preference[l]) == 0
                && supports_capability(&snapshot[i], cap_mask)
                && is_available_safe(&snapshot[i])) {
                return &g_providers[i];
            }
        }
    }

    return NULL;
}

void web_search_reset_registry(void) {
    pthread_mutex_lock(&g_registry_lock);
    g_provider_count = 0;
    memset(g_providers, 0, sizeof(g_providers));
    g_configured_search[0] = '\0';
    g_configured_extract[0] = '\0';
    g_configured_crawl[0] = '\0';
    g_configured_backend[0] = '\0';
    pthread_mutex_unlock(&g_registry_lock);
}
