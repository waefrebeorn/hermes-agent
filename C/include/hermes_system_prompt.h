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
 */

#include "hermes.h"
#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SYSTEM_PROMPT_H */
