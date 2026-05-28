/*
 * video_gen_registry.c — Video Generation Provider Registry for Hermes C.
 * Port of Python agent/video_gen_registry.py (117 lines).
 *
 * Thread-safe provider registry for video generation backends.
 * Mirrors image_gen_registry.py in structure and fallback logic.
 */
#include "video_gen_registry.h"
#include "hermes.h"
#include "hermes_tool_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

/* ================================================================
 *  Registry state
 * ================================================================ */

static video_gen_provider_t g_providers[VIDEO_GEN_MAX_PROVIDERS];
static int g_provider_count = 0;
static pthread_mutex_t g_registry_lock = PTHREAD_MUTEX_INITIALIZER;

/* ================================================================
 *  Registration
 * ================================================================ */

bool video_gen_register_provider(const video_gen_provider_t *provider) {
    if (!provider || !provider->name[0])
        return false;

    pthread_mutex_lock(&g_registry_lock);

    /* Check if already registered — overwrite */
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i].name, provider->name) == 0) {
            g_providers[i] = *provider;
            pthread_mutex_unlock(&g_registry_lock);
            return true;
        }
    }

    /* Add new provider */
    if (g_provider_count >= VIDEO_GEN_MAX_PROVIDERS) {
        pthread_mutex_unlock(&g_registry_lock);
        return false;
    }

    g_providers[g_provider_count++] = *provider;
    pthread_mutex_unlock(&g_registry_lock);
    return true;
}

int video_gen_provider_count(void) {
    pthread_mutex_lock(&g_registry_lock);
    int count = g_provider_count;
    pthread_mutex_unlock(&g_registry_lock);
    return count;
}

const video_gen_provider_t *video_gen_get_provider_by_index(int idx) {
    pthread_mutex_lock(&g_registry_lock);
    const video_gen_provider_t *p = NULL;
    if (idx >= 0 && idx < g_provider_count)
        p = &g_providers[idx];
    pthread_mutex_unlock(&g_registry_lock);
    return p;
}

const video_gen_provider_t *video_gen_get_provider(const char *name) {
    if (!name || !name[0]) return NULL;

    pthread_mutex_lock(&g_registry_lock);
    const video_gen_provider_t *found = NULL;
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
 *  Active provider resolution
 * ================================================================ */

const video_gen_provider_t *video_gen_get_active_provider(void) {
    const char *configured = NULL;

    /* Read video_gen.provider from env (config override) */
    configured = getenv("VIDEO_GEN_PROVIDER");

    /* Try config if env not set */
    if (!configured || !configured[0]) {
        configured = tool_config_get("video_gen", "provider");
    }

    pthread_mutex_lock(&g_registry_lock);

    /* Snapshot providers under lock */
    int count = g_provider_count;
    video_gen_provider_t snapshot[VIDEO_GEN_MAX_PROVIDERS];
    memcpy(snapshot, g_providers, count * sizeof(video_gen_provider_t));
    pthread_mutex_unlock(&g_registry_lock);

    /* 1. Explicit config wins — return regardless of is_available()
     *    so the user gets a precise downstream error rather than a
     *    silent backend switch. */
    if (configured && configured[0]) {
        for (int i = 0; i < count; i++) {
            if (strcmp(snapshot[i].name, configured) == 0)
                return &g_providers[i]; /* Return pointer to live entry */
        }
    }

    /* 2. Fallback: single registered + available provider */
    if (count == 1) {
        const video_gen_provider_t *p = &g_providers[0];
        if (!p->is_available || p->is_available())
            return p;
        return NULL;
    }

    /* 3. Fallback: prefer legacy FAL for backward compat */
    for (int i = 0; i < count; i++) {
        if (strcmp(snapshot[i].name, "fal") == 0) {
            const video_gen_provider_t *p = &g_providers[i];
            if (!p->is_available || p->is_available())
                return p;
            break;
        }
    }

    return NULL;
}

void video_gen_reset_registry(void) {
    pthread_mutex_lock(&g_registry_lock);
    g_provider_count = 0;
    memset(g_providers, 0, sizeof(g_providers));
    pthread_mutex_unlock(&g_registry_lock);
}
