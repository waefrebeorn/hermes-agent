#ifndef HERMES_TIRITH_H
#define HERMES_TIRITH_H

/*
 * tirith.h — Pre-exec security scanning for Hermes C.
 * Mirrors Python tools/tirith_security.py: external binary + inline checks.
 * Scans commands for: homograph URLs, pipe-to-interpreter, terminal injection.
 *
 * O13: Policy depth — rule engine for file path, network, command, env var patterns.
 * Rules loaded from YAML (security.tirith_policies) or programmatic API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 *  Basic Verdict (existing API)
 * ================================================================ */

/* Tirith verdict */
typedef enum {
    TIRITH_ALLOW = 0,
    TIRITH_BLOCK = 1,
    TIRITH_WARN  = 2,
    TIRITH_ERROR = -1,  /* Operational failure */
} tirith_verdict_t;

/* ================================================================
 *  O13: Policy Rule Engine
 * ================================================================ */

#define TIRITH_MAX_RULES      64
#define TIRITH_RULE_NAME_MAX  64
#define TIRITH_RULE_PATTERN_MAX 256
#define TIRITH_REASON_MAX     256

/* Rule type: what kind of input the rule inspects */
typedef enum {
    TIRITH_RULE_FILE_PATH = 0,  /* File path patterns in commands */
    TIRITH_RULE_NETWORK,        /* URL/network destination patterns */
    TIRITH_RULE_COMMAND,        /* Full command string patterns */
    TIRITH_RULE_ENV_VAR,        /* Environment variable patterns */
} tirith_rule_type_t;

/* Rule action: what happens when the rule matches */
typedef enum {
    TIRITH_ACTION_DENY = 0,     /* Block execution */
    TIRITH_ACTION_ALLOW,        /* Allow (overrides DENY for same input) */
    TIRITH_ACTION_WARN,         /* Warn but allow */
} tirith_rule_action_t;

/* A single policy rule */
typedef struct {
    char                name[TIRITH_RULE_NAME_MAX];
    tirith_rule_type_t  type;
    tirith_rule_action_t action;
    char                pattern[TIRITH_RULE_PATTERN_MAX];
    bool                is_regex;    /* false = glob matching (fnmatch) */
} tirith_rule_t;

/* Policy result: verdict + human-readable reason */
typedef struct {
    tirith_verdict_t verdict;
    char reason[TIRITH_REASON_MAX];
    const char *matched_rule;       /* Points to name of matching rule */
} tirith_policy_result_t;

/* Policy engine state (thread-safe for single-threaded use) */
typedef struct {
    tirith_rule_t rules[TIRITH_MAX_RULES];
    int count;
} tirith_policy_t;

/* Initialize an empty policy */
void tirith_policy_init(tirith_policy_t *policy);

/* Add a rule. Returns true on success. */
bool tirith_policy_add_rule(tirith_policy_t *policy,
                            const char *name,
                            tirith_rule_type_t type,
                            tirith_rule_action_t action,
                            const char *pattern,
                            bool is_regex);

/* Remove rule at index. True if index valid. */
bool tirith_policy_remove_rule(tirith_policy_t *policy, int index);

/* Clear all rules, reset to empty. */
void tirith_policy_clear(tirith_policy_t *policy);

/* Get rule by index. NULL if out of range. */
const tirith_rule_t *tirith_policy_get_rule(const tirith_policy_t *policy, int index);

/* Evaluate a command/input against all rules of a specific type.
 * Iterates rules in order. DENY wins unless an ALLOW rule matches the same input.
 * WARN returned when no DENY/ALLOW but a WARN rule matches.
 * Result includes the matched rule name and human-readable reason. */
tirith_policy_result_t tirith_policy_eval(const tirith_policy_t *policy,
                                          tirith_rule_type_t type,
                                          const char *input);

/* Evaluate against FILE_PATH rules: scans input for path-like patterns. */
tirith_policy_result_t tirith_policy_eval_paths(const tirith_policy_t *policy,
                                                const char *command);

/* Evaluate against NETWORK rules: scans input for URL-like patterns. */
tirith_policy_result_t tirith_policy_eval_network(const tirith_policy_t *policy,
                                                  const char *command);

/* Evaluate against COMMAND rules: matches entire command string. */
tirith_policy_result_t tirith_policy_eval_command(const tirith_policy_t *policy,
                                                  const char *command);

/* Evaluate against ENV_VAR rules: scans for env var usage. */
tirith_policy_result_t tirith_policy_eval_env(const tirith_policy_t *policy,
                                              const char *command);

/* Full policy eval: runs all type-specific evaluations, returns worst verdict.
 * Priority: DENY > WARN > ALLOW > no match (ALLOW). */
tirith_policy_result_t tirith_policy_eval_all(const tirith_policy_t *policy,
                                              const char *command);

/* Load policies from YAML config array.
 * yaml_text is YAML fragment with policy definitions.
 * Returns number of rules loaded, or -1 on error. */
int tirith_policy_load_yaml(tirith_policy_t *policy, const char *yaml_text, size_t yaml_len);

/* Load default security policies (sensible built-in rules).
 * Returns number of rules loaded. */
int tirith_policy_load_defaults(tirith_policy_t *policy);

/* Get the global policy instance (initialized at startup).
 * Returns pointer to static global policy. */
tirith_policy_t *tirith_policy_global(void);

/* Initialize the global policy from config (defaults + custom YAML).
 * Called during CLI init. Returns pointer to global policy. */
tirith_policy_t *tirith_policy_global_init(const security_config_t *sec_cfg);

/* ================================================================
 *  Existing Basic API (unchanged)
 * ================================================================ */

/* Run tirith binary on command. Returns verdict. */
tirith_verdict_t tirith_scan(const char *command);

/* Inline check: pipe-to-interpreter patterns (| sh, | bash, etc.) */
bool tirith_has_pipe_to_interpreter(const char *command);

/* Inline check: homograph/mixed-script URLs in command */
bool tirith_has_suspicious_url(const char *command);

/* Inline check: backtick or $() command substitution */
bool tirith_has_command_substitution(const char *command);

/* Inline check: environment variable expansion in shell args */
bool tirith_has_env_injection(const char *command);

/* Full inline security scan (all above checks). Returns TIRITH_BLOCK if any fail. */
tirith_verdict_t tirith_inline_scan(const char *command);

/* Set tirith binary path. NULL = use default ("tirith" on PATH). */
void tirith_set_path(const char *path);

/* Enable/disable tirith binary scanning. Inline checks always run. */
void tirith_set_enabled(bool enabled);

/* Set fail-open mode: on binary failure, allow instead of block. */
void tirith_set_fail_open(bool fail_open);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TIRITH_H */
