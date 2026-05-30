#ifndef HERMES_SYSTEM_PROMPT_H
#define HERMES_SYSTEM_PROMPT_H

/*
 * hermes_system_prompt.h — Dynamic system prompt assembly for Hermes C.
 * Port of Python agent/system_prompt.py + prompt_builder.py constants.
 *
 * Builds a three-tier system prompt:
 *   stable   — identity, tool guidance, skills prompt, env/platform hints
 *   context  — context files, caller-supplied system_message
 *   volatile — memory snapshot, user profile, timestamp
 *
 * Returns a malloc'd string (caller must free) or NULL on error.
 *
 * Context file loading (SOUL.md, .hermes.md, AGENTS.md, CLAUDE.md, .cursorrules)
 * with prompt-injection threat detection. Port of Python prompt_builder.py.
 */

#include "hermes.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Guidance constants (ported from Python prompt_builder.py)
 * ================================================================ */

/* Default AIAgent identity when no SOUL.md is present */
extern const char *SYSPRMPT_DEFAULT_IDENTITY;

/* Pointer to hermes-agent docs for user questions about Hermes itself */
extern const char *SYSPRMPT_HERMES_HELP;

/* Memory tool usage guidance */
extern const char *SYSPRMPT_MEMORY_GUIDANCE;

/* Session search tool guidance */
extern const char *SYSPRMPT_SESSION_SEARCH_GUIDANCE;

/* Skills tool usage guidance */
extern const char *SYSPRMPT_SKILLS_GUIDANCE;

/* Kanban task execution protocol */
extern const char *SYSPRMPT_KANBAN_GUIDANCE;

/* Tool-use enforcement (tell model to actually call tools) */
extern const char *SYSPRMPT_TOOL_ENFORCE;

/* OpenAI/Grok execution discipline */
extern const char *SYSPRMPT_OPENAI_EXEC;

/* Google model operational directives */
extern const char *SYSPRMPT_GOOGLE_OPS;

/* Computer-use guidance */
extern const char *SYSPRMPT_COMPUTER_USE;

/* ================================================================
 *  Context file threat detection constants
 * ================================================================ */

#define CONTEXT_FILE_MAX_CHARS    20000
#define CONTEXT_TRUNCATE_HEAD_RATIO 0.7
#define CONTEXT_TRUNCATE_TAIL_RATIO 0.2

/* Threat pattern match result */
typedef struct {
    const char *id;   /* pattern identifier */
} context_threat_match_t;

/* ================================================================
 *  Context file loading API
 * ================================================================ */

/* Scan content for prompt-injection patterns. Returns sanitized content
 * (malloc'd), or NULL if content is clean. Caller must free result.
 * When threats are found, returns a [BLOCKED: ...] message. */
char *context_scan_content(const char *content, const char *filename);

/* Walk up from start_dir looking for a .git directory. Returns malloc'd
 * path to the git root, or NULL. Caller must free. */
char *context_find_git_root(const char *start_dir);

/* Load SOUL.md from HERMES_HOME (~/.hermes/). Returns malloc'd content
 * (with threat scan applied) or NULL. Caller must free. */
char *context_load_soul_md(void);

/* Load .hermes.md / HERMES.md by walking to git root from cwd.
 * Returns malloc'd content or NULL. Caller must free. */
char *context_load_hermes_md(const char *cwd);

/* Load AGENTS.md / agents.md from cwd. Returns malloc'd or NULL. */
char *context_load_agents_md(const char *cwd);

/* Load CLAUDE.md / claude.md from cwd. Returns malloc'd or NULL. */
char *context_load_claude_md(const char *cwd);

/* Load .cursorrules + .cursor/rules *.mdc from cwd. Returns malloc'd or NULL. */
char *context_load_cursorrules(const char *cwd);

/* Build full context files prompt block (SOUL.md + highest-priority project
 * context). Returns malloc'd string or empty string (if no context found).
 * If skip_soul is true, SOUL.md is not included (loaded separately). */
char *context_build_files_prompt(const char *cwd, bool skip_soul);

/* ================================================================
 *  Platform hints (PLATFORM_HINTS dict from prompt_builder.py)
 * ================================================================ */

/* Get platform-specific hint string for system prompt. Returns a static
 * string or NULL if platform is unknown. */
const char *platform_hint_get(const char *platform_name);

/* ================================================================
 *  Environment hints (build_environment_hints from prompt_builder.py)
 * ================================================================ */

/* Build environment-specific guidance string (WSL, host info, remote
 * backends). Returns malloc'd string or NULL. Caller must free. */
char *build_environment_hints(void);

/* ================================================================
 *  System prompt assembly
 * ================================================================ */

/* Configuration for building a system prompt */
typedef struct {
    /* Stable tier flags */
    bool     use_soul;          /* true = try SOUL.md, false = use DEFAULT_IDENTITY */
    bool     has_memory;        /* inject MEMORY_GUIDANCE if memory tool available */
    bool     has_session_search;/* inject SESSION_SEARCH_GUIDANCE */
    bool     has_skills;        /* inject SKILLS_GUIDANCE */
    bool     has_kanban;        /* inject KANBAN_GUIDANCE */
    bool     has_computer_use;  /* inject COMPUTER_USE_GUIDANCE */
    bool     enforce_tools;     /* inject TOOL_ENFORCE guidance */
    bool     is_openai_family;  /* inject OPENAI_EXEC guidance */
    bool     is_google_family;  /* inject GOOGLE_OPS guidance */
    bool     is_alibaba;        /* inject Alibaba model-name workaround */
    const char *alibaba_model;  /* actual model name for Alibaba workaround */
    const char *env_hints;      /* WSL, Termux, etc. environment hints */
    const char *platform_hint;  /* Platform-specific hint (telegram, etc.) */

    /* Context tier */
    const char *system_message; /* caller-supplied system_message */
    const char *context_files;  /* AGENTS.md / .cursorrules content */

    /* Volatile tier */
    const char *memory_snapshot;   /* memory store formatted for prompt */
    const char *user_profile;      /* USER.md profile content */
    const char *ext_memory_block;  /* external memory provider block */
    const char *model_name;        /* model name for timestamp */
    const char *provider_name;     /* provider name for timestamp */
    const char *session_id;        /* session ID (optional) */
    bool         pass_session_id;  /* whether to include session_id */
} system_prompt_config_t;

/* Build a system prompt string. Returns malloc'd string, caller must free. */
char *system_prompt_build(const system_prompt_config_t *cfg);

/* Build the stable tier only (for caching) */
char *system_prompt_build_stable(const system_prompt_config_t *cfg);

/* Build the volatile tier only (for per-turn updates) */
char *system_prompt_build_volatile(const system_prompt_config_t *cfg);

/* ================================================================
 *  Utility: truncate content with head/tail marker
 * ================================================================ */

/* Truncate content with head/tail marker. Returns malloc'd string or NULL.
 * Caller must free. Always returns a copy (even if no truncation). */
char *context_truncate_content(const char *content, const char *name, int max_chars);

/* Strip YAML frontmatter from content. Returns malloc'd string (caller
 * must free) or NULL on error. */
char *context_strip_frontmatter(const char *content);

/* ================================================================
 *  Continuation prompt (port of conversation_loop._get_continuation_prompt)
 * ================================================================ */

/* Build a continuation prompt when the previous response was truncated.
 * is_partial_stub: true if a network/stream error cut the response.
 * dropped_tools_json: optional JSON array of tool names that were dropped
 *   due to size limits (pass NULL if none).
 * Returns a malloc'd string (caller must free) or NULL on error.
 * Port of Python conversation_loop._get_continuation_prompt(). */
char *agent_get_continuation_prompt(bool is_partial_stub,
                                     const char *dropped_tools_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SYSTEM_PROMPT_H */
