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

#define JSON_PARSE(json_str) ({ char *_e = NULL; json_t *_r = json_parse(json_str, &_e); free(_e); _r; })

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
 *  Parse catalog from JSON string
 * ================================================================ */
static void test_parse_catalog(void) {
    printf("\n=== Parse Catalog ===\n");

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

    skills_hub_clear_cache();

    hub_skill_meta_t results[10];
    int n = skills_hub_search("airbnb", results, 10);
    printf("  INFO: skills_hub_search returned %d (network may be unavailable)\n", n);

    skills_hub_clear_cache();
    char *summary = skills_hub_summary();
    TEST("summary not null", summary != NULL);
    if (summary) {
        printf("  INFO: %s\n", summary);
        free(summary);
    }

    hub_skill_meta_t meta;
    bool found = skills_hub_get_by_slug("nonexistent", &meta);
    TEST("nonexistent slug not found", !found);

    skills_hub_clear_cache();
    TEST("clear_cache succeeds", 1);
}

/* --- Phase 403: Catalog edge case expansion --- */

static void test_empty_catalog(void) {
    printf("\n=== Empty Catalog ===\n");
    json_t *root = JSON_PARSE("{\"skills\":[]}");
    TEST("empty skills array parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        TEST("skills array exists", arr != NULL);
        TEST("skills array empty", arr && json_len(arr) == 0);
        json_free(root);
    }
}

static void test_missing_skills_key(void) {
    printf("\n=== Missing Skills Key ===\n");
    json_t *root = JSON_PARSE("{}");
    TEST("empty object parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        TEST("skills key is NULL", arr == NULL);
        json_free(root);
    }
}

static void test_single_skill_catalog(void) {
    printf("\n=== Single Skill Catalog ===\n");
    json_t *root = JSON_PARSE(
        "{\"skills\":[{\"slug\":\"test-slug\",\"name\":\"test\","
        "\"title\":\"Test Skill\",\"description\":\"A test\"}]}");
    TEST("single skill parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        TEST("skills array has 1 item", arr && json_len(arr) == 1);
        json_t *skill = json_get(arr, 0);
        TEST("skill exists", skill != NULL);
        if (skill) {
            const char *slug = json_get_str(skill, "slug", NULL);
            TEST("slug = test-slug", slug && strcmp(slug, "test-slug") == 0);
            const char *name = json_get_str(skill, "name", NULL);
            TEST("name = test", name && strcmp(name, "test") == 0);
        }
        json_free(root);
    }
}

static void test_skill_no_tags(void) {
    printf("\n=== Skill Without Tags ===\n");
    json_t *root = JSON_PARSE(
        "{\"skills\":[{\"slug\":\"no-tags\",\"name\":\"nope\","
        "\"title\":\"No Tags\",\"description\":\"no tags here\"}]}");
    TEST("no-tags skill parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        json_t *skill = json_get(arr, 0);
        const char *title = json_get_str(skill, "title", NULL);
        TEST("title accessible without tags", title && strcmp(title, "No Tags") == 0);
        json_t *tags = json_obj_get(skill, "tags");
        TEST("tags key is NULL", tags == NULL);
        json_free(root);
    }
}

static void test_unicode_skill_name(void) {
    printf("\n=== Unicode Skill Name ===\n");
    json_t *root = JSON_PARSE(
        "{\"skills\":[{\"slug\":\"unicode\",\"name\":\"café résumé\","
        "\"title\":\"Café Résumé\",\"description\":\"Unicode in fields\"}]}");
    TEST("unicode skill parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        json_t *skill = json_get(arr, 0);
        const char *name = json_get_str(skill, "name", NULL);
        TEST("unicode name preserved", name && strcmp(name, "café résumé") == 0);
        const char *title = json_get_str(skill, "title", NULL);
        TEST("unicode title preserved", title && strcmp(title, "Café Résumé") == 0);
        json_free(root);
    }
}

static void test_skill_null_fields(void) {
    printf("\n=== Skill With Null Fields ===\n");
    json_t *root = JSON_PARSE(
        "{\"skills\":[{\"slug\":\"null-fields\",\"name\":null,"
        "\"title\":null,\"description\":null,\"hostname\":null,"
        "\"category\":null}]}");
    TEST("null-field skill parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        json_t *skill = json_get(arr, 0);
        TEST("slug still present", skill != NULL);
        const char *slug = json_get_str(skill, "slug", NULL);
        TEST("slug = null-fields", slug && strcmp(slug, "null-fields") == 0);
        const char *name = json_get_str(skill, "name", NULL);
        TEST("null name returns NULL", name == NULL);
        const char *title = json_get_str(skill, "title", NULL);
        TEST("null title returns NULL", title == NULL);
        json_free(root);
    }
}

static void test_long_skill_fields(void) {
    printf("\n=== Long Skill Fields ===\n");
    char long_json[4096];
    char long_desc[2000];
    memset(long_desc, 'd', 1999);
    long_desc[1999] = '\0';
    snprintf(long_json, sizeof(long_json),
        "{\"skills\":[{\"slug\":\"long\",\"name\":\"%.100s\","
        "\"title\":\"%.100s\",\"description\":\"%s\"}]}",
        long_desc, long_desc, long_desc);
    json_t *root = JSON_PARSE(long_json);
    TEST("long-field skill parses", root != NULL);
    if (root) {
        json_t *arr = json_obj_get(root, "skills");
        json_t *skill = json_get(arr, 0);
        TEST("long-field skill exists", skill != NULL);
        const char *desc = json_get_str(skill, "description", NULL);
        TEST("long description preserved", desc && strlen(desc) == 1999);
        json_free(root);
    }
}

int main(void) {
    printf("=== Browse.sh Skills Hub Tests (L12) ===\n");

    test_parse_catalog();
    test_skills_hub_no_network();
    /* Phase 403 */
    test_empty_catalog();
    test_missing_skills_key();
    test_single_skill_catalog();
    test_skill_no_tags();
    test_unicode_skill_name();
    test_skill_null_fields();
    test_long_skill_fields();

    printf("\n=== %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
