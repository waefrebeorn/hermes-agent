/* Test browse.sh skills hub (L12) */
#include "hermes_skills_hub.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int pass = 0, fail = 0;

#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

/* ================================================================
 *  Mock catalog JSON (subset of real browse.sh response)
 * ================================================================ */
static const char *MOCK_CATALOG = "{"
    "\"skills\":["
        "{"
            "\"slug\":\"airbnb.com/search-listings-ddgioa\","
            "\"name\":\"airbnb.com\","
            "\"title\":\"Airbnb Search Listings\","
            "\"description\":\"Search and browse Airbnb listings by location and dates.\","
            "\"hostname\":\"airbnb.com\","
            "\"category\":\"travel\","
            "\"tags\":[\"travel\",\"accommodation\"],"
            "\"sourceUrl\":\"https://github.com/browserbase/browse-sh/blob/main/skills/airbnb.com/SKILL.md\","
            "\"recommendedMethod\":\"stagehand\","
            "\"proxies\":false,"
            "\"installCount\":42"
        "},"
        "{"
            "\"slug\":\"amazon.com/search-products-xyz\","
            "\"name\":\"amazon.com\","
            "\"title\":\"Amazon Product Search\","
            "\"description\":\"Search for products on Amazon.\","
            "\"hostname\":\"amazon.com\","
            "\"category\":\"shopping\","
            "\"tags\":[\"shopping\",\"ecommerce\"],"
            "\"sourceUrl\":\"https://raw.githubusercontent.com/browserbase/browse-sh/main/skills/amazon.com/SKILL.md\","
            "\"recommendedMethod\":\"stagehand\","
            "\"proxies\":false,"
            "\"installCount\":99"
        "}"
    "]}";

/* ================================================================
 *  Mock: parse catalog from JSON string
 * ================================================================ */
static void test_parse_catalog(void) {
    printf("\n=== Parse Catalog ===\n");

    /* Parse mock JSON */
    char *err = NULL;
    json_t *root = json_parse(MOCK_CATALOG, &err);
    TEST("mock catalog parses", root != NULL);

    if (!root) {
        printf("  Parse error: %s\n", err ? err : "unknown");
        free(err);
        return;
    }

    json_t *skills_arr = json_obj_get(root, "skills");
    TEST("skills array exists", skills_arr != NULL);
    TEST("skills array has 2 items", skills_arr && json_len(skills_arr) == 2);

    /* Check first skill */
    json_t *first = skills_arr ? json_get(skills_arr, 0) : NULL;
    TEST("first skill exists", first != NULL);
    if (first) {
        const char *slug = json_get_str(first, "slug", NULL);
        TEST("slug = airbnb...", slug && strcmp(slug, "airbnb.com/search-listings-ddgioa") == 0);

        const char *name = json_get_str(first, "name", NULL);
        TEST("name = airbnb.com", name && strcmp(name, "airbnb.com") == 0);

        const char *title = json_get_str(first, "title", NULL);
        TEST("title = Airbnb Search Listings", title && strcmp(title, "Airbnb Search Listings") == 0);

        const char *desc = json_get_str(first, "description", NULL);
        TEST("description contains Airbnb listings", desc && strstr(desc, "Airbnb") != NULL);

        const char *cat = json_get_str(first, "category", NULL);
        TEST("category = travel", cat && strcmp(cat, "travel") == 0);

        const char *method = json_get_str(first, "recommendedMethod", NULL);
        TEST("method = stagehand", method && strcmp(method, "stagehand") == 0);

        double install = json_get_num(first, "installCount", 0);
        TEST("installCount = 42", install == 42);

        bool proxies = json_get_bool(first, "proxies", true);
        TEST("proxies = false", proxies == false);
    }

    /* Check second skill */
    json_t *second = skills_arr ? json_get(skills_arr, 1) : NULL;
    TEST("second skill exists", second != NULL);
    if (second) {
        const char *name = json_get_str(second, "name", NULL);
        TEST("second name = amazon.com", name && strcmp(name, "amazon.com") == 0);
    }

    json_free(root);
}

/* ================================================================
 *  Test: parse catalog via skills_hub_fetch_catalog (no network mock)
 * ================================================================ */
static void test_skills_hub_no_network(void) {
    printf("\n=== Skills Hub (no network) ===\n");

    /* Cache should be clear initially */
    skills_hub_clear_cache();

    /* Searching without data should auto-fetch (which will fail without network) */
    hub_skill_meta_t results[10];
    int n = skills_hub_search("airbnb", results, 10);
    /* May return 0 if network unavailable; that's OK */
    printf("  INFO: skills_hub_search returned %d (network may be unavailable)\n", n);

    /* Clear cache — summary should say not loaded */
    skills_hub_clear_cache();
    char *summary = skills_hub_summary();
    TEST("summary not null", summary != NULL);
    if (summary) {
        printf("  INFO: %s\n", summary);
        free(summary);
    }

    /* get_by_slug with empty catalog */
    hub_skill_meta_t meta;
    bool found = skills_hub_get_by_slug("nonexistent", &meta);
    TEST("nonexistent slug not found", !found);

    /* clear_cache doesn't crash */
    skills_hub_clear_cache();
    TEST("clear_cache succeeds", 1);
}

int main(void) {
    printf("=== Browse.sh Skills Hub Tests (L12) ===\n");

    test_parse_catalog();
    test_skills_hub_no_network();

    printf("\n=== %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
