/*
 * web_search_registry.h — Web Search Provider Registry for Hermes C.
 * Port of Python agent/web_search_registry.py (262 lines).
 *
 * Central map of registered web providers. Populated at init-time via
 * web_search_register_provider(); consumed by the web_search and web_extract
 * tools to dispatch calls to the active backend.
 *
 * Active selection mirrors Python logic:
 * 1. web.{capability}_backend / web.backend config → return (even if unavailable)
 * 2. Exactly one capability-eligible + available provider → return it
 * 3. Legacy preference walk (firecrawl→parallel→tavily→exa→searxng→brave-free→ddgs),
 *    filtered by capability + availability
 * 4. Otherwise → NULL
 */
#ifndef WEB_SEARCH_REGISTRY_H
#define WEB_SEARCH_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>

#define WEB_PROVIDER_NAME_MAX 64
#define WEB_MAX_PROVIDERS 16

/* Capability flags */
typedef enum {
    WEB_CAP_SEARCH  = 1 << 0,
    WEB_CAP_EXTRACT = 1 << 1,
    WEB_CAP_CRAWL   = 1 << 2,
} web_capability_t;

/* Provider struct */
typedef struct {
    char name[WEB_PROVIDER_NAME_MAX];
    char display_name[WEB_PROVIDER_NAME_MAX];
    bool (*is_available)(void);
    unsigned int capabilities; /* bitmask of web_capability_t */
} web_search_provider_t;

/* Register a provider. name must be non-empty. Overwrites on re-registration. */
bool web_search_register_provider(const web_search_provider_t *provider);

/* Return number of registered providers */
int web_search_provider_count(void);

/* Get provider by index (0..count-1). Returns NULL if out of range. */
const web_search_provider_t *web_search_get_provider_by_index(int idx);

/* Lookup by name. Returns NULL if not found. */
const web_search_provider_t *web_search_get_provider(const char *name);

/* Set the configured backend name for a capability ("search", "extract", "crawl").
 * This is called by config loading code to feed in values from
 * web.{capability}_backend / web.backend from the YAML config.
 * Passing NULL or empty string clears the override. */
void web_search_set_configured(const char *capability, const char *backend_name);

/* Resolve active provider for a given capability.
 * capability: "search", "extract", or "crawl".
 * Reads config override (set via web_search_set_configured), then env vars
 * WEB_{CAP}_BACKEND, then WEB_BACKEND.
 * Falls back per module docstring. */
const web_search_provider_t *web_search_get_active(const char *capability);

/* Convenience: get active search provider */
#define web_search_get_active_search() web_search_get_active("search")

/* Convenience: get active extract provider */
#define web_search_get_active_extract() web_search_get_active("extract")

/* Convenience: get active crawl provider */
#define web_search_get_active_crawl() web_search_get_active("crawl")

/* Clear registry. Test-only. */
void web_search_reset_registry(void);

#endif /* WEB_SEARCH_REGISTRY_H */
