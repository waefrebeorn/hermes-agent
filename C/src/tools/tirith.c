/*
 * tirith.c — Pre-exec security scanning.
 * Phase 116-120: Tirith integration — binary + inline checks.
 * Detects: homograph URLs, pipe-to-interpreter, terminal injection, env leaks.
 */

#include "hermes.h"
#include "hermes_tirith.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fnmatch.h>

/* ================================================================
 *  Configuration
 * ================================================================ */

static char g_tirith_path[1024] = {0}; /* Empty = use "tirith" from PATH */
static bool g_tirith_enabled = true;
static bool g_tirith_fail_open = true; /* Fail open = on error, allow */

void tirith_set_path(const char *path) {
    if (path)
        snprintf(g_tirith_path, sizeof(g_tirith_path), "%s", path);
    else
        g_tirith_path[0] = '\0';
}

void tirith_set_enabled(bool enabled) {
    g_tirith_enabled = enabled;
}

void tirith_set_fail_open(bool fail_open) {
    g_tirith_fail_open = fail_open;
}

/* ================================================================
 *  Inline Checks (always run, no external binary needed)
 * ================================================================ */

/* Suspicious pipe targets: things that pipe to an interpreter */
static const char *PIPE_INTERPRETERS[] = {
    "| sh", "| bash", "| zsh", "| dash", "| fish",
    "| python", "| python3", "| perl", "| ruby",
    "| php", "| lua", "| node",
    "| base64 -d | sh", "| base64 -d | bash",
    "| base64 --decode | sh",
    "| eval", "$(curl", "$(wget",
    NULL
};

/* URL schemes that should not appear in shell commands normally */
static const char *SUSPICIOUS_URL_PATTERNS[] = {
    "http://", "https://", "ftp://",
    "bit.ly/", "tinyurl.com/", "shorturl.at/",
    NULL
};

bool tirith_has_pipe_to_interpreter(const char *command) {
    if (!command) return false;
    char lower[4096];
    size_t i;
    for (i = 0; command[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)tolower((unsigned char)command[i]);
    lower[i] = '\0';

    for (int j = 0; PIPE_INTERPRETERS[j]; j++) {
        if (strstr(lower, PIPE_INTERPRETERS[j]))
            return true;
    }
    return false;
}

bool tirith_has_command_substitution(const char *command) {
    if (!command) return false;
    /* Check for backticks */
    if (strchr(command, '`')) return true;
    /* Check for $() */
    if (strstr(command, "$(")) return true;
    return false;
}

bool tirith_has_env_injection(const char *command) {
    if (!command) return false;
    /* Check for suspicious env var patterns in args */
    if (strstr(command, "${") &&
        (strstr(command, "LD_PRELOAD") ||
         strstr(command, "LD_LIBRARY_PATH") ||
         strstr(command, "PATH=") ||
         strstr(command, "IFS=") ||
         strstr(command, "LD_AUDIT")))
        return true;
    return false;
}

/* Simple check: does text contain ASCII + non-ASCII mixed script?
 * This catches some homograph attacks but is not comprehensive. */
bool tirith_has_suspicious_url(const char *command) {
    if (!command) return false;

    /* Find URLs in command */
    const char *p = command;
    while ((p = strstr(p, "http")) != NULL) {
        /* Check for non-ASCII characters in the URL */
        const char *url_start = p;
        const char *url_end = url_start;
        while (*url_end && !isspace((unsigned char)*url_end) &&
               *url_end != '\'' && *url_end != '"' && *url_end != '`')
            url_end++;

        /* Scan URL for suspicious encoding */
        for (const char *c = url_start; c < url_end; c++) {
            if ((unsigned char)*c > 127) {
                /* Non-ASCII in URL could be homograph attack */
                return true;
            }
        }

        /* Check URL with known shortening services */
        for (int j = 0; SUSPICIOUS_URL_PATTERNS[j]; j++) {
            size_t plen = strlen(SUSPICIOUS_URL_PATTERNS[j]);
            if (strncmp(url_start, SUSPICIOUS_URL_PATTERNS[j], plen) == 0 &&
                strstr(url_start, "|") != NULL) {
                /* URL piped to something — suspicious */
                return true;
            }
        }

        p = url_end;
    }

    return false;
}

/* S02: Port scan detection — flag commands that look like network scanning */
static bool tirith_has_port_scan(const char *command) {
    if (!command) return false;

    /* Known port scanning tools */
    const char *scan_tools[] = {
        "nmap ", "masscan ", "zmap ", "hping3 ", "hping ",
        "unicornscan ", "zenmap "
    };
    for (size_t i = 0; i < sizeof(scan_tools)/sizeof(scan_tools[0]); i++) {
        if (strstr(command, scan_tools[i])) return true;
    }

    /* Bash /dev/tcp or /dev/udp pseudo-device (used for port probes) */
    if (strstr(command, "/dev/tcp/") || strstr(command, "/dev/udp/"))
        return true;

    /* netcat / nc with verbose scan flags (-zv = port scan mode) */
    const char *nc_patterns[] = {
        "nc -z", "nc -zv", "netcat -z", "ncat -z",
        "nc -v -z", "nc -w1 -z"
    };
    for (size_t i = 0; i < sizeof(nc_patterns)/sizeof(nc_patterns[0]); i++) {
        if (strstr(command, nc_patterns[i])) return true;
    }

    /* Sequential port range in curl/wget (mass connection attempts) */
    /* Matches patterns like :80-100 or ports=1-1024 */
    if (strstr(command, ":1-") || strstr(command, ":0-") ||
        strstr(command, "ports=1-") || strstr(command, "portrange"))
        return true;

    return false;
}

tirith_verdict_t tirith_inline_scan(const char *command) {
    if (!command) return TIRITH_ALLOW;

    if (tirith_has_pipe_to_interpreter(command))
        return TIRITH_BLOCK;

    if (tirith_has_suspicious_url(command))
        return TIRITH_BLOCK;

    if (tirith_has_env_injection(command))
        return TIRITH_BLOCK;

    if (tirith_has_port_scan(command))
        return TIRITH_BLOCK;

    /* Command substitution is common in legitimate use, just warn */
    if (tirith_has_command_substitution(command))
        return TIRITH_WARN;

    return TIRITH_ALLOW;
}

/* ================================================================
 *  External Binary Check
 * ================================================================ */

tirith_verdict_t tirith_scan(const char *command) {
    /* Always run inline checks first */
    tirith_verdict_t inline_v = tirith_inline_scan(command);
    if (inline_v == TIRITH_BLOCK)
        return TIRITH_BLOCK;

    /* Skip external binary if disabled */
    if (!g_tirith_enabled)
        return inline_v;

    /* Build command: tirith [path] scan <command> */
    const char *binary = g_tirith_path[0] ? g_tirith_path : "tirith";

    /* Check if binary exists */
    if (access(binary, X_OK) != 0) {
        /* Binary not found — if fail-open, allow. If fail-closed, block. */
        if (g_tirith_fail_open)
            return inline_v;
        return TIRITH_ERROR;
    }

    /* Execute tirith via popen */
    char cmd[8192];
    int r = snprintf(cmd, sizeof(cmd), "%s scan 2>/dev/null", binary);
    if (r < 0 || (size_t)r >= sizeof(cmd)) {
        return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
    }

    /* Read output */
    char output[4096];
    output[0] = '\0';
    if (fgets(output, (int)sizeof(output) - 1, fp)) {
        /* Remove newline */
        size_t olen = strlen(output);
        while (olen > 0 && (output[olen-1] == '\n' || output[olen-1] == '\r'))
            output[--olen] = '\0';
    }

    int status = pclose(fp);

    /* Interpret exit code */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        switch (exit_code) {
            case 0:  return TIRITH_ALLOW;
            case 1:  return TIRITH_BLOCK;
            case 2:  return TIRITH_WARN;
            default: return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
        }
    }

    return g_tirith_fail_open ? inline_v : TIRITH_ERROR;
}

/* ================================================================
 *  O13: Policy Rule Engine
 * ================================================================ */

/* O13: Static global policy instance for runtime use */
static tirith_policy_t g_tirith_policy;

void tirith_policy_init(tirith_policy_t *policy) {
    if (!policy) return;
    memset(policy, 0, sizeof(*policy));
}

bool tirith_policy_add_rule(tirith_policy_t *policy,
                            const char *name,
                            tirith_rule_type_t type,
                            tirith_rule_action_t action,
                            const char *pattern,
                            bool is_regex) {
    if (!policy || !name || !pattern) return false;
    if (policy->count >= TIRITH_MAX_RULES) return false;

    tirith_rule_t *r = &policy->rules[policy->count];
    memset(r, 0, sizeof(*r));

    snprintf(r->name, sizeof(r->name), "%s", name);
    r->type = type;
    r->action = action;
    snprintf(r->pattern, sizeof(r->pattern), "%s", pattern);
    r->is_regex = is_regex;

    policy->count++;
    return true;
}

bool tirith_policy_remove_rule(tirith_policy_t *policy, int index) {
    if (!policy || index < 0 || index >= policy->count) return false;
    /* Shift remaining rules left */
    if (index < policy->count - 1) {
        memmove(&policy->rules[index], &policy->rules[index + 1],
                (size_t)(policy->count - index - 1) * sizeof(tirith_rule_t));
    }
    policy->count--;
    return true;
}

void tirith_policy_clear(tirith_policy_t *policy) {
    if (!policy) return;
    memset(policy, 0, sizeof(*policy));
}

const tirith_rule_t *tirith_policy_get_rule(const tirith_policy_t *policy, int index) {
    if (!policy || index < 0 || index >= policy->count) return NULL;
    return &policy->rules[index];
}

/* Collect path-like tokens from a command string into a static buffer.
 * Returns number of paths found. Max TIRITH_MAX_RULES paths. */
static int extract_path_tokens(const char *command, const char **paths, int max_paths) {
    if (!command) return 0;
    int count = 0;
    const char *p = command;

    while (*p && count < max_paths) {
        /* Skip non-path characters */
        while (*p && *p != '/' && *p != '.' && *p != '~')
            p++;

        if (!*p) break;

        /* Found a candidate start — collect to whitespace/quote/end */
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '\'' && *p != '"' && *p != '`')
            p++;

        if (p - start > 2) { /* At least 2 chars to count as a path */
            paths[count] = start;
            count++;
        }
    }

    return count;
}

/* Collect URL-like tokens from a command string. */
static int extract_url_tokens(const char *command, const char **urls, int max_urls) {
    if (!command) return 0;
    int count = 0;
    const char *p = command;

    while (*p && count < max_urls) {
        /* Look for http:// or https:// */
        const char *proto = strstr(p, "http://");
        const char *protos = strstr(p, "https://");
        const char *chosen = NULL;

        if (proto && protos)
            chosen = (proto < protos) ? proto : protos;
        else if (proto)
            chosen = proto;
        else if (protos)
            chosen = protos;

        if (!chosen) break;

        const char *start = chosen;
        const char *end = start;
        while (*end && !isspace((unsigned char)*end) && *end != '\'' && *end != '"' && *end != '`' && *end != '|' && *end != ';')
            end++;

        if (end - start > 8) { /* "https://" + at least 1 char */
            urls[count] = start;
            count++;
        }

        p = end;
    }

    return count;
}

/* Collect env var patterns from a command string. */
static int extract_env_tokens(const char *command, const char **vars, int max_vars) {
    if (!command) return 0;
    int count = 0;
    const char *p = command;

    while (*p && count < max_vars) {
        /* Look for $VAR, ${VAR} patterns */
        p = strchr(p, '$');
        if (!p) break;

        const char *start = p;
        /* Skip past $ */
        p++;

        if (*p == '{') {
            /* ${VAR} */
            p++; /* skip { */
            while (*p && *p != '}' && !isspace((unsigned char)*p))
                p++;
            if (*p == '}') {
                vars[count] = start;
                count++;
                p++;
            }
        } else if (isalnum((unsigned char)*p) || *p == '_') {
            /* $VAR */
            vars[count] = start;
            count++;
            while (isalnum((unsigned char)*p) || *p == '_')
                p++;
        } else {
            p++; /* skip single char */
        }
    }

    return count;
}

/* Check if a string matches a glob pattern using fnmatch.
 * Returns true if match. */
static bool glob_match(const char *pattern, const char *input) {
    if (!pattern || !input) return false;
    return fnmatch(pattern, input, 0) == 0;
}

/* Match a single input token against a rule pattern.
 * Returns true if the token matches the rule pattern. */
static bool rule_matches(const tirith_rule_t *rule, const char *input) {
    if (!rule || !input) return false;

    if (rule->is_regex) {
        /* Future: plug in regex engine. For now fallback to suffix match */
        return strstr(input, rule->pattern) != NULL;
    }

    return glob_match(rule->pattern, input);
}

tirith_policy_result_t tirith_policy_eval(const tirith_policy_t *policy,
                                          tirith_rule_type_t type,
                                          const char *input) {
    tirith_policy_result_t result;
    memset(&result, 0, sizeof(result));
    result.verdict = TIRITH_ALLOW;

    if (!policy || !input) return result;

    bool has_warn = false;
    const tirith_rule_t *deny_rule = NULL;
    const tirith_rule_t *allow_rule = NULL;

    /* If input matches both a DENY and ALLOW rule, ALLOW wins.
     * We evaluate all rules first, then decide. */
    for (int i = 0; i < policy->count; i++) {
        const tirith_rule_t *r = &policy->rules[i];
        if (r->type != type) continue;
        if (!rule_matches(r, input)) continue;

        switch (r->action) {
            case TIRITH_ACTION_DENY:
                if (!deny_rule) deny_rule = r;
                break;
            case TIRITH_ACTION_ALLOW:
                allow_rule = r;
                break;
            case TIRITH_ACTION_WARN:
                has_warn = true;
                if (!result.matched_rule) result.matched_rule = r->name;
                break;
        }
    }

    /* ALLOW overrides DENY */
    if (allow_rule) {
        result.verdict = TIRITH_ALLOW;
        result.matched_rule = allow_rule->name;
        snprintf(result.reason, sizeof(result.reason), "Allowed by policy '%s'", allow_rule->name);
        return result;
    }

    if (deny_rule) {
        result.verdict = TIRITH_BLOCK;
        result.matched_rule = deny_rule->name;
        snprintf(result.reason, sizeof(result.reason), "Blocked by policy '%s': %.100s'", deny_rule->name, deny_rule->pattern);
        return result;
    }

    if (has_warn) {
        result.verdict = TIRITH_WARN;
        snprintf(result.reason, sizeof(result.reason), "Warning by policy '%s'", result.matched_rule ? result.matched_rule : "unnamed");
        return result;
    }

    result.verdict = TIRITH_ALLOW;
    return result;
}

tirith_policy_result_t tirith_policy_eval_paths(const tirith_policy_t *policy,
                                                const char *command) {
    tirith_policy_result_t result;
    memset(&result, 0, sizeof(result));
    result.verdict = TIRITH_ALLOW;

    if (!policy || !command) return result;

    const char *paths[32];
    int npaths = extract_path_tokens(command, paths, 32);

    for (int i = 0; i < npaths; i++) {
        /* Extract the path token into a temp buffer */
        const char *start = paths[i];
        const char *end = start;
        while (*end && !isspace((unsigned char)*end) && *end != '\'' && *end != '"' && *end != '`' && *end != '|' && *end != ';')
            end++;
        size_t len = (size_t)(end - start);
        if (len > 0) {
            char buf[256];
            size_t copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, start, copy_len);
            buf[copy_len] = '\0';

            tirith_policy_result_t r = tirith_policy_eval(policy, TIRITH_RULE_FILE_PATH, buf);
            if (r.verdict != TIRITH_ALLOW) {
                result = r;
                return result;
            }
        }
    }
    return result;
}

tirith_policy_result_t tirith_policy_eval_network(const tirith_policy_t *policy,
                                                  const char *command) {
    tirith_policy_result_t result;
    memset(&result, 0, sizeof(result));
    result.verdict = TIRITH_ALLOW;

    if (!policy || !command) return result;

    const char *urls[16];
    int nurls = extract_url_tokens(command, urls, 16);

    for (int i = 0; i < nurls; i++) {
        const char *start = urls[i];
        const char *end = start;
        while (*end && !isspace((unsigned char)*end) && *end != '\'' && *end != '"' && *end != '`' && *end != '|' && *end != ';')
            end++;
        size_t len = (size_t)(end - start);
        if (len > 0) {
            char buf[512];
            size_t copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, start, copy_len);
            buf[copy_len] = '\0';

            tirith_policy_result_t r = tirith_policy_eval(policy, TIRITH_RULE_NETWORK, buf);
            if (r.verdict != TIRITH_ALLOW) {
                result = r;
                return result;
            }
        }
    }
    return result;
}

tirith_policy_result_t tirith_policy_eval_command(const tirith_policy_t *policy,
                                                  const char *command) {
    if (!policy || !command) {
        tirith_policy_result_t r;
        memset(&r, 0, sizeof(r));
        r.verdict = TIRITH_ALLOW;
        return r;
    }
    return tirith_policy_eval(policy, TIRITH_RULE_COMMAND, command);
}

tirith_policy_result_t tirith_policy_eval_env(const tirith_policy_t *policy,
                                              const char *command) {
    tirith_policy_result_t result;
    memset(&result, 0, sizeof(result));
    result.verdict = TIRITH_ALLOW;

    if (!policy || !command) return result;

    const char *vars[16];
    int nvars = extract_env_tokens(command, vars, 16);

    for (int i = 0; i < nvars; i++) {
        const char *start = vars[i];
        const char *end = start + 1;
        if (*end == '{') {
            end++;
            while (*end && *end != '}' && !isspace((unsigned char)*end))
                end++;
            if (*end == '}') end++;
        } else {
            while (isalnum((unsigned char)*end) || *end == '_')
                end++;
        }
        size_t len = (size_t)(end - start);
        if (len > 0) {
            char buf[128];
            size_t copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, start, copy_len);
            buf[copy_len] = '\0';

            tirith_policy_result_t r = tirith_policy_eval(policy, TIRITH_RULE_ENV_VAR, buf);
            if (r.verdict != TIRITH_ALLOW) {
                result = r;
                return result;
            }
        }
    }
    return result;
}

tirith_policy_result_t tirith_policy_eval_all(const tirith_policy_t *policy,
                                              const char *command) {
    tirith_policy_result_t result;
    memset(&result, 0, sizeof(result));
    result.verdict = TIRITH_ALLOW;

    if (!policy || !command) return result;

    /* Check each category. DENY from any category blocks. WARN from any is collected. */
    tirith_policy_result_t r;

    r = tirith_policy_eval_paths(policy, command);
    if (r.verdict == TIRITH_BLOCK) return r;
    if (r.verdict == TIRITH_WARN) { result = r; }

    r = tirith_policy_eval_network(policy, command);
    if (r.verdict == TIRITH_BLOCK) return r;
    if (r.verdict == TIRITH_WARN && result.verdict == TIRITH_ALLOW) { result = r; }

    r = tirith_policy_eval_command(policy, command);
    if (r.verdict == TIRITH_BLOCK) return r;
    if (r.verdict == TIRITH_WARN && result.verdict == TIRITH_ALLOW) { result = r; }

    r = tirith_policy_eval_env(policy, command);
    if (r.verdict == TIRITH_BLOCK) return r;
    if (r.verdict == TIRITH_WARN && result.verdict == TIRITH_ALLOW) { result = r; }

    return result;
}

/* Parse a YAML-style policy definition string.
 * Format (one rule per line):
 *   - name: "rule-name"
 *     type: file_path|network|command|env_var
 *     action: deny|allow|warn
 *     pattern: "/etc/passwd"
 *     regex: false
 *
 * Simple line-based parser — no external YAML dependency needed. */
int tirith_policy_load_yaml(tirith_policy_t *policy, const char *yaml_text, size_t yaml_len) {
    if (!policy || !yaml_text) return -1;

    int loaded = 0;
    const char *p = yaml_text;
    const char *end = yaml_text + yaml_len;

    /* State for current rule being parsed */
    char cur_name[TIRITH_RULE_NAME_MAX] = {0};
    tirith_rule_type_t cur_type = TIRITH_RULE_FILE_PATH;
    tirith_rule_action_t cur_action = TIRITH_ACTION_DENY;
    char cur_pattern[TIRITH_RULE_PATTERN_MAX] = {0};
    bool cur_regex = false;
    bool in_rule = false;
    bool name_set = false, type_set = false, action_set = false, pattern_set = false;

    while (p < end) {
        /* Skip leading whitespace/newlines */
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;

        /* Check for rule start marker "- name:" or "name:" */
        if ((p[0] == '-' && p[1] == ' ') || (p[0] == ' ' && p[1] == ' ')) {
            /* Commit previous rule if we have one */
            if (in_rule && name_set && pattern_set) {
                tirith_policy_add_rule(policy, cur_name, cur_type, cur_action, cur_pattern, cur_regex);
                loaded++;
            }

            /* Reset for new rule */
            cur_name[0] = cur_pattern[0] = '\0';
            cur_type = TIRITH_RULE_FILE_PATH;
            cur_action = TIRITH_ACTION_DENY;
            cur_regex = false;
            name_set = type_set = action_set = pattern_set = false;
            in_rule = true;

            /* Skip past "- " */
            if (*p == '-') p += 2;
            else p += 2;
        }

        /* Read key-value line */
        const char *line_start = p;
        while (p < end && *p != '\n') p++;

        size_t line_len = (size_t)(p - line_start);
        if (line_len > 0 && line_len < 512) {
            char line[512];
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';

            /* Trim leading spaces */
            char *trimmed = line;
            while (*trimmed == ' ') trimmed++;

            /* Parse key: "value" or key: value */
            char key[128] = {0}, val[384] = {0};
            if (sscanf(trimmed, "%127[^:]: %383[^\n]", key, val) == 2) {
                /* Remove quotes from value */
                size_t vlen = strlen(val);
                if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') {
                    val[vlen-1] = '\0';
                    memmove(val, val + 1, vlen - 1);
                }

                if (strcmp(key, "name") == 0) {
                    snprintf(cur_name, sizeof(cur_name), "%s", val);
                    name_set = true;
                } else if (strcmp(key, "type") == 0) {
                    if (strcmp(val, "file_path") == 0) cur_type = TIRITH_RULE_FILE_PATH;
                    else if (strcmp(val, "network") == 0) cur_type = TIRITH_RULE_NETWORK;
                    else if (strcmp(val, "command") == 0) cur_type = TIRITH_RULE_COMMAND;
                    else if (strcmp(val, "env_var") == 0) cur_type = TIRITH_RULE_ENV_VAR;
                    type_set = true;
                } else if (strcmp(key, "action") == 0) {
                    if (strcmp(val, "deny") == 0) cur_action = TIRITH_ACTION_DENY;
                    else if (strcmp(val, "allow") == 0) cur_action = TIRITH_ACTION_ALLOW;
                    else if (strcmp(val, "warn") == 0) cur_action = TIRITH_ACTION_WARN;
                    action_set = true;
                } else if (strcmp(key, "pattern") == 0) {
                    snprintf(cur_pattern, sizeof(cur_pattern), "%s", val);
                    pattern_set = true;
                } else if (strcmp(key, "regex") == 0) {
                    cur_regex = (strcmp(val, "true") == 0 || strcmp(val, "True") == 0 || strcmp(val, "yes") == 0);
                }
            }
        }

        if (p < end) p++; /* skip newline */
    }

    /* Commit last rule */
    if (in_rule && name_set && pattern_set) {
        tirith_policy_add_rule(policy, cur_name, cur_type, cur_action, cur_pattern, cur_regex);
        loaded++;
    }

    return loaded;
}

int tirith_policy_load_defaults(tirith_policy_t *policy) {
    if (!policy) return -1;

    const char *default_policies =
        "- name: block-etc-passwd\n"
        "  type: file_path\n"
        "  action: deny\n"
        "  pattern: \"/etc/passwd*\"\n"
        "- name: block-etc-shadow\n"
        "  type: file_path\n"
        "  action: deny\n"
        "  pattern: \"/etc/shadow*\"\n"
        "- name: block-ssh-keys\n"
        "  type: file_path\n"
        "  action: deny\n"
        "  pattern: \"/home/*/.ssh/*\"\n"
        "- name: block-ssh-config\n"
        "  type: file_path\n"
        "  action: deny\n"
        "  pattern: \"~/.ssh/*\"\n"
        "- name: block-root-ssh\n"
        "  type: file_path\n"
        "  action: deny\n"
        "  pattern: \"/root/.ssh/*\"\n"
        "- name: block-localhost-curl\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"http://127.0.0.1*\"\n"
        "- name: block-localhost-wget\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"https://127.0.0.1*\"\n"
        "- name: block-localhost-curl-local\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"http://localhost*\"\n"
        "- name: block-internal-10\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"http://10.*\"\n"
        "- name: block-internal-172\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"http://172.*\"\n"
        "- name: block-internal-192\n"
        "  type: network\n"
        "  action: deny\n"
        "  pattern: \"http://192.168.*\"\n"
        "- name: warn-env-path\n"
        "  type: env_var\n"
        "  action: warn\n"
        "  pattern: \"$PATH\"\n"
        "- name: warn-env-ld\n"
        "  type: env_var\n"
        "  action: warn\n"
        "  pattern: \"$LD_*\"\n"
        "- name: warn-env-home\n"
        "  type: env_var\n"
        "  action: warn\n"
        "  pattern: \"$HOME\"\n"
        "- name: block-curls-to-internal\n"
        "  type: command\n"
        "  action: deny\n"
        "  pattern: \"curl *@*\"\n";

    return tirith_policy_load_yaml(policy, default_policies, strlen(default_policies));
}

tirith_policy_t *tirith_policy_global(void) {
    return &g_tirith_policy;
}

tirith_policy_t *tirith_policy_global_init(const security_config_t *sec_cfg) {
    tirith_policy_init(&g_tirith_policy);
    tirith_policy_load_defaults(&g_tirith_policy);
    if (sec_cfg && sec_cfg->tirith_policy_text[0]) {
        tirith_policy_load_yaml(&g_tirith_policy,
                                sec_cfg->tirith_policy_text,
                                strlen(sec_cfg->tirith_policy_text));
    }
    return &g_tirith_policy;
}
