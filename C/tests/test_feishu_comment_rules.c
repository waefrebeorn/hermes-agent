/*
 * test_feishu_comment_rules.c — Tests for G04 Feishu comment rules.
 * Uses the actual source file to test JSON parsing + rule resolution.
 *
 * Compile:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libyaml \\
 *       -I lib/libhttp -I lib/liblineedit -I lib/libbase64 -I lib/libhash \\
 *       -I lib/libdatetime -I lib/libpath -I lib/libuuid \\
 *       -o /tmp/t_fcr test_feishu_comment_rules.c \\
 *       ../src/tools/feishu_comment_rules.c ../lib/libjson/json.c -lm
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* Include the code under test */
#include "hermes_feishu_rules.h"

/* For JSON file creation */
#include "hermes_json.h"

/* ── Test framework ────────────────────────────────────── */

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (%s)\n", name, #cond); \
        failures++; \
    } else { \
        fprintf(stdout, "  PASS: %s\n", name); \
    } \
} while(0)

#define TEST_STR_EQ(name, a, b) do { \
    const char *_a = (a); const char *_b = (b); \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  FAIL: %s (\"%s\" vs \"%s\")\n", name, _a ? _a : "NULL", _b ? _b : "NULL"); \
        failures++; \
    } else { \
        fprintf(stdout, "  PASS: %s\n", name); \
    } \
} while(0)

#define TEST_INT_EQ(name, a, b) do { \
    int _a = (int)(a); int _b = (int)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL: %s (%d vs %d)\n", name, _a, _b); \
        failures++; \
    } else { \
        fprintf(stdout, "  PASS: %s\n", name); \
    } \
} while(0)

/* ── Helpers ───────────────────────────────────────────── */

/* Create a JSON rules file for testing */
static void create_rules_file(const char *path, const char *content) {
    /* Ensure dir exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0700);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", content);
        fclose(f);
    }
}

static void delete_file(const char *path) {
    unlink(path);
}

/* ── Tests ─────────────────────────────────────────────── */

static void test_load_config_defaults(void) {
    fprintf(stdout, "\n--- test_load_config_defaults ---\n");

    /* No file on disk — should return defaults */
    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();
    TEST("cfg not null", cfg != NULL);
    if (cfg) {
        TEST("enabled true", cfg->enabled == true);
        TEST_STR_EQ("policy pairing", cfg->policy, "pairing");
        TEST("no allow_from", cfg->allow_from == NULL);
        TEST("no documents", cfg->doc_count == 0);
        feishu_rules_free_config(cfg);
    }
}

static void test_load_config_with_rules(void) {
    fprintf(stdout, "\n--- test_load_config_with_rules ---\n");

    /* Set HERMES_HOME to a temp dir for testing */
    setenv("HERMES_HOME", "/tmp/hermes_test_fcr", 1);

    const char *json =
        "{"
        "  \"enabled\": true,"
        "  \"policy\": \"pairing\","
        "  \"allow_from\": [\"user1\", \"user2\"],"
        "  \"documents\": {"
        "    \"docx:abc123\": { \"enabled\": false },"
        "    \"*\": { \"policy\": \"allowlist\", \"allow_from\": [\"admin\"] },"
        "    \"wiki:wnode\": { \"enabled\": true }"
        "  }"
        "}";

    char path[1024];
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr/feishu_comment_rules.json");
    create_rules_file(path, json);

    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();

    TEST("cfg not null", cfg != NULL);
    if (cfg) {
        TEST("enabled true", cfg->enabled == true);
        TEST_STR_EQ("policy pairing", cfg->policy, "pairing");
        TEST("allow_from count 2", cfg->allow_from_count == 2);
        if (cfg->allow_from) {
            TEST_STR_EQ("allow_from[0]", cfg->allow_from[0], "user1");
            TEST_STR_EQ("allow_from[1]", cfg->allow_from[1], "user2");
        }
        TEST_INT_EQ("doc count 3", cfg->doc_count, 3);

        feishu_rules_free_config(cfg);
    }

    delete_file(path);
}

static void test_has_wiki_keys(void) {
    fprintf(stdout, "\n--- test_has_wiki_keys ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_wiki", 1);

    /* With wiki keys */
    const char *json_with =
        "{ \"documents\": { \"wiki:abc\": {}, \"docx:123\": {} } }";
    char path[1024];
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr_wiki/feishu_comment_rules.json");
    create_rules_file(path, json_with);

    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();
    TEST("cfg not null", cfg != NULL);
    if (cfg) {
        TEST("has wiki keys", feishu_rules_has_wiki_keys(cfg) == true);
        feishu_rules_free_config(cfg);
    }
    delete_file(path);

    /* Without wiki keys */
    const char *json_without = "{ \"documents\": { \"docx:123\": {} } }";
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr_wiki/feishu_comment_rules.json");
    create_rules_file(path, json_without);

    feishu_rules_invalidate_cache();
    cfg = feishu_rules_load_config();
    TEST("cfg not null", cfg != NULL);
    if (cfg) {
        TEST("no wiki keys", feishu_rules_has_wiki_keys(cfg) == false);
        feishu_rules_free_config(cfg);
    }
    delete_file(path);
}

static void test_resolve_rule(void) {
    fprintf(stdout, "\n--- test_resolve_rule ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_resolve", 1);

    /* Config with: exact doc rule, wildcard, and top-level defaults */
    const char *json =
        "{"
        "  \"enabled\": true,"
        "  \"policy\": \"pairing\","
        "  \"allow_from\": [\"global_user\"],"
        "  \"documents\": {"
        "    \"docx:abc123\": { \"enabled\": false },"
        "    \"*\": { \"policy\": \"allowlist\", \"allow_from\": [\"admin_user\"] },"
        "    \"wiki:wiki123\": { \"enabled\": true, \"allow_from\": [\"wiki_user\"] }"
        "  }"
        "}";

    char path[1024];
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr_resolve/feishu_comment_rules.json");
    create_rules_file(path, json);

    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();
    TEST("cfg not null", cfg != NULL);

    if (cfg) {
        /* Test 1: Exact match — docx:abc123 should disable */
        feishu_resolved_rule_t *r = feishu_rules_resolve_rule(cfg, "docx", "abc123", "");
        TEST("exact rule not null", r != NULL);
        if (r) {
            TEST("exact rule disabled", r->enabled == false);
            TEST("exact match source prefix", strncmp(r->match_source, "exact:", 6) == 0);
            feishu_rules_free_resolved(r);
        }

        /* Test 2: Wildcard match — docx:xyz has no exact, falls to wildcard */
        r = feishu_rules_resolve_rule(cfg, "docx", "xyz", "");
        TEST("wildcard rule not null", r != NULL);
        if (r) {
            TEST("wildcard rule enabled (from top)", r->enabled == true);
            TEST_STR_EQ("wildcard policy", r->policy, "allowlist");
            TEST("wildcard match source", strcmp(r->match_source, "wildcard") == 0);
            feishu_rules_free_resolved(r);
        }

        /* Test 3: No exact match — falls to wildcard "documents.*" */
        r = feishu_rules_resolve_rule(cfg, "unknown", "type", "");
        TEST("default rule not null", r != NULL);
        if (r) {
            TEST("default enabled true (from top)", r->enabled == true);
            TEST_STR_EQ("default policy from wildcard", r->policy, "allowlist");
            TEST_STR_EQ("default match source wildcard", r->match_source, "wildcard");
            feishu_rules_free_resolved(r);
        }

        /* Test 4: Wiki match — wiki:wiki123 */
        r = feishu_rules_resolve_rule(cfg, "docx", "some", "wiki123");
        TEST("wiki rule not null", r != NULL);
        if (r) {
            TEST("wiki enabled true", r->enabled == true);
            TEST("wiki allow_from count 1", r->allow_from_count == 1);
            if (r->allow_from) {
                TEST_STR_EQ("wiki allow_from[0]", r->allow_from[0], "wiki_user");
            }
            feishu_rules_free_resolved(r);
        }

        feishu_rules_free_config(cfg);
    }

    delete_file(path);
}

static void test_is_user_allowed(void) {
    fprintf(stdout, "\n--- test_is_user_allowed ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_allow", 1);

    /* allow_from check */
    feishu_resolved_rule_t *rule = calloc(1, sizeof(feishu_resolved_rule_t));
    rule->enabled = true;
    rule->policy = strdup("allowlist");
    rule->allow_from = calloc(3, sizeof(char *));
    rule->allow_from[0] = strdup("alice");
    rule->allow_from[1] = strdup("bob");
    rule->allow_from[2] = NULL;
    rule->allow_from_count = 2;

    TEST("alice allowed", feishu_rules_is_user_allowed(rule, "alice") == true);
    TEST("bob allowed", feishu_rules_is_user_allowed(rule, "bob") == true);
    TEST("charlie denied", feishu_rules_is_user_allowed(rule, "charlie") == false);
    TEST("null denied", feishu_rules_is_user_allowed(rule, NULL) == false);

    feishu_rules_free_resolved(rule);

    /* Pairing check — add a user then check */
    rule = calloc(1, sizeof(feishu_resolved_rule_t));
    rule->enabled = true;
    rule->policy = strdup("pairing");

    TEST("no pairing not allowed", feishu_rules_is_user_allowed(rule, "paired_user") == false);

    /* Add to pairing store */
    bool added = feishu_rules_pairing_add("paired_user");
    TEST("pairing add success", added == true);
    TEST("pairing duplicate add false", feishu_rules_pairing_add("paired_user") == false);

    TEST("paired_user now allowed", feishu_rules_is_user_allowed(rule, "paired_user") == true);
    TEST("unpaired_user denied", feishu_rules_is_user_allowed(rule, "unpaired") == false);

    /* Remove from pairing */
    bool removed = feishu_rules_pairing_remove("paired_user");
    TEST("pairing remove success", removed == true);
    TEST("pairing remove again false", feishu_rules_pairing_remove("paired_user") == false);

    TEST("paired_user denied after remove", feishu_rules_is_user_allowed(rule, "paired_user") == false);

    feishu_rules_free_resolved(rule);

    /* Cleanup pairing files */
    char p_path[1024];
    snprintf(p_path, sizeof(p_path), "/tmp/hermes_test_fcr_allow/feishu_comment_pairing.json");
    delete_file(p_path);
}

static void test_pairing_ops(void) {
    fprintf(stdout, "\n--- test_pairing_ops ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_pair", 1);

    /* Start fresh */
    feishu_rules_invalidate_cache();

    /* Empty list */
    char *list = feishu_rules_pairing_list();
    TEST("empty list", list != NULL);
    TEST_STR_EQ("empty list json", list ? list : "", "{}");
    free(list);

    /* Add multiple users */
    TEST("add alice", feishu_rules_pairing_add("alice") == true);
    TEST("add bob", feishu_rules_pairing_add("bob") == true);
    TEST("add charlie", feishu_rules_pairing_add("charlie") == true);
    TEST("add alice again fails", feishu_rules_pairing_add("alice") == false);

    /* List should have 3 */
    list = feishu_rules_pairing_list();
    TEST("list not null", list != NULL);
    if (list) {
        /* Check it contains alice, bob, charlie */
        TEST("list has alice", strstr(list, "alice") != NULL);
        TEST("list has bob", strstr(list, "bob") != NULL);
        TEST("list has charlie", strstr(list, "charlie") != NULL);
        free(list);
    }

    /* Remove bob */
    TEST("remove bob", feishu_rules_pairing_remove("bob") == true);
    TEST("remove bob again fails", feishu_rules_pairing_remove("bob") == false);

    list = feishu_rules_pairing_list();
    TEST("list not null", list != NULL);
    if (list) {
        TEST("list has alice", strstr(list, "alice") != NULL);
        TEST("list no bob", strstr(list, "bob") == NULL);
        TEST("list has charlie", strstr(list, "charlie") != NULL);
        free(list);
    }

    /* Cleanup */
    char p_path[1024];
    snprintf(p_path, sizeof(p_path), "/tmp/hermes_test_fcr_pair/feishu_comment_pairing.json");
    delete_file(p_path);
    char r_path[1024];
    snprintf(r_path, sizeof(r_path), "/tmp/hermes_test_fcr_pair/feishu_comment_rules.json");
    delete_file(r_path);
}

static void test_invalid_rules_file(void) {
    fprintf(stdout, "\n--- test_invalid_rules_file ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_bad", 1);

    /* Invalid JSON */
    char path[1024];
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr_bad/feishu_comment_rules.json");
    create_rules_file(path, "this is not json");

    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();
    TEST("cfg not null after bad json", cfg != NULL);
    if (cfg) {
        TEST("defaults used enabled true", cfg->enabled == true);
        feishu_rules_free_config(cfg);
    }

    delete_file(path);
}

static void test_resolve_enabled_field_fallback(void) {
    fprintf(stdout, "\n--- test_resolve_enabled_field_fallback ---\n");

    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_fb", 1);

    /* Wildcard overrides policy but not enabled — should fallback to top-level for enabled */
    const char *json =
        "{"
        "  \"enabled\": true,"
        "  \"policy\": \"pairing\","
        "  \"documents\": {"
        "    \"*\": { \"policy\": \"allowlist\" }"
        "  }"
        "}";

    char path[1024];
    snprintf(path, sizeof(path), "/tmp/hermes_test_fcr_fb/feishu_comment_rules.json");
    create_rules_file(path, json);

    feishu_rules_invalidate_cache();
    feishu_rules_config_t *cfg = feishu_rules_load_config();
    TEST("cfg not null", cfg != NULL);

    if (cfg) {
        feishu_resolved_rule_t *r = feishu_rules_resolve_rule(cfg, "docx", "any", "");
        TEST("rule not null", r != NULL);
        if (r) {
            /* enabled falls back from wildcard (no value) → top-level (true) */
            TEST("enabled from top", r->enabled == true);
            /* policy from wildcard */
            TEST_STR_EQ("policy from wildcard", r->policy, "allowlist");
            TEST("match source from wildcard (policy)", strcmp(r->match_source, "wildcard") == 0);
            feishu_rules_free_resolved(r);
        }
        feishu_rules_free_config(cfg);
    }

    delete_file(path);
}

/* ── Main ──────────────────────────────────────────────── */

int main(void) {
    fprintf(stdout, "Feishu Comment Rules Test Suite (G04)\n");
    fprintf(stdout, "======================================\n");

    /* Set default HERMES_HOME for tests that don't override */
    setenv("HERMES_HOME", "/tmp/hermes_test_fcr_defaults", 1);

    test_load_config_defaults();
    test_load_config_with_rules();
    test_has_wiki_keys();
    test_resolve_rule();
    test_is_user_allowed();
    test_pairing_ops();
    test_invalid_rules_file();
    test_resolve_enabled_field_fallback();

    fprintf(stdout, "\n======================================\n");
    if (failures == 0)
        fprintf(stdout, "ALL TESTS PASSED\n");
    else
        fprintf(stdout, "%d TEST(S) FAILED\n", failures);

    /* Cleanup temp dirs */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf /tmp/hermes_test_fcr*");
    system(cmd);

    return failures > 0 ? 1 : 0;
}
