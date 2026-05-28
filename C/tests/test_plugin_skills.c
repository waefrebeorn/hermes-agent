/* test_plugin_skills.c — Tests for skills plugin.
 * Builds as standalone test by compiling plugin source directly. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include plugin source directly for unit testing */
#include "../src/plugins/plugin_skills.c"

/* Test helpers */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    fprintf(stderr, "  TEST: %s ... ", name); \
} while (0)

#define PASS() do { \
    tests_passed++; \
    fprintf(stderr, "PASS\n"); \
} while (0)

#define FAIL(msg) do { \
    tests_failed++; \
    fprintf(stderr, "FAIL: %s\n", msg); \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

/* Create a temp skill directory for testing */
static void make_temp_skill(const char *base, const char *name, const char *desc, const char *cat) {
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/%s", base, name);
    mkdir(dir, 0700);

    char path[4096];
    snprintf(path, sizeof(path), "%s/SKILL.md", dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "---\n");
        fprintf(f, "name: %s\n", name);
        if (desc) fprintf(f, "description: %s\n", desc);
        if (cat) fprintf(f, "category: %s\n", cat);
        fprintf(f, "---\n");
        fprintf(f, "# %s\n\nTest content.\n", name);
        fclose(f);
    }

    /* Create subdirectories */
    snprintf(path, sizeof(path), "%s/references", dir);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/scripts", dir);
    mkdir(path, 0700);
}

static void remove_dir(const char *path) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

/* Test scanning skills directory */
static void test_scan_skills(void) {
    TEST("scan_skills discovers .md files");
    /* Set up temp skills dir */
    char tmp_dir[] = "/tmp/test_skills_XXXXXX";
    mkdtemp(tmp_dir);

    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", tmp_dir);
    make_temp_skill(tmp_dir, "test-skill", "A test skill", "testing");

    g_skill_count = 0;
    int count = scan_skills();
    ASSERT(count > 0, "should find at least 1 skill");
    ASSERT(count <= MAX_SKILLS, "count should be within bounds");
    ASSERT(g_skill_count == count, "g_skill_count should match");

    /* Cleanup */
    remove_dir(tmp_dir);
    PASS();
}

/* Test reading skill metadata */
static void test_read_skill_metadata(void) {
    TEST("read_skill_file extracts frontmatter");
    char tmp_dir[] = "/tmp/test_skills_meta_XXXXXX";
    mkdtemp(tmp_dir);

    make_temp_skill(tmp_dir, "my-skill", "My test description", "devops");

    skill_entry_t skill;
    char skill_path[4096];
    snprintf(skill_path, sizeof(skill_path), "%s/my-skill", tmp_dir);
    int ret = read_skill_file(skill_path, &skill);
    ASSERT(ret == 1, "should read skill file");
    ASSERT(strcmp(skill.name, "my-skill") == 0, "name should match");
    ASSERT(strcmp(skill.description, "My test description") == 0, "description should match");
    ASSERT(strcmp(skill.category, "devops") == 0, "category should match");
    ASSERT(skill.has_references == 1, "should have references dir");
    ASSERT(skill.has_scripts == 1, "should have scripts dir");
    ASSERT(skill.has_templates == 0, "should NOT have templates dir");

    remove_dir(tmp_dir);
    PASS();
}

/* Test list_skills tool */
static void test_list_skills_tool(void) {
    TEST("list_skills returns valid JSON");
    char tmp_dir[] = "/tmp/test_skills_list_XXXXXX";
    mkdtemp(tmp_dir);

    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", tmp_dir);
    make_temp_skill(tmp_dir, "alpha", "Alpha skill", "cat1");
    make_temp_skill(tmp_dir, "beta", "Beta skill", "cat2");

    char *result = tool_list_skills("{}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "count") != NULL, "should contain count");
    ASSERT(strstr(result, "alpha") != NULL, "should contain alpha");
    ASSERT(strstr(result, "beta") != NULL, "should contain beta");
    ASSERT(strstr(result, "has_references") != NULL, "should contain has_references");
    free(result);

    remove_dir(tmp_dir);
    PASS();
}

/* Test get_skill tool */
static void test_get_skill_tool(void) {
    TEST("get_skill returns skill details");
    char tmp_dir[] = "/tmp/test_skills_get_XXXXXX";
    mkdtemp(tmp_dir);

    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", tmp_dir);
    make_temp_skill(tmp_dir, "myskill", "My skill description", "tools");

    /* Rescan */
    g_skill_count = 0;
    scan_skills();

    char *result = tool_get_skill("{\"name\":\"myskill\"}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "myskill") != NULL, "should contain skill name");
    ASSERT(strstr(result, "My skill description") != NULL, "should contain description");
    free(result);

    remove_dir(tmp_dir);
    PASS();
}

/* Test search_skills tool */
static void test_search_skills_tool(void) {
    TEST("search_skills finds matching skills");
    char tmp_dir[] = "/tmp/test_skills_search_XXXXXX";
    mkdtemp(tmp_dir);

    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", tmp_dir);
    make_temp_skill(tmp_dir, "debugging", "Debug and fix", "dev");
    make_temp_skill(tmp_dir, "testing", "Run tests", "qa");

    g_skill_count = 0;
    scan_skills();

    char *result = tool_search_skills("{\"query\":\"debug\"}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "debugging") != NULL, "should find debugging");
    free(result);

    result = tool_search_skills("{\"query\":\"nonexistent\"}");
    ASSERT(result != NULL, "should return non-NULL for no match");
    ASSERT(strstr(result, "\"count\":0") != NULL || strstr(result, "\"matches\":[]") != NULL,
           "empty result should have 0 matches");

    free(result);
    remove_dir(tmp_dir);
    PASS();
}

/* Test search with empty query returns all */
static void test_search_all(void) {
    TEST("search_skills with empty query returns all skills");
    char tmp_dir[] = "/tmp/test_skills_all_XXXXXX";
    mkdtemp(tmp_dir);

    snprintf(g_skills_dir, sizeof(g_skills_dir), "%s", tmp_dir);
    make_temp_skill(tmp_dir, "s1", "Skill one", "cat");
    make_temp_skill(tmp_dir, "s2", "Skill two", "cat");

    g_skill_count = 0;
    scan_skills();

    char *result = tool_search_skills("{}");
    ASSERT(result != NULL, "should return non-NULL");
    ASSERT(strstr(result, "s1") != NULL, "should include s1");
    ASSERT(strstr(result, "s2") != NULL, "should include s2");
    free(result);

    remove_dir(tmp_dir);
    PASS();
}

/* Test plugin lifecycle */
static void test_plugin_lifecycle(void) {
    TEST("plugin_init sets interface correctly");
    memset(&g_interface, 0, sizeof(g_interface));
    int ret = plugin_init();
    ASSERT(ret == 0, "init should return 0");
    ASSERT(g_interface.type == PLUGIN_SKILL, "type should be PLUGIN_SKILL");
    PASS();
}

int main(void) {
    fprintf(stderr, "Skills Plugin Tests\n");
    fprintf(stderr, "===================\n");

    test_scan_skills();
    test_read_skill_metadata();
    test_list_skills_tool();
    test_get_skill_tool();
    test_search_skills_tool();
    test_search_all();
    test_plugin_lifecycle();

    fprintf(stderr, "\nResults: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);

    if (tests_failed > 0) return 1;
    return 0;
}
