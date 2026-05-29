/**
 * @defgroup hermes Main API
 * @brief Master header for WuBu Slermes C implementation.
 *
 * Defines core types: provider_config_t, agent_state_t, message_t,
 * tool_call_t, llm_config_t, llm_response_t, gateway_config_t,
 * plugin_config_t. Includes all subsystem headers.
 *
 * @{
 */
#ifndef HERMES_H
#define HERMES_H

/*
 * hermes.h — Master header for WuBu Slermes C implementation.
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

/* Tool call loop guardrails controller */
#include "hermes_tool_guardrails.h"

/* Typed error system (K01-K05) */
#include "hermes_error.h"
#include "hermes_tokenizer.h"

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
    char           *encrypted_content; /* L07: xAI encrypted reasoning content */
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
    char   toolset[32];  /* P150: toolset group for enabled/disabled filtering (e.g. "browser", "terminal") */
} tool_t;

typedef struct {
    tool_t *tools;
    size_t  count;
    size_t  capacity;
} tool_registry_t;

/* ================================================================
 *  LLM Provider
 * ================================================================ */
#define HERMES_STOP_SEQUENCES_MAX 8

typedef struct {
    char  base_url[256];
    char  api_key[256];
    char  model[128];
    char  provider[64];
    bool  system_cached; /* P91: system prompt cache primed */
    /* LLM request params forwarded from config */
    int   max_tokens;
    float temperature;
    float top_p;
    char  stop_sequences[HERMES_STOP_SEQUENCES_MAX][128];
    int   stop_count;
    float presence_penalty;
    float frequency_penalty;
    int   seed;
    bool  logprobs;
    int   top_logprobs;
    char  user[128];
    char  service_tier[32];
    char  reasoning_effort[32];
    char  response_format[256];    /* JSON: "json_object" or {"type":"json_schema","json_schema":{...}} */
    char  metadata[256];           /* key-value map JSON string */
    char  tool_choice[32];         /* "auto", "none", "required", or JSON for specific function */
    bool  parallel_tool_calls;     /* allow parallel tool calls (default: true) */
    int   max_tool_calls;          /* max tool calls per turn (0 = unlimited) */
    int   n;                       /* number of response choices (1 = single) */
    int   top_k;                    /* B30: top-k sampling (Google, etc.); 0=unset */
    int   candidate_count;          /* B30: number of response candidates (Google); 0=unset */
    bool  json_mode;                /* B23: convenience — auto-set response_format to json_object */
    bool  response_format_strict;   /* B24: strict JSON schema enforcement (OpenAI) */
    char  safety_settings[2048];   /* B29: Google safety settings JSON array */
    char  extra_body[4096];        /* L05: extra JSON fields to merge into request body */
    char  azure_deployment_id[128]; /* B37: Azure deployment name override */
    char  azure_api_version[32];   /* B38: Azure API version override */
    char  openrouter_provider[2048]; /* B43-B46: OpenRouter provider preferences JSON */
    char  bedrock_inference_profile[256]; /* B39: Bedrock inference profile ARN/name */
    char  bedrock_guardrail_config[2048]; /* B40: Bedrock guardrail config JSON */
    bool  bedrock_trace_enabled;         /* B41: Bedrock trace header */
    int   max_retries;              /* agent.api_max_retries: API call retries (0=no retry) */
    char  fallback_model[128];     /* model to fallback to on error */
    char  fallback_providers[1024]; /* comma-separated fallback providers */
    void *cred_pool;                /* credential_pool_t * — opaque pool for key rotation */
} llm_config_t;

/* P95: Stream diagnostic — token-level timing and latency breakdown */
typedef struct {
    double  time_to_first_token;   /* seconds from request to first token */
    double  total_stream_time;     /* seconds from first to last token */
    double  tokens_per_second;     /* average throughput during stream */
    size_t  total_tokens;          /* total tokens received in stream */
    bool    first_token_received;  /* set when first content token arrives */
    int     http_status;           /* P95: HTTP status code from API response */
    int     retry_count;           /* P95: number of retries performed */
    char    rate_limit_remaining[16]; /* P95: X-RateLimit-Remaining header value */
    char    rate_limit_reset[16];     /* P95: X-RateLimit-Reset header value */
    /* Internal: timing markers */
    double  request_start_time;    /* monotonic time when request was sent */
    double  first_token_time;      /* monotonic time when first token arrived */
    double  stream_end_time;       /* monotonic time when stream finished */
    char    upstream_headers[384];  /* captured upstream headers (cf-ray, x-openrouter-*) */
} stream_diag_t;

/* LLM response */
typedef struct {
    char         *content;
    char         *reasoning;
    char         *encrypted_content; /* L07: xAI encrypted reasoning content */
    int           input_tokens;
    int           output_tokens;
    int           reasoning_tokens;      /* G04: reasoning-only tokens */
    int           cache_read_tokens;     /* G05: prompt cache hit tokens */
    int           cache_write_tokens;    /* G06: prompt cache write tokens */
    int           tool_calls_count;
    tool_call_t   tool_calls[64]; /* Max 64 tool calls per turn */
    char          finish_reason[32]; /* B22: "stop", "length", "tool_calls", "content_filter" */
    /* P95: Stream diagnostic — populated by streaming path */
    stream_diag_t diag;
    bool          compress_hint;        /* Set by error_classify: should compress context */
    bool          credential_expired;   /* Set by error_classify: should rotate credential */
} llm_response_t;

/* Forward declaration for session database (defined in hermes_db.h) */
typedef struct db_t db_t;

/* Streaming output callback. Called with each token content during LLM response.
 * Return non-zero to abort. */
typedef int (*llm_token_cb_t)(const char *token, void *userdata);

/* Tool event callback. Called before/after tool dispatch operations.
 * event_type: "tool.started" | "tool.completed" | "tool.failed"
 * tool_name: the name of the tool
 * args_json: serialized arguments (for started) or result (for completed/failed)
 * userdata: opaque user pointer.
 * Return non-zero to allow event, zero to suppress. */
typedef int (*tool_event_cb_t)(const char *event_type, const char *tool_name,
                                const char *args_json, void *userdata);

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
    char              user_title[256]; /* user-set session title (via /title) */
    char              hermes_home[HERMES_PATH_MAX];
    db_t             *db;           /* session database (optional) */
    llm_token_cb_t    stream_cb;   /* streaming token callback (optional) */
    void             *stream_data; /* userdata for stream callback */
    tool_event_cb_t   tool_event_cb;   /* tool event callback (optional) */
    void             *tool_event_data; /* userdata for tool event callback */
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
    /* P99: Anti-thrashing — timestamp of last compression to prevent re-compressing same content */
    time_t last_compress_time;
    /* B03: Failure cooldown — skip compression if last failure was <600s ago */
    time_t last_compress_failure_time;
    /* L02: Configurable cooldown from compression config */
    int compress_cooldown_secs;         /* compression.cooldown_secs */
    int compress_failure_cooldown_secs; /* compression.failure_cooldown_secs */
    /* P99b: Tail messages to preserve during compression (default 2, token-budget-aware config) */
    int compress_tail_messages;
    /* P98: Checkpoint manager — auto-save, named checkpoints, rollback */
    checkpoint_manager_t  checkpoints;
    /* G15-G16: Enabled/disabled toolsets — comma-separated lists for tool filtering */
    char enabled_toolsets[1024];
    char disabled_toolsets[1024];
    /* G17: Per-conversation system message override */
    char system_message[4096];
    /* G19: Session routing metadata — thread/user/chat IDs */
    char thread_id[64];
    char user_id[64];
    char chat_id[64];
    /* G01-G03: Session token tracking — cumulative across all turns */
    int session_total_tokens;
    int session_input_tokens;
    int session_output_tokens;
    /* G04-G06: Deep token tracking — reasoning, cache cost */
    int session_reasoning_tokens;
    int session_cache_read_tokens;
    int session_cache_write_tokens;
    /* G07-G08: Cost tracking */
    double session_estimated_cost_usd;
    char   session_cost_source[32];  /* budget_tracker, provider_report, estimated */
    /* G09: Turn counters */
    int user_turn_count;       /* user-initiated turns */
    int tool_turn_count;       /* tool-calling turns */
    /* G10: Idle tracking */
    time_t last_activity_ts;  /* monotonic timestamp of last agent activity */
    /* G11: Pending steer — queued assistant/system prefill for next LLM call */
    char pending_steer[4096];
    /* G12: Structured interrupt message */
    char interrupt_message[1024];
    /* G13-G14: Runtime overrides for tool_choice and parallel_tool_calls */
    char tool_choice[32];      /* overrides llm.tool_choice per conversation */
    bool parallel_tool_calls;  /* overrides llm.parallel_tool_calls */
    /* G20: Model family tracking for accurate pricing */
    char model_family[32];     /* e.g. "gpt-4", "claude-3", "deepseek-chat" */

    /* G21: Selected compression strategy override (from config) */
    int compression_strategy; /* 0=EVICT_OLDEST_TOOL_FIRST, 1=EVICT_OLDEST_USER, 2=EVICT_KEEP_RECENT_N */
    /* G23: Preserve attachment metadata during compression (references to images/files) */
    bool preserve_attachments;

    /* G24: Per-turn tool call tracking for iteration budget */
    int  tool_calls_this_turn;       /* tool calls executed in current turn */
    int  max_tool_calls_per_turn;    /* per-turn tool call cap (0 = unlimited) */

    /* G26: Budget enforcement mode — 0=soft (grace call), 1=hard (immediate stop) */
    bool budget_hard_limit;

    /* G31-G32: Prefill role variant — MSG_ASSISTANT (default) or MSG_SYSTEM */
    message_role_t prefill_role;

    /* G33-G34: Steer queue — up to 8 pending steering messages with type + priority */
#define HERMES_MAX_STEERS 8
    char steer_queue[HERMES_MAX_STEERS][4096];   /* steer message contents */
    message_role_t steer_roles[HERMES_MAX_STEERS]; /* MSG_SYSTEM, MSG_ASSISTANT, or MSG_USER */
    int steer_count;                              /* number of queued steers */

    /* G35-G36: Typed interrupt */
    int interrupt_type;         /* 0=NONE, 1=GRACEFUL, 2=FORCE */
    bool partial_results_saved; /* G36: partial tool results preserved */

    /* G28-G30: Tool call loop guardrails controller */
    tool_guardrail_controller_t guardrails_ctrl;

    /* G37: Optional background tool result review (see llm_background_review) */
    bool enable_background_review;

    /* L03: Vision disabled flag — set when provider rejects image content */
    bool vision_disabled;

    /* L09: Memory nudge — periodic suggestion to update memory */
    int  memory_nudge_interval;  /* turns between nudges (0=disabled) */
    int  turns_since_memory;     /* turns since last memory nudge */

    /* L10: Skill nudge — periodic suggestion after N tool iterations */
    int  skill_nudge_interval;   /* iterations between nudges (0=disabled) */
    int  iters_since_skill;      /* tool iterations since last skill nudge */
} agent_state_t;

/* Interrupt type constants (interrupt_type field) */
#define INTERRUPT_NONE     0
#define INTERRUPT_GRACEFUL 1
#define INTERRUPT_FORCE    2

/* Compression strategy constants (compression_strategy field) */
#define COMPRESS_OLDEST_TOOL_FIRST 0
#define COMPRESS_OLDEST_USER       1
#define COMPRESS_KEEP_RECENT_N     2

/* ================================================================
 *  Provider Config Sub-group (P1)
 * ================================================================ */
#define HERMES_STOP_SEQUENCES_MAX 8

typedef struct {
    char  model[128];
    char  provider[64];
    char  base_url[256];
    char  api_key[256];
    char  deepseek_api_key[256];   /* deepseek-specific API key from DEEPSEEK_API_KEY env */
    char  api_mode[32];            /* chat_completions, codex_responses, etc. */
    int   max_tokens;              /* max output tokens per response */
    float temperature;             /* sampling temperature (0.0-2.0) */
    float top_p;                   /* nucleus sampling (0.0-1.0) */
    char  stop_sequences[HERMES_STOP_SEQUENCES_MAX][128];  /* stop strings */
    int   stop_count;
    float presence_penalty;        /* -2.0 to 2.0, penalize new tokens based on existence */
    float frequency_penalty;       /* -2.0 to 2.0, penalize new tokens based on frequency */
    int   seed;                    /* deterministic sampling seed (-1 = random) */
    bool  logprobs;                /* request token-level log probabilities */
    int   top_logprobs;            /* number of top logprobs to return (0 = none via logprobs) */
    char  user[128];               /* per-request user identifier */
    char  fallback_model[128];     /* model to fallback to on error */
    char  fallback_providers[1024]; /* comma-separated fallback providers (P83) */
    char  service_tier[32];        /* auto, default (for Anthropic) */
    char  reasoning_effort[32];    /* low, medium, high */
    char  codex_runtime[32];       /* auto | codex_app_server — OpenAI runtime backend */
    char  response_format[256];    /* "json_object" or JSON schema string */
    char  metadata[256];           /* key-value map JSON string */
    char  tool_choice[32];         /* "auto", "none", "required", or JSON for specific function */
    bool  parallel_tool_calls;     /* allow parallel tool calls (default: true) */
    int   max_tool_calls;          /* max tool calls per turn (0 = unlimited) */
    int   n;                       /* number of response choices (1 = single) */
    int   top_k;                    /* B30: top-k sampling (Google, etc.); 0=unset */
    int   candidate_count;          /* B30: number of response candidates (Google); 0=unset */
    bool  json_mode;                /* B23: convenience — auto-set response_format to json_object */
    bool  response_format_strict;   /* B24: strict JSON schema enforcement (OpenAI) */
    char  safety_settings[2048];   /* B29: Google safety settings JSON array */
    char  extra_body[4096];        /* L05: extra JSON fields to merge into request body */
    char  azure_deployment_id[128]; /* B37: Azure deployment name override (default: model name) */
    char  azure_api_version[32];   /* B38: Azure API version override (default: 2024-10-01-preview) */
    char  openrouter_provider[2048]; /* B43-B46: OpenRouter provider preferences JSON */
    char  bedrock_inference_profile[256]; /* B39: Bedrock inference profile ARN/name */
    char  bedrock_guardrail_config[2048]; /* B40: Bedrock guardrail config JSON */
    bool  bedrock_trace_enabled;         /* B41: Bedrock trace header */
    bool  local_provider;          /* N05: true if base_url is localhost/127.0.0.1 */
    bool  supports_vision;         /* L06: override model vision capability (false=auto from metadata) */
    char  vision_overrides[1024];  /* S06: comma-sep model prefixes to force vision-capable (e.g. "qwen-vl,llava,pixtral") */
    int   deepseek_cache_ttl;      /* B33: DeepSeek context cache TTL in seconds (-1=disabled, 0=default 300) */
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
    int   compress_tail_messages;  /* compression.tail_messages: tail messages to keep */
    char  reasoning_effort[32];    /* agent.reasoning_effort: low/medium/high */
    int   api_max_retries;         /* agent.api_max_retries: API call retries */
    int   clarify_timeout;         /* agent.clarify_timeout: seconds before auto-deny */
    char  image_input_mode[16];    /* agent.image_input_mode: auto|native|text */
    char  skill_search_paths[1024]; /* agent.skill_search_paths: comma-sep custom skill dirs */
    char  model_metadata_path[512]; /* agent.model_metadata: path to model capabilities JSON */
    bool  moa_enabled;             /* agent.mixture_of_agents.enabled */
    char  moa_model[128];          /* agent.mixture_of_agents.model */
    char  moa_strategy[32];        /* agent.mixture_of_agents.strategy: round_robin | weighted | parallel */
    int   moa_workers;             /* agent.mixture_of_agents.num_workers */
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
    char  terminal_backend[32];    /* terminal.backend: local/ssh/docker/modal */
    bool  persistent_shell;        /* terminal.persistent_shell: keep shell across commands */
    char  vision_model[128];       /* auxiliary.vision.model: vision model name */
    int   vision_timeout;          /* auxiliary.vision.timeout: seconds */
    int   web_search_timeout;      /* web search timeout in seconds */
    char  web_backend[32];         /* web.backend: shared fallback for search/extract */
    char  web_search_backend[32];  /* web.search_backend: searxng, google, etc. */
    char  web_extract_backend[32]; /* web.extract_backend: native, jina, etc. */
    char  enabled_toolsets[1024];  /* tools.enabled_toolsets: comma-separated toolset list */
    char  disabled_toolsets[1024]; /* tools.disabled_toolsets: comma-separated toolset list */
    char  environments[512];       /* tools.environments: comma-separated enabled backends */
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
    char  user_agent[256];         /* browser.user_agent: custom UA string */
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
    int   protect_last_n;         /* protect_last_n: recent messages to keep */
    int   protect_first_n;        /* protect_first_n: head messages to preserve */
    int   hygiene_hard_message_limit; /* hygiene_hard_message_limit: force-compress threshold */
    int   cooldown_secs;            /* compression.cooldown_secs: anti-thrashing cooldown (default 30) */
    int   failure_cooldown_secs;    /* compression.failure_cooldown_secs: failure cooldown (default 600) */
    bool  preserve_system;        /* preserve_system: keep system prompt */
    bool  abort_on_summary_failure; /* abort_on_summary_failure: skip drop on LLM error */
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
    int   scheduler_poll_interval; /* cron.scheduler_poll_interval: seconds between scheduler ticks */
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
    char  tirith_policy_text[4096]; /* O13: YAML policy definitions */
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
    char  memory_provider[128];   /* plugins.memory.provider: name of memory plugin (e.g. honcho) */
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
    char  authorization_url[1024]; /* OAuth authorization endpoint (for PKCE flow) */
    char  redirect_uri[1024];      /* Redirect URI for callback server */
    char  scopes[1024];           /* space-separated scopes */
    int   refresh_before_sec;     /* refresh if < N seconds left on expiry */
    char  url[1024];              /* MCP server base URL (for OAuth metadata discovery) */
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
 *  Auxiliary Config (Python auxiliary group — 11 sub-tasks, ~66 keys)
 *  Each sub-task specifies provider/model/timeout for a specific
 *  auxiliary LLM call (vision, web_extract, compression, etc.).
 * ================================================================ */
typedef struct {
    char  provider[64];            /* auto | openrouter | nous | codex | custom */
    char  model[128];              /* e.g. "google/gemini-2.5-flash" */
    char  base_url[256];           /* direct OpenAI-compatible endpoint */
    char  api_key[256];            /* API key for base_url */
    int   timeout;                 /* seconds — LLM API call timeout */
    char  extra_body[1024];        /* JSON: extra provider-specific fields */
} auxiliary_task_config_t;

#define AUX_TASK_CONFIG_DEFAULT_TIMEOUT 30

typedef struct {
    auxiliary_task_config_t vision;          /* auxiliary.vision.* — +download_timeout below */
    int                     vision_download_timeout; /* auxiliary.vision.download_timeout: seconds */
    auxiliary_task_config_t web_extract;      /* auxiliary.web_extract.* */
    auxiliary_task_config_t compression;      /* auxiliary.compression.* */
    auxiliary_task_config_t skills_hub;       /* auxiliary.skills_hub.* */
    auxiliary_task_config_t approval;         /* auxiliary.approval.* */
    auxiliary_task_config_t mcp;              /* auxiliary.mcp.* */
    auxiliary_task_config_t title_generation; /* auxiliary.title_generation.* */
    auxiliary_task_config_t triage_specifier; /* auxiliary.triage_specifier.* */
    auxiliary_task_config_t kanban_decomposer;/* auxiliary.kanban_decomposer.* */
    auxiliary_task_config_t profile_describer;/* auxiliary.profile_describer.* */
    auxiliary_task_config_t curator;          /* auxiliary.curator.* */
} auxiliary_config_t;

/* ================================================================
 *  Terminal Config (P2, expanded from Python terminal group, 22 keys)
 * ================================================================ */
typedef struct {
    char  backend[32];             /* terminal.backend: local/ssh/docker/modal */
    int   timeout;                 /* terminal.timeout: max seconds per command */
    bool  persistent_shell;        /* terminal.persistent_shell */
    char  cwd[HERMES_PATH_MAX];   /* terminal.cwd: working directory */
    char  env_passthrough[1024];    /* terminal.env_passthrough (comma-sep) */
    char  shell_init_files[1024];  /* terminal.shell_init_files (comma-sep) */
    bool  auto_source_bashrc;      /* terminal.auto_source_bashrc */
    char  docker_image[256];       /* terminal.docker_image */
    char  docker_forward_env[1024];/* terminal.docker_forward_env (comma-sep) */
    char  docker_env[1024];        /* terminal.docker_env: JSON string */
    char  singularity_image[256];  /* terminal.singularity_image */
    char  modal_image[256];        /* terminal.modal_image */
    char  daytona_image[256];      /* terminal.daytona_image */
    char  vercel_runtime[32];      /* terminal.vercel_runtime */
    int   container_cpu;           /* terminal.container_cpu */
    int   container_memory;        /* terminal.container_memory (MB) */
    int   container_disk;          /* terminal.container_disk (MB) */
    bool  container_persistent;    /* terminal.container_persistent */
    char  docker_volumes[1024];    /* terminal.docker_volumes (comma-sep) */
    bool  docker_mount_cwd;        /* terminal.docker_mount_cwd_to_workspace */
    char  docker_extra_args[1024]; /* terminal.docker_extra_args (comma-sep) */
    bool  docker_run_as_host_user; /* terminal.docker_run_as_host_user */
} terminal_config_t;

typedef struct {
    char  provider[32];            /* tts.provider: edge/elevenlabs/openai/xai/mistral/neutts/piper */
    char  edge_voice[64];          /* tts.edge.voice */
    char  elevenlabs_voice_id[64]; /* tts.elevenlabs.voice_id */
    char  elevenlabs_model_id[64]; /* tts.elevenlabs.model_id */
    char  openai_model[64];        /* tts.openai.model */
    char  openai_voice[32];        /* tts.openai.voice */
    char  xai_voice_id[64];        /* tts.xai.voice_id */
    char  xai_language[16];        /* tts.xai.language */
    int   xai_sample_rate;         /* tts.xai.sample_rate */
    int   xai_bit_rate;            /* tts.xai.bit_rate */
    char  mistral_model[64];       /* tts.mistral.model */
    char  mistral_voice_id[64];    /* tts.mistral.voice_id */
    char  neutts_ref_audio[256];   /* tts.neutts.ref_audio */
    char  neutts_ref_text[256];    /* tts.neutts.ref_text */
    char  neutts_model[128];       /* tts.neutts.model */
    char  neutts_device[16];       /* tts.neutts.device: cpu/cuda/mps */
    char  piper_voice[64];         /* tts.piper.voice */
} tts_config_t;

typedef struct {
    bool  enabled;                  /* stt.enabled */
    char  provider[32];             /* stt.provider: local/groq/openai/mistral */
    char  local_model[32];          /* stt.local.model: tiny/base/small/medium/large-v3 */
    char  local_language[16];       /* stt.local.language: auto-detect if empty */
    char  openai_model[32];         /* stt.openai.model: whisper-1/gpt-4o-mini-transcribe */
    char  mistral_model[48];        /* stt.mistral.model */
} stt_config_t;

typedef struct {
    char  record_key[16];           /* voice.record_key: e.g. "ctrl+b" */
    int   max_recording_seconds;    /* voice.max_recording_seconds */
    bool  auto_tts;                 /* voice.auto_tts */
    bool  beep_enabled;             /* voice.beep_enabled */
    int   silence_threshold;        /* voice.silence_threshold: RMS value 0-32767 */
    float silence_duration;         /* voice.silence_duration: seconds */
} voice_config_t;

/* ================================================================
 *  Discord Config (12 keys)
 * ================================================================ */
typedef struct {
    char  token[256];               /* discord.token: bot token */
    char  guild_id[64];             /* discord.guild_id */
    char  command_prefix[16];       /* discord.command_prefix */
    char  status[32];               /* discord.status: online/idle/dnd */
    char  activity_type[16];        /* discord.activity_type */
    char  activity_name[128];       /* discord.activity_name */
    char  allowed_roles[1024];      /* discord.allowed_roles: comma-sep */
    char  allowed_channels[1024];   /* discord.allowed_channels: comma-sep */
    int   max_message_length;       /* discord.max_message_length */
    bool  sync_permissions;         /* discord.sync_permissions */
    bool  slash_commands_enabled;   /* discord.slash_commands_enabled */
    bool  thread_auto_archive;      /* discord.thread_auto_archive */
} discord_config_t;

/* ================================================================
 *  Kanban Config (10 keys)
 * ================================================================ */
typedef struct {
    char  board_dir[HERMES_PATH_MAX];/* kanban.board_dir */
    int   max_wip;                  /* kanban.max_wip */
    int   default_sprint_days;      /* kanban.default_sprint_days */
    int   auto_archive_days;        /* kanban.auto_archive_days */
    char  ticket_template[HERMES_PATH_MAX];/* kanban.ticket_template */
    char  label_colors[256];        /* kanban.label_colors: JSON */
    char  assignee_field[64];       /* kanban.assignee_field */
    char  priority_field[64];       /* kanban.priority_field */
    char  estimate_field[64];       /* kanban.estimate_field */
    bool  auto_sync;                /* kanban.auto_sync */
} kanban_config_t;

/* ================================================================
 *  Tool Loop Guardrails Config (8 keys)
 * ================================================================ */
typedef struct {
    int   max_consecutive_failures; /* guardrails.max_consecutive_failures */
    int   max_tool_calls_per_turn;  /* guardrails.max_tool_calls_per_turn */
    int   max_result_bytes;         /* guardrails.max_result_bytes */
    bool  abort_on_safety_violation;/* guardrails.abort_on_safety_violation */
    int   rate_limit_per_minute;    /* guardrails.rate_limit_per_minute */
    int   cooldown_seconds;         /* guardrails.cooldown_seconds */
    char  allowed_patterns[1024];   /* guardrails.allowed_patterns: comma-sep */
    char  denied_patterns[1024];    /* guardrails.denied_patterns: comma-sep */
} guardrails_config_t;

/* ================================================================
 *  Approvals Config (5 keys)
 * ================================================================ */
typedef struct {
    char  mode[16];                 /* approvals.mode: off/manual/auto */
    int   timeout;                  /* approvals.timeout: seconds */
    bool  require_reason;           /* approvals.require_reason */
    bool  notify_on_pending;        /* approvals.notify_on_pending */
    char  auto_approve_patterns[1024];/* approvals.auto_approve_patterns */
} approvals_config_t;

/* ================================================================
 *  Small Gateway/Platform Configs
 * ================================================================ */
typedef struct {
    char  api_key[256];             /* x_search.api_key */
    char  engine[32];               /* x_search.engine */
} x_search_config_t;

typedef struct {
    char  token[256];               /* slack.token */
    char  signing_secret[256];      /* slack.signing_secret */
    char  app_token[256];           /* slack.app_token */
} slack_config_t;

typedef struct {
    char  homeserver[256];          /* matrix.homeserver */
    char  user_id[128];             /* matrix.user_id */
    char  access_token[256];        /* matrix.access_token */
} matrix_config_t;

typedef struct {
    char  url[256];                 /* mattermost.url */
    char  token[256];               /* mattermost.token */
    char  team_id[64];              /* mattermost.team_id */
} mattermost_config_t;

typedef struct {
    bool  auto_update;              /* model_catalog.auto_update */
    char  cache_file[256];          /* model_catalog.cache_file */
    char  custom_models[1024];      /* model_catalog.custom_models: JSON */
} model_catalog_config_t;

typedef struct {
    char  api_key[256];             /* openrouter.api_key */
    char  referer[256];             /* openrouter.referer */
    char  title[128];               /* openrouter.title */
} openrouter_config_t;

typedef struct {
    int   min_ms;                   /* human_delay.min_ms */
    int   max_ms;                   /* human_delay.max_ms */
    bool  enabled;                  /* human_delay.enabled */
} human_delay_config_t;

typedef struct {
    char  bot_token[256];           /* telegram.bot_token */
    char  allowed_updates[1024];    /* telegram.allowed_updates: comma-sep */
} telegram_config_t;

typedef struct {
    int   check_interval;           /* updates.check_interval: hours */
    char  channel[64];              /* updates.channel: release/beta */
} updates_config_t;

typedef struct {
    int   port;                     /* dashboard.port */
    char  theme[32];                /* dashboard.theme */
} dashboard_config_t;

/* ================================================================
 *  Logging Config (Python logging group, 5 keys)
 * ================================================================ */
typedef struct {
    char  level[16];               /* logging.level: debug/info/warning/error */
    char  format[32];              /* logging.format: text/json */
    char  dir[HERMES_PATH_MAX];   /* logging.dir: log directory */
    int   max_files;               /* logging.max_files: retention count */
    int   max_size_mb;             /* logging.max_size_mb: per-file max */
} logging_config_t;

/* ================================================================
 *  Skills Config (Python skills group, 5 keys)
 * ================================================================ */
typedef struct {
    char  dir[HERMES_PATH_MAX];   /* skills.dir: skill search directory */
    char  enabled[1024];           /* skills.enabled: comma-sep skill names */
    bool  auto_discover;           /* skills.auto_discover: scan on startup */
    int   bundle_size_limit;       /* skills.bundle_size_limit: max KB per bundle */
    int   validate_on_load;        /* skills.validate: 0=no, 1=warn, 2=strict */
} skills_config_t;

/* ================================================================
 *  Checkpoints Config (Python checkpoints group, 8 keys)
 * ================================================================ */
typedef struct {
    bool  enabled;                 /* checkpoints.enabled */
    int   interval;                /* checkpoints.interval: auto-save every N turns */
    int   max_checkpoints;         /* checkpoints.max: retention limit */
    char  dir[HERMES_PATH_MAX];   /* checkpoints.dir: storage directory */
    bool  auto_rollback;           /* checkpoints.auto_rollback: revert on crash */
    bool  save_on_interrupt;       /* checkpoints.save_on_interrupt */
    int   compression_level;       /* checkpoints.compression: 0-9, 0=off */
    bool  include_tool_results;    /* checkpoints.include_tool_results */
} checkpoints_config_t;

/* ================================================================
 *  Secrets Config (L01: Bitwarden Secrets Manager)
 * ================================================================ */
typedef struct {
    bool  enabled;                 /* secrets.bitwarden.enabled */
    char  access_token[512];       /* secrets.bitwarden.access_token: BSM access token */
    char  organization_id[128];    /* secrets.bitwarden.organization_id: optional org scope */
    char  bws_path[512];           /* secrets.bitwarden.bws_path: path to bws CLI (auto-discovered) */
    int   install_timeout;         /* secrets.bitwarden.install_timeout: seconds for lazy install */
} secrets_config_t;

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
    /* Terminal config */
    terminal_config_t terminal;
    /* Logging config */
    logging_config_t logging;
    /* Skills config */
    skills_config_t skills;
    /* Checkpoints config */
    checkpoints_config_t checkpoints;
    /* Auxiliary config */
    auxiliary_config_t auxiliary;
    /* TTS config */
    tts_config_t tts;
    /* STT config */
    stt_config_t stt;
    /* Voice config */
    voice_config_t voice;
    /* Discord config */
    discord_config_t discord;
    /* Kanban config */
    kanban_config_t kanban;
    /* Tool loop guardrails config */
    guardrails_config_t guardrails;
    /* Approvals config */
    approvals_config_t approvals;
    /* X/Twitter search config */
    x_search_config_t x_search;
    /* Slack config */
    slack_config_t slack;
    /* Matrix config */
    matrix_config_t matrix;
    /* Mattermost config */
    mattermost_config_t mattermost;
    /* Model catalog config */
    model_catalog_config_t model_catalog;
    /* OpenRouter config */
    openrouter_config_t openrouter;
    /* Human delay config */
    human_delay_config_t human_delay;
    /* Telegram config */
    telegram_config_t telegram;
    /* Updates config */
    updates_config_t updates;
    /* Dashboard config */
    dashboard_config_t dashboard;
    /* Secrets config */
    secrets_config_t secrets;
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
    int   secret_rotation_interval;  /* gateway.secret_rotation: interval in seconds (0=disabled) */
    int   webhook_port;               /* gateway.webhook_port: webhook server port (0=auto/env/8080) */
    char  credential_sources[512];/* credentials.sources: comma-separated resolution order */
    char  signal_number[64];      /* gateway.signal.number: Signal phone number (+1234567890) */
    char  proxy_https[512];       /* proxy.https_proxy: HTTPS proxy URL (e.g. http://proxy:8080) */
    char  proxy_no[1024];         /* proxy.no_proxy: comma-separated bypass list */
    char  vault_path[512];        /* agent.vault.path: vault file path */
    char  hooks_json[16384];       /* B07: raw JSON of hooks: config block for shell hooks */
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
/* U01: Config init — create default config.yaml + .env template */
bool hermes_config_init(const char *config_dir);

bool hermes_config_export(const hermes_config_t *cfg, const char *path);
bool hermes_config_import(hermes_config_t *cfg, const char *path);

/* P24: Config schema generation — returns malloc'd JSON string, caller must free */
char *hermes_config_schema(void);

/* P25: Config migration — upgrade config version, returns true if migration ran */
bool hermes_config_migrate(hermes_config_t *cfg, const char *config_dir);

/* P19: Config hot-reload via SIGHUP — setup signal handler and poll flag */
void hermes_config_setup_reload(void);
/* Returns true and reloads config if SIGHUP was received */
bool hermes_config_check_reload(hermes_config_t *cfg, const char *config_dir);

/* O15: File permission hardening — set 0600/0700 on sensitive files */
/* Secures config.yaml, .env, session DB, vault, cron store, and home dir.
   owner argument: pass geteuid(). Skips if running as root.  */
void hermes_file_permissions_harden(const char *hermes_home,
                                    const char *session_db_path,
                                    const char *cron_store_path,
                                    uid_t owner);

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
#include "hermes_tirith.h"
#include "hermes_media_cache.h"

/* ================================================================
 *  Entry Points
 * ================================================================ */
int  hermes_cli_main(int argc, char **argv);
int  hermes_gateway_main(int argc, char **argv);

/* Command system */
typedef struct command_def_t {
    const char *name;
    const char *alias;
    const char *description;
    void (*handler)(const char *args, agent_state_t *state);
} command_def_t;
bool commands_dispatch(const char *input, agent_state_t *state);
bool commands_try_skill(const char *input, agent_state_t *state);
const command_def_t *commands_resolve(const char *input);
const command_def_t *commands_get_all(void);
int commands_count(void);
const char *commands_list_json(void);

/* Security approval */
int approval_check(const char *tool_name, const char *args_json);
void approval_reset_session(void);
void approval_set_yolo(bool enabled);  /* When true, skip all approval prompts */
void approval_set_allowlist_path(const char *path);
void approval_load_allowlist(void);
void approval_save_allowlist(void);

/* Gateway approval — wire messaging platform send/response for async approval prompts */
void approval_set_gateway_send(bool (*fn)(const char *, const char *, const char *),
                                const char *platform, const char *chat_id);
void approval_set_gateway_wait(char *(*fn)(int timeout_sec));
/* Called from approval.c during gateway path to register pending approval context */
void gw_approval_set_context(const char *platform, const char *chat_id);

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
const char *cdp_get_url(void);

/* Registry accessors */
size_t registry_get_count(void);
const char *registry_get_name(size_t i);

/* Find tool by name in registry. Returns NULL if not found. */
tool_t *registry_find(const char *name);

/* P52: Per-tool timeout. Set seconds, 0 = default, -1 = no timeout. */
void registry_set_timeout(const char *name, int seconds);
/* P150: Filter tool registry by enabled/disabled toolsets. Marks matching tools unavailable. */
void registry_filter_by_toolset(tool_registry_t *reg, const char *enabled_csv, const char *disabled_csv);
/* P150: Get toolset name for a registered tool. Returns "" if not set. */
const char *registry_get_toolset(const char *name);
/* P150: Set toolset name for a registered tool (after registration). */
void registry_set_toolset(const char *name, const char *toolset);
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

/* B34: Surrogate character sanitization — replace lone UTF-8 surrogates with U+FFFD */
char *sanitize_surrogates(const char *text);
bool  sanitize_json_surrogates(void *json_obj);

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
bool vault_rotate_key(const char *old_passphrase, const char *new_passphrase);

/* O12: Audit log rotation parameters */
void audit_set_rotation(size_t max_size_kb, int max_files, int max_age_days);

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
char *cron_sqlite_list_to_json(cron_sqlite_store_t *store);
/* Inject a human-readable repeat_display field into a cron job JSON object.
 * Mirrors Python cronjob_tools._repeat_display(). */
void cron_inject_repeat_display(json_node_t *job);
bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name);
bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);

/* P170: Cron expression parser (libcron already exists — see lib/libcron/cron.h) */

/* P171: Job locking */
void  cron_lock_set_dir(const char *dir);
bool  cron_lock_acquire(const char *lock_name);
void  cron_lock_release(const char *lock_name);
bool  cron_lock_is_locked(const char *lock_name);
bool  cron_shutdown_requested(void);
void  cron_release_all_locks(void);

/* P172: Job retry */
bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec);
int  cron_job_get_retry_count(const char *job_name);
int  cron_job_get_max_retries(const char *job_name);

/* P173: Job notification */
bool cron_notify_set_channel(const char *channel_id);
void cron_notify_set_send_fn(bool (*fn)(const char *platform, const char *chat_id, const char *text));
bool cron_notify_on_complete(const char *job_name, bool enabled);
bool cron_notify_on_failure(const char *job_name, bool enabled);

/* P174: Job chaining */
bool cron_chain_set_context(const char *job_name, const char *context_from);
const char *cron_chain_get_context(const char *job_name);
char *cron_chain_get_output(const char *job_name);
void cron_chain_store_output(const char *job_name, const char *output);

/* P176: Cron utility functions (port of cronjob_tools.py helpers) */
char **cron_canonical_skills(const char *skill, json_node_t *skills, size_t *out_count);
char  *cron_normalize_value(const char *value, bool strip_trailing_slash);
char  *cron_normalize_deliver(json_node_t *deliver);
int    cron_parse_duration(const char *s);   /* Parse "30m", "2h", "1d" → minutes */
bool   cron_secure_dir(const char *path);     /* chmod 0700 */
bool   cron_secure_file(const char *path);    /* chmod 0600 */
const char *cron_coerce_job_text(const char *value, const char *fallback); /* nullable string coercion */
const char *cron_schedule_display_for_job(json_node_t *job);              /* Extract display string from job schedule */
bool        cron_ensure_dirs(const char *hermes_home);                    /* mkdir -p ~/.hermes/cron/ + ~/.hermes/cron/output/ */

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

/* ================================================================
 *  P179-P188: Skills system depth
 * ================================================================ */

/* Skill provenance/origin */
typedef enum {
    SKILL_ORIGIN_LOCAL,
    SKILL_ORIGIN_HUB,
    SKILL_ORIGIN_BUNDLE,
    SKILL_ORIGIN_UNKNOWN,
} skill_origin_t;

/* Skill metadata (full) */
typedef struct {
    char  name[128];              /* skill name (directory name) */
    char  path[HERMES_PATH_MAX];  /* full path to skill directory */
    char  version[32];            /* semver from SKILL.md frontmatter */
    char  author[256];            /* author field */
    char  description[512];       /* description */
    char  category[128];          /* category tag */
    char  tags[1024];             /* comma-separated tags */
    char  dependencies[1024];     /* comma-separated skill deps */
    char  bundles[1024];          /* comma-separated bundle aliases */
    skill_origin_t origin;        /* provenance: local/hub/bundle */
    time_t last_updated;          /* mtime of SKILL.md */
    time_t last_used;             /* last time skill was invoked */
    int   usage_count;            /* how many times skill was used */
    bool  validated;              /* passed frontmatter validation */
    char  validation_error[256];  /* validation error if any */
} skill_meta_t;

/* Cache entry for LRU cache */
typedef struct skill_cache_entry_t {
    char  name[128];
    char *content;                /* full SKILL.md content */
    skill_meta_t meta;
    time_t loaded_at;
    struct skill_cache_entry_t *prev;
    struct skill_cache_entry_t *next;
} skill_cache_entry_t;

/* Skill cache (doubly-linked list for LRU) */
typedef struct {
    skill_cache_entry_t *head;    /* most recently used */
    skill_cache_entry_t *tail;    /* least recently used */
    size_t count;
    size_t max_entries;           /* 0 = unlimited */
} skill_cache_t;

/* Skill scanning result (list of metadata) */
typedef struct {
    skill_meta_t *skills;
    size_t count;
    size_t capacity;
} skill_list_t;

/* Dependency node for resolution */
typedef struct skill_dep_node_t {
    char  name[128];
    char  version[32];
    bool  resolved;
    struct skill_dep_node_t **deps; /* array of dependency names */
    size_t deps_count;
} skill_dep_node_t;

/* Search result */
typedef struct {
    char  name[128];
    char  path[HERMES_PATH_MAX];
    float score;                  /* relevance score 0.0-1.0 */
} skill_search_result_t;

/* P179: Scan skills directory — recursive, extract metadata, cache */
skill_list_t *skills_scan_all(void);
void skills_scan_free(skill_list_t *list);

/* P180: Validate skill frontmatter */
bool skill_validate(const char *skill_name, char *error_out, size_t err_sz);
bool skill_validate_all(void);

/* P181: Get/set skill provenance */
skill_origin_t skill_get_origin(const char *skill_name);
bool skill_set_origin(const char *skill_name, skill_origin_t origin);

/* P182: Sync skills from git-based hub */
bool skill_sync_from_hub(const char *hub_url, const char *branch, char *log_out, size_t log_sz);

/* P183: Bundle management */
bool skill_bundle_create(const char *bundle_name, const char *skills_csv);
bool skill_bundle_delete(const char *bundle_name);
skill_list_t *skill_bundle_get_skills(const char *bundle_name);

/* P184: Usage tracking */
void skill_record_usage(const char *skill_name);
int  skill_get_usage_count(const char *skill_name);
time_t skill_get_last_used(const char *skill_name);
void skill_get_recommendations(skill_meta_t *out, size_t *count, size_t max_count);

/* P185: Cache management */
bool skill_cache_init(size_t max_entries);
void skill_cache_destroy(void);
bool skill_cache_preload(const char *skill_name);
void skill_cache_evict(const char *skill_name);
const char *skill_cache_get(const char *skill_name);
size_t skill_cache_count(void);

/* P186: Search skills */
skill_search_result_t *skill_search(const char *query, const char *tag_filter,
                                     size_t *result_count, size_t max_results);
void skill_search_free(skill_search_result_t *results, size_t count);

/* L12: Browse.sh skills hub — search and install */
skill_search_result_t *skill_search_hub(const char *query,
                                         size_t *result_count, size_t max_results);
void skill_search_hub_free(skill_search_result_t *results, size_t count);
bool skill_install_from_hub(const char *slug, char *error_out, size_t err_sz);

/* P187: Curator — stale detection and auto-update */
bool skill_curator_run(char *report_out, size_t report_sz);
bool skill_curator_set_stale_days(int days);
int  skill_curator_get_stale_days(void);

/* P188: Dependency resolution */
bool skill_deps_resolve(const char *skill_name,
                         char ordered[][128], size_t *count, size_t max);
char **skill_deps_get_missing(const char *skill_name, size_t *count);
bool skill_deps_validate_order(const char ordered[][128], size_t count);

/* L04: xAI model retirement detection */
bool xai_is_model_retired(const char *model_name,
                           char *replacement_out, size_t replacement_sz,
                           char *reasoning_out, size_t reasoning_sz);

/* P49: Tool result preview generation — truncate at last newline within max_chars */
char *tool_result_generate_preview(const char *content, int max_chars, bool *has_more);

/* D07: Delegate spawn pause — global gate for TUI/gateway */
bool set_spawn_paused(bool paused);
bool is_spawn_paused(void);

/** @} */ /* end of hermes group */

#endif /* HERMES_H */
