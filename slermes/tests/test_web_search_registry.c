/* test_web_search_registry.c — Tests for web_search_registry.
 * Port of Python tests/agent/test_web_search_registry.py concepts.
 */
#include "web_search_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Helper: register a test provider */
static void add_provider(const char *name, unsigned int caps, bool available) {
    web_search_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "%s", name);
    snprintf(p.display_name, sizeof(p.display_name), "%s", name);
    p.capabilities = caps;
    /* No is_available function pointer — use availability flag via custom fn */
    assert(web_search_register_provider(&p));
}

/* is_available with custom state — use NULL for "always available" */

static bool g_avail_firecrawl = true;
static bool firecrawl_available(void) { return g_avail_firecrawl; }

static bool g_avail_tavily = true;
static bool tavily_available(void) { return g_avail_tavily; }

static bool always_unavail(void) { return false; }

static void register_test_providers(void) {
    web_search_reset_registry();

    /* Register firecrawl with SEARCH + EXTRACT + CRAWL */
    web_search_provider_t fc;
    memset(&fc, 0, sizeof(fc));
    snprintf(fc.name, sizeof(fc.name), "firecrawl");
    fc.capabilities = WEB_CAP_SEARCH | WEB_CAP_EXTRACT | WEB_CAP_CRAWL;
    fc.is_available = firecrawl_available;
    web_search_register_provider(&fc);

    /* Register tavily with SEARCH + CRAWL */
    web_search_provider_t tv;
    memset(&tv, 0, sizeof(tv));
    snprintf(tv.name, sizeof(tv.name), "tavily");
    tv.capabilities = WEB_CAP_SEARCH | WEB_CAP_CRAWL;
    tv.is_available = tavily_available;
    web_search_register_provider(&tv);

    /* Register ddgs with SEARCH only */
    web_search_provider_t dd;
    memset(&dd, 0, sizeof(dd));
    snprintf(dd.name, sizeof(dd.name), "ddgs");
    dd.capabilities = WEB_CAP_SEARCH;
    dd.is_available = NULL; /* always available */
    web_search_register_provider(&dd);
}

/* Test registration and basic lookup */
static void test_register_and_lookup(void) {
    web_search_reset_registry();
    assert(web_search_provider_count() == 0);

    web_search_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "test-provider");
    p.capabilities = WEB_CAP_SEARCH;
    assert(web_search_register_provider(&p) == true);
    assert(web_search_provider_count() == 1);

    const web_search_provider_t *found = web_search_get_provider("test-provider");
    assert(found != NULL);
    assert(strcmp(found->name, "test-provider") == 0);
    assert(found->capabilities == WEB_CAP_SEARCH);

    assert(web_search_get_provider("nonexistent") == NULL);
}

/* Test re-registration overwrites */
static void test_reregister(void) {
    web_search_reset_registry();

    web_search_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "p1");
    snprintf(a.display_name, sizeof(a.display_name), "Old");
    web_search_register_provider(&a);

    memset(&b, 0, sizeof(b));
    snprintf(b.name, sizeof(b.name), "p1");
    snprintf(b.display_name, sizeof(b.display_name), "New");
    web_search_register_provider(&b);

    assert(web_search_provider_count() == 1);
    assert(strcmp(web_search_get_provider("p1")->display_name, "New") == 0);
}

/* Test empty name rejection */
static void test_empty_name(void) {
    web_search_reset_registry();

    web_search_provider_t p;
    memset(&p, 0, sizeof(p));
    assert(web_search_register_provider(&p) == false);

    assert(web_search_register_provider(NULL) == false);
}

/* Test: no providers → NULL */
static void test_no_providers(void) {
    web_search_reset_registry();
    assert(web_search_get_active_search() == NULL);
}

/* Test: single available provider is returned */
static void test_single_provider(void) {
    web_search_reset_registry();

    web_search_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "solo");
    p.capabilities = WEB_CAP_SEARCH;
    p.is_available = NULL;
    web_search_register_provider(&p);

    const web_search_provider_t *active = web_search_get_active_search();
    assert(active != NULL);
    assert(strcmp(active->name, "solo") == 0);
}

/* Test: configured backend wins even if unavailable */
static void test_configured_wins(void) {
    web_search_reset_registry();

    web_search_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "configured-one");
    p.capabilities = WEB_CAP_SEARCH;
    p.is_available = always_unavail;
    web_search_register_provider(&p);

    web_search_set_configured("search", "configured-one");
    const web_search_provider_t *active = web_search_get_active_search();
    assert(active != NULL);
    assert(strcmp(active->name, "configured-one") == 0);
    web_search_set_configured("search", NULL);
}

/* Test: extract falls back to search provider when none registered for extract */
static void test_extract_fallback(void) {
    g_avail_firecrawl = true;
    g_avail_tavily = true;
    register_test_providers();

    /* Firecrawl supports extract → should be active */
    const web_search_provider_t *extract = web_search_get_active_extract();
    assert(extract != NULL);
    assert(strcmp(extract->name, "firecrawl") == 0); /* highest legacy pref with extract cap */
}

/* Test: crawl picks firecrawl (only one with crawl capability) */
static void test_crawl(void) {
    g_avail_firecrawl = true;
    g_avail_tavily = true;
    register_test_providers();

    const web_search_provider_t *crawl = web_search_get_active_crawl();
    assert(crawl != NULL);
    /* Both firecrawl and tavily have CRAWL — firecrawl is higher in legacy pref */
    assert(strcmp(crawl->name, "firecrawl") == 0);
}

/* Test: unavail providers skip in fallback path */
static void test_unavail_fallback_skip(void) {
    g_avail_firecrawl = false;  /* firecrawl unavailable */
    g_avail_tavily = true;
    register_test_providers();

    /* Firecrawl unavail → should skip to next in legacy pref */
    const web_search_provider_t *search = web_search_get_active_search();
    assert(search != NULL);
    assert(strcmp(search->name, "tavily") == 0); /* next in pref that's avail */
}

/* Test: index access */
static void test_by_index(void) {
    web_search_reset_registry();

    web_search_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "a-first");
    web_search_register_provider(&a);
    snprintf(b.name, sizeof(b.name), "b-second");
    web_search_register_provider(&b);

    assert(web_search_provider_count() == 2);
    assert(strcmp(web_search_get_provider_by_index(0)->name, "a-first") == 0);
    assert(strcmp(web_search_get_provider_by_index(1)->name, "b-second") == 0);
    assert(web_search_get_provider_by_index(-1) == NULL);
    assert(web_search_get_provider_by_index(2) == NULL);
}

int main(void) {
    test_register_and_lookup();
    test_reregister();
    test_empty_name();
    test_no_providers();
    test_single_provider();
    test_configured_wins();
    test_extract_fallback();
    test_crawl();
    test_unavail_fallback_skip();
    test_by_index();

    printf("web_search_registry: 10/10 pass\n");
    return 0;
}
