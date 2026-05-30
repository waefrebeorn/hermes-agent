/**
 * @file hermes_skills_hub.h
 * @brief L12: Browse.sh skills catalog source.
 *
 * Fetches and searches the browse.sh skills catalog (169+ browser
 * automation skills). Provides a simple HTTP-based catalog source.
 */
#ifndef HERMES_SKILLS_HUB_H
#define HERMES_SKILLS_HUB_H

#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 *  Constants
 * ================================================================ */
#define SKILLS_HUB_CATALOG_URL "https://browse.sh/api/skills"
#define SKILLS_HUB_SOURCE_ID "browse-sh"
#define SKILLS_HUB_MAX_RESULTS 50
#define SKILLS_HUB_MAX_SKILLS 512
#define SKILLS_HUB_CACHE_TTL_SEC 300 /* 5 min in-memory cache */

/* ================================================================
 *  Types
 * ================================================================ */
typedef struct {
    char slug[128];
    char name[128];
    char title[256];
    char description[1024];
    char source_url[512];
    char category[64];
    char tags[1024];        /* comma-separated */
    char recommended_method[32];
    int  install_count;
    bool needs_proxy;
} hub_skill_meta_t;

typedef struct {
    hub_skill_meta_t skills[SKILLS_HUB_MAX_SKILLS];
    int count;
    bool loaded;
} skills_catalog_t;

/* ================================================================
 *  API
 * ================================================================ */

/**
 * Fetch the browse.sh skills catalog (with in-memory cache).
 * Returns true on success (cached or fresh), false on error.
 */
bool skills_hub_fetch_catalog(void);

/**
 * Search cached catalog by query string.
 * Returns up to limit results. query="" returns all.
 */
int skills_hub_search(const char *query, hub_skill_meta_t *results, int limit);

/**
 * Get a skill by slug. Returns true if found.
 */
bool skills_hub_get_by_slug(const char *slug, hub_skill_meta_t *out);

/**
 * Clear the in-memory cache (force next fetch to go to network).
 */
void skills_hub_clear_cache(void);

/**
 * Get a human-readable summary string. Caller must free().
 */
char *skills_hub_summary(void);

#endif /* HERMES_SKILLS_HUB_H */
