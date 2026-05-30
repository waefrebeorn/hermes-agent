/* skill_utils test — core skill metadata utilities. */

#include "skill_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s (line %d)\\n", name, __LINE__); \
        fail++; \
    } else { \
        pass++; \
    } \
} while(0)

#define TEST_STR(name, actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (!_a && !_e) { pass++; } \
    else if (!_a || !_e || strcmp(_a, _e) != 0) { \
        fprintf(stderr, "FAIL: %s (line %d): got '%s', expected '%s'\\n", \
                name, __LINE__, _a ? _a : "(null)", _e ? _e : "(null)"); \
        fail++; \
    } else { pass++; } \
} while(0)

/* ── is_excluded_path ─────────────────────────────────────── */

static void test_excluded_path(void) {
    TEST("excluded: .git",            skill_is_excluded_path("project/.git/refs"));
    TEST("excluded: __pycache__",     skill_is_excluded_path("skills/__pycache__/foo.py"));
    TEST("excluded: node_modules",    skill_is_excluded_path("project/node_modules/express"));
    TEST("excluded: .venv",           skill_is_excluded_path("/home/user/.venv/bin"));
    TEST("not excluded: src",         !skill_is_excluded_path("src/main.c"));
    TEST("not excluded: normal dir",  !skill_is_excluded_path("skills/my-skill/SKILL.md"));
    TEST("not excluded: empty",       !skill_is_excluded_path(""));
    TEST("not excluded: null",        !skill_is_excluded_path(NULL));
}

/* ── parse_frontmatter ────────────────────────────────────── */

static void test_parse_frontmatter_simple(void) {
    const char *content =
        "---\n"
        "name: test-skill\n"
        "description: A test skill\n"
        "---\n"
        "Body text here\n";
    skill_frontmatter_t fm;
    int n = skill_parse_frontmatter(content, &fm);
    TEST("fm: parsed count", n >= 2);
    TEST_STR("fm: name", skill_fm_get(&fm, "name"), "test-skill");
    TEST_STR("fm: description", skill_fm_get(&fm, "description"), "A test skill");
}

static void test_parse_frontmatter_no_frontmatter(void) {
    const char *content = "Just body text\\nNo frontmatter\\n";
    skill_frontmatter_t fm;
    int n = skill_parse_frontmatter(content, &fm);
    TEST("fm: no frontmatter", n == 0);
}

static void test_parse_frontmatter_malformed(void) {
    const char *content = "---\\nbroken\\n";
    skill_frontmatter_t fm;
    int n = skill_parse_frontmatter(content, &fm);
    TEST("fm: malformed handles gracefully", n >= 0);
}

/* ── platform matching ────────────────────────────────────── */

static void test_matches_platform_no_constraint(void) {
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    TEST("platform: no constraint = match", skill_matches_platform(&fm));
}

static void test_matches_platform(void) {
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    snprintf(fm.entries[0].key, sizeof(fm.entries[0].key), "platforms");
    snprintf(fm.entries[0].value, sizeof(fm.entries[0].value), "linux");
    fm.count = 1;
    /* linux on linux = match */
    TEST("platform: linux on linux", skill_matches_platform(&fm));
}

/* ── description extraction ───────────────────────────────── */

static void test_extract_description(void) {
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    snprintf(fm.entries[0].key, sizeof(fm.entries[0].key), "description");
    snprintf(fm.entries[0].value, sizeof(fm.entries[0].value), "Short description");
    fm.count = 1;

    char out[128];
    skill_extract_description(&fm, out, sizeof(out));
    TEST_STR("desc: short", out, "Short description");
}

static void test_extract_description_truncated(void) {
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    snprintf(fm.entries[0].key, sizeof(fm.entries[0].key), "description");
    snprintf(fm.entries[0].value, sizeof(fm.entries[0].value),
             "This is a very long description that should definitely be truncated at sixty characters for display purposes");
    fm.count = 1;

    char out[128];
    skill_extract_description(&fm, out, sizeof(out));
    TEST("desc: truncated", strlen(out) <= 60);
    TEST("desc: ellipsis", strstr(out, "...") != NULL);
}

static void test_extract_description_null(void) {
    char out[128];
    skill_extract_description(NULL, out, sizeof(out));
    TEST("desc: null fm", out[0] == '\0');
}

/* ── conditions extraction ────────────────────────────────── */

static void test_extract_conditions_empty(void) {
    skill_conditions_t conds;
    skill_extract_conditions(NULL, &conds);
    TEST("conds: null fm", conds.fallback_for_toolsets[0] == '\0');
}

static void test_extract_conditions(void) {
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    snprintf(fm.entries[0].key, sizeof(fm.entries[0].key), "metadata.hermes.fallback_for_toolsets");
    snprintf(fm.entries[0].value, sizeof(fm.entries[0].value), "web_search,vision");
    snprintf(fm.entries[1].key, sizeof(fm.entries[1].key), "metadata.hermes.requires_toolsets");
    snprintf(fm.entries[1].value, sizeof(fm.entries[1].value), "terminal");
    fm.count = 2;

    skill_conditions_t conds;
    skill_extract_conditions(&fm, &conds);
    TEST_STR("conds: fallback", conds.fallback_for_toolsets, "web_search,vision");
    TEST_STR("conds: requires", conds.requires_toolsets, "terminal");
}

/* ── namespace helpers ────────────────────────────────────── */

static void test_parse_qualified_name(void) {
    char *ns, *bare;
    skill_parse_qualified_name("myplugin:my-skill", &ns, &bare);
    TEST_STR("qname: namespace", ns, "myplugin");
    TEST_STR("qname: bare", bare, "my-skill");
    free(ns); free(bare);
}

static void test_parse_qualified_name_no_namespace(void) {
    char *ns, *bare;
    skill_parse_qualified_name("simple-name", &ns, &bare);
    TEST("qname: no namespace", ns == NULL);
    TEST_STR("qname: bare only", bare, "simple-name");
    free(bare);
}

static void test_is_valid_namespace(void) {
    TEST("ns: valid alpha",        skill_is_valid_namespace("myplugin"));
    TEST("ns: valid with hyphen",  skill_is_valid_namespace("my-plugin"));
    TEST("ns: valid with number",  skill_is_valid_namespace("plugin2"));
    TEST("ns: invalid empty",      !skill_is_valid_namespace(""));
    TEST("ns: invalid null",       !skill_is_valid_namespace(NULL));
    TEST("ns: invalid colon",      !skill_is_valid_namespace("bad:ns"));
    TEST("ns: invalid dot",        !skill_is_valid_namespace("bad.ns"));
}

/* ── config vars (basic) ──────────────────────────────────── */

static void test_extract_config_vars_empty(void) {
    skill_config_var_t vars[8];
    skill_frontmatter_t fm;
    memset(&fm, 0, sizeof(fm));
    int n = skill_extract_config_vars(&fm, vars, 8);
    TEST("cfg: empty fm", n == 0);
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    test_excluded_path();
    test_parse_frontmatter_simple();
    test_parse_frontmatter_no_frontmatter();
    test_parse_frontmatter_malformed();
    test_matches_platform_no_constraint();
    test_matches_platform();
    test_extract_description();
    test_extract_description_truncated();
    test_extract_description_null();
    test_extract_conditions_empty();
    test_extract_conditions();
    test_parse_qualified_name();
    test_parse_qualified_name_no_namespace();
    test_is_valid_namespace();
    test_extract_config_vars_empty();

    fprintf(stderr, "skill_utils: %d/%d pass\\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
