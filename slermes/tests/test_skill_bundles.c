/*
 * test_skill_bundles.c — Tests for skill_bundles module.
 *
 * Tests YAML-based skill bundle loading: scan, find, print,
 * edge cases (empty dir, invalid YAML, duplicate slugs, no skills).
 */

#include "skill_bundles.h"
#include "yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* Create a YAML file at path with given content. Returns 0 on success. */
static int create_yaml(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(content, 1, strlen(content), f);
    fclose(f);
    return (n == strlen(content)) ? 0 : -1;
}

/* Remove a directory and all contents (simple recursive rm) */
static void rmdir_recursive(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *entry;
    char buf[1024];
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);
        remove(buf);
    }
    closedir(d);
    rmdir(path);
}

/* ================================================================
 *  Basic scan and find
 * ================================================================ */

static void test_basic_scan_and_find(void) {
    const char *dir = "/tmp/hermes_test_bundles_basic";
    mkdir(dir, 0755);

    /* Create valid bundle YAML */
    ASSERT(create_yaml("/tmp/hermes_test_bundles_basic/backend.yaml",
        "name: Backend Dev\n"
        "description: Backend feature work\n"
        "skills:\n"
        "  - github-code-review\n"
        "  - test-driven-development\n"
    ) == 0, "failed to create backend.yaml");

    ASSERT(create_yaml("/tmp/hermes_test_bundles_basic/frontend.yaml",
        "name: Frontend Dev\n"
        "description: Frontend work\n"
        "instruction: Use React patterns\n"
        "skills:\n"
        "  - design-md\n"
    ) == 0, "failed to create frontend.yaml");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan loads 2 bundles");
    ASSERT(n == 2, "expected 2 bundles");
    PASS();

    TEST("find 'backend-dev' by slug");
    const skill_bundle_t *b = skill_bundle_find(&reg, "backend-dev");
    ASSERT(b != NULL, "backend-dev not found");
    ASSERT(strcmp(b->name, "Backend Dev") == 0, "wrong name");
    ASSERT(strcmp(b->slug, "backend-dev") == 0, "wrong slug");
    ASSERT(strcmp(b->description, "Backend feature work") == 0, "wrong desc");
    ASSERT(b->skill_count == 2, "expected 2 skills");
    ASSERT(strcmp(b->skills[0], "github-code-review") == 0, "wrong skill[0]");
    ASSERT(strcmp(b->skills[1], "test-driven-development") == 0, "wrong skill[1]");
    PASS();

    TEST("find 'frontend-dev' by slug");
    b = skill_bundle_find(&reg, "frontend-dev");
    ASSERT(b != NULL, "frontend-dev not found");
    ASSERT(strcmp(b->slug, "frontend-dev") == 0, "wrong slug");
    ASSERT(b->skill_count == 1, "expected 1 skill");
    ASSERT(strcmp(b->skills[0], "design-md") == 0, "wrong skill");
    ASSERT(strstr(b->instruction, "React") != NULL, "missing instruction");
    ASSERT(strstr(b->description, "Frontend") != NULL, "missing description");
    PASS();

    TEST("find nonexistent slug returns NULL");
    b = skill_bundle_find(&reg, "nonexistent");
    ASSERT(b == NULL, "should return NULL");
    PASS();

    TEST("find with NULL slug returns NULL");
    b = skill_bundle_find(&reg, NULL);
    ASSERT(b == NULL, "should return NULL");
    PASS();

    TEST("find on NULL registry returns NULL");
    b = skill_bundle_find(NULL, "backend-dev");
    ASSERT(b == NULL, "should return NULL");
    PASS();

    /* Cleanup */
    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

/* ================================================================
 *  Edge cases
 * ================================================================ */

static void test_empty_directory(void) {
    setenv("HERMES_BUNDLES_DIR", "/tmp/hermes_test_bundles_empty", 1);
    mkdir("/tmp/hermes_test_bundles_empty", 0755);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan empty directory returns 0");
    ASSERT(n == 0, "expected 0 bundles");
    PASS();

    rmdir_recursive("/tmp/hermes_test_bundles_empty");
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_nonexistent_directory(void) {
    setenv("HERMES_BUNDLES_DIR", "/tmp/hermes_test_bundles_nope", 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan nonexistent directory returns 0 (not -1)");
    ASSERT(n == 0, "expected 0 bundles for missing dir");
    PASS();

    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_invalid_yaml(void) {
    const char *dir = "/tmp/hermes_test_bundles_invalid";
    mkdir(dir, 0755);
    ASSERT(create_yaml("/tmp/hermes_test_bundles_invalid/bad.yaml",
        "this is not valid yaml: [[[") == 0, "failed to create bad.yaml");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan with invalid YAML returns 0 (file skipped)");
    ASSERT(n == 0, "expected 0 bundles for invalid YAML");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_no_skills_bundle(void) {
    const char *dir = "/tmp/hermes_test_bundles_noskills";
    mkdir(dir, 0755);
    ASSERT(create_yaml("/tmp/hermes_test_bundles_noskills/empty.yaml",
        "name: Empty Bundle\n"
        "description: Has no skills\n"
        "skills:\n"
    ) == 0, "failed to create empty.yaml");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan bundle with no skills returns 0");
    ASSERT(n == 0, "expected 0 bundles");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_duplicate_slug(void) {
    const char *dir = "/tmp/hermes_test_bundles_dup";
    mkdir(dir, 0755);

    /* Both files would produce the same slug "my-bundle" */
    ASSERT(create_yaml("/tmp/hermes_test_bundles_dup/one.yaml",
        "name: My Bundle\n"
        "skills:\n"
        "  - skill-one\n"
    ) == 0, "failed to create one.yaml");

    ASSERT(create_yaml("/tmp/hermes_test_bundles_dup/two.yaml",
        "name: My Bundle\n"
        "skills:\n"
        "  - skill-two\n"
    ) == 0, "failed to create two.yaml");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan with duplicate slugs keeps first only");
    ASSERT(n == 1, "expected 1 bundle (first kept)");
    const skill_bundle_t *b = skill_bundle_find(&reg, "my-bundle");
    ASSERT(b != NULL, "my-bundle should exist");
    ASSERT(b->skill_count == 1, "should have 1 skill");
    ASSERT(strcmp(b->skills[0], "skill-one") == 0, "should be first file's skill");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_yaml_extension_variants(void) {
    const char *dir = "/tmp/hermes_test_bundles_ext";
    mkdir(dir, 0755);

    ASSERT(create_yaml("/tmp/hermes_test_bundles_ext/alpha.yaml",
        "name: Alpha\nskills:\n  - skill-a\n") == 0, "failed to create alpha");
    ASSERT(create_yaml("/tmp/hermes_test_bundles_ext/beta.yml",
        "name: Beta\nskills:\n  - skill-b\n") == 0, "failed to create beta");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan handles both .yaml and .yml extensions");
    ASSERT(n == 2, "expected 2 bundles");
    ASSERT(skill_bundle_find(&reg, "alpha") != NULL, "alpha not found");
    ASSERT(skill_bundle_find(&reg, "beta") != NULL, "beta not found");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_filename_as_name(void) {
    const char *dir = "/tmp/hermes_test_bundles_fname";
    mkdir(dir, 0755);

    /* No 'name' field — should use filename stem */
    ASSERT(create_yaml("/tmp/hermes_test_bundles_fname/my-custom-bundle.yaml",
        "skills:\n"
        "  - skill-x\n"
        "  - skill-y\n"
    ) == 0, "failed to create");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan uses filename as name when no YAML name");
    ASSERT(n == 1, "expected 1 bundle");
    const skill_bundle_t *b = skill_bundle_find(&reg, "my-custom-bundle");
    ASSERT(b != NULL, "my-custom-bundle not found");
    ASSERT(strcmp(b->name, "my-custom-bundle") == 0, "name should be filename stem");
    ASSERT(b->skill_count == 2, "expected 2 skills");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_default_description(void) {
    const char *dir = "/tmp/hermes_test_bundles_desc";
    mkdir(dir, 0755);

    /* No description field — should get default */
    ASSERT(create_yaml("/tmp/hermes_test_bundles_desc/test.yaml",
        "name: Test\n"
        "skills:\n"
        "  - skill-a\n"
        "  - skill-b\n"
        "  - skill-c\n"
    ) == 0, "failed to create");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan generates default description when none provided");
    ASSERT(n == 1, "expected 1 bundle");
    const skill_bundle_t *b = skill_bundle_find(&reg, "test");
    ASSERT(b != NULL, "test not found");
    ASSERT(strstr(b->description, "Load 3 skills") != NULL, "should mention skill count");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_skill_count_cap(void) {
    const char *dir = "/tmp/hermes_test_bundles_cap";
    mkdir(dir, 0755);

    /* Create a file with BUNDLE_SKILLS_MAX+ skills */
    char content[8192];
    int pos = snprintf(content, sizeof(content),
        "name: Overskilled\nskills:\n");
    for (int i = 0; i < 70; i++) {
        pos += snprintf(content + pos, sizeof(content) - pos,
            "  - skill-%03d\n", i);
    }

    ASSERT(create_yaml("/tmp/hermes_test_bundles_cap/big.yaml", content) == 0,
           "failed to create big.yaml");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    int n = skill_bundles_scan(&reg);

    TEST("scan caps skills at BUNDLE_SKILLS_MAX (64)");
    ASSERT(n == 1, "expected 1 bundle");
    const skill_bundle_t *b = skill_bundle_find(&reg, "overskilled");
    ASSERT(b != NULL, "overskilled not found");
    ASSERT(b->skill_count <= 64, "skill count exceeds BUNDLE_SKILLS_MAX");
    ASSERT(b->skill_count >= 64, "should have capped at 64");
    ASSERT(strcmp(b->skills[63], "skill-063") == 0, "last skill should be skill-063");
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_print(void) {
    const char *dir = "/tmp/hermes_test_bundles_print";
    mkdir(dir, 0755);

    ASSERT(create_yaml("/tmp/hermes_test_bundles_print/print.yaml",
        "name: Print Test\n"
        "description: Testing print output\n"
        "skills:\n"
        "  - skill-one\n"
    ) == 0, "failed to create");

    setenv("HERMES_BUNDLES_DIR", dir, 1);

    skill_bundle_registry_t reg;
    skill_bundles_scan(&reg);

    /* Capture stdout via pipe — fflush first to clear buffered output */
    fflush(stdout);
    int pipe_fds[2];
    ASSERT(pipe(pipe_fds) == 0, "pipe failed");
    int old_stdout = dup(STDOUT_FILENO);
    ASSERT(old_stdout != -1, "dup failed");
    ASSERT(dup2(pipe_fds[1], STDOUT_FILENO) != -1, "dup2 failed");

    skill_bundles_print(&reg);
    fflush(stdout);

    /* Restore stdout */
    dup2(old_stdout, STDOUT_FILENO);
    close(old_stdout);
    close(pipe_fds[1]);

    /* Read captured output */
    char buf[512] = {0};
    ssize_t rn = read(pipe_fds[0], buf, sizeof(buf) - 1);
    close(pipe_fds[0]);
    if (rn < 0) rn = 0;
    buf[rn] = '\0';

    TEST("print outputs bundle info");
    ASSERT(strstr(buf, "Skill bundles") != NULL, "missing header");
    ASSERT(strstr(buf, "print-test") != NULL, "missing slug");
    ASSERT(strstr(buf, "Testing print output") != NULL, "missing description");
    ASSERT(strstr(buf, "skill-one") != NULL, "missing skill");
    PASS();

    TEST("print NULL registry doesn't crash");
    skill_bundles_print(NULL);
    PASS();

    rmdir_recursive(dir);
    unsetenv("HERMES_BUNDLES_DIR");
}

static void test_scan_with_null_registry(void) {
    TEST("scan with NULL registry returns -1");
    int n = skill_bundles_scan(NULL);
    ASSERT(n == -1, "expected -1 for NULL registry");
    PASS();
}

/* ================================================================
 *  Main
 * ================================================================ */

int main(void) {
    printf("=== Skill Bundle Tests ===\n\n");

    test_basic_scan_and_find();
    test_empty_directory();
    test_nonexistent_directory();
    test_invalid_yaml();
    test_no_skills_bundle();
    test_duplicate_slug();
    test_yaml_extension_variants();
    test_filename_as_name();
    test_default_description();
    test_skill_count_cap();
    test_print();
    test_scan_with_null_registry();

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
