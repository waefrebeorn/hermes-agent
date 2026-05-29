/**
 * @file hermes_feishu_rules.h
 * @brief Feishu comment access-control rules (port of G04 feishu_comment_rules.py).
 *
 * 3-tier rule resolution: exact doc > wildcard "*" > top-level > code defaults.
 * Config: HERMES_HOME/feishu_comment_rules.json (mtime-cached, hot-reload).
 * Pairing store: HERMES_HOME/feishu_comment_pairing.json.
 *
 * @{
 */
#ifndef HERMES_FEISHU_RULES_H
#define HERMES_FEISHU_RULES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Constants
 * ================================================================ */

#define FEISHU_RULES_MAX_STR 4096
#define FEISHU_RULES_MAX_ALLOW_FROM 256
#define FEISHU_RULES_VALID_POLICIES "allowlist|pairing"

/* ================================================================
 *  Data structures
 * ================================================================ */

/** Per-document rule. enabled==-1 / policy==NULL / allow_from==NULL means
 *  "inherit from lower tier" (like Python Optional[field]). */
typedef struct {
    int         enabled;        /* -1 = inherit, 0 = disabled, 1 = enabled */
    char       *policy;         /* NULL if not set; "allowlist" or "pairing" if set */
    char      **allow_from;     /* NULL-terminated array of open_ids, or NULL if not set */
    size_t      allow_from_count;
} feishu_doc_rule_t;

/** Top-level comment access config. */
typedef struct {
    bool        enabled;
    char       *policy;         /* "allowlist" or "pairing" */
    char      **allow_from;     /* NULL-terminated array, may be empty */
    size_t      allow_from_count;
    feishu_doc_rule_t *documents;  /* array of document rules */
    size_t      doc_count;
    char      **doc_keys;       /* parallel array of doc key strings */
} feishu_rules_config_t;

/** Fully resolved rule after field-by-field fallback. */
typedef struct {
    bool        enabled;
    char       *policy;
    char      **allow_from;     /* NULL-terminated */
    size_t      allow_from_count;
    char        match_source[128]; /* e.g. "exact:docx:xxx" | "wildcard" | "top" | "default" */
} feishu_resolved_rule_t;

/* ================================================================
 *  Public API
 * ================================================================ */

/** Load comment rules from disk (mtime-cached, hot-reload).
 *  Returns NULL on error (caller must free with feishu_rules_free_config). */
feishu_rules_config_t *feishu_rules_load_config(void);

/** Free a config returned by feishu_rules_load_config. */
void feishu_rules_free_config(feishu_rules_config_t *cfg);

/** Check if any document rule key starts with "wiki:". */
bool feishu_rules_has_wiki_keys(const feishu_rules_config_t *cfg);

/** Resolve effective rule: exact doc -> wiki key -> wildcard -> top-level -> defaults.
 *  Caller must free result with feishu_rules_free_resolved. */
feishu_resolved_rule_t *feishu_rules_resolve_rule(
    const feishu_rules_config_t *cfg,
    const char *file_type,
    const char *file_token,
    const char *wiki_token);

/** Free a resolved rule. */
void feishu_rules_free_resolved(feishu_resolved_rule_t *rule);

/** Add a user to the pairing-approved list.
 *  Returns true if newly added, false if already present or on error. */
bool feishu_rules_pairing_add(const char *user_open_id);

/** Remove a user from the pairing-approved list.
 *  Returns true if removed, false if not found or on error. */
bool feishu_rules_pairing_remove(const char *user_open_id);

/** Get pairing-approved users as JSON string.
 *  Returns malloc'd string like '{"user1":{"approved_at":...},"user2":...}'.
 *  Caller must free. Returns NULL on error. */
char *feishu_rules_pairing_list(void);

/** Check if a user passes the resolved rule's policy gate.
 *  allow_from is checked first; if policy is "pairing", check pairing store. */
bool feishu_rules_is_user_allowed(const feishu_resolved_rule_t *rule,
                                   const char *user_open_id);

/** Invalidate mtime cache so next load re-reads from disk. */
void feishu_rules_invalidate_cache(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FEISHU_RULES_H */
/** @} */
