#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int assertions = 0, passed = 0;

#define TEST(name, cond) do { \
    assertions++; \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void) {
    /* Create temp profiles dir */
    const char *homedir = getenv("HOME") ? getenv("HOME") : "/tmp";
    char profiles_dir[1024];
    snprintf(profiles_dir, sizeof(profiles_dir), "%s/slermes-test/profiles", homedir);
    mkdir(profiles_dir, 0755);

    /* Write test profile */
    char profile_path[1024];
    snprintf(profile_path, sizeof(profile_path), "%s/test.yaml", profiles_dir);
    FILE *f = fopen(profile_path, "w");
    fprintf(f, "model:\n  default: gpt-4o\n  temperature: 0.7\nagent:\n  verbose: 1\n  max_turns: 50\n");
    fclose(f);

    /* Test 1: load profile by name */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    bool ok = hermes_config_load_profile(&cfg, "test", homedir);
    TEST("load_profile returns true", ok);
    TEST("model set by profile", strcmp(cfg.provider_cfg.model, "gpt-4o") == 0);

    /* Test 2: non-existent profile returns false */
    hermes_config_t cfg2;
    memset(&cfg2, 0, sizeof(cfg2));
    bool ok2 = hermes_config_load_profile(&cfg2, "nonexistent", homedir);
    TEST("nonexistent profile returns false", !ok2);

    /* Test 3: null/empty name returns false */
    bool ok3 = hermes_config_load_profile(&cfg, "", homedir);
    TEST("empty profile name returns false", !ok3);
    bool ok4 = hermes_config_load_profile(&cfg, NULL, homedir);
    TEST("null profile name returns false", !ok4);

    /* Test 4: set_profile + get_profile */
    hermes_set_profile("test");
    const char *p = hermes_get_profile();
    TEST("get_profile returns set name", p && strcmp(p, "test") == 0);

    hermes_set_profile(NULL);
    p = hermes_get_profile();
    TEST("get_profile returns NULL after clear", p == NULL);

    hermes_set_profile("");
    p = hermes_get_profile();
    TEST("get_profile returns NULL after empty set", p == NULL);

    /* Cleanup */
    remove(profile_path);
    rmdir(profiles_dir);

    printf("\n=== Profile test: %d/%d passed ===\n", passed, assertions);
    return (assertions == passed) ? 0 : 1;
}
