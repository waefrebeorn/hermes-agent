#ifndef HERMES_H
#define HERMES_H

/*
 * hermes.h — Master header for WuBu Hermes C implementation.
 * Defines core types, config, and forward declarations.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for agent subsystem types */
typedef struct budget_tracker_t budget_tracker_t;

/* ================================================================
 *  Version
 * ================================================================ */
#define HERMES_VERSION_MAJOR 0
#define HERMES_VERSION_MINOR 14
#define HERMES_VERSION_PATCH 1
#define HERMES_VERSION "0.14.1-wubu"

/* Config file format version — incremented when struct layout changes
 * and migration is required. Stored as "config_version" in YAML. */
#define HERMES_CONFIG_VERSION      1
#define HERMES_CONFIG_VERSION_KEY  "config_version"

/* ================================================================
 *  Core Constants
 * ================================================================ */
#define HERMES_MAX_TOOL_CALLS    90
#define HERMES_MAX_MESSAGES      4096
#define HERMES_MAX_TOOLS         256
#define HERMES_PATH_MAX          4096
#define HERMES_LINE_MAX          65536
#define HERMES_MAX_CTX_TOKENS    131072

/* ================================================================
 *  Message Types
 * ================================================================ */

/* Tool call metadata (must be before message_t which uses it) */
typedef struct {
    char  id[64];
    char  name[128];
    char  arguments[4096]; /* JSON args string */
} tool_call_t;

typedef enum {
    MSG_SYSTEM,
    MSG_USER,
    MSG_ASSISTANT,
    MSG_TOOL,
} message_role_t;

typedef struct {
    message_role_t  role;
    char           *content;     /* text content */
    char           *tool_call_id; /* for tool results */
    char           *tool_name;    /* for tool calls (single) */
    char           *reasoning;   /* reasoning content if any */
    int             tool_calls_count;
    tool_call_t     tool_calls[64];
} message_t;

/* ================================================================
 *  Tool System
 * ================================================================ */
typedef struct {
    char   name[64];
    char   description[512];
    char   schema_json[4096];
    char *(*handler)(const char *args_json, const char *task_id);
    bool  available;
    int   timeout_sec;   /* P52: per-tool timeout. 0 = inherit default. -1 = no timeout. */
} tool_t;

typedef struct {
    tool_t *tools;
    size_t  count;
    size_t  capacity;
} tool_registry_t;

/* ================================================================
 *  LLM Provider
 * ================================================================ */
typedef struct {
    char  base_url[256];
    char  api_key[256];
    char  model[128];
    char  provider[64];
    bool  system_cached; /* P91: system prompt cache primed */
} llm_config_t;

/* P95: Stream diagnostic — token-level timing and latency breakdown */
typedef struct {
    double  time_to_first_token;   /* seconds from request to first token */
    double  total_stream_time;     /* seconds from first to last token */
    double  tokens_per_second;     /* average throughput during stream */
    size_t  total_tokens;          /* total tokens received in stream */
    bool    first_token_received;  /* set when first content token arrives */
    /* Internal: timing markers */
    double  request_start_time;    /* monotonic time when request was sent */
    double  first_token_time;      /* monotonic time when first token arrived */
    double  stream_end_time;       /* monotonic time when stream finished */
} stream_diag_t;

/* LLM response */
typedef struct {
    char         *content;
    char         *reasoning;
    int           input_tokens;
    int           output_tokens;
    int           tool_calls_count;
    tool_call_t   tool_calls[64]; /* Max 64 tool calls per turn */
    /* P95: Stream diagnostic — populated by streaming path */
    stream_diag_t diag;
} llm_response_t;

/* Forward declaration for session database (defined in hermes_db.h) */
typedef struct db_t db_t;

/* Streaming output callback. Called with each token content during LLM response.
 * Return non-zero to abort. */
typedef int (*llm_token_cb_t)(const char *token, void *userdata);

/* P97: Compression feedback — user-rated quality, adaptive threshold */
typedef struct {
    int   total_compressions;    /* number of compression events tracked */
    int   positive_feedback;     /* user rated compression as good */
    int   negative_feedback;     /* user rated compression as bad */
    float quality_score;         /* running quality score (0-1) */
    float adapt_threshold;       /* adaptive threshold factor (0.1-0.9) */
} compression_feedback_t;

/* P98: Checkpoint types */
typedef struct {
    message_t  **messages;
    size_t       count;
    size_t       capacity;
    char         id[64];       /* timestamp-based unique ID */
    char         label[128];   /* optional user label (e.g., "before_debug") */
    time_t       created_at;
} checkpoint_t;

typedef struct {
    checkpoint_t *checkpoints;
    size_t        count;
    size_t        capacity;
    int           max_snapshots;        /* max checkpoints to keep */
    int           auto_save_interval;   /* turns between auto-saves */
    int           turn_counter;         /* turns since last auto-save */
} checkpoint_manager_t;

/* ================================================================
 *  Agent State
 * ================================================================ */
typedef struct {
    message_t       **messages;
    size_t            message_count;
    size_t            message_capacity;
    llm_config_t      llm;
    tool_registry_t   tools;
    int               max_iterations;
    int               iteration_count;
    bool              interrupted;
    char              session_id[64];
    char              hermes_home[HERMES_PATH_MAX];
    db_t             *db;           /* session database (optional) */
    llm_token_cb_t    stream_cb;   /* streaming token callback (optional) */
    void             *stream_data; /* userdata for stream callback */
    void             *plugin_reg;  /* plugin registry (optional, opaque) */
    bool              compress_enabled; /* smart context compression via LLM */
    /* P28: Undo snapshot — copy of messages before last modification */
    message_t       **snapshot;
    size_t            snapshot_count;
    size_t            snapshot_capacity;
    char              snapshot_id[64];  /* session ID at snapshot time */
    /* P86: Budget tracker — token/cost/turn budgets and limits */
    budget_tracker_t *budget;
    /* P92: Prefill message — assistant message injected before first user turn */
    char  prefill[4096];
    /* P97: Compression feedback tracker */
    compression_feedback_t compression_fb;
    /* P98: Checkpoint manager — auto-save, named checkpoints, rollback */
    checkpoint_manager_t  checkpoints;
} agent_state_t;

/* ================================================================
 *  Provider Config Sub-group (P1)
 * ================================================================ */
#define HERMES_STOP_SEQUENCES_MAX 8

typedef struct {
    char  model[128];
    char  provider[64];
    char  base_url[256];
    char  api_key[256];
    char  api_mode[32];            /* chat_completions, codex_responses, etc. */
    int   max_tokens;              /* max output tokens per response */
    float temperature;             /* sampling temperature (0.0-2.0) */
    float top_p;                   /* nucleus sampling (0.0-1.0) */
    char  stop_sequences[HERMES_STOP_SEQUENCES_MAX][128];  /* stop strings */
    int   stop_count;
    char  fallback_model[128];     /* model to fallback to on error */
    char  service_tier[32];        /* auto, default (for Anthropic) */
    char  reasoning_effort[32];    /* low, medium, high */
} provider_config_t;

/* ================================================================
 *  Display Config Sub-group (P2)
 * ================================================================ */
typedef struct {
    char  skin[64];               /* display.skin: theme name */
    char  banner[128];            /* display.banner: custom banner text */
    char  spinner_style[32];      /* display.spinner: kawaii/dots/classic */
    bool  stream;                 /* display.streaming: token streaming */
    bool  show_reasoning;         /* display.show_reasoning: show reasoning content */
    char  indicator[32];          /* display.indicator: kaomoji/dots/minimal */
    bool  statusbar;              /* display.statusbar: show status bar */
    char  footer[128];            /* display.footer: custom footer text */
    bool  compact;                /* display.compact: compact mode */
    char  personality[1024];      /* display.personality: system prompt override */
    char  language[16];           /* display.language: UI language */
    bool  show_cost;              /* display.show_cost: show token cost */
    bool  timestamps;             /* display.timestamps: show message timestamps */
} display_config_t;

/* ================================================================
 *  Agent Config Sub-group (P3)
 * ================================================================ */
typedef struct {
    int   max_iterations;          /* agent.max_turns: max tool-calling turns */
    int   max_tool_calls_round;    /* agent.max_tool_calls_round: per-turn limit */
    int   max_output_tokens;       /* agent.max_output_tokens: per-response tokens */
    char  system_prompt[4096];     /* agent.system_prompt: custom system prompt */
    char  profile[64];             /* agent.profile: active profile name */
    int   verbose_level;           /* agent.verbose: 0=off, 1=normal, 2=verbose */
    bool  fast_mode;               /* agent.fast: skip system prompt for speed */
    bool  quiet_mode;              /* agent.quiet_mode: suppress non-essential output */
    float compress_threshold;      /* compression.threshold: auto-compress ratio */
    char  reasoning_effort[32];    /* agent.reasoning_effort: low/medium/high */
    int   api_max_retries;         /* agent.api_max_retries: API call retries */
    int   clarify_timeout;         /* agent.clarify_timeout: seconds before auto-deny */
} agent_config_t;

/* ================================================================
 *  Tools Config Sub-group (P4)
 * ================================================================ */
typedef struct {
    bool  allow_background;        /* tools.allow_background: allow background processes */
    bool  local_process_cleanup;   /* tools.local_process_cleanup: auto-cleanup on exit */
    char  approval_mode[16];       /* approvals.mode: off/manual/auto */
    int   approval_timeout;        /* approvals.timeout: seconds */
    int   max_result_size;         /* tool_output.max_bytes: max bytes per tool result */
    int   terminal_timeout;        /* terminal.timeout: max seconds for terminal commands */
    char  vision_model[128];       /* auxiliary.vision.model: vision model name */
    int   vision_timeout;          /* auxiliary.vision.timeout: seconds */
} tools_config_t;

/* ================================================================
 *  Delegation Config (P5)
 * ================================================================ */
typedef struct {
    int   max_concurrent_children; /* delegation.max_concurrent_children */
    int   max_spawn_depth;         /* delegation.max_spawn_depth */
    int   child_timeout;           /* delegation.child_timeout_seconds */
    char  child_model[128];        /* delegation.model */
    char  child_provider[64];      /* delegation.provider */
    int   child_max_turns;         /* delegation.max_iterations */
} delegation_config_t;

/* ================================================================
 *  Browser Config (P6)
 * ================================================================ */
typedef struct {
    char  cdp_url[512];           /* browser.cdp_url */
    char  browser_type[32];       /* browser.engine: auto/chromium/firefox */
    bool  headless;                /* browser.engine auto uses headless */
    int   viewport_width;          /* browser viewport width */
    int   viewport_height;         /* browser viewport height */
    int   timeout;                /* browser.command_timeout */
    bool  enable_javascript;       /* enable JS in browser */
} browser_config_t;

/* ================================================================
 *  Memory Config (P7) — extended for P151-P158
 * ================================================================ */
typedef struct {
    char  provider[64];           /* memory.provider: memory provider backend */
    int   char_limit;             /* memory.memory_char_limit */
    int   user_char_limit;        /* memory.user_char_limit */
    int   ttl_days;               /* memory TTL (P152) */
    bool  auto_save;              /* memory auto-save (P156) */
    int   auto_save_interval;     /* memory auto-save interval in seconds (P156) */
    bool  dedup_enabled;          /* memory dedup (P154) */
    int   search_limit;           /* max search results (P155) */
    bool  compression_enabled;    /* memory compression (P158) */
    int   compression_min_entries;/* min entries before compression triggers (P158) */
    int   storage_type;           /* 0=inmem, 1=file, 2=sqlite, 3=plugin (P151) */
    char  storage_path[512];      /* path for file/sqlite storage (P151) */
} memory_config_t;

/* ================================================================
 *  Compression Config (P8)
 * ================================================================ */
typedef struct {
    char  model[128];             /* compression model override */
    char  strategy[32];           /* compression_strategy: smart/summary */
    float target_ratio;           /* compression.target_ratio */
    int   min_messages;           /* min_messages_before_compress */
    bool  preserve_system;        /* preserve_system: keep system prompt */
} compression_config_t;

/* ================================================================
 *  Cron Config (P9)
 * ================================================================ */
typedef struct {
    char  dir[HERMES_PATH_MAX];   /* cron_dir */
    int   max_concurrent_jobs;    /* max_concurrent_jobs */
    int   job_timeout;            /* job_timeout */
    int   retention_days;         /* retention_days */
    bool  notify_on_failure;      /* notify_on_failure */
} cron_config_t;

/* ================================================================
 *  Notification Config (P10)
 * ================================================================ */
typedef struct {
    char  provider[64];           /* notify_provider */
    bool  on_complete;            /* notify_on_complete */
    bool  on_error;               /* notify_on_error */
    bool  on_approval;            /* notify_on_approval */
    char  sound[64];              /* notification_sound */
} notification_config_t;

/* ================================================================
 *  Security Config (P11)
 * ================================================================ */
typedef struct {
    char  redact_patterns[1024];  /* security.redact_secrets patterns */
    char  tirith_path[512];       /* security.tirith_path */
    int   tirith_timeout;         /* security.tirith_timeout */
    bool  tirith_enabled;         /* security.tirith_enabled */
    bool  allow_private_urls;     /* security.allow_private_urls */
    bool  website_blocklist_enabled; /* security.website_blocklist.enabled */
} security_config_t;

/* ================================================================
 *  Session Config (P12)
 * ================================================================ */
typedef struct {
    char  db_path[HERMES_PATH_MAX];  /* sessions DB path */
    int   retention_days;            /* sessions.retention_days, P145: auto-prune */
    int   auto_save_interval;        /* auto-save interval in turns, P144: auto-save */
    bool  compress;                  /* session_compress */
    bool  store_trajectories;        /* session_store_trajectories */
} session_config_t;

/* ================================================================
 *  Plugin Config (P13)
 * ================================================================ */
typedef struct {
    char  dirs[1024];             /* plugin_dirs (comma-separated) */
    char  enabled[1024];          /* enabled_plugins (comma-separated) */
} plugin_config_t;

/* ================================================================
 *  MCP Auth per-server config (P63-P64)
 * ================================================================ */
typedef struct {
    char  type[16];               /* "none", "header", "env", "oauth" */
    char  header_name[128];       /* HTTP header name (default: Authorization) */
    char  token[1024];            /* static token value (API key, Bearer token) */
    char  env_var[128];           /* env var name for stdio transport */
    /* OAuth-specific (P64) */
    char  client_id[256];         /* OAuth client ID */
    char  client_secret[512];     /* OAuth client secret */
    char  token_url[1024];        /* OAuth token endpoint */
    char  scopes[1024];           /* space-separated scopes */
    int   refresh_before_sec;     /* refresh if < N seconds left on expiry */
} mcp_auth_t;

/* ================================================================
 *  MCP Config (P14)
 * ================================================================ */
typedef struct {
    int   timeout;                /* mcp_timeout */
    int   max_tools;              /* mcp_max_tools */
    bool  auth_enabled;           /* mcp_auth_enabled (global toggle) */
    char  credential_store[HERMES_PATH_MAX]; /* path to credential store JSON */
} mcp_config_t;

/* ================================================================
 *  Config
 * ================================================================ */
typedef struct {
    char  config_path[HERMES_PATH_MAX];
    char  env_path[HERMES_PATH_MAX];
    int   config_version;          /* P25: config file format version for migration */
    /* Provider settings (flat fields for backward compat) */
    char  model[128];
    char  provider[64];
    char  api_key[256];
    char  base_url[256];
    /* Full provider config sub-struct */
    provider_config_t provider_cfg;
    /* Display config */
    display_config_t display;
    /* Agent config */
    agent_config_t agent;
    /* Tools config */
    tools_config_t tools;
    /* Delegation config */
    delegation_config_t delegation;
    /* Browser config */
    browser_config_t browser_cfg;
    /* Memory config */
    memory_config_t memory;
    /* Compression config */
    compression_config_t compression;
    /* Cron config */
    cron_config_t cron;
    /* Notification config */
    notification_config_t notification;
    /* Security config */
    security_config_t security;
    /* Session config */
    session_config_t session;
    /* Plugin config */
    plugin_config_t plugin;
    /* MCP config */
    mcp_config_t mcp;
    char  skin_path[HERMES_PATH_MAX];
    char  personality[1024];   /* display.personality: system prompt override */
    char  cdp_url[512];        /* browser.cdp_url: Chrome DevTools Protocol WebSocket URL */
    int   max_turns;
    int   verbose;             /* agent.verbose: 0=off, 1=normal, 2=verbose */
    bool  quiet_mode;
    bool  yolo_mode;           /* approvals.mode=off or --yolo flag */
    bool  fast_mode;           /* agent.fast: skip system prompt for speed */
    bool  compress_enabled;    /* compression.enabled: smart context compression */
    char  gateway_platforms[256];  /* Comma-separated: "telegram,discord,webhook" */
} hermes_config_t;

bool hermes_config_load(hermes_config_t *cfg, const char *config_dir);
bool hermes_config_load_env(hermes_config_t *cfg);

/* P15: Config validation — returns true if valid, logs warnings for issues */
typedef struct {
    char key[128];
    char message[256];
} config_issue_t;

typedef struct {
    config_issue_t issues[64];
    int count;
} config_validation_t;

bool hermes_config_validate(const hermes_config_t *cfg, config_validation_t *result);

/* P17: Config profiles — load named profile from ~/.slermes/profiles/<name>.yaml */
bool hermes_config_load_profile(hermes_config_t *cfg, const char *profile_name, const char *config_dir);

/* P18: Config display utilities */
typedef enum { CFG_DIFF_ADDED, CFG_DIFF_CHANGED, CFG_DIFF_MISSING } cfg_diff_type_t;
typedef struct {
    char key[128];
    cfg_diff_type_t type;
    char default_value[256];
    char active_value[256];
} cfg_diff_entry_t;
typedef struct {
    cfg_diff_entry_t entries[128];
    int count;
} cfg_diff_t;

/* Get default config (factory settings) */
void hermes_config_defaults(hermes_config_t *cfg);
/* Diff active config vs defaults, returns entries grouped by section */
bool hermes_config_diff(const hermes_config_t *active, cfg_diff_t *diff);

/* P20: Config import/export */
bool hermes_config_export(const hermes_config_t *cfg, const char *path);
bool hermes_config_import(hermes_config_t *cfg, const char *path);

/* P24: Config schema generation — returns malloc'd JSON string, caller must free */
char *hermes_config_schema(void);

/* P25: Config migration — upgrade config version, returns true if migration ran */
bool hermes_config_migrate(hermes_config_t *cfg, const char *config_dir);

/* ================================================================
 *  P21: Path resolution (hermes_constants port)
 * ================================================================ */

/* Resolve SLERMES_HOME (profile-aware). Returns path in buf. */
void hermes_get_home(char *buf, size_t sz);

/* Resolve XDG-style paths relative to SLERMES_HOME */
void hermes_config_dir(char *buf, size_t sz);    /* ~/.slermes/ */
void hermes_data_dir(char *buf, size_t sz);      /* ~/.slermes/data/ */
void hermes_cache_dir(char *buf, size_t sz);     /* ~/.slermes/cache/ */
void hermes_log_dir(char *buf, size_t sz);       /* ~/.slermes/logs/ */

/* Display resolved Hermes paths to stdout */
void hermes_display_home(void);

/* Resolve path within SLERMES_HOME: hermes_resolve_path("profiles/default.yaml") */
void hermes_resolve_path(const char *sub, char *buf, size_t sz);

/* Set active profile name (affects hermes_get_home resolution) */
void hermes_set_profile(const char *name);
const char *hermes_get_profile(void);

/* P22: Config merge — deep merge src into dst (dst values take priority for non-default fields) */
void hermes_config_merge(hermes_config_t *dst, const hermes_config_t *src);

/* ================================================================
 *  Include sub-headers for dependency wrappers
 * ================================================================ */
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include "hermes_crypto.h"
#include "hermes_db.h"
#include "hermes_display.h"
#include "hermes_skin.h"
#include "hermes_agent.h"
#include "hermes_plugin.h"
#include "hermes_memory.h"

/* ================================================================
 *  Entry Points
 * ================================================================ */
int  hermes_cli_main(int argc, char **argv);
int  hermes_gateway_main(int argc, char **argv);

/* Command system */
typedef struct command_def_t command_def_t;
bool commands_dispatch(const char *input, agent_state_t *state);
const command_def_t *commands_resolve(const char *input);
const command_def_t *commands_get_all(void);

/* Security approval */
int approval_check(const char *tool_name, const char *args_json);
void approval_reset_session(void);
void approval_set_yolo(bool enabled);  /* When true, skip all approval prompts */
void approval_set_allowlist_path(const char *path);
void approval_load_allowlist(void);
void approval_save_allowlist(void);

/* P39: Approval cache query */
int approval_cache_count(void);
const char *approval_cache_entry(int index);
void approval_cache_clear_last(int n);

/* Config runtime toggles (wired from config.yaml or /commands) */
void commands_set_verbose(int level);   /* 0=off, 1=normal, 2=verbose */
void commands_set_yolo(bool enabled);
void commands_set_fast(bool enabled);
int  commands_get_verbose(void);
bool commands_get_yolo(void);
bool commands_get_fast(void);

/* CDP browser URL config */
void cdp_set_url(const char *url);

/* Registry accessors */
size_t registry_get_count(void);
const char *registry_get_name(size_t i);

/* Find tool by name in registry. Returns NULL if not found. */
tool_t *registry_find(const char *name);

/* P52: Per-tool timeout. Set seconds, 0 = default, -1 = no timeout. */
void registry_set_timeout(const char *name, int seconds);
int  registry_get_timeout(const char *name);

/* P55: Tool wildcard matching — enable/disable all tools matching a pattern */
/* Pattern supports '*' wildcard: "discord:*", "browser_*", "*_search" */
void registry_set_available_pattern(const char *pattern, bool available);

/* Check if tool name matches a wildcard pattern. Returns true on match. */
bool registry_name_matches(const char *name, const char *pattern);

/* P49-P50: Tool result storage and output limits */
char *tool_result_store(const char *data, size_t size, size_t max_inline);
void tool_result_cleanup(int max_age_seconds);

/* ================================================================
 *  P141-P150: Session DB depth — metadata, search, CRUD, export, branching, migration
 * ================================================================ */

/* P141: Save session metadata (title, model, token count, timestamps) to sidecar file */
bool agent_save_meta(agent_state_t *state);

/* P141: Load session metadata */
bool agent_load_meta(agent_state_t *state, session_meta_t *meta);

/* P143: Session CRUD — create new session with metadata */
bool agent_session_create(agent_state_t *state, const char *session_id);

/* P143: List sessions with metadata filtering. Returns malloc'd array. Caller must free. */
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit);

/* P143: Delete a session */
bool agent_session_delete(agent_state_t *state, const char *session_id);

/* P144: Auto-save current session (checks interval config) */
bool agent_auto_save(agent_state_t *state, int interval);

/* P144: Crash recovery — check for and restore backup */
bool agent_crash_recover(agent_state_t *state);

/* P145: Auto-prune — remove old sessions based on retention_days. Returns number removed. */
int agent_auto_prune(agent_state_t *state, int retention_days);

/* P146: Add tag to session metadata */
bool agent_session_add_tag(agent_state_t *state, const char *tag);

/* P146: Remove tag from session metadata */
bool agent_session_remove_tag(agent_state_t *state, const char *tag);

/* P148: Export session as JSON string. Caller must free. */
char *agent_session_export_json(agent_state_t *state, const char *session_id);

/* P148: Export session as Markdown string. Caller must free. */
char *agent_session_export_markdown(agent_state_t *state, const char *session_id);

/* P149: Branch session — fork at message index N into new session */
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point);

/* P150: Migrate all sessions to latest schema. Returns number migrated. */
int agent_session_migrate(agent_state_t *state);

/* ================================================================
 *  P159-P168: Security phase functions
 * ================================================================ */

/* P159: Secrets redaction */
char *hermes_redact(const char *input);
bool hermes_redact_add_pattern(const char *pattern);
void hermes_redact_clear_patterns(void);
void hermes_redact_load_config(const char *patterns_str);

/* P160: Website blocklist */
void url_blocklist_enable(bool enabled);
bool url_blocklist_add_domain(const char *domain);
bool url_blocklist_remove_domain(const char *domain);
bool url_blocklist_add_category(const char *category);
bool url_blocklist_remove_category(const char *category);
void url_blocklist_clear(void);
void url_blocklist_load_config(const security_config_t *cfg);

/* P161: Command allowlist */
bool allowlist_add(const char *tool, const char *pattern);
bool allowlist_remove(const char *tool, const char *pattern);
void allowlist_clear(void);
bool allowlist_check(const char *tool, const char *command);

/* P162: Approval timeout */
void approval_set_timeout(int seconds);
int approval_get_timeout(void);

/* P163: Tirith security policy (wired via approval/tool dispatch) */

/* P164: Audit log */
bool audit_init(const char *log_dir);
void audit_shutdown(void);
void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail);
void audit_log_approval(const char *tool, const char *command, bool approved);
void audit_log_redaction(const char *context, const char *pattern_matched);
void audit_log_violation(const char *rule, const char *detail);

/* P165: Rate limiting */
bool rate_limit_init_tool(const char *tool_name, int max_per_minute);
bool rate_limit_init_provider(const char *provider_name, int max_per_minute);
bool rate_limit_check_tool(const char *tool_name);
bool rate_limit_check_provider(const char *provider_name);
int rate_limit_remaining_tool(const char *tool_name);
void rate_limit_reset_all(void);
void rate_limit_clear(void);

/* P166: Output sanitization */
char *hermes_sanitize_output(const char *tool_name, const char *raw_output);

/* P167: Credential vault */
bool vault_set_master_key(const char *passphrase);
bool vault_has_master_key(void);
void vault_lock(void);
void vault_set_path(const char *path);
bool vault_save(void);
bool vault_load(void);
bool vault_store(const char *service, const char *key, const char *value);
const char *vault_retrieve(const char *service, const char *key);
bool vault_delete(const char *service, const char *key);
int vault_list_services(char services[][128], int max_count);

/* P168: File sandbox */
void sandbox_init(void);
void sandbox_enable(bool enabled);
bool sandbox_add_allowed_dir(const char *dir);
bool sandbox_remove_allowed_dir(const char *dir);
void sandbox_clear(void);
bool sandbox_check_path(const char *path);
void sandbox_set_symlink_check(bool enabled);

/* ================================================================
 *  P169-P178: Cron/Scheduler phase functions
 * ================================================================ */

/* P169: SQLite job store */
typedef struct cron_sqlite_store_t cron_sqlite_store_t;
cron_sqlite_store_t *cron_sqlite_open(const char *path);
void cron_sqlite_close(cron_sqlite_store_t *store);
bool cron_sqlite_save_job(cron_sqlite_store_t *store, const char *name,
                           const char *schedule, const char *command,
                           bool active, int retry_count, int max_retries,
                           const char *chain_from, const char *template_name,
                           const char *script_type);
bool cron_sqlite_load_jobs(cron_sqlite_store_t *store);
bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name);
bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);

/* P170: Cron expression parser (libcron already exists — see lib/libcron/cron.h) */

/* P171: Job locking */
bool cron_lock_acquire(const char *lock_name);
void cron_lock_release(const char *lock_name);
bool cron_lock_is_locked(const char *lock_name);

/* P172: Job retry */
bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec);
int  cron_job_get_retry_count(const char *job_name);
int  cron_job_get_max_retries(const char *job_name);

/* P173: Job notification */
bool cron_notify_set_channel(const char *channel_id);
bool cron_notify_on_complete(const char *job_name, bool enabled);
bool cron_notify_on_failure(const char *job_name, bool enabled);

/* P174: Job chaining */
bool cron_chain_set_context(const char *job_name, const char *context_from);
const char *cron_chain_get_context(const char *job_name);
char *cron_chain_get_output(const char *job_name);
void cron_chain_store_output(const char *job_name, const char *output);

/* P175: Job templating */
bool cron_template_create(const char *name, const char *schedule,
                           const char *command, const char *params_json);
bool cron_template_instantiate(const char *template_name,
                                const char *params_json,
                                char *out_name, size_t out_name_sz,
                                char *out_schedule, size_t out_sched_sz,
                                char *out_command, size_t out_cmd_sz);

/* P176: Scheduler CLI */
char *cron_cmd_handler(const char *args_json, const char *task_id);

/* P177: Script-based jobs */
bool cron_script_set_interpreter(const char *job_name, const char *interpreter);
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *args, int *exit_code);

/* P178: Watchdog mode */
bool cron_watchdog_enable(void);
void cron_watchdog_disable(void);
bool cron_watchdog_is_active(void);
void cron_watchdog_ping(void);
int  cron_watchdog_check(time_t timeout_sec);

#endif /* HERMES_H */
