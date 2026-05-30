/**
 * @file skills_hub.c
 * @brief L12: Browse.sh skills catalog source.
 *
 * Fetches and searches browse.sh's catalog of 169+ browser automation
 * skills. Uses hermes_http for simple HTTP GET + libjson for parsing.
 * In-memory cache with 5-minute TTL.
 */
#include "hermes_skills_hub.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Internal state — cached catalog
 * ================================================================ */
static skills_catalog_t g_catalog = {0};
static time_t g_last_fetch = 0;

/* ================================================================
 *  Helpers
 * ================================================================ */
static bool cache_valid(void) {
    if (!g_catalog.loaded) return false;
    if (g_last_fetch == 0) return false;
    return (time(NULL) - g_last_fetch) < SKILLS_HUB_CACHE_TTL_SEC;
}

static hub_skill_meta_t parse_skill_item(json_node_t *item) {
    hub_skill_meta_t meta = {0};

    const char *slug = json_get_str(item, "slug", NULL);
    if (slug) snprintf(meta.slug, sizeof(meta.slug), "%s", slug);

    const char *name = json_get_str(item, "name", NULL);
    if (name) snprintf(meta.name, sizeof(meta.name), "%s", name);

    const char *title = json_get_str(item, "title", NULL);
    if (title) snprintf(meta.title, sizeof(meta.title), "%s", title);
    else snprintf(meta.title, sizeof(meta.title), "%s", name ? name : "");

    const char *desc = json_get_str(item, "description", NULL);
    if (desc) {
        size_t dlen = strlen(desc);
        if (dlen >= sizeof(meta.description))
            dlen = sizeof(meta.description) - 4;
        snprintf(meta.description, sizeof(meta.description), "%.*s%s",
                 (int)(dlen > sizeof(meta.description) - 4 ?
                       sizeof(meta.description) - 4 : dlen),
                 desc, dlen >= sizeof(meta.description) - 4 ? "..." : "");
    }

    const char *url = json_get_str(item, "sourceUrl", NULL);
    if (url) snprintf(meta.source_url, sizeof(meta.source_url), "%s", url);

    const char *cat = json_get_str(item, "category", NULL);
    if (cat) snprintf(meta.category, sizeof(meta.category), "%s", cat);

    /* Parse tags array */
    json_node_t *tags_arr = json_obj_get(item, "tags");
    if (tags_arr && tags_arr->type == JSON_ARRAY) {
        char tags_buf[1024] = "";
        for (size_t i = 0; i < json_len(tags_arr); i++) {
            json_node_t *tag_node = json_get(tags_arr, i);
            if (tag_node && tag_node->type == JSON_STRING && tag_node->str_val) {
                if (tags_buf[0]) strncat(tags_buf, ",", sizeof(tags_buf) - strlen(tags_buf) - 1);
                strncat(tags_buf, tag_node->str_val, sizeof(tags_buf) - strlen(tags_buf) - 1);
            }
        }
        snprintf(meta.tags, sizeof(meta.tags), "%s", tags_buf);
    }

    const char *method = json_get_str(item, "recommendedMethod", NULL);
    if (method) snprintf(meta.recommended_method, sizeof(meta.recommended_method), "%s", method);

    meta.install_count = (int)json_get_num(item, "installCount", 0);
    meta.needs_proxy = json_get_bool(item, "proxies", false);

    return meta;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool skills_hub_fetch_catalog(void) {
    /* Use cached version if still valid */
    if (cache_valid()) return true;

    /* Fetch from browse.sh API */
    http_client_t *client = http_client_new(20);
    if (!client) {
        fprintf(stderr, "Error: failed to create HTTP client for skill hub\n");
        return false;
    }

    http_response_t *resp = http_request(client, HTTP_GET,
        SKILLS_HUB_CATALOG_URL, "Accept: application/json", NULL, 0);

    if (!resp || !resp->body) {
        fprintf(stderr, "Error: skill hub HTTP request failed (network error?)\n");
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Parse JSON response */
    char *err = NULL;
    json_node_t *root = json_parse(resp->body, &err);
    if (!root) {
        fprintf(stderr, "Error: skill hub response parse failed: %s\n", err ? err : "unknown");
        free(err);
        http_resp_free(resp);
        http_free(client);
        return false;
    }

    /* Clear existing catalog */
    memset(&g_catalog, 0, sizeof(g_catalog));

    /* Response can be {"skills": [...]} or just [...] */
    json_node_t *skills_arr = json_obj_get(root, "skills");
    if (!skills_arr) skills_arr = root;

    if (skills_arr && skills_arr->type == JSON_ARRAY) {
        size_t n = json_len(skills_arr);
        if (n > SKILLS_HUB_MAX_SKILLS) n = SKILLS_HUB_MAX_SKILLS;

        for (size_t i = 0; i < n; i++) {
            json_node_t *item = json_get(skills_arr, i);
            if (!item) continue;
            g_catalog.skills[g_catalog.count] = parse_skill_item(item);
            g_catalog.count++;
        }
    }

    json_free(root);
    http_resp_free(resp);
    http_free(client);

    g_catalog.loaded = true;
    g_last_fetch = time(NULL);
    return true;
}

int skills_hub_search(const char *query, hub_skill_meta_t *results, int limit) {
    if (!results || limit <= 0) return 0;

    /* Auto-fetch if not loaded */
    if (!g_catalog.loaded) {
        skills_hub_fetch_catalog();
    }

    if (g_catalog.count == 0) return 0;

    int found = 0;
    bool empty_query = (!query || !query[0]);

    for (int i = 0; i < g_catalog.count && found < limit; i++) {
        bool match = false;

        if (empty_query) {
            match = true;
        } else {
            /* Simple substring match against name, title, description, category, tags */
            const hub_skill_meta_t *s = &g_catalog.skills[i];
            if (strcasestr(s->name, query)) match = true;
            else if (strcasestr(s->title, query)) match = true;
            else if (strcasestr(s->description, query)) match = true;
            else if (strcasestr(s->category, query)) match = true;
            else if (strcasestr(s->tags, query)) match = true;
            else if (strcasestr(s->slug, query)) match = true;
        }

        if (match) {
            results[found++] = g_catalog.skills[i];
        }
    }

    return found;
}

bool skills_hub_get_by_slug(const char *slug, hub_skill_meta_t *out) {
    if (!slug || !out) return false;

    /* Auto-fetch if not loaded */
    if (!g_catalog.loaded) {
        skills_hub_fetch_catalog();
    }

    for (int i = 0; i < g_catalog.count; i++) {
        if (strcmp(g_catalog.skills[i].slug, slug) == 0) {
            *out = g_catalog.skills[i];
            return true;
        }
    }
    return false;
}

void skills_hub_clear_cache(void) {
    memset(&g_catalog, 0, sizeof(g_catalog));
    g_last_fetch = 0;
}

char *skills_hub_summary(void) {
    /* Auto-fetch if not loaded */
    if (!g_catalog.loaded) {
        skills_hub_fetch_catalog();
    }

    char buf[2048];
    if (g_catalog.count == 0) {
        snprintf(buf, sizeof(buf),
            "browse.sh skills hub: not loaded or empty catalog");
    } else {
        /* Count by category */
        char categories[512] = "";
        /* Simple unique category counter */
        char seen[32][64];
        int seen_count = 0;
        for (int i = 0; i < g_catalog.count && i < 1000; i++) {
            if (!g_catalog.skills[i].category[0]) continue;
            bool found = false;
            for (int j = 0; j < seen_count; j++) {
                if (strcmp(seen[j], g_catalog.skills[i].category) == 0) {
                    found = true; break;
                }
            }
            if (!found && seen_count < 32) {
                snprintf(seen[seen_count], sizeof(seen[0]), "%s",
                         g_catalog.skills[i].category);
                seen_count++;
                if (categories[0]) strncat(categories, ", ", sizeof(categories) - strlen(categories) - 1);
                strncat(categories, g_catalog.skills[i].category, sizeof(categories) - strlen(categories) - 1);
            }
        }
        snprintf(buf, sizeof(buf),
            "browse.sh skills hub: %d skills across %d categories (%s)",
            g_catalog.count, seen_count, categories[0] ? categories : "uncategorized");
    }
    return strdup(buf);
}
