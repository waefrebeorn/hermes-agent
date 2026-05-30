/*
 * auxiliary_client.c — Resolve auxiliary task provider/model config.
 *
 * B04: Port of Python auxiliary_client.py's provider resolution chain.
 * Implements "auto" → main provider resolution and task-specific overrides.
 */

#include "auxiliary_client.h"
#include <string.h>
#include <stdio.h>

/* ─── Task name → label mapping ─── */

typedef struct {
    const char *name;
    const char *label;
} task_label_entry;

static const task_label_entry TASK_LABELS[] = {
    {"vision",             "vision analysis"},
    {"web_extract",        "web page extraction"},
    {"compression",        "context compression"},
    {"skills_hub",         "skill hub"},
    {"approval",           "tool approval"},
    {"mcp",                "MCP tool"},
    {"title_generation",   "title generation"},
    {"triage_specifier",   "triage specifier"},
    {"kanban_decomposer",  "kanban decomposition"},
    {"profile_describer",  "profile description"},
    {"curator",            "skill curation"},
    {NULL, NULL}
};

const char *auxiliary_task_label(const char *task_name) {
    if (!task_name) return "auxiliary";
    for (const task_label_entry *e = TASK_LABELS; e->name; e++) {
        if (strcmp(e->name, task_name) == 0)
            return e->label;
    }
    return task_name;
}

/* ─── Resolve auxiliary task config to llm_config_t ─── */

bool auxiliary_resolve_llm_config(const hermes_config_t *cfg,
                                   const auxiliary_task_config_t *task_cfg,
                                   llm_config_t *out)
{
    if (!cfg || !task_cfg || !out) return false;

    memset(out, 0, sizeof(*out));

    const char *resolved_provider = NULL;

    /* Determine provider: explicit name vs "auto" */
    if (task_cfg->provider[0] != '\0' &&
        strcmp(task_cfg->provider, "auto") != 0) {
        resolved_provider = task_cfg->provider;
    } else {
        resolved_provider = cfg->provider;
    }

    if (!resolved_provider || resolved_provider[0] == '\0')
        return false;

    /* Copy provider name */
    strncpy(out->provider, resolved_provider, sizeof(out->provider) - 1);

    /* Model: task override or main */
    if (task_cfg->model[0] != '\0') {
        strncpy(out->model, task_cfg->model, sizeof(out->model) - 1);
    } else {
        strncpy(out->model, cfg->model, sizeof(out->model) - 1);
    }

    /* API key: task override, credential pool, or main */
    if (task_cfg->api_key[0] != '\0') {
        strncpy(out->api_key, task_cfg->api_key, sizeof(out->api_key) - 1);
    } else {
        strncpy(out->api_key, cfg->api_key, sizeof(out->api_key) - 1);
    }

    /* Base URL: task override or main */
    if (task_cfg->base_url[0] != '\0') {
        strncpy(out->base_url, task_cfg->base_url, sizeof(out->base_url) - 1);
    } else {
        strncpy(out->base_url, cfg->base_url, sizeof(out->base_url) - 1);
    }

    /* Timeout from task config (llm_config_t doesn't have its own timeout;
     * consumers use this independently) */

    /* Copy main llm params: max_tokens, temperature, top_p, etc. */
    out->max_tokens         = cfg->provider_cfg.max_tokens;
    out->temperature        = cfg->provider_cfg.temperature;
    out->top_p              = cfg->provider_cfg.top_p;
    out->presence_penalty   = cfg->provider_cfg.presence_penalty;
    out->frequency_penalty  = cfg->provider_cfg.frequency_penalty;
    out->seed               = cfg->provider_cfg.seed;
    out->logprobs           = cfg->provider_cfg.logprobs;
    out->top_logprobs       = cfg->provider_cfg.top_logprobs;

    strncpy(out->stop_sequences[0],
            cfg->provider_cfg.stop_sequences[0],
            sizeof(out->stop_sequences[0]) - 1);
    out->stop_count = cfg->provider_cfg.stop_count;

    strncpy(out->user,             cfg->provider_cfg.user,             sizeof(out->user) - 1);
    strncpy(out->service_tier,     cfg->provider_cfg.service_tier,     sizeof(out->service_tier) - 1);
    strncpy(out->reasoning_effort, cfg->provider_cfg.reasoning_effort, sizeof(out->reasoning_effort) - 1);
    strncpy(out->response_format,  cfg->provider_cfg.response_format,  sizeof(out->response_format) - 1);
    strncpy(out->metadata,         cfg->provider_cfg.metadata,         sizeof(out->metadata) - 1);
    strncpy(out->tool_choice,      cfg->provider_cfg.tool_choice,      sizeof(out->tool_choice) - 1);
    strncpy(out->extra_body,       cfg->provider_cfg.extra_body,       sizeof(out->extra_body) - 1);
    strncpy(out->safety_settings,  cfg->provider_cfg.safety_settings,  sizeof(out->safety_settings) - 1);

    out->parallel_tool_calls  = cfg->provider_cfg.parallel_tool_calls;
    out->max_tool_calls       = cfg->provider_cfg.max_tool_calls;
    out->n                    = cfg->provider_cfg.n;
    out->top_k                = cfg->provider_cfg.top_k;
    out->candidate_count      = cfg->provider_cfg.candidate_count;
    out->json_mode            = cfg->provider_cfg.json_mode;
    out->response_format_strict = cfg->provider_cfg.response_format_strict;

    strncpy(out->azure_deployment_id,       cfg->provider_cfg.azure_deployment_id,
            sizeof(out->azure_deployment_id) - 1);
    strncpy(out->azure_api_version,         cfg->provider_cfg.azure_api_version,
            sizeof(out->azure_api_version) - 1);
    strncpy(out->openrouter_provider,       cfg->provider_cfg.openrouter_provider,
            sizeof(out->openrouter_provider) - 1);
    strncpy(out->bedrock_inference_profile, cfg->provider_cfg.bedrock_inference_profile,
            sizeof(out->bedrock_inference_profile) - 1);
    strncpy(out->bedrock_guardrail_config,  cfg->provider_cfg.bedrock_guardrail_config,
            sizeof(out->bedrock_guardrail_config) - 1);
    out->bedrock_trace_enabled = cfg->provider_cfg.bedrock_trace_enabled;

    strncpy(out->fallback_model,    cfg->provider_cfg.fallback_model,
            sizeof(out->fallback_model) - 1);
    strncpy(out->fallback_providers, cfg->provider_cfg.fallback_providers,
            sizeof(out->fallback_providers) - 1);

    return true;
}

/* ─── Resolve by task name ─── */

bool auxiliary_resolve_task(const hermes_config_t *cfg,
                             const char *task_name,
                             llm_config_t *out)
{
    if (!cfg || !task_name || !out) return false;

    const auxiliary_task_config_t *task_cfg = NULL;

    if      (strcmp(task_name, "vision")            == 0) task_cfg = &cfg->auxiliary.vision;
    else if (strcmp(task_name, "web_extract")       == 0) task_cfg = &cfg->auxiliary.web_extract;
    else if (strcmp(task_name, "compression")       == 0) task_cfg = &cfg->auxiliary.compression;
    else if (strcmp(task_name, "skills_hub")        == 0) task_cfg = &cfg->auxiliary.skills_hub;
    else if (strcmp(task_name, "approval")          == 0) task_cfg = &cfg->auxiliary.approval;
    else if (strcmp(task_name, "mcp")               == 0) task_cfg = &cfg->auxiliary.mcp;
    else if (strcmp(task_name, "title_generation")  == 0) task_cfg = &cfg->auxiliary.title_generation;
    else if (strcmp(task_name, "triage_specifier")  == 0) task_cfg = &cfg->auxiliary.triage_specifier;
    else if (strcmp(task_name, "kanban_decomposer") == 0) task_cfg = &cfg->auxiliary.kanban_decomposer;
    else if (strcmp(task_name, "profile_describer") == 0) task_cfg = &cfg->auxiliary.profile_describer;
    else if (strcmp(task_name, "curator")           == 0) task_cfg = &cfg->auxiliary.curator;
    else return false;

    return auxiliary_resolve_llm_config(cfg, task_cfg, out);
}
