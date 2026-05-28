#ifndef HERMES_WEBSITE_POLICY_H
#define HERMES_WEBSITE_POLICY_H

/*
 * website_policy.h — Website access policy checking for Hermes C.
 * Port of Python tools/website_policy.py.
 *
 * Checks URLs against a user-managed website blocklist with
 * glob/wildcard pattern support (fnmatch).
 *
 * Security feature: prevents web/browser tools from accessing
 * blocked domains configured in policy.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/** Maximum number of blocklist rules */
#define WEBSITE_MAX_RULES 1024

/** A single blocklist rule with source tracking */
typedef struct {
    char *pattern;   /**< Glob pattern (e.g. "*.example.com") */
    char *source;    /**< Source description ("config" or file path) */
} website_rule_t;

/** Parsed website blocklist policy */
typedef struct {
    bool enabled;         /**< Whether the blocklist is active */
    website_rule_t rules[WEBSITE_MAX_RULES];
    int rule_count;
} website_policy_t;

/** Block result returned when a URL is denied */
typedef struct {
    char *url;      /**< The original URL */
    char *host;     /**< Extracted hostname */
    char *rule;     /**< Matching rule pattern */
    char *source;   /**< Rule source */
    char *message;  /**< Human-readable block message */
} website_block_t;

/**
 * Initialize a policy struct with defaults.
 * @param policy  Pointer to policy struct to initialize.
 */
void website_policy_init(website_policy_t *policy);

/**
 * Free resources held by a policy struct.
 * @param policy  Pointer to policy struct to clean up.
 */
void website_policy_free(website_policy_t *policy);

/**
 * Add a blocklist rule to the policy.
 * Rules are glob patterns matching hostnames.
 * @param policy   Pointer to policy struct.
 * @param pattern  Glob pattern (e.g. "*.malware.com", "badhost.org").
 * @param source   Source description ("config" or file path). May be NULL.
 * @return 0 on success, -1 if the rules array is full.
 */
int website_add_rule(website_policy_t *policy,
                     const char *pattern, const char *source);

/**
 * Load blocklist rules from a text file (one pattern per line).
 * Lines starting with '#' are comments. Empty lines are skipped.
 * @param policy  Pointer to policy struct.
 * @param path    Path to blocklist file.
 * @return Number of rules loaded, or -1 on error.
 */
int website_load_rules_from_file(website_policy_t *policy, const char *path);

/**
 * Extract hostname from a URL.
 * @param url  The URL string.
 * @return  Allocated hostname string (caller must free), or NULL on failure.
 */
char *website_extract_host(const char *url);

/**
 * Check whether a host matches a rule pattern (supports fnmatch glob).
 * E.g. "*.example.com" matches "sub.example.com" and "example.com".
 * E.g. "badhost.org" matches "badhost.org" and "sub.badhost.org".
 * @param host     The hostname to check (lowercase).
 * @param pattern  The rule pattern.
 * @return true if the host matches the pattern.
 */
bool website_match_host(const char *host, const char *pattern);

/**
 * Check whether a URL is allowed by the policy.
 * @param url     The URL to check.
 * @param policy  Pointer to policy struct.
 * @return  NULL if access is allowed, or a website_block_t with block metadata
 *          (caller must free with website_block_free()).
 */
website_block_t *website_check_access(const char *url,
                                       const website_policy_t *policy);

/**
 * Free a block result returned by website_check_access().
 * @param block  Pointer to block result to free.
 */
void website_block_free(website_block_t *block);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEBSITE_POLICY_H */
