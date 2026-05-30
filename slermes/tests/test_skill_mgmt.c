/*
 * test_skill_mgmt.c — M-series test for skill_mgmt tool.
 *
 * Tests skill_view and skill_manage handlers:
 * - Valid skill loading
 * - Linked file loading
 * - Skill listing
 * - Error handling (missing name, invalid JSON)
 * - NULL/empty safety
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

/* Declare the handlers we're testing */
extern char *skill_view_handler(const char *args_json, const char *task_id);
extern char *skill_manage_handler(const char *args_json, const char *task_id);

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    if (test_##name()) { \
        tests_passed++; \
        printf("  PASS: %s\n", #name); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    const char *_a = (a); \
    const char *_b = (b); \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        printf("    assertion failed: %s -- expected \"%s\" got \"%s\" (%s:%d)\n", \
               msg, _b ? _b : "(null)", _a ? _a : "(null)", __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_JSON_STR(json, key, expected, msg) do { \
    const char *_v = json_get_str(json, key, NULL); \
    if (!_v || strcmp(_v, expected) != 0) { \
        printf("    assertion failed: %s -- %s expected \"%s\" got \"%s\" (%s:%d)\n", \
               msg, key, expected ? expected : "(null)", _v ? _v : "(null)", \
               __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

/* ─── Test helpers ────────────────────────────────────────── */

static char g_test_skills_dir[4096] = "";

/* Create a temp skills directory with a test skill */
static bool setup_test_skill(const char *name, const char *content) {
    char skill_dir[4096];
    snprintf(skill_dir, sizeof(skill_dir), "%s/skills/%s", g_test_skills_dir, name);
    mkdir(skill_dir, 0755);

    char path[4096];
    snprintf(path, sizeof(path), "%s/SKILL.md", skill_dir);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "%s", content ? content : "# Test Skill\n");
    fclose(f);
    return true;
}

/* Create a linked file within a skill directory */
static bool setup_skill_file(const char *skill_name, const char *file_path, const char *content) {
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/skills/%s/%s", g_test_skills_dir, skill_name, file_path);
    /* Create parent directory if needed */
    char *slash = strrchr(full_path, '/');
    if (slash) {
        *slash = '\0';
        mkdir(full_path, 0755);
        *slash = '/';
    }
    FILE *f = fopen(full_path, "w");
    if (!f) return false;
    fprintf(f, "%s", content ? content : "test content");
    fclose(f);
    return true;
}

static void init_test_env(void) {
    snprintf(g_test_skills_dir, sizeof(g_test_skills_dir), "/tmp/hermes_test_skills_%d", getpid());
    mkdir(g_test_skills_dir, 0755);
    /* Create skills subdirectory — get_skills_dir() looks for $HERMES_HOME/skills/ */
    char skills_sub[4096];
    snprintf(skills_sub, sizeof(skills_sub), "%s/skills", g_test_skills_dir);
    mkdir(skills_sub, 0755);
    setenv("HERMES_HOME", g_test_skills_dir, 1);
}

static void cleanup_test_env(void) {
    /* Remove recursively */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_test_skills_dir);
    system(cmd);
}

/* ─── Tests ───────────────────────────────────────────────── */

static bool test_view_valid_skill(void) {
    ASSERT(setup_test_skill("test-skill", "# My Test Skill\n\nHello world!"), "setup skill");

    char args[512];
    snprintf(args, sizeof(args), "{\"name\":\"test-skill\"}");
    char *resp = skill_view_handler(args, NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON response");
    free(err);

    const char *name = json_get_str(j, "name", NULL);
    ASSERT(name != NULL && strcmp(name, "test-skill") == 0, "name matches");

    const char *content = json_get_str(j, "content", NULL);
    ASSERT(content != NULL, "content present");
    ASSERT(strstr(content, "My Test Skill") != NULL, "content matches");

    json_free(j);
    free(resp);

    /* Cleanup */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/skills/test-skill", g_test_skills_dir);
    system(cmd);
    return true;
}

static bool test_view_missing_skill(void) {
    char *resp = skill_view_handler("{\"name\":\"nonexistent-skill\"}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON for missing skill");
    free(err);

    const char *error = json_get_str(j, "error", NULL);
    ASSERT(error != NULL, "error present for missing skill");
    ASSERT_STR_EQ(error, "Skill not found", "error msg");
    ASSERT(json_get_str(j, "content", NULL) == NULL, "no content for missing");

    json_free(j);
    free(resp);
    return true;
}

static bool test_view_with_file_path(void) {
    ASSERT(setup_test_skill("test-skill-fp", "# Skill with files\n"), "setup");
    ASSERT(setup_skill_file("test-skill-fp", "refs/guide.md", "# Reference Guide\n"), "setup file");

    char *resp = skill_view_handler(
        "{\"name\":\"test-skill-fp\",\"file_path\":\"refs/guide.md\"}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON");
    free(err);

    const char *fc = json_get_str(j, "file_content", NULL);
    ASSERT(fc != NULL, "file_content present");
    ASSERT(strstr(fc, "Reference Guide") != NULL, "file_content matches");

    json_free(j);
    free(resp);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/skills/test-skill-fp", g_test_skills_dir);
    system(cmd);
    return true;
}

static bool test_view_null_args(void) {
    char *resp = skill_view_handler(NULL, NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON for NULL args");
    free(err);

    const char *error = json_get_str(j, "error", NULL);
    ASSERT(error != NULL, "error for NULL args");

    json_free(j);
    free(resp);
    return true;
}

static bool test_view_invalid_json(void) {
    char *resp = skill_view_handler("{bad json}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON response for bad input");
    free(err);

    const char *error = json_get_str(j, "error", NULL);
    ASSERT(error != NULL, "error for bad JSON");

    json_free(j);
    free(resp);
    return true;
}

static bool test_view_missing_name(void) {
    char *resp = skill_view_handler("{}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON");
    free(err);

    const char *error = json_get_str(j, "error", NULL);
    ASSERT(error != NULL, "error for missing name");
    ASSERT_STR_EQ(error, "Missing 'name'", "error msg");

    json_free(j);
    free(resp);
    return true;
}

static bool test_manage_list(void) {
    /* Set up 2 skills */
    ASSERT(setup_test_skill("alpha", "# Alpha skill\n"), "setup alpha");
    ASSERT(setup_test_skill("beta", "# Beta skill\n"), "setup beta");

    char *resp = skill_manage_handler("{}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON");
    free(err);

    json_t *skills = json_object_get(j, "skills");
    ASSERT(skills != NULL && json_len(skills) >= 2, "skills array with >=2 entries");

    double count = json_get_num(j, "count", 0);
    ASSERT(count >= 2, "count >= 2");

    json_free(j);
    free(resp);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/skills/alpha %s/skills/beta",
             g_test_skills_dir, g_test_skills_dir);
    system(cmd);
    return true;
}

static bool test_manage_empty(void) {
    /* Use a completely empty temp dir */
    char empty_dir[4096];
    snprintf(empty_dir, sizeof(empty_dir), "/tmp/hermes_empty_skills_%d", getpid());
    mkdir(empty_dir, 0755);
    setenv("HERMES_HOME", empty_dir, 1);

    char *resp = skill_manage_handler("{}", NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON for empty");
    free(err);

    json_t *skills = json_object_get(j, "skills");
    ASSERT(skills != NULL, "skills array present");
    ASSERT(json_len(skills) == 0, "empty skills array");

    json_free(j);
    free(resp);

    /* Restore env to test dir */
    setenv("HERMES_HOME", g_test_skills_dir, 1);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", empty_dir);
    system(cmd);
    return true;
}

static bool test_manage_null_args(void) {
    char *resp = skill_manage_handler(NULL, NULL);
    ASSERT(resp != NULL, "response non-null");

    char *err = NULL;
    json_t *j = json_parse(resp, &err);
    ASSERT(j != NULL, "valid JSON");
    free(err);

    json_t *skills = json_object_get(j, "skills");
    ASSERT(skills != NULL, "skills array present even with NULL args");

    json_free(j);
    free(resp);
    return true;
}

/* ─── Main ────────────────────────────────────────────────── */

int main(void) {
    printf("=== Skill Management Tests ===\n\n");
    init_test_env();

    TEST(view_valid_skill);
    TEST(view_missing_skill);
    TEST(view_with_file_path);
    TEST(view_null_args);
    TEST(view_invalid_json);
    TEST(view_missing_name);
    TEST(manage_list);
    TEST(manage_empty);
    TEST(manage_null_args);

    cleanup_test_env();

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
