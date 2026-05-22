/*
 * test_tirith_policy.c — O13: TIRITH policy rule engine depth tests.
 * Tests: policy lifecycle, rule matching, type-specific eval, YAML parsing, defaults.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hermes.h"

static int pass = 0, fail = 0;

#define TEST(name) do { \
    printf("  %s ... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { pass++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { fail++; printf("FAIL: %s\n", msg); } while(0)

/* ================================================================
 *  Test: Policy lifecycle
 * ================================================================ */

static void test_lifecycle(void) {
    printf("\n=== Policy Lifecycle ===\n");

    TEST("init creates empty policy");
    tirith_policy_t p;
    tirith_policy_init(&p);
    assert(p.count == 0);
    PASS();

    TEST("add_rule succeeds");
    assert(tirith_policy_add_rule(&p, "test-rule", TIRITH_RULE_FILE_PATH,
                                   TIRITH_ACTION_DENY, "/etc/*", false));
    assert(p.count == 1);
    PASS();

    TEST("get_rule returns correct rule");
    const tirith_rule_t *r = tirith_policy_get_rule(&p, 0);
    assert(r != NULL);
    assert(strcmp(r->name, "test-rule") == 0);
    assert(r->type == TIRITH_RULE_FILE_PATH);
    assert(r->action == TIRITH_ACTION_DENY);
    assert(strcmp(r->pattern, "/etc/*") == 0);
    assert(r->is_regex == false);
    PASS();

    TEST("get_rule out of range returns NULL");
    assert(tirith_policy_get_rule(&p, 99) == NULL);
    assert(tirith_policy_get_rule(&p, -1) == NULL);
    PASS();

    TEST("add_rule rejects NULL args");
    assert(!tirith_policy_add_rule(NULL, "x", TIRITH_RULE_FILE_PATH, TIRITH_ACTION_DENY, "/x", false));
    assert(!tirith_policy_add_rule(&p, NULL, TIRITH_RULE_FILE_PATH, TIRITH_ACTION_DENY, "/x", false));
    assert(!tirith_policy_add_rule(&p, "x", TIRITH_RULE_FILE_PATH, TIRITH_ACTION_DENY, NULL, false));
    PASS();

    TEST("remove_rule shifts remaining");
    tirith_policy_add_rule(&p, "second", TIRITH_RULE_NETWORK, TIRITH_ACTION_WARN, "http://*", false);
    assert(p.count == 2);
    assert(tirith_policy_remove_rule(&p, 0)); /* Remove first */
    assert(p.count == 1);
    r = tirith_policy_get_rule(&p, 0);
    assert(strcmp(r->name, "second") == 0); /* Second now at index 0 */
    PASS();

    TEST("clear empties policy");
    tirith_policy_clear(&p);
    assert(p.count == 0);
    PASS();

    TEST("remove_rule invalid index");
    tirith_policy_init(&p);
    assert(!tirith_policy_remove_rule(&p, 0)); /* Empty */
    assert(!tirith_policy_remove_rule(NULL, 0));
    PASS();

    tirith_policy_clear(&p);
}

/* ================================================================
 *  Test: Rule evaluation
 * ================================================================ */

static void test_eval(void) {
    printf("\n=== Rule Evaluation ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    TEST("no rules → ALLOW");
    tirith_policy_result_t res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/passwd");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("NULL policy → ALLOW");
    res = tirith_policy_eval(NULL, TIRITH_RULE_FILE_PATH, "test");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("NULL input → ALLOW");
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, NULL);
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("DENY rule blocks matching input");
    tirith_policy_add_rule(&p, "block-passwd", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/etc/passwd", false);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/passwd");
    assert(res.verdict == TIRITH_BLOCK);
    assert(strcmp(res.matched_rule, "block-passwd") == 0);
    PASS();

    TEST("DENY rule does not block non-matching input");
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/home/user/file.txt");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("WARN rule warns on match");
    tirith_policy_add_rule(&p, "warn-etc", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_WARN, "/etc/*", false);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/issue");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    TEST("ALLOW overrides DENY on same input");
    tirith_policy_add_rule(&p, "allow-etc-issue", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_ALLOW, "/etc/issue", false);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/issue");
    assert(res.verdict == TIRITH_ALLOW);
    assert(strcmp(res.matched_rule, "allow-etc-issue") == 0);
    PASS();

    TEST("ALLOW only overrides for its specific pattern");
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/shadow");
    assert(res.verdict == TIRITH_WARN); /* warn-etc matches */
    PASS();

    tirith_policy_clear(&p);
}

/* ================================================================
 *  Test: Type-specific eval functions
 * ================================================================ */

static void test_type_specific(void) {
    printf("\n=== Type-Specific Eval Functions ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    /* Add rules for each type */
    tirith_policy_add_rule(&p, "block-root", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/root/*", false);
    tirith_policy_add_rule(&p, "block-local", TIRITH_RULE_NETWORK,
                            TIRITH_ACTION_DENY, "http://127.0.0.1*", false);
    tirith_policy_add_rule(&p, "block-rmrf", TIRITH_RULE_COMMAND,
                            TIRITH_ACTION_DENY, "rm -rf /*", false);
    tirith_policy_add_rule(&p, "warn-ld-path", TIRITH_RULE_ENV_VAR,
                            TIRITH_ACTION_WARN, "$LD_*", false);

    TEST("eval_paths blocks /root/ path in command");
    tirith_policy_result_t res = tirith_policy_eval_paths(&p, "cat /root/.bashrc");
    assert(res.verdict == TIRITH_BLOCK);
    assert(strcmp(res.matched_rule, "block-root") == 0);
    PASS();

    TEST("eval_paths allows safe command");
    res = tirith_policy_eval_paths(&p, "ls /home/user/");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("eval_paths NULL safe");
    res = tirith_policy_eval_paths(&p, NULL);
    assert(res.verdict == TIRITH_ALLOW);
    res = tirith_policy_eval_paths(NULL, "test");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("eval_network blocks localhost URL");
    res = tirith_policy_eval_network(&p, "curl http://127.0.0.1:8080/api");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("eval_network allows external URL");
    res = tirith_policy_eval_network(&p, "curl https://api.example.com");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("eval_command blocks rm -rf");
    res = tirith_policy_eval_command(&p, "rm -rf /*");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("eval_command allows safe command");
    res = tirith_policy_eval_command(&p, "ls -la");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("eval_env warns on LD_* env var");
    res = tirith_policy_eval_env(&p, "echo $LD_PRELOAD");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    TEST("eval_env allows safe command");
    res = tirith_policy_eval_env(&p, "echo hello");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    tirith_policy_clear(&p);
}

/* ================================================================
 *  Test: Full pipeline eval_all
 * ================================================================ */

static void test_eval_all(void) {
    printf("\n=== Full Pipeline (eval_all) ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    tirith_policy_add_rule(&p, "block-passwd", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/etc/passwd", false);
    tirith_policy_add_rule(&p, "block-local", TIRITH_RULE_NETWORK,
                            TIRITH_ACTION_DENY, "http://localhost*", false);

    TEST("eval_all blocks file path violation");
    tirith_policy_result_t res = tirith_policy_eval_all(&p, "cat /etc/passwd");
    assert(res.verdict == TIRITH_BLOCK);
    assert(strcmp(res.matched_rule, "block-passwd") == 0);
    PASS();

    TEST("eval_all blocks network violation");
    res = tirith_policy_eval_all(&p, "curl http://localhost:3000");
    assert(res.verdict == TIRITH_BLOCK);
    assert(strcmp(res.matched_rule, "block-local") == 0);
    PASS();

    TEST("eval_all allows clean command");
    res = tirith_policy_eval_all(&p, "ls /home/user/");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    TEST("eval_all NULL safe");
    res = tirith_policy_eval_all(&p, NULL);
    assert(res.verdict == TIRITH_ALLOW);
    res = tirith_policy_eval_all(NULL, "test");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    /* Test multiple violations — first match wins */
    TEST("eval_all first DENY wins");
    tirith_policy_add_rule(&p, "block-shadow", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/etc/shadow", false);
    res = tirith_policy_eval_all(&p, "cat /etc/shadow");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    tirith_policy_clear(&p);
}

/* ================================================================
 *  Test: YAML loading
 * ================================================================ */

static void test_yaml_load(void) {
    printf("\n=== YAML Policy Loading ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    TEST("load empty YAML returns 0");
    assert(tirith_policy_load_yaml(&p, "", 0) == 0);
    PASS();

    TEST("load NULL returns -1");
    assert(tirith_policy_load_yaml(&p, NULL, 10) == -1);
    assert(tirith_policy_load_yaml(NULL, "test", 4) == -1);
    PASS();

    TEST("load single rule");
    const char *yaml = "- name: block-passwd\n"
                       "  type: file_path\n"
                       "  action: deny\n"
                       "  pattern: \"/etc/passwd\"\n";
    int n = tirith_policy_load_yaml(&p, yaml, strlen(yaml));
    assert(n == 1);
    assert(p.count == 1);
    PASS();

    TEST("loaded rule matches correctly");
    tirith_policy_result_t res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/passwd");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("load multiple rules");
    const char *yaml2 = "- name: block-shadow\n"
                        "  type: file_path\n"
                        "  action: deny\n"
                        "  pattern: \"/etc/shadow\"\n"
                        "- name: block-local\n"
                        "  type: network\n"
                        "  action: deny\n"
                        "  pattern: \"http://localhost*\"\n"
                        "- name: warn-home\n"
                        "  type: env_var\n"
                        "  action: warn\n"
                        "  pattern: \"$HOME\"\n";
    n = tirith_policy_load_yaml(&p, yaml2, strlen(yaml2));
    assert(n == 3);
    assert(p.count == 4); /* 1 previous + 3 new */
    PASS();

    TEST("all loaded rules work");
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/shadow");
    assert(res.verdict == TIRITH_BLOCK);
    res = tirith_policy_eval(&p, TIRITH_RULE_NETWORK, "http://localhost:3000");
    assert(res.verdict == TIRITH_BLOCK);
    res = tirith_policy_eval(&p, TIRITH_RULE_ENV_VAR, "$HOME");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    tirith_policy_clear(&p);

    TEST("load with all types and actions");
    const char *yaml3 = "- name: allow-log\n"
                        "  type: file_path\n"
                        "  action: allow\n"
                        "  pattern: \"/var/log/*\"\n"
                        "- name: block-curl-exec\n"
                        "  type: command\n"
                        "  action: deny\n"
                        "  pattern: \"curl | sh\"\n";
    n = tirith_policy_load_yaml(&p, yaml3, strlen(yaml3));
    assert(n == 2);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/var/log/syslog");
    assert(res.verdict == TIRITH_ALLOW);
    res = tirith_policy_eval(&p, TIRITH_RULE_COMMAND, "curl | sh");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    tirith_policy_clear(&p);
}

/* ================================================================
 *  Test: Default policies
 * ================================================================ */

static void test_defaults(void) {
    printf("\n=== Default Policies ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    TEST("load_defaults returns positive count");
    int n = tirith_policy_load_defaults(&p);
    assert(n > 0);
    printf(" (%d rules loaded) ", n);
    PASS();

    TEST("defaults block /etc/passwd");
    tirith_policy_result_t res = tirith_policy_eval_paths(&p, "cat /etc/passwd");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("defaults block /etc/shadow");
    res = tirith_policy_eval_paths(&p, "cat /etc/shadow");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("defaults block ~/.ssh keys");
    res = tirith_policy_eval_paths(&p, "cat ~/.ssh/id_rsa");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("defaults block localhost curl");
    res = tirith_policy_eval_network(&p, "curl http://127.0.0.1:8080");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("defaults warn $HOME env usage");
    res = tirith_policy_eval_env(&p, "echo $HOME");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    TEST("defaults warn $PATH env usage");
    res = tirith_policy_eval_env(&p, "echo $PATH");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    TEST("defaults warn $LD_PRELOAD env usage");
    res = tirith_policy_eval_env(&p, "echo $LD_PRELOAD");
    assert(res.verdict == TIRITH_WARN);
    PASS();

    TEST("defaults allow safe commands");
    res = tirith_policy_eval_all(&p, "ls -la /home/user/");
    assert(res.verdict == TIRITH_ALLOW);
    PASS();

    tirith_policy_clear(&p);

    TEST("load_defaults NULL safe");
    assert(tirith_policy_load_defaults(NULL) == -1);
    PASS();
}

/* ================================================================
 *  Test: Edge cases
 * ================================================================ */

static void test_edge(void) {
    printf("\n=== Edge Cases ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    TEST("max rules boundary");
    for (int i = 0; i < TIRITH_MAX_RULES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "rule-%d", i);
        assert(tirith_policy_add_rule(&p, name, TIRITH_RULE_FILE_PATH,
                                       TIRITH_ACTION_DENY, "/x", false));
    }
    assert(p.count == TIRITH_MAX_RULES);
    /* Next rule should fail */
    assert(!tirith_policy_add_rule(&p, "overflow", TIRITH_RULE_FILE_PATH,
                                    TIRITH_ACTION_DENY, "/x", false));
    assert(p.count == TIRITH_MAX_RULES);
    PASS();

    /* Test eval with pattern matching edge cases */
    TEST("glob pattern with wildcard matches");
    tirith_policy_clear(&p);
    tirith_policy_add_rule(&p, "block-all-etc", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/etc/*", false);
    tirith_policy_result_t res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/hosts");
    assert(res.verdict == TIRITH_BLOCK);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/etc/init.d/script");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("glob ? matches single char");
    tirith_policy_clear(&p);
    tirith_policy_add_rule(&p, "block-2char", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/??", false);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/ab");
    assert(res.verdict == TIRITH_BLOCK);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/abc");
    assert(res.verdict == TIRITH_ALLOW); /* 4 chars, doesn't match /?? */
    PASS();

    TEST("eval_paths extracts paths from complex command");
    tirith_policy_clear(&p);
    tirith_policy_add_rule(&p, "block-tmp", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/tmp/*", false);
    res = tirith_policy_eval_paths(&p, "cat /tmp/evil.sh | bash");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("eval_paths with tilde paths");
    tirith_policy_clear(&p);
    tirith_policy_add_rule(&p, "block-ssh", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "~/.ssh/*", false);
    res = tirith_policy_eval_paths(&p, "cat ~/.ssh/authorized_keys");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("eval_network with multiple URLs");
    tirith_policy_clear(&p);
    tirith_policy_add_rule(&p, "block-10net", TIRITH_RULE_NETWORK,
                            TIRITH_ACTION_DENY, "http://10.*", false);
    res = tirith_policy_eval_network(&p, "curl http://10.0.0.1:8000/api");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();

    TEST("regex fallback does substring match");
    tirith_policy_add_rule(&p, "substr-test", TIRITH_RULE_COMMAND,
                            TIRITH_ACTION_DENY, "EVILCMD", true);
    res = tirith_policy_eval(&p, TIRITH_RULE_COMMAND, "run-EVILCMD-now");
    assert(res.verdict == TIRITH_BLOCK);
    PASS();
}

/* ================================================================
 *  Test: verify reason strings
 * ================================================================ */

static void test_reasons(void) {
    printf("\n=== Reason Strings ===\n");

    tirith_policy_t p;
    tirith_policy_init(&p);

    TEST("DENY reason includes rule name and pattern");
    tirith_policy_add_rule(&p, "my-rule", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_DENY, "/secret/*", false);
    tirith_policy_result_t res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/secret/file");
    assert(res.verdict == TIRITH_BLOCK);
    assert(strstr(res.reason, "my-rule") != NULL);
    assert(strstr(res.reason, "/secret/*") != NULL);
    PASS();

    TEST("ALLOW reason includes rule name");
    tirith_policy_add_rule(&p, "allow-my", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_ALLOW, "/secret/readme", false);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/secret/readme");
    assert(res.verdict == TIRITH_ALLOW);
    assert(strstr(res.reason, "allow-my") != NULL);
    PASS();

    TEST("WARN reason includes rule name");
    tirith_policy_add_rule(&p, "warn-my", TIRITH_RULE_FILE_PATH,
                            TIRITH_ACTION_WARN, "/secret/*", false);
    /* Remove deny rule first to test WARN */
    tirith_policy_remove_rule(&p, 0);
    res = tirith_policy_eval(&p, TIRITH_RULE_FILE_PATH, "/secret/file");
    assert(res.verdict == TIRITH_WARN);
    assert(strstr(res.reason, "warn-my") != NULL);
    PASS();

    tirith_policy_clear(&p);
}

int main(void) {
    printf("=== TIRITH Policy Engine Tests (O13) ===\n");

    test_lifecycle();
    test_eval();
    test_type_specific();
    test_eval_all();
    test_yaml_load();
    test_defaults();
    test_edge();
    test_reasons();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", pass, fail);
    printf("========================================\n");

    return fail > 0 ? 1 : 0;
}
