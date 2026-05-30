/*
 * test_skills.c — Skills system unit tests (M38).
 *
 * Tests core API: scan, validate, provenance, sync, bundles,
 * usage tracking, cache, search, curator, hub install.
 *
 * Build:
 *   gcc -O2 -g -Wall -Werror -I include -I lib/libjson -I lib/libyaml -I lib/libplugin \
 *       tests/test_skills.c src/tools/skills.c \
 *       lib/libjson/json.c lib/libyaml/yaml.c \
 *       -o /tmp/hermes_test_skills -lm -Wl,--unresolved-symbols=ignore-all
 *
 * Run:
 *   HERMES_HOME=/tmp/hermes_test_skills_home /tmp/hermes_test_skills
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR(name, expr, expected) do { \
    const char *_got = (expr); \
    if (_got && strcmp(_got, expected) == 0) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d) -- got '%s', expected '%s'\n", name, __LINE__, _got ? _got : "(null)", expected); } \
} while(0)

/* Helper: create directory tree */
static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(path, 0755);
}

/* Helper: write a file */
static int write_str(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

/* ================================================================
 *  Helper -- ensure test's Skills dir exists
 * ================================================================ */
static const char *test_home(void) {
    const char *h = getenv("HERMES_HOME");
    return h ? h : "/tmp/hermes_test_skills_home";
}

static void ensure_skills_dir(void) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", test_home());
    mkdir_p(buf);
}

static void cleanup_skills_dir(void) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/.hermes/skills", test_home());
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s 2>/dev/null", buf);
    (void)system(cmd);
}

/* ================================================================
 *  Tests
 * ================================================================ */

static void test_scan(void) {
    printf("\n--- Scan Tests ---\n");

    cleanup_skills_dir();
    ensure_skills_dir();
    skill_list_t *list = skills_scan_all();
    TEST("scan all returns non-null", list != NULL);
    TEST("scan all has count 0", list->count == 0);
    skills_scan_free(list);

    cleanup_skills_dir();
    ensure_skills_dir();
    char dir[1024], sk_path[1024];
    snprintf(dir, sizeof(dir), "%s/.hermes/skills/testskill", test_home());
    mkdir_p(dir);
    snprintf(sk_path, sizeof(sk_path), "%s/SKILL.md", dir);
    write_str(sk_path,
        "---\nname: testskill\nversion: 1.0.0\ndescription: A test skill\n---\n\nHello\n");

    list = skills_scan_all();
    TEST("scan finds 1 skill", list != NULL && list->count == 1);
    if (list && list->count > 0) {
        TEST_STR("skill name is testskill", list->skills[0].name, "testskill");
        TEST_STR("skill version is 1.0.0", list->skills[0].version, "1.0.0");
    }
    skills_scan_free(list);

    skills_scan_free(NULL);
    TEST("scan_free(null) safe", 1);

    cleanup_skills_dir();
}

static void test_validate(void) {
    printf("\n--- Validate Tests ---\n");

    char err[256];
    bool ok = skill_validate("nonexistent", err, sizeof(err));
    TEST("validate non-existent returns false", !ok);
    TEST("validate non-existent sets error", err[0] != '\0');

    ok = skill_validate(NULL, err, sizeof(err));
    TEST("validate null name handled gracefully", !ok);

    ok = skill_validate("nonexistent", NULL, 0);
    TEST("validate with null error buffer doesn't crash", !ok);

    ensure_skills_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.hermes/skills/validskill", test_home());
    mkdir_p(dir);
    char sk_path[1024];
    snprintf(sk_path, sizeof(sk_path), "%s/.hermes/skills/validskill/SKILL.md", test_home());
    write_str(sk_path,
        "---\nname: validskill\nversion: 2.0.0\ndescription: A valid test skill\n---\n\nContent\n");

    err[0] = '\0';
    ok = skill_validate("validskill", err, sizeof(err));
    TEST("validate valid skill returns true", ok);

    ok = skill_validate_all();
    TEST("validate_all returns true", ok);

    cleanup_skills_dir();
}

static void test_origin(void) {
    printf("\n--- Origin Tests ---\n");
    ensure_skills_dir();

    skill_origin_t o = skill_get_origin("nosuchskill");
    TEST("origin of non-existent is LOCAL", o == SKILL_ORIGIN_LOCAL);

    bool ok = skill_set_origin("nosuchskill", SKILL_ORIGIN_HUB);
    TEST("set origin to HUB", ok);
    o = skill_get_origin("nosuchskill");
    TEST("origin now HUB", o == SKILL_ORIGIN_HUB);

    ok = skill_set_origin("nosuchskill", SKILL_ORIGIN_BUNDLE);
    TEST("set origin to BUNDLE", ok);
    o = skill_get_origin("nosuchskill");
    TEST("origin now BUNDLE", o == SKILL_ORIGIN_BUNDLE);

    o = skill_get_origin(NULL);
    TEST("origin of null is LOCAL", o == SKILL_ORIGIN_LOCAL);

    cleanup_skills_dir();
}

static void test_cache(void) {
    printf("\n--- Cache Tests ---\n");

    bool ok = skill_cache_init(0);
    TEST("cache init with 0 entries", ok);

    size_t cnt = skill_cache_count();
    TEST("cache count is 0 after init", cnt == 0);

    const char *content = skill_cache_get("nonexistent");
    TEST("get from empty cache returns NULL", content == NULL);

    ok = skill_cache_preload("nonexistent");
    TEST("preload non-existent returns false", !ok);

    skill_cache_evict("nonexistent");
    TEST("evict non-existent doesn't crash", 1);

    skill_cache_destroy();
    TEST("cache destroy doesn't crash", 1);

    ok = skill_cache_init(10);
    TEST("cache init with 10 max entries", ok);
    cnt = skill_cache_count();
    TEST("cache count is 0 after re-init", cnt == 0);

    skill_cache_destroy();
    skill_cache_destroy();
    TEST("double destroy doesn't crash", 1);
}

static void test_usage(void) {
    printf("\n--- Usage Tracking Tests ---\n");
    ensure_skills_dir();

    int cnt = skill_get_usage_count("neverused");
    TEST("usage count of unused is 0", cnt == 0);

    time_t last = skill_get_last_used("neverused");
    TEST("last_used of unused is 0", last == 0);

    {
        char spath[1024];
        snprintf(spath, sizeof(spath), "%s/.hermes/skills/myfavoriteskill", test_home());
        mkdir_p(spath);
    }
    skill_record_usage("myfavoriteskill");
    cnt = skill_get_usage_count("myfavoriteskill");
    TEST("usage count after record is 1", cnt == 1);

    skill_record_usage("myfavoriteskill");
    skill_record_usage("myfavoriteskill");
    cnt = skill_get_usage_count("myfavoriteskill");
    TEST("usage count after 3 records is 3", cnt == 3);

    {
        char spath[1024];
        snprintf(spath, sizeof(spath), "%s/.hermes/skills/otherskill", test_home());
        mkdir_p(spath);
    }
    skill_record_usage("otherskill");
    cnt = skill_get_usage_count("otherskill");
    TEST("other skill count is 1", cnt == 1);

    cnt = skill_get_usage_count("myfavoriteskill");
    TEST("first skill still 3", cnt == 3);

    last = skill_get_last_used("myfavoriteskill");
    TEST("last_used is non-zero", last > 0);

    skill_record_usage(NULL);
    TEST("record usage(null) doesn't crash", 1);

    skill_meta_t recs[5];
    size_t rec_count = 5;
    skill_get_recommendations(recs, &rec_count, 5);
    TEST("recommendations count >= 0", rec_count >= 0);
}

static void test_search(void) {
    printf("\n--- Search Tests ---\n");

    size_t count = 0;
    skill_search_result_t *results = skill_search(NULL, NULL, &count, 10);
    TEST("search with null query returns NULL (no skills)", results == NULL);
    TEST("search null query has 0 results", count == 0);
    skill_search_free(results, count);

    results = skill_search("", NULL, &count, 10);
    TEST("search with empty query returns NULL (no skills)", results == NULL);
    skill_search_free(results, count);

    results = skill_search("important", NULL, &count, 10);
    TEST("search with no skills returns NULL (no skills)", results == NULL);
    skill_search_free(results, count);

    skill_search_free(NULL, 0);
    TEST("search_free(null, 0) doesn't crash", 1);

    skill_search_hub_free(NULL, 0);
    TEST("hub_search_free(null, 0) doesn't crash", 1);

    char err[256] = "";
    bool ok = skill_install_from_hub("!!!invalid!!!", err, sizeof(err));
    TEST("install from hub with bad slug returns false", !ok);
}

static void test_bundle(void) {
    printf("\n--- Bundle Tests ---\n");

    skill_list_t *list = skill_bundle_get_skills("nosuchbundle");
    TEST("get skills from non-existent bundle returns NULL", list == NULL);

    bool ok = skill_bundle_delete("nosuchbundle");
    TEST("delete non-existent bundle returns false", !ok);

    ok = skill_bundle_create(NULL, "skill1,skill2");
    TEST("create bundle with null name returns false", !ok);

    ok = skill_bundle_create("", "skill1,skill2");
    TEST("create bundle with empty name returns false", !ok);

    list = skill_bundle_get_skills(NULL);
    TEST("get skills with null name returns NULL", list == NULL);
}

static void test_sync(void) {
    printf("\n--- Sync Tests ---\n");

    char log[512] = "";
    bool ok = skill_sync_from_hub("http://invalid.test.local:1/nonexistent.git", "main", log, sizeof(log));
    TEST("sync from bad URL returns false", !ok);

    ok = skill_sync_from_hub(NULL, "main", log, sizeof(log));
    TEST("sync with null URL returns false", !ok);

    ok = skill_sync_from_hub("http://invalid.test.local:1/test.git", "main", NULL, 0);
    TEST("sync with null log buffer doesn't crash", !ok);
}

static void test_curator(void) {
    printf("\n--- Curator Tests ---\n");

    skill_curator_run(NULL, 0);
    TEST("curator with null report doesn't crash", 1);

    char report[1024] = "";
    skill_curator_run(report, sizeof(report));
    TEST("curator with report buffer doesn't crash", 1);
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("Skills System Tests (M38)\n");
    printf("========================\n");

    test_scan();
    test_validate();
    test_origin();
    test_cache();
    test_usage();
    test_search();
    test_bundle();
    test_sync();
    test_curator();

    printf("\n========================\n");
    printf("  %d passed, %d failed\n", passed, failed);
    printf("========================\n");

    cleanup_skills_dir();

    return failed > 0 ? 1 : 0;
}
