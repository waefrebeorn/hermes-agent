/* test_skill_commands.c — Tests for skill_commands module.
 * Single-scan flow: create temp skills, scan once, run all tests.
 * Compile with skill_commands.c + -Wl,--unresolved-symbols=ignore-all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "hermes_skill_commands.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        tests_passed++; \
        printf("  \xe2\x9c\x85 %s\n", name); \
    } else { \
        tests_failed++; \
        printf("  \xe2\x9d\x8c %s\n", name); \
    } \
} while(0)

static void cleanup_dir(const char *path) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    int ret = system(cmd);
    (void)ret;
}

static int create_skill(const char *base, const char *name) {
    char dir[512], md[520];
    snprintf(dir, sizeof(dir), "%s/%s", base, name);
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    snprintf(md, sizeof(md), "%s/SKILL.md", dir);
    FILE *f = fopen(md, "w");
    if (!f) return -1;
    fprintf(f, "---\nname: %s\nslug: /%s\ndescription: Test %s\n---\n\nContent.\n", name, name, name);
    fclose(f);
    return 0;
}

int main(void) {
    const char *test_home = "/tmp/hermes_sk_test";

    /* Setup: create temp skills dir with 3 skills */
    char skills_dir[640];
    snprintf(skills_dir, sizeof(skills_dir), "%s/.hermes/skills", test_home);
    cleanup_dir(test_home);
    /* Create dir tree: test_home/.hermes/skills/ */
    if (mkdir(test_home, 0755) != 0) { perror("mkdir test_home"); return 1; }
    char hermes_dir[640];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", test_home);
    if (mkdir(hermes_dir, 0755) != 0) { perror("mkdir .hermes"); return 1; }
    if (mkdir(skills_dir, 0755) != 0) { perror("mkdir skills"); return 1; }
    create_skill(skills_dir, "test-one");
    create_skill(skills_dir, "test-two");
    create_skill(skills_dir, "my-skill");
    create_skill(skills_dir, "code-review");

    setenv("HOME", test_home, 1);
    unsetenv("HERMES_HOME");

    /* Single scan */
    int cnt = skill_cmd_scan();
    printf("=== Skill Commands Tests ===\n");

    printf("\n--- Scan ---\n");
    TEST("scan finds 4 skills", cnt == 4);

    printf("\n--- Get ---\n");
    const skill_cmd_entry_t *e = skill_cmd_get("/test-one");
    TEST("get /test-one", e != NULL);
    if (e) TEST("get slug matches", strcmp(e->slug, "/test-one") == 0);

    e = skill_cmd_get("/nonexistent");
    TEST("get nonexistent returns NULL", e == NULL);

    e = skill_cmd_get(NULL);
    TEST("get NULL returns NULL", e == NULL);

    printf("\n--- Get All ---\n");
    int ac = 0;
    const skill_cmd_entry_t *all = skill_cmd_get_all(&ac);
    TEST("get_all count == 4", ac == 4);
    TEST("get_all array non-NULL", all != NULL);

    printf("\n--- Resolve ---\n");
    const char *r = skill_cmd_resolve("/code-review");
    TEST("exact /code-review", r && strcmp(r, "/code-review") == 0);

    r = skill_cmd_resolve("code-review");
    TEST("no leading slash", r && strcmp(r, "/code-review") == 0);

    r = skill_cmd_resolve("/Code-Review");
    TEST("case insensitive", r && strcmp(r, "/code-review") == 0);

    r = skill_cmd_resolve("/code_review");
    TEST("underscore to hyphen", r && strcmp(r, "/code-review") == 0);

    r = skill_cmd_resolve("//code-review");
    TEST("multiple slashes", r && strcmp(r, "/code-review") == 0);

    r = skill_cmd_resolve("/nonexistent");
    TEST("nonexistent returns NULL", r == NULL);

    r = skill_cmd_resolve("");
    TEST("empty returns NULL", r == NULL);

    r = skill_cmd_resolve(NULL);
    TEST("NULL returns NULL", r == NULL);

    printf("\n--- Rescan ---\n");
    int added = 0, removed = 0;
    int rv = skill_cmd_rescan(&added, &removed);
    TEST("rescan same skills returns 0 diff", rv == 0);

    /* Add a skill and rescan */
    create_skill(skills_dir, "new-skill");
    rv = skill_cmd_rescan(&added, &removed);
    TEST("rescan after add returns 1", rv == 1);
    TEST("rescan reports 1 added", added == 1);
    TEST("rescan reports 0 removed", removed == 0);

    /* Cleanup */
    cleanup_dir(test_home);
    setenv("HOME", "/home/wubu", 1);

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
