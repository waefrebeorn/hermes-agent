/* test_skill_manage.c — Tests for skill_mgmt.c CRUD operations */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Everything in a single translation unit so we can call the handlers */
#include "../src/tools/skill_mgmt.c"

static int pass = 0, fail = 0;

#define TEST(name) do { printf("  TEST: %s ... ", #name); \
    if (test_##name()) { printf("PASS\n"); pass++; } \
    else { printf("FAIL\n"); fail++; } } while(0)

static int test_create_skill(void) {
    char *r = skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-unit\",\"content\":\"# Test\\nHello\"}",
        NULL);
    int ok = (r != NULL && strstr(r, "\"ok\"") != NULL);
    /* Cleanup */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-unit'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

static int test_create_duplicate(void) {
    char *r1 = skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-dup\",\"content\":\"# First\"}", NULL);
    char *r2 = skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-dup\",\"content\":\"# Second\"}", NULL);
    int ok = (r1 && strstr(r1, "\"ok\"") != NULL) &&
             (r2 && strstr(r2, "already exists") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-dup'", get_skills_dir());
    system(cmd);
    free(r1); free(r2);
    return ok;
}

static int test_create_invalid_name(void) {
    /* Empty name should fail */
    char *r = skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"\",\"content\":\"# Test\"}", NULL);
    int ok = (r != NULL && strstr(r, "\"error\"") != NULL);
    free(r);
    return ok;
}

static int test_edit_skill(void) {
    /* Create first */
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-edit\",\"content\":\"# Original\"}", NULL);
    /* Then edit */
    char *r = skill_manage_handler(
        "{\"action\":\"edit\",\"name\":\"test-edit\",\"content\":\"# Edited\"}", NULL);
    int ok = (r != NULL && strstr(r, "\"ok\"") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-edit'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

static int test_edit_nonexistent(void) {
    char *r = skill_manage_handler(
        "{\"action\":\"edit\",\"name\":\"nonexistent-skill\",\"content\":\"# Nope\"}", NULL);
    int ok = (r != NULL && strstr(r, "not found") != NULL);
    free(r);
    return ok;
}

static int test_delete_skill(void) {
    /* Create first */
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-del\",\"content\":\"# DeleteMe\"}", NULL);
    /* Then delete */
    char *r = skill_manage_handler(
        "{\"action\":\"delete\",\"name\":\"test-del\"}", NULL);
    int ok = (r != NULL && strstr(r, "\"ok\"") != NULL);
    free(r);
    return ok;
}

static int test_write_file(void) {
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-wf\",\"content\":\"# Test\"}", NULL);
    char *r = skill_manage_handler(
        "{\"action\":\"write_file\",\"name\":\"test-wf\","
        "\"file_path\":\"references/howto.md\",\"file_content\":\"# How to use\"}", NULL);
    int ok = (r != NULL && strstr(r, "\"ok\"") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-wf'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

static int test_write_file_bad_path(void) {
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-bad\",\"content\":\"# Test\"}", NULL);
    char *r = skill_manage_handler(
        "{\"action\":\"write_file\",\"name\":\"test-bad\","
        "\"file_path\":\"random/evil.sh\",\"file_content\":\"rm -rf /\"}", NULL);
    int ok = (r != NULL && strstr(r, "must be under") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-bad'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

static int test_patch_skill(void) {
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-pat\",\"content\":\"# Hello World\\nFoo\\n\"}", NULL);
    char *r = skill_manage_handler(
        "{\"action\":\"patch\",\"name\":\"test-pat\","
        "\"old_string\":\"Hello World\",\"new_string\":\"Goodbye World\"}", NULL);
    int ok = (r != NULL && strstr(r, "\"ok\"") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-pat'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

static int test_list_skills(void) {
    /* Create a skill to list */
    skill_manage_handler(
        "{\"action\":\"create\",\"name\":\"test-list\",\"content\":\"# ListMe\"}", NULL);
    char *r = skill_manage_handler("{}", NULL);
    int ok = (r != NULL && strstr(r, "test-list") != NULL);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s/test-list'", get_skills_dir());
    system(cmd);
    free(r);
    return ok;
}

int main(void) {
    printf("=== Skill Manage CRUD Tests (D81) ===\n");
    TEST(create_skill);
    TEST(create_duplicate);
    TEST(create_invalid_name);
    TEST(edit_skill);
    TEST(edit_nonexistent);
    TEST(delete_skill);
    TEST(write_file);
    TEST(write_file_bad_path);
    TEST(patch_skill);
    TEST(list_skills);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
